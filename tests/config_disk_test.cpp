#define HOST_TEST 1
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#include "ConfigDisk.h"
#include "DaplinkExe.h"
#include "Protocol.h"

using namespace wd;

int main() {
  printf("Starting ConfigDisk unit tests...\n");

  configDisk.init();

  assert(configDisk.config().pairCode == 123456);
  assert(configDisk.config().channel == 80);
  assert(configDisk.config().txPower == 8);
  assert(configDisk.config().role == 0);
  assert(configDisk.config().protocol == 2);

  // 1. Read Sector 0 (BPB)
  uint8_t buf[512]={};
  int32_t r = ConfigDisk::readCallback(0, buf, sizeof(buf));
  assert(r == 512);
  assert(buf[0] == 0xEB && buf[1] == 0x3C && buf[2] == 0x90);
  assert(buf[19] == 0x00 && buf[20] == 0x08); // 2048 sectors total (1MB)
  assert(buf[22] == 0x06 && buf[23] == 0x00); // 6 sectors per FAT
  assert(buf[510] == 0x55 && buf[511] == 0xAA);
  assert(memcmp(buf + 3, "MSDOS5.0", 8) == 0);
  assert(memcmp(buf + 43, "DAPLINK    ", 11) == 0);

  // 2. Read Sector 1 (FAT Table)
  memset(buf, 0, sizeof(buf));
  r = ConfigDisk::readCallback(1, buf, sizeof(buf));
  assert(r == 512);
  assert(buf[0] == 0xF8 && buf[1] == 0xFF && buf[2] == 0xFF);
  assert(buf[3] == 0xFF && buf[4] == 0xFF && buf[5] == 0xFF); // Clusters 2 & 3

  // 3. Read Sector 7 (Root Directory)
  memset(buf, 0, sizeof(buf));
  r = ConfigDisk::readCallback(7, buf, sizeof(buf));
  assert(r == 512);
  assert(memcmp(buf, "DAPLINK    ", 11) == 0);
  assert(buf[11] == 0x08); // Volume ID

  // Entry 1: CONFIG.TXT (Cluster 2)
  assert(memcmp(buf + 32, "CONFIG  TXT", 11) == 0);
  assert(buf[32 + 26] == 0x02);

  // Entry 2: STATUS.TXT (Cluster 3)
  assert(memcmp(buf + 64, "STATUS  TXT", 11) == 0);
  assert(buf[64 + 26] == 0x03);

  // Entry 3: DAPLINK.EXE (Cluster 4)
  assert(memcmp(buf + 96, "DAPLINK EXE", 11) == 0);
  assert(buf[96 + 26] == 0x04);

  // 4. Read Sector 8 (CONFIG.TXT data)
  memset(buf, 0, sizeof(buf));
  r = ConfigDisk::readCallback(8, buf, sizeof(buf));
  assert(r == 512);
  assert((uint8_t)buf[0] == 0xEF && (uint8_t)buf[1] == 0xBB && (uint8_t)buf[2] == 0xBF); // UTF-8 BOM
  assert(strstr((char*)buf, "PAIR_CODE=123456") != nullptr);
  assert(strstr((char*)buf, "CHANNEL=80") != nullptr);
  assert(strstr((char*)buf, "TX_POWER=8") != nullptr);
  assert(strstr((char*)buf, "PROTOCOL=2") != nullptr);

  // 5. Read Sector 9 (STATUS.TXT data)
  uint8_t hops[4] = {80, 19, 40, 60};
  configDisk.setTelemetry(-45, 100, 2, 98, 0, true, false, true, 240, 1, hops);
  memset(buf, 0, sizeof(buf));
  r = ConfigDisk::readCallback(9, buf, sizeof(buf));
  assert(r == 512);
  assert((uint8_t)buf[0] == 0xEF && (uint8_t)buf[1] == 0xBB && (uint8_t)buf[2] == 0xBF); // UTF-8 BOM
  assert(strstr((char*)buf, "ROLE: HOST") != nullptr);
  assert(strstr((char*)buf, "LINK: 已连接") != nullptr);
  assert(strstr((char*)buf, "PAIR_CODE: 123456") != nullptr);
  assert(strstr((char*)buf, "CHANNEL: CH80") != nullptr);
  assert(strstr((char*)buf, "[AFH: 80, 19, 40, 60]") != nullptr);
  assert(strstr((char*)buf, "AFH_HOPS: 1 次切换") != nullptr);
  assert(strstr((char*)buf, "RSSI: -45 dBm") != nullptr);
  assert(strstr((char*)buf, "PKT_SIZE: 240") != nullptr);
  assert(strstr((char*)buf, "PROTOCOL: CMSIS-DAP v2") != nullptr);

  // 6. Read DAPLINK.EXE sectors (10..10+exeClusters-1)
  uint32_t exeLen = sizeof(DAPLINK_EXE);
  uint32_t exeClusters = (exeLen + 511) / 512;
  uint8_t assembledExe[65536] = {};
  for (uint32_t s = 0; s < exeClusters; s++) {
    r = ConfigDisk::readCallback(10 + s, assembledExe + s * 512, 512);
    assert(r == 512);
  }
  assert(memcmp(assembledExe, DAPLINK_EXE, exeLen) == 0);
  assert(assembledExe[0] == 'M' && assembledExe[1] == 'Z');
  printf("DAPLINK.EXE verified 100%% bit-for-bit (%u bytes across sectors 10..%u)!\n",
         (unsigned int)exeLen, (unsigned int)(10 + exeClusters - 1));

  // 7. Test modifying CONFIG.TXT via virtual write (including protocol switch!)
  const char* newConfig = 
    "# Custom Settings\r\n"
    "PAIR_CODE=888999\r\n"
    "CHANNEL=76\r\n"
    "TX_POWER=4\r\n"
    "ROLE=HOST\r\n"
    "PROTOCOL=1\r\n";
  memset(buf, 0, sizeof(buf));
  strncpy((char*)buf, newConfig, sizeof(buf) - 1);
  
  ConfigDisk::writeCallback(8, buf, sizeof(buf));
  ConfigDisk::flushCallback();

  assert(configDisk.config().pairCode == 888999);
  assert(configDisk.config().channel == 76);
  assert(configDisk.config().txPower == 4);
  assert(configDisk.config().role == 1);
  assert(configDisk.config().protocol == 1);
  assert(configDisk.isFlashPending() == true);
  assert(configDisk.isRebootPending() == true);

  configDisk.commitToFlash();
  assert(configDisk.isFlashPending() == false);
  printf("Config modification, protocol switch (v1/v2), and flash persistence - PASS\n");

  printf("PASS: ConfigDisk 1MB VFS, AFH telemetry, Protocol switch, and Flash persistence!\n");
  return 0;
}
