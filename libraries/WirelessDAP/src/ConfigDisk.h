#pragma once
#include <stdint.h>
#include <stddef.h>

#include <Adafruit_TinyUSB.h>

namespace wd {

#define DISK_BLOCK_NUM       2048
#define DISK_BLOCK_SIZE      512

struct DeviceConfig {
  uint32_t magic;      // 0x57444150 ("WDAP")
  uint32_t version;    // 1
  uint32_t pairCode;   // Default: 123456
  uint8_t  channel;    // Default: 80
  int8_t   txPower;    // Default: +8
  uint8_t  role;       // 0: AUTO, 1: HOST, 2: TARGET
  uint8_t  protocol;   // 0 or 2: CMSIS-DAP v2 (WinUSB), 1: CMSIS-DAP v1 (HID)
  uint32_t crc;
};

uint32_t calcConfigCrc(const DeviceConfig& cfg);

class ConfigDisk {
public:
  void init();
  void begin();
  const DeviceConfig& config() const { return config_; }
  DeviceConfig& config() { return config_; }

  void setTelemetry(int rssi, uint32_t tx, uint32_t retries, uint32_t rx,
                    uint32_t crcErrors, bool isHost, bool isLocal, bool isReady,
                    size_t pktSize, uint32_t afhHops = 0, const uint8_t* hopChs = nullptr);

  bool isRebootPending() const { return rebootPending_; }
  uint32_t rebootAt() const { return rebootAt_; }
  void clearReboot() { rebootPending_ = false; }

  bool isFlashPending() const { return flashPending_; }
  void commitToFlash();

  // MSC Callbacks
  static int32_t readCallback(uint32_t lba, void* buffer, uint32_t bufsize);
  static int32_t writeCallback(uint32_t lba, uint8_t* buffer, uint32_t bufsize);
  static void flushCallback();

  bool handleSectorWrite(uint32_t lba, const uint8_t* buffer, uint32_t bufsize);
  bool handleFlush();
  void refreshConfigText();
  void refreshStatusText();

  static ConfigDisk* instance() { return instance_; }

private:
  DeviceConfig config_{};
  Adafruit_USBD_MSC usbMsc_;
  bool rebootPending_ = false;
  uint32_t rebootAt_ = 0;
  bool flashPending_ = false;

  // Telemetry cache
  int rssi_ = 0;
  uint32_t txCount_ = 0;
  uint32_t retries_ = 0;
  uint32_t rxCount_ = 0;
  uint32_t crcErrors_ = 0;
  bool isHost_ = false;
  bool isLocal_ = false;
  bool isReady_ = false;
  size_t pktSize_ = 240;
  uint32_t afhHops_ = 0;
  uint8_t hopChannels_[4] = {80, 19, 40, 60};

  // File text buffers
  char configText_[512]={};
  size_t configTextLen_ = 0;
  char statusText_[512]={};
  size_t statusTextLen_ = 0;
  bool writeDirty_ = false;

  // VFS RAM structures:
  // Sector 0: Boot sector (512B)
  // Sectors 1..6: FAT table (3072B)
  // Sector 7: Root Directory (512B)
  // Sector 8: CONFIG.TXT (512B)
  // Sector 9: STATUS.TXT (512B)
  // Sectors 10..N: DAPLINK.EXE
  uint8_t bootSector_[512]={};
  uint8_t fatTable_[3072]={};
  uint8_t rootDir_[512]={};
  uint8_t configSector_[512]={};
  uint8_t statusSector_[512]={};

  void setFat12Entry(uint16_t cluster, uint16_t val);
  uint16_t getFat12Entry(uint16_t cluster) const;

  static ConfigDisk* instance_;
};

extern ConfigDisk configDisk;

} // namespace wd
