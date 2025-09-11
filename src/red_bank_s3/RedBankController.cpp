#include "RedBankController.h"
#include <Arduino.h>
#include "DebugConfiguration.h"
#include "main.h"
#include "FSCommon.h"
#include "variant.h"
#include "graphics/Screen.h"
#include "graphics/EInkDisplay2.h"
#include "GxEPD2_BW.h"
#include "AntennaManager.h"
#include "input/InputBroker.h"

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

    void RedBankController::scanAdcKeypad()
    {
        int val1 = analogRead(KEY1_ADC_PIN);
        int val2 = analogRead(KEY2_ADC_PIN);
        KeypadKey key1_current = detect_key(val1, true);
        KeypadKey key2_current = detect_key(val2, false);

        // 添加按键防抖处理
        static uint32_t lastKeyTime = 0;
        if (millis() - lastKeyTime < 200)
        { // 200ms防抖
            key1_last = KeypadKey::NONE;
            key2_last = KeypadKey::NONE;
            return;
        }
        lastKeyTime = millis();

        // 检测 RIGHT 按键状态变化
        bool rightPressed = (key2_current == KeypadKey::RIGHT);
        
        if (rightPressed && !rightButtonPressed) {
            // RIGHT 按键刚按下
            handleRightButtonPress();
        } else if (!rightPressed && rightButtonPressed) {
            // RIGHT 按键刚释放
            handleRightButtonRelease();
        }
        
        // 检测菜单退出：ESC 按键真正关闭菜单
        if (menuActive && key1_current == KeypadKey::ESC) {
            // 发送INPUT_BROKER_CANCEL事件来真正关闭菜单
            InputEvent event;
            event.inputEvent = INPUT_BROKER_CANCEL;
            event.source = "RedBankController";
            event.kbchar = 0;
            event.touchX = 0;
            event.touchY = 0;
            inputBroker->injectInputEvent(&event);
            LOG_INFO("ESC pressed - closing menu");
        }

        key1_last = key1_current;
        key2_last = key2_current;
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
#ifdef RED_BANK_S3
        pinMode(KEY1_ADC_PIN, INPUT);
        pinMode(KEY2_ADC_PIN, INPUT);
        pinMode(PIN_LORA_EN, OUTPUT);
        digitalWrite(PIN_LORA_EN, HIGH);

        currentRotation = 3; // 默认旋转角度

        // 初始化天线管理器
        AntennaManager::init(config.lora.region);

        applyRotation(); // 应用屏幕旋转
#endif
    }

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

            einkDisplay->setRotation(currentRotation);
            LOG_INFO("Applied rotation %d to EInk display", currentRotation);
            LOG_INFO("EInk display width = %d, height = %d", einkDisplay->width(), einkDisplay->height());

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

            // 强制刷新屏幕
            einkDisplay->fillScreen(GxEPD_WHITE);
            screen->forceDisplay(true);
            EINK_ADD_FRAMEFLAG(einkDisplay, DEMAND_FAST);

#else
            screen->dispdev->setRotation(currentRotation);
            LOG_INFO("Applied rotation %d to display", currentRotation);
#endif
        }
    }

    void RedBankController::loop()
    {
#ifdef RED_BANK_S3
        // 使用天线管理器处理天线切换
        AntennaManager::switchAntennaForRegion(config.lora.region);
        scanAdcKeypad(); // ADC按键扫描

        // 组合按键旋转检测
        // LEFT+UP = 左旋，RIGHT+UP = 右旋
        // 使用静态变量跟踪组合按键状态，避免重复触发
        // 当组合按键被触发时，会阻止单独按键功能的执行
        static bool leftUpCombo = false;
        static bool rightUpCombo = false;
        static uint32_t lastComboTime = 0;

        // 检测LEFT+UP组合（左旋）
        if (key1_last == KeypadKey::UP && key2_last == KeypadKey::LEFT)
        {
            if (!leftUpCombo && (millis() - lastComboTime) > 200)
            { // 500ms防抖
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
            { // 500ms防抖
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

        // 检查是否有组合按键被触发，如果有则跳过单独按键处理
        bool comboTriggered = leftUpCombo || rightUpCombo;

        if (screen)
        {
            delay(200);

            // 只有当没有组合按键被触发时，才处理单独按键功能
            if (!comboTriggered)
            {
                // 按键1功能处理
                switch (key1_last)
                {
                case KeypadKey::UP:
                    screen->showPrevPacket();
                    break;
                case KeypadKey::ENTER:
                    LOG_DEBUG("ENTER key pressed (currently unused)");
                    break;
                case KeypadKey::ESC:
                    LOG_DEBUG("ESC key pressed (currently unused)");
                    break;
                default:
                    break;
                }

                // 按键2功能处理
                switch (key2_last)
                {
                case KeypadKey::DOWN:
                    screen->showNextPacket();
                    break;
                case KeypadKey::LEFT:
                    screen->showPrevFrame();
                    break;
                case KeypadKey::RIGHT:
                    // RIGHT 按键现在通过 handleRightButtonPress/Release 处理
                    // 这里不再处理，避免重复触发
                    break;
                default:
                    break;
                }
            }
            else
            {
                // 组合按键被触发时，记录调试信息
                LOG_DEBUG("Combo key active, skipping individual key processing");
            }
        }
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
        if (mp.to != 0xffffffff)
        {
            return; // 私信不保存
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

    // void RedBankController::_handleShuttingDownButtonPress()  //关机
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

    void RedBankController::handleRightButtonPress()
    {
        rightButtonPressed = true;
        rightButtonPressTime = millis();
        LOG_DEBUG("RIGHT button pressed");
    }

    void RedBankController::handleRightButtonRelease()
    {
        if (!rightButtonPressed) return;
        
        uint32_t pressDuration = millis() - rightButtonPressTime;
        rightButtonPressed = false;
        
        LOG_DEBUG("RIGHT button released after %d ms", pressDuration);
        
        if (menuActive) {
            // 在菜单状态下
            if (pressDuration >= LONG_PRESS_THRESHOLD) {
                // 长按：确定选项
                InputEvent event;
                event.inputEvent = INPUT_BROKER_SELECT;
                event.source = "RedBankController";
                event.kbchar = 0;
                event.touchX = 0;
                event.touchY = 0;
                inputBroker->injectInputEvent(&event);
                LOG_INFO("Menu: Long press - Select option");
            } else {
                // 短按：下一项（菜单使用 UP/DOWN 导航）
                InputEvent event;
                event.inputEvent = INPUT_BROKER_DOWN;
                event.source = "RedBankController";
                event.kbchar = 0;
                event.touchX = 0;
                event.touchY = 0;
                inputBroker->injectInputEvent(&event);
                LOG_INFO("Menu: Short press - Next item");
            }
        } else {
            // 在正常状态下
            if (pressDuration >= LONG_PRESS_THRESHOLD) {
                // 长按：呼出菜单
                InputEvent event;
                event.inputEvent = INPUT_BROKER_SELECT;
                event.source = "RedBankController";
                event.kbchar = 0;
                event.touchX = 0;
                event.touchY = 0;
                inputBroker->injectInputEvent(&event);
                menuActive = true;
                LOG_INFO("Normal: Long press - Open menu");
            } else {
                // 短按：下一帧
                InputEvent event;
                event.inputEvent = INPUT_BROKER_RIGHT;
                event.source = "RedBankController";
                event.kbchar = 0;
                event.touchX = 0;
                event.touchY = 0;
                inputBroker->injectInputEvent(&event);
                LOG_INFO("Normal: Short press - Next frame");
            }
        }
    }

    bool RedBankController::isMenuActive()
    {
        return menuActive;
    }

    void RedBankController::setMenuActive(bool active)
    {
        menuActive = active;
        LOG_DEBUG("Menu active state set to: %s", active ? "true" : "false");
    }

}
