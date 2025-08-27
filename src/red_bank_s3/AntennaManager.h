#pragma once

#include "configuration.h"
#include "mesh/generated/meshtastic/config.pb.h"

/**
 * @brief 天线管理器类
 *
 * 负责根据LoRa区域配置自动切换天线
 */
class AntennaManager
{
public:
    /**
     * @brief 初始化天线管理器
     */
    static void init(meshtastic_Config_LoRaConfig_RegionCode newRegion);

    /**
     * @brief 根据区域切换天线
     * @param newRegion 新的区域设置
     */
    static void switchAntennaForRegion(meshtastic_Config_LoRaConfig_RegionCode newRegion);

    /**
     * @brief 获取当前天线状态
     * @return 当前激活的天线类型
     */
    static const char* getCurrentAntennaType();

    /**
     * @brief 检查是否需要切换天线
     * @param oldRegion 旧区域
     * @param newRegion 新区域
     * @return 是否需要切换天线
     */
    static bool needsAntennaSwitch(meshtastic_Config_LoRaConfig_RegionCode oldRegion,
                                  meshtastic_Config_LoRaConfig_RegionCode newRegion);

private:
    static void switchTo432MHzAntenna();
    static void switchTo900MHzAntenna();
    static void sendAntennaSwitchNotification(const char* message);

    // 天线状态跟踪，当前（旧的）使用的天线类型，
    static meshtastic_Config_LoRaConfig_RegionCode currentRegion;
};