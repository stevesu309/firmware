#include "AntennaManager.h"
#include "main.h"
#include "mesh/MeshService.h"
#include "NodeDB.h"
#include "DebugConfiguration.h"
#include "variant.h"

// 天线状态跟踪
meshtastic_Config_LoRaConfig_RegionCode AntennaManager::currentRegion = meshtastic_Config_LoRaConfig_RegionCode_UNSET;

void AntennaManager::init(meshtastic_Config_LoRaConfig_RegionCode newRegion)
{
    // 由于切换地区会重启设备，重启后会重新初始化 RadioLib，
    // 这里只记录当前区域，实际选择哪个 LoRa 模块（CS 引脚）
    // 由 main.cpp 中的 SX1262 初始化逻辑根据 config.lora.region 决定。

    // 根据区域设置初始天线状态（逻辑状态）
    // 注意：不要先设置 currentRegion，让 switchAntennaForRegion 来更新它
    if (newRegion != meshtastic_Config_LoRaConfig_RegionCode_UNSET)
    {
        switchAntennaForRegion(newRegion);
    }
    else
    {
        // 如果区域未设置，默认使用900MHz天线
        switchTo900MHzAntenna();
        currentRegion = meshtastic_Config_LoRaConfig_RegionCode_UNSET;
    }

    LOG_INFO("AntennaManager initialized with region=%d", newRegion);
}

void AntennaManager::switchAntennaForRegion(meshtastic_Config_LoRaConfig_RegionCode newRegion)
{
    if (!needsAntennaSwitch(currentRegion, newRegion))
    {
        return;
    }

    LOG_INFO("Switching antenna: old region=%d, new region=%d", currentRegion, newRegion);

    // 根据区域切换天线
    switch (newRegion)
    {
    case meshtastic_Config_LoRaConfig_RegionCode_CN:
    case meshtastic_Config_LoRaConfig_RegionCode_EU_433:
    case meshtastic_Config_LoRaConfig_RegionCode_UA_433:
    case meshtastic_Config_LoRaConfig_RegionCode_MY_433:
    case meshtastic_Config_LoRaConfig_RegionCode_PH_433:
        switchTo433MHzAntenna();
        break;

    case meshtastic_Config_LoRaConfig_RegionCode_US:
    case meshtastic_Config_LoRaConfig_RegionCode_ANZ:
    case meshtastic_Config_LoRaConfig_RegionCode_JP:
    case meshtastic_Config_LoRaConfig_RegionCode_KR:
    case meshtastic_Config_LoRaConfig_RegionCode_TW:
    case meshtastic_Config_LoRaConfig_RegionCode_TH:
    case meshtastic_Config_LoRaConfig_RegionCode_MY_919:
    case meshtastic_Config_LoRaConfig_RegionCode_SG_923:
    case meshtastic_Config_LoRaConfig_RegionCode_PH_915:
        switchTo900MHzAntenna();
        break;

    case meshtastic_Config_LoRaConfig_RegionCode_EU_868:
    case meshtastic_Config_LoRaConfig_RegionCode_RU:
    case meshtastic_Config_LoRaConfig_RegionCode_IN:
    case meshtastic_Config_LoRaConfig_RegionCode_NZ_865:
    case meshtastic_Config_LoRaConfig_RegionCode_UA_868:
    case meshtastic_Config_LoRaConfig_RegionCode_PH_868:
        // 868MHz区域，可能需要特殊处理或使用900MHz天线
        switchTo900MHzAntenna();
        break;

    default:
        LOG_WARN("Unknown region %d, using default antenna", newRegion);
        switchTo900MHzAntenna(); // 默认使用900MHz天线
        break;
    }

    currentRegion = newRegion;
}

bool AntennaManager::needsAntennaSwitch(meshtastic_Config_LoRaConfig_RegionCode oldRegion,
                                        meshtastic_Config_LoRaConfig_RegionCode newRegion)
{
    // 如果区域没有变化，不需要切换
    if (oldRegion == newRegion)
    {
        return false;
    }

    // 如果是从未设置状态初始化，需要切换
    if (oldRegion == meshtastic_Config_LoRaConfig_RegionCode_UNSET)
    {
        return true;
    }

    // 检查是否需要切换天线类型
    bool oldIs432MHz = (oldRegion == meshtastic_Config_LoRaConfig_RegionCode_CN ||
                        oldRegion == meshtastic_Config_LoRaConfig_RegionCode_EU_433 ||
                        oldRegion == meshtastic_Config_LoRaConfig_RegionCode_UA_433 ||
                        oldRegion == meshtastic_Config_LoRaConfig_RegionCode_MY_433 ||
                        oldRegion == meshtastic_Config_LoRaConfig_RegionCode_PH_433);

    bool newIs433MHz = (newRegion == meshtastic_Config_LoRaConfig_RegionCode_CN ||
                        newRegion == meshtastic_Config_LoRaConfig_RegionCode_EU_433 ||
                        newRegion == meshtastic_Config_LoRaConfig_RegionCode_UA_433 ||
                        newRegion == meshtastic_Config_LoRaConfig_RegionCode_MY_433 ||
                        newRegion == meshtastic_Config_LoRaConfig_RegionCode_PH_433);

    return oldIs432MHz != newIs433MHz;
}

void AntennaManager::switchTo433MHzAntenna()
{
    // 对于 RED_BANK_S3，切换到 433MHz 仅意味着：
    // - 在逻辑上当前区域为 433MHz（CN/EU_433 等）
    // - 实际使用哪一个 LoRa 模块由 main.cpp 中的 csPin 选择决定
    LOG_INFO("Switching logical antenna to 432MHz band (handled via CS pin selection)");
    // #ifdef LORA_ANT
    //     digitalWrite(LORA_ANT, LOW); // 开启432MHz天线 关闭900MHz天线
    // #endif

    // #ifdef LORA_ANT_413
    //     digitalWrite(LORA_ANT_413, HIGH); // 开启432MHz天线
    // #endif

    // sendAntennaSwitchNotification("Antenna switched to 432MHz");
}

void AntennaManager::switchTo900MHzAntenna()
{
    LOG_INFO("Switching logical antenna to 900MHz band (handled via CS pin selection)");
    // #ifdef LORA_ANT_413
    //     digitalWrite(LORA_ANT_413, LOW); // 关闭432MHz天线
    // #endif
    // #ifdef LORA_ANT
    //     digitalWrite(LORA_ANT, HIGH); // 开启900MHz天线
    // #endif

    // sendAntennaSwitchNotification("Antenna switched to 900MHz");
}

const char *AntennaManager::getCurrentAntennaType()
{
    // 根据当前区域返回当前天线类型
    switch (currentRegion)
    {
    // 如果当前区域是CN、EU_433、UA_433、MY_433、PH_433，则返回432MHz
    case meshtastic_Config_LoRaConfig_RegionCode_CN:
    case meshtastic_Config_LoRaConfig_RegionCode_EU_433:
    case meshtastic_Config_LoRaConfig_RegionCode_UA_433:
    case meshtastic_Config_LoRaConfig_RegionCode_MY_433:
    case meshtastic_Config_LoRaConfig_RegionCode_PH_433:
        return "433MHz";

    // 如果当前区域是US、ANZ、JP、KR、TW、TH、MY_919、SG_923、PH_915、EU_868、RU、IN、NZ_865、UA_868、PH_868，则返回900MHz
    case meshtastic_Config_LoRaConfig_RegionCode_US:
    case meshtastic_Config_LoRaConfig_RegionCode_ANZ:
    case meshtastic_Config_LoRaConfig_RegionCode_JP:
    case meshtastic_Config_LoRaConfig_RegionCode_KR:
    case meshtastic_Config_LoRaConfig_RegionCode_TW:
    case meshtastic_Config_LoRaConfig_RegionCode_TH:
    case meshtastic_Config_LoRaConfig_RegionCode_MY_919:
    case meshtastic_Config_LoRaConfig_RegionCode_SG_923:
    case meshtastic_Config_LoRaConfig_RegionCode_PH_915:
    case meshtastic_Config_LoRaConfig_RegionCode_EU_868:
    case meshtastic_Config_LoRaConfig_RegionCode_RU:
    case meshtastic_Config_LoRaConfig_RegionCode_IN:
    case meshtastic_Config_LoRaConfig_RegionCode_NZ_865:
    case meshtastic_Config_LoRaConfig_RegionCode_UA_868:
    case meshtastic_Config_LoRaConfig_RegionCode_PH_868:
        return "900MHz";

    // 默认返回Unknown
    default:
        return "Unknown";
    }
}

// 向天线发送切换通知
void AntennaManager::sendAntennaSwitchNotification(const char *message)
{
    // // 如果服务存在
    // if (service) {
    //     // 分配一个客户端通知对象，并初始化为0
    //     meshtastic_ClientNotification *cn = clientNotificationPool.allocZeroed();
    //     // 设置通知级别为INFO
    //     cn->level = meshtastic_LogRecord_Level_INFO;
    //     // 将消息复制到通知对象中
    //     strncpy(cn->message, message, sizeof(cn->message) - 1);
    //     // 确保消息以null结尾
    //     cn->message[sizeof(cn->message) - 1] = '\0';
    //     // 发送通知
    //     service->sendClientNotification(cn);
    // }
}