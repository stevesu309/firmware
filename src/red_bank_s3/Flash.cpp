// Power log storage backend:
// - ESP32: Preferences (NVS)
// - NRF52: InternalFS (LittleFS) file under /prefs
#include "configuration.h" // defines ARCH_NRF52 via architecture.h on nRF52 builds

#if defined(ARDUINO_ARCH_ESP32) || defined(ARCH_NRF52)
#include "Flash.h"
#include "PowerStatus.h"
#include <Arduino.h>
#include "concurrency/Periodic.h"
#include <cstring>
#include <time.h>

#if defined(ARDUINO_ARCH_ESP32)
#include <Preferences.h>
#elif defined(ARCH_NRF52)
#include "FSCommon.h"
#include "SPILock.h"
#if defined(EXTERNAL_FLASH_USE_QSPI) && __has_include(<Adafruit_SPIFlash.h>)
// Some nRF52 variants (e.g. t-echo) don't define generic SS, but Adafruit_SPIFlash headers reference it.
#ifndef SS
#define SS PIN_QSPI_CS
#endif
#include <Adafruit_SPIFlash.h>
#include <flash_devices.h>
#define PWRLOG_USE_EXTERNAL_QSPI 1
#endif
#endif

// powerStatus 是在 main.cpp 全局命名空间里定义的单例指针
extern meshtastic::PowerStatus *powerStatus;

namespace Esp32PowerLog
{

  // Storage key (ESP32) or filename (NRF52)
  static constexpr const char *PWRLOG_NS = "pwrlog";
  static constexpr const char *PWRLOG_KEY = "blob";
  static constexpr const char *PWRLOG_FILE = "/prefs/pwrlog.bin";
  static constexpr uint16_t PWRLOG_CAPACITY = 72;

#pragma pack(push, 1)
  struct PowerLogEntry
  {
    uint32_t unixTime;  // 有效时间（无效则为0）
    uint32_t uptimeSec; // millis()/1000
    uint16_t battMv;    // 电池电压(mV)
    uint8_t battPct;    // 电量%
    uint8_t flags;      // bit0 hasBat, bit1 hasUSB, bit2 charging, bit3 timeValid
  };
  struct PowerLogBlob
  {
    uint32_t magic; // 'PLG1'
    uint16_t capacity;
    uint16_t count;
    uint16_t head; // 下一次写入位置
    PowerLogEntry entries[PWRLOG_CAPACITY];
  };
#pragma pack(pop)

  static constexpr uint32_t PWRLOG_MAGIC = 0x504C4731; // "PLG1"
  // Reserve one 4KB sector right before Chinese font raw region (0x00300000).
  static constexpr uint32_t PWRLOG_EXT_ADDR = 0x002FF000U;

#if defined(ARCH_NRF52) && defined(PWRLOG_USE_EXTERNAL_QSPI)
  static Adafruit_FlashTransport_QSPI pwrlogExtFlashTransport;
  static Adafruit_SPIFlash pwrlogExtFlash(&pwrlogExtFlashTransport);
  static bool extFlashInitDone = false;
  static bool extFlashReady = false;
  static constexpr uint32_t kExtFlashSectorSize = 4096;

  static uint32_t jedecCapacityCodeToBytes(uint8_t capacityCode)
  {
    return (capacityCode < 32U) ? (1UL << capacityCode) : 0UL;
  }

  static void probeRawExternalFlash()
  {
    uint8_t jedec[4] = {0};
    uint8_t sr1 = 0;
    uint8_t sr2 = 0;

    pwrlogExtFlashTransport.begin();
    const bool jedecOk = pwrlogExtFlashTransport.readCommand(SFLASH_CMD_READ_JEDEC_ID, jedec, sizeof(jedec));
    const bool sr1Ok = pwrlogExtFlashTransport.readCommand(SFLASH_CMD_READ_STATUS, &sr1, 1);
    const bool sr2Ok = pwrlogExtFlashTransport.readCommand(SFLASH_CMD_READ_STATUS2, &sr2, 1);
    pwrlogExtFlashTransport.end();

    LOG_INFO("[EXTTEST] probe JEDEC cmd=%s raw=%02x %02x %02x %02x", jedecOk ? "OK" : "FAIL", jedec[0], jedec[1], jedec[2],
             jedec[3]);
    LOG_INFO("[EXTTEST] probe SR1=%s 0x%02x SR2=%s 0x%02x", sr1Ok ? "OK" : "FAIL", sr1, sr2Ok ? "OK" : "FAIL", sr2);

    if (jedecOk)
    {
      uint8_t normType = jedec[1];
      uint8_t normCap = jedec[2];
      if (normType == 0x7F)
      {
        normType = jedec[2];
        normCap = jedec[3];
      }

      const uint32_t sizeBytes = jedecCapacityCodeToBytes(normCap);
      LOG_INFO("[EXTTEST] probe normalized mfg=0x%02x type=0x%02x cap=0x%02x size_guess=%luKB", jedec[0], normType, normCap,
               (unsigned long)(sizeBytes / 1024U));
    }
  }

  static bool beginExternalFlashWithConfiguredDevice()
  {
    // Keep device descriptor alive for the whole runtime.
    // Some SPIFlash implementations may retain this pointer after begin().
    static SPIFlash_Device_t gd25q32cType60 = []() {
      SPIFlash_Device_t d = (SPIFlash_Device_t)GD25Q32C;
      d.memory_type = 0x60;
      return d;
    }();

    if (pwrlogExtFlash.begin(&gd25q32cType60, 1))
    {
      LOG_INFO("[EXTTEST] begin(GD25Q32C type=0x60) => OK");
      return true;
    }

    LOG_INFO("[EXTTEST] begin(GD25Q32C type=0x60) => FAIL");
    return false;
  }

  static bool ensureExtFlashReady()
  {
    if (extFlashInitDone)
      return extFlashReady;
    extFlashInitDone = true;

    if (!beginExternalFlashWithConfiguredDevice())
    {
      LOG_WARN("[EXTTEST] raw flash begin failed");
      return false;
    }

    extFlashReady = true;
    LOG_INFO("[EXTTEST] raw flash ready: JEDEC=0x%08lx size=%luKB",
             (unsigned long)pwrlogExtFlash.getJEDECID(),
             (unsigned long)(pwrlogExtFlash.size() / 1024U));
    return true;
  }

  bool ExtFlashRawReady()
  {
    return ensureExtFlashReady();
  }

  bool ExtFlashRawRead(uint32_t addr, void *buf, uint32_t len)
  {
    if (!buf || len == 0 || !ensureExtFlashReady())
      return false;
    return pwrlogExtFlash.readBuffer(addr, static_cast<uint8_t *>(buf), len) == len;
  }

  bool ExtFlashRawWrite(uint32_t addr, const void *buf, uint32_t len)
  {
    if (!buf || len == 0 || !ensureExtFlashReady())
      return false;
    return pwrlogExtFlash.writeBuffer(addr, static_cast<const uint8_t *>(buf), len) == len;
  }

  bool ExtFlashRawErase(uint32_t addr, uint32_t len)
  {
    if (len == 0 || !ensureExtFlashReady())
      return false;

    const uint32_t startSector = addr / kExtFlashSectorSize;
    const uint32_t endAddr = addr + len - 1U;
    const uint32_t endSector = endAddr / kExtFlashSectorSize;
    for (uint32_t s = startSector; s <= endSector; ++s)
    {
      if (!pwrlogExtFlash.eraseSector(s))
      {
        return false;
      }
    }
    pwrlogExtFlash.waitUntilReady();
    return true;
  }

  void ExtFlashSelfTest()
  {
    LOG_INFO("[EXTTEST] start");
    probeRawExternalFlash();

    if (!ensureExtFlashReady())
    {
      LOG_WARN("[EXTTEST] flash begin failed");
      return;
    }
  }
#else
  void ExtFlashSelfTest() {}
  bool ExtFlashRawReady() { return false; }
  bool ExtFlashRawRead(uint32_t, void *, uint32_t) { return false; }
  bool ExtFlashRawWrite(uint32_t, const void *, uint32_t) { return false; }
  bool ExtFlashRawErase(uint32_t, uint32_t) { return false; }
#endif

  static bool pwrlogLoadBackend(PowerLogBlob &b)
  {
#if defined(ARDUINO_ARCH_ESP32)
    Preferences p;
    if (!p.begin(PWRLOG_NS, true))
      return false;
    size_t need = sizeof(PowerLogBlob);
    size_t gotLen = p.getBytesLength(PWRLOG_KEY);
    LOG_INFO("[PWRLOG] load: gotLen=%u need=%u", (unsigned)gotLen, (unsigned)need);
    bool ok = (gotLen == need) && (p.getBytes(PWRLOG_KEY, &b, need) == need);
    LOG_INFO("[PWRLOG] load: magic=%08x capacity=%u count=%u head=%u", (unsigned)b.magic, (unsigned)b.capacity, (unsigned)b.count,
             (unsigned)b.head);
    p.end();
    return ok;
#elif defined(ARCH_NRF52)
#if defined(PWRLOG_USE_EXTERNAL_QSPI)
    if (!ExtFlashRawReady())
    {
      LOG_WARN("[PWRLOG][EXT] raw flash not ready, fallback internal");
    }
    else
    {
      if (sizeof(PowerLogBlob) > kExtFlashSectorSize)
      {
        LOG_WARN("[PWRLOG][EXT] blob too large (%u), fallback internal", (unsigned)sizeof(PowerLogBlob));
      }
      else if (ExtFlashRawRead(PWRLOG_EXT_ADDR, &b, sizeof(b)))
      {
        LOG_INFO("[PWRLOG][EXT] load: readBytes=%u need=%u", (unsigned)sizeof(b), (unsigned)sizeof(b));
        LOG_INFO("[PWRLOG] load source: external");
        return true;
      }
      else
      {
        LOG_WARN("[PWRLOG][EXT] load failed, fallback internal");
      }
    }
#endif
#ifdef FSCom
    concurrency::LockGuard g(spiLock);
    if (!FSCom.exists(PWRLOG_FILE))
    {
      LOG_WARN("[PWRLOG] load: file not exist");
      return false;
    }
    auto f = FSCom.open(PWRLOG_FILE, FILE_O_READ);
    if (!f)
    {
      LOG_WARN("[PWRLOG] load: open failed");
      return false;
    }
    size_t got = f.readBytes((char *)&b, sizeof(b));
    f.close();
    LOG_INFO("[PWRLOG] load: readBytes=%u need=%u", (unsigned)got, (unsigned)sizeof(b));
    if (got != sizeof(b))
    {
      LOG_WARN("[PWRLOG] load: size mismatch");
      return false;
    }
    LOG_INFO("[PWRLOG] load source: internal");
    LOG_INFO("[PWRLOG] load: magic=%08x capacity=%u count=%u head=%u", (unsigned)b.magic, (unsigned)b.capacity, (unsigned)b.count,
             (unsigned)b.head);
    return true;
#else
    return false;
#endif
#else
    return false;
#endif
  }

  static bool pwrlogSaveBackend(const PowerLogBlob &b)
  {
#if defined(ARDUINO_ARCH_ESP32)
    Preferences p;
    if (!p.begin(PWRLOG_NS, false))
      return false;
    size_t wrote = p.putBytes(PWRLOG_KEY, &b, sizeof(b));
    p.end();
    return wrote == sizeof(b);
#elif defined(ARCH_NRF52)
#if defined(PWRLOG_USE_EXTERNAL_QSPI)
    if (ExtFlashRawReady())
    {
      if (sizeof(PowerLogBlob) > kExtFlashSectorSize)
      {
        LOG_WARN("[PWRLOG][EXT] blob too large (%u), fallback internal", (unsigned)sizeof(PowerLogBlob));
      }
      else if (ExtFlashRawErase(PWRLOG_EXT_ADDR, kExtFlashSectorSize) &&
               ExtFlashRawWrite(PWRLOG_EXT_ADDR, &b, sizeof(b)))
      {
        LOG_INFO("[PWRLOG][EXT] save: wrote=%u addr=0x%08x", (unsigned)sizeof(b), (unsigned)PWRLOG_EXT_ADDR);
        return true;
      }
      else
      {
        LOG_WARN("[PWRLOG][EXT] save failed, fallback internal");
      }
    }
    else
    {
      LOG_WARN("[PWRLOG][EXT] raw flash not ready, fallback internal");
    }
#endif
#ifdef FSCom
    // Ensure /prefs exists
    {
      concurrency::LockGuard g(spiLock);
      FSCom.mkdir("/prefs");
    }

    concurrency::LockGuard g(spiLock);
    auto f = FSCom.open(PWRLOG_FILE, FILE_O_WRITE); // overwrite
    if (!f)
      return false;
    size_t wrote = f.write((uint8_t *)&b, sizeof(b));
    f.flush();
    f.close();
    return wrote == sizeof(b);
#else
    return false;
#endif
#else
    return false;
#endif
  }

  static void pwrlogClearBackend()
  {
#if defined(ARDUINO_ARCH_ESP32)
    Preferences p;
    if (!p.begin(PWRLOG_NS, false))
    {
      LOG_WARN("[PWRLOG] clear: begin failed");
      return;
    }
    bool ok = p.remove(PWRLOG_KEY); // 删除 blob key
    p.end();
    LOG_INFO("[PWRLOG] clear: %s", ok ? "OK" : "NOT_FOUND/FAIL");
#elif defined(ARCH_NRF52)
#if defined(PWRLOG_USE_EXTERNAL_QSPI)
    if (ExtFlashRawReady())
    {
      const bool extOk = ExtFlashRawErase(PWRLOG_EXT_ADDR, kExtFlashSectorSize);
      LOG_INFO("[PWRLOG][EXT] clear: %s addr=0x%08x", extOk ? "OK" : "FAIL", (unsigned)PWRLOG_EXT_ADDR);
      if (extOk)
      {
        return;
      }
    }
    else
    {
      LOG_WARN("[PWRLOG][EXT] raw flash not ready, fallback internal");
    }
#endif
#ifdef FSCom
    concurrency::LockGuard g(spiLock);
    bool ok = FSCom.remove(PWRLOG_FILE);
    LOG_INFO("[PWRLOG] clear file: %s", ok ? "OK" : "NOT_FOUND/FAIL");
#else
    LOG_WARN("[PWRLOG] clear: no filesystem");
#endif
#else
    LOG_WARN("[PWRLOG] clear: unsupported platform");
#endif
  }

  static bool isValidPwrlogBlob(const PowerLogBlob &b)
  {
    if (b.magic != PWRLOG_MAGIC || b.capacity != PWRLOG_CAPACITY)
      return false;
    if (b.count > PWRLOG_CAPACITY || b.head >= PWRLOG_CAPACITY)
      return false;
    return true;
  }

  static bool pwrlogLoad(PowerLogBlob &b)
  {
    if (!pwrlogLoadBackend(b))
      return false;
    return isValidPwrlogBlob(b);
  }

  static void pwrlogInit(PowerLogBlob &b)
  {
    memset(&b, 0, sizeof(b));
    b.magic = PWRLOG_MAGIC;
    b.capacity = PWRLOG_CAPACITY;
    b.count = 0;
    b.head = 0;
  }

  static bool pwrlogSave(const PowerLogBlob &b)
  {
    return pwrlogSaveBackend(b);
  }

  void PwrLogSampleAndStoreOnce()
  {
    PowerLogBlob b;
    if (!pwrlogLoad(b))
      pwrlogInit(b);

    time_t now = time(NULL);
    bool timeValid = (now > 1700000000); // 简单判定：大于2023年左右才算已校时

    bool hasBat = ::powerStatus ? ::powerStatus->getHasBattery() : false;
    bool hasUsb = ::powerStatus ? ::powerStatus->getHasUSB() : false;
    bool chg = ::powerStatus ? ::powerStatus->getIsCharging() : false;
    uint16_t mv = ::powerStatus ? (uint16_t)max(0, ::powerStatus->getBatteryVoltageMv()) : 0;
    uint8_t pct = ::powerStatus ? (uint8_t)::powerStatus->getBatteryChargePercent() : 0;

    PowerLogEntry e{};
    e.unixTime = timeValid ? (uint32_t)now : 0;
    e.uptimeSec = (uint32_t)(millis() / 1000UL);
    e.battMv = mv;
    e.battPct = pct;
    e.flags = (hasBat ? 0x01 : 0) | (hasUsb ? 0x02 : 0) | (chg ? 0x04 : 0) | (timeValid ? 0x08 : 0);

    b.entries[b.head] = e;
    b.head = (uint16_t)((b.head + 1) % PWRLOG_CAPACITY);
    if (b.count < PWRLOG_CAPACITY)
      b.count++;

    if (!pwrlogSave(b))
    {
      LOG_WARN("[PWRLOG] save failed");
    }
    else
    {
      LOG_INFO("[PWRLOG] saved: t=%u up=%us mv=%u pct=%u usb=%u chg=%u count=%u",
               (unsigned)e.unixTime, (unsigned)e.uptimeSec, (unsigned)e.battMv, (unsigned)e.battPct,
               (e.flags & 0x02) ? 1 : 0, (e.flags & 0x04) ? 1 : 0, (unsigned)b.count);
    }
  }
  void PwrLogDump(uint16_t maxLines)
  {
    PowerLogBlob b;
    if (!pwrlogLoad(b))
    {
      LOG_WARN("[PWRLOG] no data");
      return;
    }

    uint16_t count = b.count;
    if (count > PWRLOG_CAPACITY)
      count = PWRLOG_CAPACITY;

    uint16_t n = count;
    if (maxLines != 0 && n > maxLines)
      n = maxLines;

    LOG_INFO("[PWRLOG] dump: count=%u head=%u show=%u", (unsigned)count, (unsigned)b.head, (unsigned)n);

    // 从最新往回打印：最新一条在 head-1
    for (uint16_t i = 0; i < n; i++)
    {
      int idx = (int)b.head - 1 - (int)i;
      while (idx < 0)
        idx += PWRLOG_CAPACITY;

      const PowerLogEntry &e = b.entries[idx];

      const int hasBat = (e.flags & 0x01) ? 1 : 0;
      const int hasUsb = (e.flags & 0x02) ? 1 : 0;
      const int chg = (e.flags & 0x04) ? 1 : 0;
      const int timeOk = (e.flags & 0x08) ? 1 : 0;

      LOG_INFO("[PWRLOG] #%u idx=%d t=%u timeOk=%d up=%us mv=%u pct=%u hasBat=%d usb=%d chg=%d",
               (unsigned)i, idx,
               (unsigned)e.unixTime, timeOk,
               (unsigned)e.uptimeSec,
               (unsigned)e.battMv,
               (unsigned)e.battPct,
               hasBat, hasUsb, chg);
    }
  }
  static concurrency::Periodic *pwrlogPeriodic = nullptr;
  static uint32_t pwrlogIntervalMs = 60U * 60U * 1000U;

  static int32_t pwrlogTick()
  {
    PwrLogSampleAndStoreOnce();
    return (int32_t)pwrlogIntervalMs;
  }

  void PwrLogStart(uint32_t intervalMs)
  {
    if (intervalMs == 0)
    {
      intervalMs = 60U * 60U * 1000U;
    }
    pwrlogIntervalMs = intervalMs;

    if (!pwrlogPeriodic)
    {
      pwrlogPeriodic = new concurrency::Periodic("PwrLog", pwrlogTick);
    }
  }

  void PwrLogClear()
  {
    pwrlogClearBackend();
  }

} // namespace Esp32PowerLog
#endif // ESP32 || NRF52