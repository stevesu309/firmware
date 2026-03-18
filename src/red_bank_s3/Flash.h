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
  void PwrLogDump(uint16_t maxLines = 72);

  /// 清除已保存的数据（删除 Preferences 的 key）。
  void PwrLogClear();

  /// nRF52 外置 QSPI Flash 探测与自检（非 nRF52 平台为 no-op）
  void ExtFlashSelfTest();

  /// 外置 Flash 原始读写接口（非支持平台返回 false）
  bool ExtFlashRawReady();
  bool ExtFlashRawRead(uint32_t addr, void *buf, uint32_t len);
  bool ExtFlashRawWrite(uint32_t addr, const void *buf, uint32_t len);
  bool ExtFlashRawErase(uint32_t addr, uint32_t len);

  /// 中文字库镜像导入接口：用于主机分块上传并直接写入外置 flash。
  bool ExtFlashBeginChineseFontUpload();
  bool ExtFlashWriteChineseFontUploadChunk(uint32_t offset, const void *buf, uint32_t len);
  bool ExtFlashFinishChineseFontUpload(uint32_t totalBytes);
  void ExtFlashAbortChineseFontUpload();

} // namespace Esp32PowerLog
