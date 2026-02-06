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
#endif

// powerStatus 是在 main.cpp 全局命名空间里定义的单例指针
extern meshtastic::PowerStatus *powerStatus;

namespace Esp32PowerLog
{

  // Storage key (ESP32) or filename (NRF52)
  static constexpr const char *PWRLOG_NS = "pwrlog";
  static constexpr const char *PWRLOG_KEY = "blob";
  static constexpr const char *PWRLOG_FILE = "/prefs/pwrlog.bin";
  static constexpr uint16_t PWRLOG_CAPACITY = 168; // 7天*24小时，按需改小/改大

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

  static bool pwrlogLoad(PowerLogBlob &b)
  {
#if defined(ARDUINO_ARCH_ESP32)
    Preferences p;
    if (!p.begin(PWRLOG_NS, true))
      return false;
    size_t need = sizeof(PowerLogBlob);
    size_t gotLen = p.getBytesLength(PWRLOG_KEY);
    bool ok = (gotLen == need) && (p.getBytes(PWRLOG_KEY, &b, need) == need);
    p.end();
    if (!ok)
      return false;
#elif defined(ARCH_NRF52)
#ifdef FSCom
    concurrency::LockGuard g(spiLock);
    if (!FSCom.exists(PWRLOG_FILE))
      return false;
    auto f = FSCom.open(PWRLOG_FILE, FILE_O_READ);
    if (!f)
      return false;
    size_t got = f.readBytes((char *)&b, sizeof(b));
    f.close();
    if (got != sizeof(b))
      return false;
#else
    return false;
#endif
#else
    return false;
#endif
    if (b.magic != PWRLOG_MAGIC || b.capacity != PWRLOG_CAPACITY)
      return false;
    if (b.count > PWRLOG_CAPACITY || b.head >= PWRLOG_CAPACITY)
      return false;
    return true;
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
#if defined(ARDUINO_ARCH_ESP32)
    Preferences p;
    if (!p.begin(PWRLOG_NS, false))
      return false;
    size_t wrote = p.putBytes(PWRLOG_KEY, &b, sizeof(b));
    p.end();
    return wrote == sizeof(b);
#elif defined(ARCH_NRF52)
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

  static void pwrlogSampleAndStoreOnceImpl()
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
  static void pwrlogDumpImpl(uint16_t maxLines /* 0=全部 */)
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
  static void pwrlogClearImpl()
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
  static concurrency::Periodic *pwrlogPeriodic = nullptr;
  static uint32_t pwrlogIntervalMs = 60U * 60U * 1000U;

  static int32_t pwrlogTick()
  {
    pwrlogSampleAndStoreOnceImpl();
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

  void PwrLogSampleAndStoreOnce()
  {
    pwrlogSampleAndStoreOnceImpl();
  }

  void PwrLogDump(uint16_t maxLines)
  {
    pwrlogDumpImpl(maxLines);
  }

  void PwrLogClear()
  {
    pwrlogClearImpl();
  }

} // namespace Esp32PowerLog
#endif // ESP32 || NRF52