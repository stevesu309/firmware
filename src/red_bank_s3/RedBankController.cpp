#include "RedBankController.h"
// #include "esp32-hal-gpio.h"
#include "DebugConfiguration.h"
#include "main.h"
#include "FSCommon.h"
#include "../variants/red_bank_s3/variant.h"
#include "graphics/Screen.h"

namespace RedBankS3
{
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
        // pinMode(BUTTON_PRE_MESH_PACKET, INPUT_PULLDOWN);
        pinMode(BUTTON_NEX_MESH_PACKET, INPUT_PULLDOWN);
        pinMode(BUTTON_PRE_CHANNEL_PACKET, INPUT_PULLDOWN);
        pinMode(BUTTON_NEX_CHANNEL_PACKET, INPUT_PULLDOWN);
        pinMode(BUTTON_NEX_PAGE_PACKET, INPUT_PULLDOWN);

        // pinMode(GNSS_MPOW_CTRL_PIN, OUTPUT);
        // digitalWrite(GNSS_MPOW_CTRL_PIN, HIGH);
        // delay(10);
        // pinMode(GNSS_POW_CTRL_PIN, OUTPUT);
        // digitalWrite(GNSS_POW_CTRL_PIN, HIGH);
    }

    void RedBankController::loop()
    {
        _handlePreMeshPacketButtonPress();
        _handleNextMeshPacketButtonPress();
        // _handleShuttingDownButtonPress();
        _handlePrePageButtonPress();
        _handleNextPageButtonPress();
        // _handleNextPages();
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
#if defined(RED_BANK_S3)
    void RedBankController::_previousMeshPacket()
    {

        // screen->showPrevPacket(); 注释屏幕

        direction = 0;
    }

    void RedBankController::_nextMeshPacket()
    {

        // screen->showNextPacket(); 注释屏幕

        direction = 1;
    }
#endif
    uint8_t RedBankController::getDirection(void)
    {
        return (direction);
    }

    void RedBankController::_handlePreMeshPacketButtonPress() // 上一条信息
    {
        static bool lastPreMeshPacketButtonState = HIGH;
        bool curState = digitalRead(42);
        // bool curState = digitalRead(42);
        if (lastPreMeshPacketButtonState != curState && curState == HIGH)
        {
            _previousMeshPacket();
        }
        lastPreMeshPacketButtonState = curState;
    }

    void RedBankController::_handleNextMeshPacketButtonPress() // 下一条信息
    {
        static bool lastNextMeshPacketButtonState = HIGH;
        bool curState = digitalRead(BUTTON_NEX_MESH_PACKET);
        if (lastNextMeshPacketButtonState != curState && curState == HIGH)
        {
            _nextMeshPacket();
        }
        lastNextMeshPacketButtonState = curState;
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

#if defined(RED_BANK_S3)
    void RedBankController::_handlePrePageButtonPress() // 上一帧
    {
        static bool lastButtonState = HIGH;
        bool curState = digitalRead(BUTTON_PRE_CHANNEL_PACKET);
        if (lastButtonState != curState && curState == HIGH)
        {
            if (screen)
            {
                // screen->showPrevFrame(); 注释屏幕
            }
        }

        lastButtonState = curState;
    }

    void RedBankController::_handleNextPageButtonPress() // 下一帧
    {
        static bool lastNextMeshPacketButtonState = HIGH;
        bool curState = digitalRead(BUTTON_NEX_CHANNEL_PACKET);
        if (lastNextMeshPacketButtonState != curState && curState == HIGH)
        {
            if (screen)
            {
                // screen->showNextFrame(); 注释屏幕
            }
        }
        lastNextMeshPacketButtonState = curState;
    }
#endif

}