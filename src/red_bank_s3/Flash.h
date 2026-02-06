#pragma once

#include <stdint.h>

// 通用 ESP32 电量日志工具（不绑定具体硬件型号）
namespace Esp32PowerLog
{

  /**
   * 在 Flash(NVS/Preferences) 中周期性保存电量日志（ring buffer），并支持手动读取/清除。
   *
   * - 保存内容：unixTime（若无有效时间则为0）、uptimeSec、电池电压(mV)、电量(%)、USB/充电标志
   * - 存储位置：Preferences namespace="pwrlog", key="blob"
   */

  /// 启动周期性保存（默认每小时一次）。重复调用会更新周期。
  void PwrLogStart(uint32_t intervalMs = 60U * 60U * 1000U);

  void PwrLogSampleAndStoreOnce();

  /// 从 Flash 读取并打印最近 N 条（0=全部）。
  void PwrLogDump(uint16_t maxLines = 24);

  /// 清除已保存的数据（删除 Preferences 的 key）。
  void PwrLogClear();

} // namespace Esp32PowerLog