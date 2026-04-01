#include "RedBankController.h"
#include <Arduino.h>
#include "DebugConfiguration.h"
#include "main.h"
#include "FSCommon.h"
#include "variant.h"
#include "graphics/Screen.h"
#include "graphics/EInkDisplay2.h"
// #include "GxEPD2_BW.h"
#include "AntennaManager.h"
#include "input/InputBroker.h"
#include <algorithm>
#include "meshUtils.h"
namespace RedBankS3
{
#define KEY1_ADC_PIN 1 // IO1
#define KEY2_ADC_PIN 2 // IO2

    // #define GxEPD_BLACK 0x0000
    // #define GxEPD_WHITE 0xFFFF

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

#if HAS_SCREEN
    // 根据屏幕旋转角度映射物理按键到逻辑按键
    KeypadKey RedBankController::mapKeyByRotation(KeypadKey physicalKey)
    {

        switch (currentRotation)
        {
        case 0:
            return physicalKey;

        case 3:
            // 逆时针旋转90度
            switch (physicalKey)
            {
            case KeypadKey::UP:
                return KeypadKey::RIGHT;
            case KeypadKey::DOWN:
                return KeypadKey::LEFT;
            case KeypadKey::LEFT:
                return KeypadKey::UP;
            case KeypadKey::RIGHT:
                return KeypadKey::DOWN;
            default:
                return physicalKey;
            }

        case 1:
            // 顺时针旋转90度
            switch (physicalKey)
            {
            case KeypadKey::UP:
                return KeypadKey::LEFT;
            case KeypadKey::DOWN:
                return KeypadKey::RIGHT;
            case KeypadKey::LEFT:
                return KeypadKey::DOWN;
            case KeypadKey::RIGHT:
                return KeypadKey::UP;
            default:
                return physicalKey;
            }

        default:
            // 其他角度保持默认
            return physicalKey;
        }
    }

    void RedBankController::scanAdcKeypad()
    {
        int val1 = analogRead(KEY1_ADC_PIN);
        int val2 = analogRead(KEY2_ADC_PIN);
        KeypadKey key1_current = detect_key(val1, true);
        KeypadKey key2_current = detect_key(val2, false);

        static uint32_t lastKeyChangeTime = 0;
        static KeypadKey key1_prev = KeypadKey::NONE;
        static KeypadKey key2_prev = KeypadKey::NONE;

        // 检测按键状态是否发生变化
        bool key1Changed = (key1_current != key1_prev);
        bool key2Changed = (key2_current != key2_prev);
        bool anyKeyChanged = key1Changed || key2Changed;

        // 如果按键状态发生变化，更新防抖时间戳
        if (anyKeyChanged)
        {
            uint32_t now = millis();
            // 如果距离上次按键变化时间太短（< 50ms），可能是抖动，忽略
            if (now - lastKeyChangeTime < 50)
            {
                return;
            }
            lastKeyChangeTime = now;
            key1_prev = key1_current;
            key2_prev = key2_current;
        }

        // 检查是否是组合按键（LEFT+UP 或 RIGHT+UP）
        bool isComboKey = (key1_current == KeypadKey::UP &&
                           (key2_current == KeypadKey::LEFT || key2_current == KeypadKey::RIGHT));

        if (!isComboKey)
        {
            // 使用mapKeyByRotation映射按键
            KeypadKey mappedKey1 = mapKeyByRotation(key1_current);
            KeypadKey mappedKey2 = mapKeyByRotation(key2_current);

            // 检测映射后的逻辑按键状态（需要同时检查两个ADC通道）
            bool leftPressed = (mappedKey1 == KeypadKey::LEFT) || (mappedKey2 == KeypadKey::LEFT);
            bool rightPressed = (mappedKey1 == KeypadKey::RIGHT) || (mappedKey2 == KeypadKey::RIGHT);
            bool upPressed = (mappedKey1 == KeypadKey::UP) || (mappedKey2 == KeypadKey::UP);
            bool downPressed = (mappedKey1 == KeypadKey::DOWN) || (mappedKey2 == KeypadKey::DOWN);
            bool physicalEnterPressed = (key1_current == KeypadKey::ENTER);
            bool physicalEscPressed = (key1_current == KeypadKey::ESC);

            // LEFT 按键处理
            if (leftPressed && !leftButtonPressed)
            {
                handleLeftButtonPress();
            }
            else if (!leftPressed && leftButtonPressed)
            {
                handleLeftButtonRelease();
            }

            // RIGHT 按键处理
            if (rightPressed && !rightButtonPressed)
            {
                handleRightButtonPress();
            }
            else if (!rightPressed && rightButtonPressed)
            {
                handleRightButtonRelease();
            }

            // ENTER 按键处理
            if (physicalEnterPressed && !enterButtonPressed)
            {
                handleEnterButtonPress();
            }
            else if (!physicalEnterPressed && enterButtonPressed)
            {
                handleEnterButtonRelease();
            }

            // ESC 按键处理
            if (physicalEscPressed && !escButtonPressed)
            {
                handleEscButtonPress();
            }
            else if (!physicalEscPressed && escButtonPressed)
            {
                handleEscButtonRelease();
            }

            // UP 按键处理
            if (upPressed && !upButtonPressed)
            {
                handleUpButtonPress();
            }
            else if (!upPressed && upButtonPressed)
            {
                handleUpButtonRelease();
            }

            // DOWN 按键处理
            if (downPressed && !downButtonPressed)
            {
                handleDownButtonPress();
            }
            else if (!downPressed && downButtonPressed)
            {
                handleDownButtonRelease();
            }

            // ENTER 按键长按检查（在按下期间持续检查）
            if (physicalEnterPressed && enterButtonPressed && !enterLongPressTriggered)
            {
                uint32_t pressDuration = millis() - enterButtonPressTime;
                bool isOverlayActive = screen && screen->isOverlayBannerShowing();

                // 在正常状态下，如果长按时间达到阈值，立即触发菜单
                if (!isOverlayActive && !menuActive && pressDuration >= LONG_PRESS_THRESHOLD)
                {
                    enterLongPressTriggered = true; // 标记已触发，防止重复
                    InputEvent event;
                    event.inputEvent = INPUT_BROKER_SELECT;
                    event.source = "RedBankController";
                    event.kbchar = 0;
                    event.touchX = 0;
                    event.touchY = 0;
                    inputBroker->injectInputEvent(&event);
                    menuActive = true;
                    LOG_INFO("Normal: Long press detected - Open menu immediately");
                }
            }
        }

        key1_last = key1_current;
        key2_last = key2_current;
    }

    KeypadKey RedBankController::getKey1() { return key1_last; }
    KeypadKey RedBankController::getKey2() { return key2_last; }
#endif
    RedBankController::RedBankController()
    {
        // m_meshPacketList = new std::vector<meshtastic_MeshPacket>();
        m_currentMeshPacketIndex = -1;
        restoreChannelPackets();
        restoreDirectMessages();
    }
    RedBankController::~RedBankController()
    {
        // delete m_meshPacketList;
        m_currentMeshPacketIndex = -1;
    }

    void RedBankController::setup()
    {
        LOG_INFO("this is redbank setup");
#ifdef RED_BANK_S3
        pinMode(KEY1_ADC_PIN, INPUT);
        pinMode(KEY2_ADC_PIN, INPUT);
        pinMode(PIN_LORA_EN, OUTPUT);
        digitalWrite(PIN_LORA_EN, HIGH);
        // 初始化天线管理器
        // AntennaManager::init(config.lora.region);
#if HAS_SCREEN
        applyRotation(); // 应用屏幕旋转
#endif
#endif
    }

#if HAS_SCREEN
    void RedBankController::rotateScreenLeft()
    {
        switch (currentRotation)
        {
        case 0:
            currentRotation = 1;
            break;
        case 1:
            currentRotation = 3;
            break;
        case 3:
            currentRotation = 0;
            break;
        default:
            currentRotation = 0;
            break;
        }
        applyRotation();
        LOG_INFO("Screen rotated LEFT to rotation %d", currentRotation);
    }

    void RedBankController::rotateScreenRight()
    {
        switch (currentRotation)
        {
        case 0:
            currentRotation = 3;
            break;
        case 1:
            currentRotation = 0;
            break;
        case 3:
            currentRotation = 1;
            break;
        default:
            currentRotation = 0;
            break;
        }
        applyRotation();
        LOG_INFO("Screen rotated RIGHT to rotation %d", currentRotation);
    }

    void RedBankController::applyRotation()
    {

        if (screen && screen->getDisplayDevice())
        {
#ifdef USE_EINK
            EInkDisplay *einkDisplay = static_cast<EInkDisplay *>(screen->getDisplayDevice());
            OLEDDisplay *oledDisplay = static_cast<OLEDDisplay *>(einkDisplay);

            // 先设置几何尺寸，再设置旋转
            // 根据旋转角度动态调整屏幕几何
            if (currentRotation == 1 || currentRotation == 3)
            {
                // 横屏模式：宽264，高176
                oledDisplay->setGeometry(GEOMETRY_RAWMODE, 264, 176);
                LOG_INFO("Set landscape geometry: 264x176");
            }
            else
            {
                // 竖屏模式：宽176，高264
                oledDisplay->setGeometry(GEOMETRY_RAWMODE, 176, 264);
                LOG_INFO("Set portrait geometry: 176x264");
            }

            // 设置旋转角度
            einkDisplay->setRotation(currentRotation);
            LOG_INFO("Applied rotation %d to EInk display", currentRotation);
            LOG_INFO("EInk display width = %d, height = %d", einkDisplay->width(), einkDisplay->height());

            // 触发一次刷新以确保屏幕正确显示
            EINK_ADD_FRAMEFLAG(einkDisplay, COSMETIC);

#endif
        }
    }
#endif
    void RedBankController::loop()
    {
#ifdef RED_BANK_S3
        // 使用天线管理器处理天线切换
        // AntennaManager::switchAntennaForRegion(config.lora.region);
#if HAS_SCREEN
        scanAdcKeypad();

        // LEFT+UP = 左旋，RIGHT+UP = 右旋
        static bool leftUpCombo = false;
        static bool rightUpCombo = false;
        static uint32_t lastComboTime = 0;

        // 检测LEFT+UP组合（左旋）
        if (key1_last == KeypadKey::UP && key2_last == KeypadKey::LEFT)
        {
            if (!leftUpCombo && (millis() - lastComboTime) > 200)
            { // 200ms防抖
                leftUpCombo = true;
                lastComboTime = millis();
                rotateScreenLeft();
                LOG_INFO("LEFT+UP combination triggered - Screen rotated LEFT");
            }
        }
        else if (leftUpCombo)
        {
            leftUpCombo = false;
        }

        // 检测RIGHT+UP组合（右旋）
        if (key1_last == KeypadKey::UP && key2_last == KeypadKey::RIGHT)
        {
            if (!rightUpCombo && (millis() - lastComboTime) > 200)
            { // 200ms防抖
                rightUpCombo = true;
                lastComboTime = millis();
                rotateScreenRight();
                LOG_INFO("RIGHT+UP combination triggered - Screen rotated RIGHT");
            }
        }
        else if (rightUpCombo)
        {
            rightUpCombo = false;
        }
#endif
#endif
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

        // 私信保存到私信列表
        if (mp.to != 0xffffffff)
        {
            push_direct_message(mp);
            return;
        }

        // 频道消息保存到频道列表
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
            if (channelFile.channels[i].role == meshtastic_Channel_Role_PRIMARY ||
                channelFile.channels[i].role == meshtastic_Channel_Role_SECONDARY)
            {

                LOG_INFO("restoring channel %d packets...", i);
                channelPackets[i].erase(channelPackets[i].begin(), channelPackets[i].end());
                LOG_INFO("channel %d packets size = %d", i, channelPackets[i].size());
                for (j = 0; j < 10; ++j)
                {

                    if (nodeDB->restoreMeshPacket(i, j, mp))
                    {
                        push_packet(i, mp);
                    }
                }
            }
        }
    }

    void RedBankController::restoreDirectMessages(void)
    {
        // 遍历节点数据库，查找所有有私信的节点
        for (size_t i = 0; i < nodeDB->getNumMeshNodes(); ++i)
        {
            meshtastic_NodeInfoLite *node = nodeDB->getMeshNodeByIndex(i);
            if (!node)
                continue;

            NodeNum nodeNum = node->num;
            meshtastic_MeshPacket mp;

            // 尝试恢复该节点的私信（最多20条）
            auto &nodeMessages = directMessagesByNode[nodeNum];
            nodeMessages.clear(); // 清空现有消息

            for (uint8_t j = 0; j < DIRECT_MESSAGE_LIST_CAPACITY; ++j)
            {
                if (nodeDB->restoreDirectMessagePacket(nodeNum, j, mp))
                {
                    nodeMessages.push_back(mp);
                }
            }

            if (!nodeMessages.empty())
            {
                LOG_INFO("restored %zu direct messages for node 0x%08x", nodeMessages.size(), nodeNum);
            }
        }

        // 如果当前没有选中的节点，自动选中第一个有消息的节点
        if (currentDirectMessageNode == 0)
        {
            bool foundNodeWithMessages = false;
            for (const auto &pair : directMessagesByNode)
            {
                if (!pair.second.empty())
                {
                    currentDirectMessageNode = pair.first;
                    currentDirectMessageIndex = pair.second.size() - 1;
                    LOG_INFO("Auto-selected first node with messages: 0x%08x", currentDirectMessageNode);
                    foundNodeWithMessages = true;
                    break;
                }
            }

            // 如果没有任何节点有私信，默认选中第一个节点（用于显示空聊天信息）
            if (!foundNodeWithMessages && nodeDB->getNumMeshNodes() > 0)
            {
                meshtastic_NodeInfoLite *firstNode = nodeDB->getMeshNodeByIndex(0);
                if (firstNode)
                {
                    currentDirectMessageNode = firstNode->num;
                    currentDirectMessageIndex = 0;
                    LOG_INFO("No nodes with messages, auto-selected first node: 0x%08x", currentDirectMessageNode);
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

    // 私信相关方法实现
    void RedBankController::push_direct_message(const meshtastic_MeshPacket &mp)
    {
        // 确定对方节点号（发送者或接收者）
        NodeNum otherNode;
        NodeNum myNodeNum = nodeDB->getNodeNum();
        LOG_DEBUG("myNodeNum: 0x%08x", myNodeNum);
        LOG_DEBUG("getFrom:0x%08x", getFrom(&mp));
        if (getFrom(&mp) == myNodeNum)
        {
            // 我发送的私信，对方是接收者
            otherNode = mp.to;
        }
        else
        {
            // 我接收的私信，对方是发送者
            otherNode = getFrom(&mp);
        }

        // 如果节点号为0或无效，不保存
        if (otherNode == 0 || otherNode == 0xffffffff)
        {
            return;
        }

        // 获取或创建该节点的私信列表
        auto &nodeMessages = directMessagesByNode[otherNode];

        // 如果列表已满，删除最旧的消息
        if (nodeMessages.size() >= DIRECT_MESSAGE_LIST_CAPACITY)
        {
            nodeMessages.erase(nodeMessages.begin());
        }

        // 添加新消息
        nodeMessages.push_back(mp);

        // 保存所有该节点的消息到磁盘
        for (uint8_t i = 0; i < nodeMessages.size(); ++i)
        {
            nodeDB->saveDirectMessagePacketToDisk(otherNode, i, nodeMessages.at(i));
        }

        // 如果当前没有选中的节点，自动选中这个节点
        if (currentDirectMessageNode == 0)
        {
            currentDirectMessageNode = otherNode;
            currentDirectMessageIndex = nodeMessages.size() - 1;
        }
        // 如果当前选中的就是这个节点，更新索引到最新消息
        else if (currentDirectMessageNode == otherNode)
        {
            currentDirectMessageIndex = nodeMessages.size() - 1;
        }
    }

    bool RedBankController::isDirectMessageListEmpty()
    {
        return directMessagesByNode.empty();
    }

    bool RedBankController::isDirectMessageListEmptyForNode(NodeNum nodeNum)
    {
        auto it = directMessagesByNode.find(nodeNum);
        return (it == directMessagesByNode.end() || it->second.empty());
    }

    meshtastic_MeshPacket RedBankController::getRecentDirectMessage(uint8_t recent_index)
    {
        if (currentDirectMessageNode == 0)
        {
            meshtastic_MeshPacket emptyPacket;
            memset(&emptyPacket, 0, sizeof(emptyPacket));
            return emptyPacket;
        }

        auto it = directMessagesByNode.find(currentDirectMessageNode);
        if (it == directMessagesByNode.end() || recent_index >= it->second.size())
        {
            meshtastic_MeshPacket emptyPacket;
            memset(&emptyPacket, 0, sizeof(emptyPacket));
            return emptyPacket;
        }

        return it->second.at(recent_index);
    }

    int RedBankController::_getDirectMessageListSize()
    {
        if (currentDirectMessageNode == 0)
        {
            return 0;
        }

        auto it = directMessagesByNode.find(currentDirectMessageNode);
        if (it == directMessagesByNode.end())
        {
            return 0;
        }

        return it->second.size();
    }

    int RedBankController::_getDirectMessageListSizeForNode(NodeNum nodeNum)
    {
        auto it = directMessagesByNode.find(nodeNum);
        if (it == directMessagesByNode.end())
        {
            return 0;
        }
        return it->second.size();
    }

    void RedBankController::setCurrentDirectMessageNode(NodeNum nodeNum)
    {
        currentDirectMessageNode = nodeNum;
        // 设置索引为最新消息
        auto it = directMessagesByNode.find(nodeNum);
        if (it != directMessagesByNode.end() && !it->second.empty())
        {
            currentDirectMessageIndex = it->second.size() - 1;
        }
        else
        {
            currentDirectMessageIndex = 0;
        }
    }

    std::vector<NodeNum> RedBankController::getDirectMessageNodeList()
    {
        std::vector<NodeNum> nodeList;
        for (const auto &pair : directMessagesByNode)
        {
            if (!pair.second.empty())
            {
                nodeList.push_back(pair.first);
            }
        }
        return nodeList;
    }

    int RedBankController::getDirectMessageNodeCount()
    {
        int count = 0;
        for (const auto &pair : directMessagesByNode)
        {
            if (!pair.second.empty())
            {
                count++;
            }
        }
        return count;
    }

    void RedBankController::deleteCurrentDirectMessage()
    {
        if (currentDirectMessageNode == 0)
        {
            return;
        }

        auto it = directMessagesByNode.find(currentDirectMessageNode);
        if (it == directMessagesByNode.end() || it->second.empty())
        {
            return;
        }

        auto &nodeMessages = it->second;
        if (currentDirectMessageIndex >= nodeMessages.size())
        {
            return;
        }

        // 删除磁盘上的文件
        nodeDB->deleteDirectMessagePacketFromDisk(currentDirectMessageNode, currentDirectMessageIndex);

        // 从内存中删除
        nodeMessages.erase(nodeMessages.begin() + currentDirectMessageIndex);

        // 重新保存剩余的消息到磁盘（重新编号）
        for (uint8_t i = 0; i < nodeMessages.size(); ++i)
        {
            nodeDB->saveDirectMessagePacketToDisk(currentDirectMessageNode, i, nodeMessages.at(i));
        }

        // 删除旧的文件（如果有的话）
        for (uint8_t i = nodeMessages.size(); i < DIRECT_MESSAGE_LIST_CAPACITY; ++i)
        {
            nodeDB->deleteDirectMessagePacketFromDisk(currentDirectMessageNode, i);
        }

        // 调整索引
        if (currentDirectMessageIndex >= nodeMessages.size() && !nodeMessages.empty())
        {
            currentDirectMessageIndex = nodeMessages.size() - 1;
        }
        else if (nodeMessages.empty())
        {
            currentDirectMessageIndex = 0;
        }

        LOG_INFO("Deleted direct message at index %d for node 0x%08x", currentDirectMessageIndex, currentDirectMessageNode);
    }

    void RedBankController::deleteAllDirectMessagesForNode(NodeNum nodeNum)
    {
        if (nodeNum == 0)
        {
            return;
        }

        auto it = directMessagesByNode.find(nodeNum);
        if (it == directMessagesByNode.end())
        {
            return;
        }

        // 删除磁盘上的所有文件
        nodeDB->deleteAllDirectMessagePacketsForNode(nodeNum);

        // 从内存中删除
        directMessagesByNode.erase(it);

        // 如果删除的是当前选中的节点，重置选中状态
        if (currentDirectMessageNode == nodeNum)
        {
            currentDirectMessageNode = 0;
            currentDirectMessageIndex = 0;
        }

        LOG_INFO("Deleted all direct messages for node 0x%08x", nodeNum);
    }

    void RedBankController::deleteCurrentChannelMessage(uint8_t channel_index, uint16_t packet_index)
    {
        if (channel_index >= 8)
        {
            return;
        }

        auto &channelMessages = channelPackets[channel_index];
        if (packet_index >= channelMessages.size())
        {
            LOG_WARN("deleteCurrentChannelMessage: packet_index %d >= size %zu", packet_index, channelMessages.size());
            return;
        }

        LOG_INFO("Deleting channel message: channel=%d, index=%d, total=%zu", channel_index, packet_index, channelMessages.size());

        // 先删除所有可能存在的旧文件（防止残留）
        for (uint8_t i = 0; i < MESH_PACKET_LIST_CAPCITY; ++i)
        {
            nodeDB->deleteChannelPacketFromDisk(channel_index, i);
        }

        // 从内存中删除
        channelMessages.erase(channelMessages.begin() + packet_index);

        LOG_INFO("After erase: channel %d has %zu messages", channel_index, channelMessages.size());

        // 重新保存剩余的消息到磁盘（重新编号从0开始）
        for (uint8_t i = 0; i < channelMessages.size(); ++i)
        {
            if (!nodeDB->saveChannelPacketToDisk(channel_index, i, channelMessages.at(i)))
            {
                LOG_WARN("Failed to save channel %d packet %d to disk", channel_index, i);
            }
        }

        LOG_INFO("Deleted channel message at index %d for channel %d, remaining %zu messages",
                 packet_index, channel_index, channelMessages.size());
    }

    void RedBankController::deleteAllChannelMessagesForChannel(uint8_t channel_index)
    {
        if (channel_index >= 8)
        {
            return;
        }

        // 删除磁盘上的所有文件
        nodeDB->deleteAllChannelPacketsForChannel(channel_index);

        // 从内存中删除
        channelPackets[channel_index].clear();

        LOG_INFO("Deleted all channel messages for channel %d", channel_index);
    }

#if HAS_SCREEN
#ifdef RED_BANK_S3
    bool RedBankController::isMenuActive()
    {
        return menuActive;
    }

    void RedBankController::setMenuActive(bool active)
    {
        bool wasActive = menuActive;
        menuActive = active;
        // LOG_DEBUG("Menu active state set to: %s", active ? "true" : "false");

        // 菜单关闭时进行全面刷新（类似底部导航栏隐藏后的刷新）
        if (wasActive && !active && screen && screen->getDisplayDevice())
        {
#ifdef USE_EINK
            EINK_ADD_FRAMEFLAG(screen->getDisplayDevice(), COSMETIC); // Full refresh when menu closes
#endif
        }
    }

    // LEFT 按键处理函数
    void RedBankController::handleLeftButtonPress()
    {
        leftButtonPressed = true;
        leftButtonPressTime = millis();
    }

    void RedBankController::handleLeftButtonRelease()
    {
        if (!leftButtonPressed)
            return;

        uint32_t pressDuration = millis() - leftButtonPressTime;
        leftButtonPressed = false;

        if (screen && !screen->getScreenOn())
            return;

        if (pressDuration < LONG_PRESS_THRESHOLD)
        {
            LOG_INFO("Normal: LEFT short press");
            screen->showPrevFrame();
        }
    }

    // RIGHT 按键处理函数
    void RedBankController::handleRightButtonPress()
    {
        rightButtonPressed = true;
        rightButtonPressTime = millis();
    }

    void RedBankController::handleRightButtonRelease()
    {
        if (!rightButtonPressed)
            return;

        uint32_t pressDuration = millis() - rightButtonPressTime;
        rightButtonPressed = false;

        if (screen && !screen->getScreenOn())
            return;

        if (pressDuration < LONG_PRESS_THRESHOLD)
        {
            LOG_INFO("Normal: RIGHT short press");
            screen->showNextFrame();
        }
    }

    // ENTER 按键处理函数
    void RedBankController::handleEnterButtonPress()
    {
        enterButtonPressed = true;
        enterButtonPressTime = millis();
        enterLongPressTriggered = false; // 重置长按触发标志
    }

    void RedBankController::handleEnterButtonRelease()
    {
        if (!enterButtonPressed)
            return;

        uint32_t pressDuration = millis() - enterButtonPressTime;
        bool wasLongPressTriggered = enterLongPressTriggered;
        enterButtonPressed = false;
        enterLongPressTriggered = false; // 重置标志

        // 检查是否在overlay banner（地区选择菜单等）状态
        bool isOverlayActive = screen && screen->isOverlayBannerShowing();

        // RED_BANK_S3: If we think a menu is active but no overlay is actually showing,
        // clear the stale flag to prevent "ghost" menu selection.
        if (menuActive && !isOverlayActive)
        {
            setMenuActive(false);
        }

        if (isOverlayActive || menuActive)
        {
            // 在overlay banner或菜单状态下，短按ENTER确定选择
            if (pressDuration < LONG_PRESS_THRESHOLD)
            {
                InputEvent event;
                event.inputEvent = INPUT_BROKER_SELECT;
                event.source = "RedBankController";
                event.kbchar = 0;
                event.touchX = 0;
                event.touchY = 0;
                inputBroker->injectInputEvent(&event);
                LOG_INFO("Overlay/Menu: Short press - Select option");
            }
        }
        else
        {
            // 在正常状态下，如果长按已经在按下期间触发，这里不需要重复触发
            // 如果还没有触发（可能时间刚好达到阈值），则在这里触发
            if (!wasLongPressTriggered && pressDuration >= LONG_PRESS_THRESHOLD)
            {
                InputEvent event;
                event.inputEvent = INPUT_BROKER_SELECT;
                event.source = "RedBankController";
                event.kbchar = 0;
                event.touchX = 0;
                event.touchY = 0;
                inputBroker->injectInputEvent(&event);
                menuActive = true;
                LOG_INFO("Normal: Long press on release - Open menu");
            }
            else if (pressDuration < LONG_PRESS_THRESHOLD)
            {
                // 短按点亮屏幕
                if (screen)
                    screen->setOn(true);
            }
        }
    }

    // ESC 按键处理函数
    void RedBankController::handleEscButtonPress()
    {
        escButtonPressed = true;
        escButtonPressTime = millis();
    }

    void RedBankController::handleEscButtonRelease()
    {
        if (!escButtonPressed)
            return;

        uint32_t pressDuration = millis() - escButtonPressTime;
        escButtonPressed = false;

        if (screen && !screen->getScreenOn())
            return;

        LOG_DEBUG("ESC button released after %d ms", pressDuration);

        // 检查是否在overlay banner（菜单、选择器等）状态
        bool isOverlayActive = screen && screen->isOverlayBannerShowing();

        if (isOverlayActive || menuActive)
        {
            // 在菜单状态下，短按ESC关闭菜单
            if (pressDuration < LONG_PRESS_THRESHOLD)
            {
                InputEvent event;
                event.inputEvent = INPUT_BROKER_CANCEL;
                event.source = "RedBankController";
                event.kbchar = 0;
                event.touchX = 0;
                event.touchY = 0;
                inputBroker->injectInputEvent(&event);
                setMenuActive(false); // 使用 setMenuActive 以触发刷新
                LOG_INFO("Menu: Short press - Close menu");
            }
        }
        else
        {
            // 在正常状态下，长按ESC 6秒触发关机
            if (pressDuration >= 6000)
            {
                // if (screen)
                //     screen->startAlert("Shutting down...");
                shutdownAtMsec = millis() + DEFAULT_SHUTDOWN_SECONDS * 1000;
                LOG_INFO("Normal: Long press 6s - Shutdown triggered");
            }
        }
    }

    // UP 按键处理函数
    void RedBankController::handleUpButtonPress()
    {
        upButtonPressed = true;
        upButtonPressTime = millis();
    }

    void RedBankController::handleUpButtonRelease()
    {
        if (!upButtonPressed)
            return;

        uint32_t pressDuration = millis() - upButtonPressTime;
        upButtonPressed = false;

        if (screen && !screen->getScreenOn())
            return;

        if (pressDuration < LONG_PRESS_THRESHOLD)
        {
            InputEvent event;
            event.inputEvent = INPUT_BROKER_UP;
            event.source = "RedBankController";
            event.kbchar = 0;
            event.touchX = 0;
            event.touchY = 0;
            inputBroker->injectInputEvent(&event);
            LOG_INFO("UP button: Inject INPUT_BROKER_UP event");
        }
    }

    // DOWN 按键处理函数
    void RedBankController::handleDownButtonPress()
    {
        downButtonPressed = true;
        downButtonPressTime = millis();
        LOG_DEBUG("DOWN button pressed");
    }

    void RedBankController::handleDownButtonRelease()
    {
        if (!downButtonPressed)
            return;

        uint32_t pressDuration = millis() - downButtonPressTime;
        downButtonPressed = false;

        if (screen && !screen->getScreenOn())
            return;

        LOG_DEBUG("DOWN button released after %d ms", pressDuration);

        if (pressDuration < LONG_PRESS_THRESHOLD)
        {
            InputEvent event;
            event.inputEvent = INPUT_BROKER_DOWN;
            event.source = "RedBankController";
            event.kbchar = 0;
            event.touchX = 0;
            event.touchY = 0;
            inputBroker->injectInputEvent(&event);
            LOG_INFO("DOWN button: Inject INPUT_BROKER_DOWN event");
        }
    }
#endif
#endif
}
