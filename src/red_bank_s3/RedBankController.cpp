#include "RedBankController.h"
#include <Arduino.h>
#include "DebugConfiguration.h"
#include "main.h"
#include "FSCommon.h"
#include "../variants/red_bank_s3/variant.h"
#include "graphics/Screen.h"
#include "AntennaManager.h"
namespace RedBankS3
{
#define KEY1_ADC_PIN 1 // IO1
#define KEY2_ADC_PIN 2 // IO2
    static int key1_adc_val = 0;
    static int key2_adc_val = 0;

    static KeypadKey key1_last = KeypadKey::NONE;
    static KeypadKey key2_last = KeypadKey::NONE;

    static KeypadKey detect_key(int adc_value, bool isKey1)
    {
        if (adc_value > 3200 && adc_value < 3800)
            return isKey1 ? KeypadKey::UP : KeypadKey::DOWN;
        else if (adc_value > 1800 && adc_value < 3200)
            return isKey1 ? KeypadKey::ESC : KeypadKey::RIGHT;
        else if (adc_value > 300 && adc_value < 1800)
            return isKey1 ? KeypadKey::ENTER : KeypadKey::LEFT;
        else
            return KeypadKey::NONE;
    }

    void RedBankController::scanAdcKeypad()
    {
        int val1 = analogRead(KEY1_ADC_PIN);
        int val2 = analogRead(KEY2_ADC_PIN);
        key1_adc_val = val1;
        key2_adc_val = val2;
        key1_last = detect_key(val1, true);
        key2_last = detect_key(val2, false);
    }

    KeypadKey RedBankController::getKey1() { return key1_last; }
    KeypadKey RedBankController::getKey2() { return key2_last; }

    RedBankController::RedBankController()
    {
        // m_meshPacketList = new std::vector<meshtastic_MeshPacket>();
        m_currentMeshPacketIndex = -1;
        restoreChannelPackets();
    }
    RedBankController::~RedBankController()
    {
        // delete m_meshPacketList;
        m_currentMeshPacketIndex = -1;
    }

    void RedBankController::setup()
    {
        pinMode(KEY1_ADC_PIN, INPUT);
        pinMode(KEY2_ADC_PIN, INPUT);
        // 初始化天线管理器
        AntennaManager::init(config.lora.region);
    }

    void RedBankController::loop()
    {
        // 使用天线管理器处理天线切换
        AntennaManager::switchAntennaForRegion(config.lora.region);
        scanAdcKeypad(); // ADC按键扫描
        if (screen)
        {
            delay(200);
            switch (key1_last)
            {
            case KeypadKey::UP:
                screen->showPrevPacket();
                break;
            case KeypadKey::ENTER:
                LOG_INFO("RedBankController: key1_last == KeypadKey::ENTER");
                break;
            case KeypadKey::ESC:
                LOG_INFO("RedBankController: key1_last == KeypadKey::ESC");
                break;
            default:
                break;
            }

            switch (key2_last)
            {
            case KeypadKey::DOWN:
                screen->showNextPacket();
                break;
            case KeypadKey::LEFT:
                screen->showPrevFrame();
                break;
            case KeypadKey::RIGHT:
                screen->showNextFrame();
                break;
            default:
                break;
            }
        }
    }

    bool RedBankController::isMeshPacketListEmpty(uint8_t channel)
    {
        if (channel >= 8)
        {
            LOG_INFO("RedBankController: incorrect channel num = 0x%x!", channel);
            return (false);
        }

        return channelPackets[channel].empty();
    }

    void RedBankController::push_packet(uint8_t channel_index, const meshtastic_MeshPacket &mp)
    {
        if (channelPackets[channel_index].size() >= MESH_PACKET_LIST_CAPCITY)
        {
            channelPackets[channel_index].erase(channelPackets[channel_index].begin());
        }

        channelPackets[channel_index].push_back(mp);
    }

    void RedBankController::saveMeshPacket(const meshtastic_MeshPacket &mp)
    {
        uint8_t i;
        LOG_INFO("mp:to = 0x%x", mp.to);
        if (mp.channel >= 8)
        {
            LOG_INFO("RedBankController: incorrect mp.channel = 0x%x!", mp.channel);
            return;
        }

        push_packet(mp.channel, mp);

        for (i = 0; i < channelPackets[mp.channel].size(); ++i)
        {
            nodeDB->saveChannelPacketToDisk(mp.channel, i, channelPackets[mp.channel].at(i));
        }
    }

    void RedBankController::restoreChannelPackets(void)
    {
        uint8_t i = 0, j = 0;
        meshtastic_MeshPacket mp;

        for (i = 0; i < 8; ++i)
        {
            LOG_INFO("restoring channel %d packets...", i);
            channelPackets[i].erase(channelPackets[i].begin(), channelPackets[i].end());

            for (j = 0; j < 10; ++j)
            {
                if (nodeDB->restoreMeshPacket(i, j, mp))
                {
                    push_packet(i, mp);
                }
            }
        }
    }

    meshtastic_MeshPacket RedBankController::getRecentMeshPacket(uint8_t channel, uint8_t recent_index)
    {
        if ((channel >= 8) || (recent_index >= channelPackets[channel].size()))
        {
            LOG_INFO("RedBankController: invalid argument, channel num = 0x%x, recent_index = %d!", channel, recent_index);

            meshtastic_MeshPacket emptyPacket;
            return emptyPacket;
        }

        return (channelPackets[channel].at(recent_index));
    }

    int RedBankController::_getMeshPacketListSize(uint8_t channel)
    {
        return (channelPackets[channel].size());
    }
    void RedBankController::_previousMeshPacket()
    {

        screen->showPrevPacket();

        direction = 0;
    }

    void RedBankController::_nextMeshPacket()
    {

        screen->showNextPacket();

        direction = 1;
    }
    uint8_t RedBankController::getDirection(void)
    {
        return (direction);
    }

    // void RedBankController::_handleShuttingDownButtonPress()
    // {
    //     static bool lastShuttingDownButtonState = HIGH;
    //     bool curState = digitalRead(BUTTON_PRE_CHANNEL_PACKET);
    //     if (lastShuttingDownButtonState != curState && curState == HIGH)
    //     {
    //         if (screen)
    //             screen->startAlert("Shutting down...");
    //         shutdownAtMsec = millis() + DEFAULT_SHUTDOWN_SECONDS * 1000;
    //     }
    //     lastShuttingDownButtonState = curState;
    // }

}