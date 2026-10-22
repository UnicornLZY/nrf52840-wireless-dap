#include "ConfigDisk.h"
#include "DaplinkExe.h"
#include "Protocol.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#if !defined(HOST_TEST)
#include <Arduino.h>
#include <nrf.h>
#endif

namespace wd {

#define WD_CONFIG_FLASH_ADDR 0xED000UL

ConfigDisk* ConfigDisk::instance_ = nullptr;
ConfigDisk configDisk;

uint32_t calcConfigCrc(const DeviceConfig& cfg) {
  const uint8_t* p = (const uint8_t*)&cfg;
  uint32_t sum = 0x5a5a5a5aUL;
  for (size_t i = 0; i < offsetof(DeviceConfig, crc); i++) {
    sum = (sum << 5) - sum + p[i];
  }
  return sum;
}

#if !defined(HOST_TEST)
static void flashErasePage(uint32_t addr) {
  NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Een << NVMC_CONFIG_WEN_Pos;
  while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {}
  NRF_NVMC->ERASEPAGE = addr;
  while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {}
  NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren << NVMC_CONFIG_WEN_Pos;
  while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {}
}

static void flashWriteWords(uint32_t addr, const uint32_t* src, size_t numWords) {
  NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Wen << NVMC_CONFIG_WEN_Pos;
  while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {}
  volatile uint32_t* dst = (volatile uint32_t*)addr;
  for (size_t i = 0; i < numWords; i++) {
    dst[i] = src[i];
    while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {}
  }
  NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren << NVMC_CONFIG_WEN_Pos;
  while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {}
}
#else
static uint8_t mockFlash[4096] = {};
static void flashErasePage(uint32_t) { memset(mockFlash, 0xFF, sizeof(mockFlash)); }
static void flashWriteWords(uint32_t, const uint32_t* src, size_t numWords) {
  memcpy(mockFlash, src, numWords * 4);
}
#endif

void ConfigDisk::setFat12Entry(uint16_t cluster, uint16_t val) {
  uint32_t byteOffset = (cluster * 3) / 2;
  if (byteOffset + 1 >= sizeof(fatTable_)) return;
  if (cluster & 1) {
    fatTable_[byteOffset] = (fatTable_[byteOffset] & 0x0F) | ((val & 0x0F) << 4);
    fatTable_[byteOffset + 1] = (val >> 4) & 0xFF;
  } else {
    fatTable_[byteOffset] = val & 0xFF;
    fatTable_[byteOffset + 1] = (fatTable_[byteOffset + 1] & 0xF0) | ((val >> 8) & 0x0F);
  }
}

uint16_t ConfigDisk::getFat12Entry(uint16_t cluster) const {
  uint32_t byteOffset = (cluster * 3) / 2;
  if (byteOffset + 1 >= sizeof(fatTable_)) return 0xFFF;
  if (cluster & 1) {
    return ((uint16_t)(fatTable_[byteOffset] >> 4)) | ((uint16_t)fatTable_[byteOffset + 1] << 4);
  } else {
    return ((uint16_t)fatTable_[byteOffset]) | (((uint16_t)(fatTable_[byteOffset + 1] & 0x0F)) << 8);
  }
}

void ConfigDisk::init() {
  instance_ = this;
  
  // Try loading from Flash
#if !defined(HOST_TEST)
  const DeviceConfig* flashCfg = (const DeviceConfig*)WD_CONFIG_FLASH_ADDR;
#else
  const DeviceConfig* flashCfg = (const DeviceConfig*)mockFlash;
#endif

  if (flashCfg->magic == 0x57444150 && flashCfg->version == 1 &&
      flashCfg->crc == calcConfigCrc(*flashCfg)) {
    config_ = *flashCfg;
  } else {
    // Default Configuration
    config_.magic = 0x57444150;
    config_.version = 1;
    config_.pairCode = 123456; // 默认配对码 123456
    config_.channel = 80;      // 默认信道 80 (2480 MHz 避开 Wi-Fi)
    config_.txPower = 8;       // 默认功率 +8 dBm
    config_.role = 0;          // 0: AUTO, 1: HOST, 2: TARGET
    config_.protocol = 2;      // 默认 CMSIS-DAP v2 (WinUSB 极速版)
    config_.crc = calcConfigCrc(config_);
  }

  refreshConfigText();
  refreshStatusText();

  // Clear VFS structures
  memset(bootSector_, 0, sizeof(bootSector_));
  memset(fatTable_, 0, sizeof(fatTable_));
  memset(rootDir_, 0, sizeof(rootDir_));
  memset(configSector_, 0, sizeof(configSector_));
  memset(statusSector_, 0, sizeof(statusSector_));

  // Sector 0: Boot Sector (BPB, Superfloppy FAT12 format, 2048 sectors = 1MB)
  static const uint8_t bpb[62] = {
    0xEB, 0x3C, 0x90,
    'M', 'S', 'D', 'O', 'S', '5', '.', '0',
    0x00, 0x02,       // 512 bytes / sector
    0x01,             // 1 sector / cluster
    0x01, 0x00,       // 1 reserved sector (Sector 0)
    0x01,             // 1 FAT table
    0x10, 0x00,       // 16 root entries (Sector 7)
    0x00, 0x08,       // 2048 sectors total (1MB)
    0xF8,             // Media descriptor
    0x06, 0x00,       // 6 sectors / FAT (Sectors 1..6)
    0x01, 0x00,       // 1 sector / track
    0x01, 0x00,       // 1 head
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x80, 0x00, 0x29,
    0x34, 0x12, 0x00, 0x00,
    'D', 'A', 'P', 'L', 'I', 'N', 'K', ' ', ' ', ' ', ' ',
    'F', 'A', 'T', '1', '2', ' ', ' ', ' '
  };
  memcpy(bootSector_, bpb, sizeof(bpb));
  bootSector_[510] = 0x55;
  bootSector_[511] = 0xAA;

  // Sectors 1..6: FAT12 Table
  setFat12Entry(0, 0xFF8); // Media byte
  setFat12Entry(1, 0xFFF);
  setFat12Entry(2, 0xFFF); // Cluster 2: CONFIG.TXT (Sector 8)
  setFat12Entry(3, 0xFFF); // Cluster 3: STATUS.TXT (Sector 9)

  uint32_t exeLen = sizeof(DAPLINK_EXE);
  uint32_t exeClusters = (exeLen + 511) / 512;

  // Cluster 4..(4 + exeClusters - 1): DAPLINK.EXE (Sectors 10..10+exeClusters-1)
  for (uint32_t c = 0; c < exeClusters - 1; c++) {
    setFat12Entry(4 + c, 4 + c + 1);
  }
  if (exeClusters > 0) {
    setFat12Entry(4 + exeClusters - 1, 0xFFF);
  }

  // Sector 7: Root Directory
  // Entry 0: Volume Label
  memcpy(rootDir_, "DAPLINK    ", 11);
  rootDir_[11] = 0x08; // Volume ID

  // Entry 1: CONFIG.TXT (Cluster 2)
  uint8_t* e1 = rootDir_ + 32;
  memcpy(e1, "CONFIG  TXT", 11);
  e1[11] = 0x20; // Archive
  e1[22] = 0x4F; e1[23] = 0x6D; // Time
  e1[24] = 0x65; e1[25] = 0x43; // Date
  e1[26] = 0x02; e1[27] = 0x00; // Cluster 2
  uint32_t cfgLen = configTextLen_;
  memcpy(e1 + 28, &cfgLen, 4);

  // Entry 2: STATUS.TXT (Cluster 3)
  uint8_t* e2 = rootDir_ + 64;
  memcpy(e2, "STATUS  TXT", 11);
  e2[11] = 0x21; // Read-Only | Archive
  e2[22] = 0x4F; e2[23] = 0x6D;
  e2[24] = 0x65; e2[25] = 0x43;
  e2[26] = 0x03; e2[27] = 0x00; // Cluster 3
  uint32_t statusSize = statusTextLen_;
  memcpy(e2 + 28, &statusSize, 4);

  // Entry 3: DAPLINK.EXE (Cluster 4)
  uint8_t* e3 = rootDir_ + 96;
  memcpy(e3, "DAPLINK EXE", 11);
  e3[11] = 0x21; // Read-Only | Archive
  e3[22] = 0x4F; e3[23] = 0x6D;
  e3[24] = 0x65; e3[25] = 0x43;
  e3[26] = 0x04; e3[27] = 0x00; // Cluster 4
  memcpy(e3 + 28, &exeLen, 4);

  memcpy(configSector_, configText_, configTextLen_);
  memcpy(statusSector_, statusText_, statusTextLen_);
}

void ConfigDisk::begin() {
  usbMsc_.setID("WDAP", "Wireless DAPLink", "4.00");
  usbMsc_.setCapacity(DISK_BLOCK_NUM, DISK_BLOCK_SIZE);
  usbMsc_.setReadWriteCallback(readCallback, writeCallback, flushCallback);
  usbMsc_.setUnitReady(true);
  usbMsc_.begin();
}

void ConfigDisk::refreshConfigText() {
  const char* roleStr = (config_.role == 1) ? "HOST" : ((config_.role == 2) ? "TARGET" : "AUTO");
  memset(configText_, 0, sizeof(configText_));
  configTextLen_ = snprintf(configText_, sizeof(configText_),
    "\xEF\xBB\xBF"
    "# ==========================================\r\n"
    "# nRF52840 Wireless DAPLink Configuration\r\n"
    "# ==========================================\r\n"
    "# 配对码 (两板须相同, 默认 123456)\r\n"
    "PAIR_CODE=%lu\r\n"
    "\r\n"
    "# 射频信道 (0~80, 默认 80 避开 WiFi)\r\n"
    "CHANNEL=%u\r\n"
    "\r\n"
    "# 发射功率 (+8, +4, 0, -4, -8, -12, -16, -20 dBm)\r\n"
    "TX_POWER=%d\r\n"
    "\r\n"
    "# 运行角色 (AUTO, HOST, TARGET)\r\n"
    "ROLE=%s\r\n"
    "\r\n"
    "# DAP协议 (2: v2 WinUSB极速, 1: v1 HID兼容)\r\n"
    "PROTOCOL=%u\r\n",
    (unsigned long)config_.pairCode,
    (unsigned int)config_.channel,
    (int)config_.txPower,
    roleStr,
    (config_.protocol == 1) ? 1 : 2
  );

  if (instance_) {
    memset(configSector_, 0, 512);
    memcpy(configSector_, configText_, configTextLen_);
    uint32_t cfgLen = configTextLen_;
    memcpy(rootDir_ + 32 + 28, &cfgLen, 4);
  }
}

void ConfigDisk::refreshStatusText() {
  const char* rStr = isHost_ ? "HOST (电脑端)" : "TARGET (目标侧)";
  if (isLocal_) rStr = "TARGET (有线直连)";
  const char* lStr = isReady_ ? "已连接 (Connected)" : "等待连接 (Connecting...)";
  const char* protoStr = (config_.protocol == 1) ? "CMSIS-DAP v1 (HID 兼容)" : "CMSIS-DAP v2 (WinUSB 极速)";
  float retRate = (txCount_ > 0) ? ((float)retries_ / (txCount_ + 1) * 100.0f) : 0.0f;
  float crcRate = ((rxCount_ + crcErrors_) > 0) ? ((float)crcErrors_ / (rxCount_ + crcErrors_ + 1) * 100.0f) : 0.0f;

  memset(statusText_, 0, sizeof(statusText_));
  int stLen = snprintf(statusText_, sizeof(statusText_),
    "\xEF\xBB\xBF"
    "# nRF52840 Wireless DAPLink 实时状态\r\n"
    "ROLE: %s\r\n"
    "LINK: %s\r\n"
    "PROTOCOL: %s\r\n"
    "PAIR_CODE: %lu\r\n"
    "CHANNEL: CH%u (%uMHz) [AFH: %u, %u, %u, %u]\r\n"
    "AFH_HOPS: %lu 次切换\r\n"
    "TX_POWER: %+d dBm\r\n"
    "RSSI: %d dBm\r\n"
    "PKT_SIZE: %u 字节\r\n"
    "TX_STAT: %lu 包 (重发 %lu 次 / %.1f%%)\r\n"
    "RX_STAT: %lu 包 (错包 %lu 次 / %.1f%%)\r\n"
    "# 说明: 修改 CONFIG.TXT 保存生效。Keil 即插即用！\r\n",
    rStr, lStr,
    protoStr,
    (unsigned long)config_.pairCode,
    (unsigned int)config_.channel,
    (unsigned int)(2400 + config_.channel),
    hopChannels_[0], hopChannels_[1], hopChannels_[2], hopChannels_[3],
    (unsigned long)afhHops_,
    (int)config_.txPower,
    rssi_,
    (unsigned int)pktSize_,
    (unsigned long)txCount_,
    (unsigned long)retries_,
    retRate,
    (unsigned long)rxCount_,
    (unsigned long)crcErrors_,
    crcRate
  );
  if (stLen > 500) stLen = 500;
  statusTextLen_ = (stLen > 0) ? (size_t)stLen : 0;

  if (instance_) {
    memset(statusSector_, 0, 512);
    memcpy(statusSector_, statusText_, statusTextLen_);
    uint32_t sLen = statusTextLen_;
    memcpy(rootDir_ + 64 + 28, &sLen, 4);
  }
}

void ConfigDisk::setTelemetry(int rssi, uint32_t tx, uint32_t retries,
                              uint32_t rx, uint32_t crcErr, bool isHost,
                              bool isLocal, bool isReady, size_t pktSize,
                              uint32_t afhHops, const uint8_t* hopChs) {
  rssi_ = rssi;
  txCount_ = tx;
  retries_ = retries;
  rxCount_ = rx;
  crcErrors_ = crcErr;
  isHost_ = isHost;
  isLocal_ = isLocal;
  isReady_ = isReady;
  pktSize_ = pktSize;
  afhHops_ = afhHops;
  if (hopChs) {
    memcpy(hopChannels_, hopChs, 4);
  }
  refreshStatusText();
}

int32_t ConfigDisk::readCallback(uint32_t lba, void* buffer, uint32_t bufsize) {
  if (!instance_ || lba >= DISK_BLOCK_NUM) return -1;

  uint32_t exeLen = sizeof(DAPLINK_EXE);
  uint32_t exeClusters = (exeLen + 511) / 512;
  uint32_t exeStartLba = 10;
  uint32_t exeEndLba = exeStartLba + exeClusters;

  if (lba == 0) {
    memcpy(buffer, instance_->bootSector_, bufsize);
    return bufsize;
  }
  if (lba >= 1 && lba <= 6) {
    memcpy(buffer, instance_->fatTable_ + (lba - 1) * 512, bufsize);
    return bufsize;
  }
  if (lba == 7) {
    memcpy(buffer, instance_->rootDir_, bufsize);
    return bufsize;
  }
  if (lba == 8) {
    memcpy(buffer, instance_->configSector_, bufsize);
    return bufsize;
  }
  if (lba == 9) {
    memcpy(buffer, instance_->statusSector_, bufsize);
    return bufsize;
  }
  if (lba >= exeStartLba && lba < exeEndLba) {
    uint32_t offset = (lba - exeStartLba) * 512;
    memset(buffer, 0, bufsize);
    if (offset < exeLen) {
      uint32_t chunk = exeLen - offset;
      if (chunk > bufsize) chunk = bufsize;
      memcpy(buffer, DAPLINK_EXE + offset, chunk);
    }
    return bufsize;
  }

  // Data clusters: return empty/zeros
  memset(buffer, 0, bufsize);
  return bufsize;
}

int32_t ConfigDisk::writeCallback(uint32_t lba, uint8_t* buffer, uint32_t bufsize) {
  if (!instance_ || lba >= DISK_BLOCK_NUM) return -1;
  instance_->handleSectorWrite(lba, buffer, bufsize);
  return bufsize;
}

static bool containsKeyword(const uint8_t* buf, size_t len, const char* kw, size_t kwLen) {
  if (len < kwLen) return false;
  for (size_t i = 0; i <= len - kwLen; i++) {
    if (memcmp(buf + i, kw, kwLen) == 0) return true;
  }
  return false;
}

bool ConfigDisk::handleSectorWrite(uint32_t lba, const uint8_t* buffer, uint32_t bufsize) {
  if (lba >= 1 && lba <= 6) {
    uint32_t offset = (lba - 1) * 512;
    if (offset + bufsize <= sizeof(fatTable_)) {
      memcpy(fatTable_ + offset, buffer, bufsize);
    }
    return true;
  }

  if (lba == 7) {
    memcpy(rootDir_, buffer, bufsize);
    return true;
  }

  if (lba == 8 || (bufsize >= 16 && (containsKeyword(buffer, bufsize, "PAIR_CODE", 9) || containsKeyword(buffer, bufsize, "pair_code", 9)))) {
    memcpy(configSector_, buffer, bufsize);
    writeDirty_ = true;
    return true;
  }

  return true;
}

void ConfigDisk::flushCallback() {
  if (instance_) {
    instance_->handleFlush();
  }
}

static bool iequals(const char* a, const char* b) {
  while (*a && *b) {
    char ca = (*a >= 'A' && *a <= 'Z') ? (*a + 32) : *a;
    char cb = (*b >= 'A' && *b <= 'Z') ? (*b + 32) : *b;
    if (ca != cb) return false;
    a++; b++;
  }
  return *a == *b;
}

bool ConfigDisk::handleFlush() {
  // Check Sector 8 (CONFIG.TXT)
  const char* src = (const char*)configSector_;
  bool hasKey = (strstr(src, "PAIR_CODE") != nullptr || strstr(src, "pair_code") != nullptr);

  if (!writeDirty_ && !hasKey) {
    return false;
  }
  writeDirty_ = false;

  DeviceConfig newCfg = config_;
  bool modified = false;

  const char* p = src;
  const char* end = src + 512;
  char line[128];

  while (p < end && *p) {
    size_t lineLen = 0;
    while (p < end && *p != '\r' && *p != '\n' && *p != '\0' && lineLen < sizeof(line) - 1) {
      line[lineLen++] = *p++;
    }
    line[lineLen] = '\0';
    while (p < end && (*p == '\r' || *p == '\n')) p++;

    char* trim = line;
    while (*trim == ' ' || *trim == '\t') trim++;
    if (*trim == '#' || *trim == '\0') continue;

    char* eq = strchr(trim, '=');
    if (!eq) continue;

    *eq = '\0';
    char* key = trim;
    char* val = eq + 1;
    while (*val == ' ' || *val == '\t') val++;

    // Trim trailing spaces from key
    char* kEnd = key + strlen(key) - 1;
    while (kEnd > key && (*kEnd == ' ' || *kEnd == '\t')) *kEnd-- = '\0';

    // Trim trailing spaces from val
    char* vEnd = val + strlen(val) - 1;
    while (vEnd > val && (*vEnd == ' ' || *vEnd == '\t')) *vEnd-- = '\0';

    if (iequals(key, "PAIR_CODE")) {
      uint32_t v = (uint32_t)strtoul(val, nullptr, 10);
      if (v != newCfg.pairCode) { newCfg.pairCode = v; modified = true; }
    } else if (iequals(key, "CHANNEL")) {
      uint8_t ch = (uint8_t)atoi(val);
      if (ch <= 80 && ch != newCfg.channel) { newCfg.channel = ch; modified = true; }
    } else if (iequals(key, "TX_POWER")) {
      int pwr = atoi(val);
      if (pwr >= -20 && pwr <= 8 && (int8_t)pwr != newCfg.txPower) {
        newCfg.txPower = (int8_t)pwr; modified = true;
      }
    } else if (iequals(key, "ROLE")) {
      uint8_t r = 0;
      if (iequals(val, "HOST")) r = 1;
      else if (iequals(val, "TARGET")) r = 2;
      else r = 0;
      if (r != newCfg.role) { newCfg.role = r; modified = true; }
    } else if (iequals(key, "PROTOCOL")) {
      uint8_t proto = (uint8_t)atoi(val);
      if (proto != 1 && proto != 2) proto = 2;
      if (proto != newCfg.protocol) { newCfg.protocol = proto; modified = true; }
    }
  }

  if (modified) {
    newCfg.crc = calcConfigCrc(newCfg);
    config_ = newCfg;
    refreshConfigText();
    refreshStatusText();
    flashPending_ = true;
#if !defined(HOST_TEST)
    rebootAt_ = millis() + 600; // Reboot after 600ms to apply new configuration
#else
    rebootAt_ = 600;
#endif
    rebootPending_ = true;
    return true;
  }

  return false;
}

void ConfigDisk::commitToFlash() {
  if (!flashPending_) return;
  flashPending_ = false;

#if !defined(HOST_TEST)
  flashErasePage(WD_CONFIG_FLASH_ADDR);
  flashWriteWords(WD_CONFIG_FLASH_ADDR, (const uint32_t*)&config_, sizeof(config_) / 4);
#else
  flashErasePage(0);
  flashWriteWords(0, (const uint32_t*)&config_, sizeof(config_) / 4);
#endif
}

} // namespace wd
