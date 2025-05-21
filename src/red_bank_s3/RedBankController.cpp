#include "RedBankController.h"
#include "esp32-hal-gpio.h"
#include "DebugConfiguration.h"
#include "main.h"
#include "FSCommon.h"
#include "../variants/red_bank_s3/variant.h"
#include "graphics/Screen.h"

namespace RedBankS3
{
    RedBankController::RedBankController()
    {
        m_meshPacketList = new std::vector<meshtastic_MeshPacket>();
        m_currentMeshPacketIndex = -1;
    }
    RedBankController::~RedBankController()
    {
        delete m_meshPacketList;
        m_currentMeshPacketIndex = -1;
    }

    void RedBankController::setup()
    {
        pinMode(BUTTON_PRE_MESH_PACKET, INPUT_PULLDOWN);
        pinMode(BUTTON_NEX_MESH_PACKET, INPUT_PULLDOWN);
        pinMode(BUTTON_PRE_CHANNEL_PACKET, INPUT_PULLDOWN);
        pinMode(BUTTON_NEX_CHANNEL_PACKET, INPUT_PULLDOWN);
        pinMode(BUTTON_NEX_PAGE_PACKET, INPUT_PULLDOWN);
    }

    void RedBankController::loop()
    {
        _handlePreMeshPacketButtonPress();
        _handleNextMeshPacketButtonPress();
        // _handleShuttingDownButtonPress();
        _handlePrePageButtonPress();
        _handleNextPageButtonPress();
        _handleNextPages();
    }

    bool RedBankController::isMeshPacketListEmpty()
    {
        return m_meshPacketList->empty();
    }

    void RedBankController::saveMeshPacket(const meshtastic_MeshPacket &mp)
    {
        // 如果消息列表已满，删除最早的消息
        if (m_meshPacketList->size() >= MESH_PACKET_LIST_CAPCITY)
        {
            m_meshPacketList->erase(m_meshPacketList->begin());
        }
        // 添加新消息到列表
        m_meshPacketList->push_back(mp);
        // 更新当前消息索引
        m_currentMeshPacketIndex = _getMeshPacketListSize() - 1;
    }

    meshtastic_MeshPacket RedBankController::getCurrentMeshPacket()
    {
        // 如果消息列表为空，返回空消息
        if (isMeshPacketListEmpty())
        {
            meshtastic_MeshPacket emptyPacket;
            return emptyPacket;
        }
        // 返回当前消息
        return m_meshPacketList->at(m_currentMeshPacketIndex);
    }

    int RedBankController::_getMeshPacketListSize()
    {
        return m_meshPacketList->size();
    }

    void RedBankController::_previousMeshPacket()
    {
        if (m_currentMeshPacketIndex > 0)
        {
            m_currentMeshPacketIndex--;
        }
        else
        {
            m_currentMeshPacketIndex = _getMeshPacketListSize() - 1;
        }
    }

    void RedBankController::_nextMeshPacket()
    {
        if (m_currentMeshPacketIndex < _getMeshPacketListSize() - 1)
        {
            m_currentMeshPacketIndex++;
        }
        else
        {
            m_currentMeshPacketIndex = 0;
        }
    }

    void RedBankController::_handlePreMeshPacketButtonPress() // 上一条信息
    {
        static bool lastPreMeshPacketButtonState = HIGH;
        bool curState = digitalRead(BUTTON_PRE_MESH_PACKET);
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

    void RedBankController::_handleShuttingDownButtonPress()
    {
        static bool lastShuttingDownButtonState = HIGH;
        bool curState = digitalRead(BUTTON_PRE_CHANNEL_PACKET);
        if (lastShuttingDownButtonState != curState && curState == HIGH)
        {
            // 执行关机流程
            if (screen)
                screen->startAlert("Shutting down...");
            shutdownAtMsec = millis() + DEFAULT_SHUTDOWN_SECONDS * 1000;
        }
        lastShuttingDownButtonState = curState;
    }

    void RedBankController::_handlePrePageButtonPress() // 上一个channel
    {
        static bool lastButtonState = HIGH;
        bool curState = digitalRead(BUTTON_PRE_CHANNEL_PACKET);
        if (lastButtonState != curState && curState == HIGH)
        {
            selectedLine = max(0, selectedLine - 1);
        }
        lastButtonState = curState;
        printf("selectedLine: %d\n", selectedLine);
    }

    void RedBankController::_handleNextPageButtonPress() // 下一个channel
    {
        static bool lastButtonState = HIGH;
        bool curState = digitalRead(BUTTON_NEX_CHANNEL_PACKET);
        if (lastButtonState != curState && curState == HIGH)
        {
            int totalNodes = nodeDB->getNumMeshNodes();
            int lastVisible = min(nodesPerPage - 1, totalNodes - currentPageIndex - 1);
            selectedLine = min(lastVisible, selectedLine + 1);
        }
        lastButtonState = curState;
        printf("selectedLine: %d\n", selectedLine);
    }

    void RedBankController::_handleNextPages() // Node列表翻页
    {
        static bool lastButtonState = HIGH;
        bool curState = digitalRead(BUTTON_NEX_PAGE_PACKET);

        if (lastButtonState != curState && curState == HIGH)
        {
            int totalNodes = nodeDB->getNumMeshNodes();

            currentPageIndex += nodesPerPage;

            if (currentPageIndex >= totalNodes)
                currentPageIndex = 0;
        }

        lastButtonState = curState;
    }

}