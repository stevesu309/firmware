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
#include "GxEPD2_BW.h"
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

        // 检测所有按键状态变化
        bool leftPressed = (key2_current == KeypadKey::LEFT);
        bool rightPressed = (key2_current == KeypadKey::RIGHT);
        bool enterPressed = (key1_current == KeypadKey::ENTER);
        bool escPressed = (key1_current == KeypadKey::ESC);
        bool upPressed = (key1_current == KeypadKey::UP);
        bool downPressed = (key2_current == KeypadKey::DOWN);

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
        if (enterPressed && !enterButtonPressed)
        {
            handleEnterButtonPress();
        }
        else if (!enterPressed && enterButtonPressed)
        {
            handleEnterButtonRelease();
        }

        // ESC 按键处理
        if (escPressed && !escButtonPressed)
        {
            handleEscButtonPress();
        }
        else if (!escPressed && escButtonPressed)
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
            // einkDisplay->fillScreen(1);
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

            // einkDisplay->clearPixel(einkDisplay->width(), einkDisplay->height()); // 清屏
            // screen->forceDisplay(true);
            EINK_ADD_FRAMEFLAG(einkDisplay, COSMETIC);

#endif
        }
    }

    void RedBankController::loop()
    {
#ifdef RED_BANK_S3
        // 使用天线管理器处理天线切换
        AntennaManager::switchAntennaForRegion(config.lora.region);

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

        // 检查是否有组合按键被触发，如果有则跳过单独按键处理
        bool comboTriggered = leftUpCombo || rightUpCombo;

        if (!comboTriggered)
        {
            scanAdcKeypad(); // ADC按键扫描
        }

        // if (screen)
        // {
        //     delay(200);
        //     if (!comboTriggered)
        //     {
        //         // 按键处理通过scanAdcKeypad()函数完成
        //         switch (key2_last)
        //         {
        //         case KeypadKey::LEFT:
        //             screen->showPrevFrame();
        //             break;
        //         case KeypadKey::RIGHT:
        //             // screen->showNextFrame();
        //             InputEvent event;
        //             event.inputEvent = INPUT_BROKER_RIGHT;
        //             event.source = "RedBankController";
        //             event.kbchar = 0;
        //             event.touchX = 0;
        //             event.touchY = 0;
        //             inputBroker->injectInputEvent(&event);
        //             menuActive = false;
        //             LOG_INFO("INPUT_BROKER_RIGHT event injected");

        //             break;
        //         default:
        //             break;
        //         }
        //     }
        //     else
        //     {
        //         // 组合按键被触发时，记录调试信息
        //         LOG_DEBUG("Combo key active, skipping individual key processing");
        //     }
        // }
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

    bool RedBankController::isMenuActive()
    {
        return menuActive;
    }

    void RedBankController::setMenuActive(bool active)
    {
        menuActive = active;
        LOG_DEBUG("Menu active state set to: %s", active ? "true" : "false");
    }

    // LEFT 按键处理函数
    void RedBankController::handleLeftButtonPress()
    {
        leftButtonPressed = true;
        leftButtonPressTime = millis();
        LOG_DEBUG("LEFT button pressed");
    }

    void RedBankController::handleLeftButtonRelease()
    {
        if (!leftButtonPressed)
            return;

        uint32_t pressDuration = millis() - leftButtonPressTime;
        leftButtonPressed = false;

        LOG_DEBUG("LEFT button released after %d ms", pressDuration);

        // 检查是否在overlay banner（地区选择菜单等）状态
        bool isOverlayActive = screen && screen->isOverlayBannerShowing();

        if (isOverlayActive)
        {
            // 在overlay banner状态下，LEFT短按映射为UP（向上选择）
            if (pressDuration < LONG_PRESS_THRESHOLD)
            {
                InputEvent event;
                event.inputEvent = INPUT_BROKER_UP;
                event.source = "RedBankController";
                event.kbchar = 0;
                event.touchX = 0;
                event.touchY = 0;
                inputBroker->injectInputEvent(&event);
                LOG_INFO("Overlay active: LEFT -> UP (Previous option)");
            }
        }
        else
        {
            // 在正常状态下的LEFT按键行为（如果需要的话）
            if (pressDuration < LONG_PRESS_THRESHOLD)
            {
                LOG_INFO("Normal: LEFT short press");
                screen->showPrevFrame();
            }
        }
    }

    // RIGHT 按键处理函数
    void RedBankController::handleRightButtonPress()
    {
        rightButtonPressed = true;
        rightButtonPressTime = millis();
        LOG_DEBUG("RIGHT button pressed");
    }

    void RedBankController::handleRightButtonRelease()
    {
        if (!rightButtonPressed)
            return;

        uint32_t pressDuration = millis() - rightButtonPressTime;
        rightButtonPressed = false;

        LOG_DEBUG("RIGHT button released after %d ms", pressDuration);

        // 检查是否在overlay banner（地区选择菜单等）状态
        bool isOverlayActive = screen && screen->isOverlayBannerShowing();

        if (isOverlayActive)
        {
            // 在overlay banner状态下，RIGHT短按映射为DOWN（向下选择）
            if (pressDuration < LONG_PRESS_THRESHOLD)
            {
                InputEvent event;
                event.inputEvent = INPUT_BROKER_DOWN;
                event.source = "RedBankController";
                event.kbchar = 0;
                event.touchX = 0;
                event.touchY = 0;
                inputBroker->injectInputEvent(&event);
                LOG_INFO("Overlay active: RIGHT -> DOWN (Next option)");
            }
        }
        else
        {
            // 在正常状态下的RIGHT按键行为（如果需要的话）
            if (pressDuration < LONG_PRESS_THRESHOLD)
            {
                LOG_INFO("Normal: RIGHT short press");
                screen->showNextFrame();
            }
        }
    }

    // ENTER 按键处理函数
    void RedBankController::handleEnterButtonPress()
    {
        enterButtonPressed = true;
        enterButtonPressTime = millis();
        LOG_DEBUG("ENTER button pressed");
    }

    void RedBankController::handleEnterButtonRelease()
    {
        if (!enterButtonPressed)
            return;

        uint32_t pressDuration = millis() - enterButtonPressTime;
        enterButtonPressed = false;

        LOG_DEBUG("ENTER button released after %d ms", pressDuration);

        // 检查是否在overlay banner（地区选择菜单等）状态
        bool isOverlayActive = screen && screen->isOverlayBannerShowing();

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
            // 在正常状态下，长按ENTER呼出菜单
            if (pressDuration >= LONG_PRESS_THRESHOLD)
            {
                InputEvent event;
                event.inputEvent = INPUT_BROKER_SELECT;
                event.source = "RedBankController";
                event.kbchar = 0;
                event.touchX = 0;
                event.touchY = 0;
                inputBroker->injectInputEvent(&event);
                menuActive = true;
                LOG_INFO("Normal: Long press - Open menu");
            }
        }
    }

    // ESC 按键处理函数
    void RedBankController::handleEscButtonPress()
    {
        escButtonPressed = true;
        escButtonPressTime = millis();
        LOG_DEBUG("ESC button pressed");
    }

    void RedBankController::handleEscButtonRelease()
    {
        if (!escButtonPressed)
            return;

        uint32_t pressDuration = millis() - escButtonPressTime;
        escButtonPressed = false;

        LOG_DEBUG("ESC button released after %d ms", pressDuration);

        if (menuActive)
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
                menuActive = false;
                LOG_INFO("Menu: Short press - Close menu");
            }
        }
    }

    // UP 按键处理函数
    void RedBankController::handleUpButtonPress()
    {
        upButtonPressed = true;
        upButtonPressTime = millis();
        LOG_DEBUG("UP button pressed");
    }

    void RedBankController::handleUpButtonRelease()
    {
        if (!upButtonPressed)
            return;

        uint32_t pressDuration = millis() - upButtonPressTime;
        upButtonPressed = false;

        LOG_DEBUG("UP button released after %d ms", pressDuration);

        if (menuActive)
        {
            // 在菜单状态下，短按UP控制菜单选项
            if (pressDuration < LONG_PRESS_THRESHOLD)
            {
                InputEvent event;
                event.inputEvent = INPUT_BROKER_UP;
                event.source = "RedBankController";
                event.kbchar = 0;
                event.touchX = 0;
                event.touchY = 0;
                inputBroker->injectInputEvent(&event);
                LOG_INFO("Menu: Short press - Previous option");
            }
        }
        else
        {
            // 在正常状态下，短按UP浏览上一条消息
            if (pressDuration < LONG_PRESS_THRESHOLD && screen)
            {
                screen->showPrevPacket();
                LOG_INFO("Normal: Short press - Previous packet");
            }
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

        LOG_DEBUG("DOWN button released after %d ms", pressDuration);

        if (menuActive)
        {
            // 在菜单状态下，短按DOWN控制菜单选项
            if (pressDuration < LONG_PRESS_THRESHOLD)
            {
                InputEvent event;
                event.inputEvent = INPUT_BROKER_DOWN;
                event.source = "RedBankController";
                event.kbchar = 0;
                event.touchX = 0;
                event.touchY = 0;
                inputBroker->injectInputEvent(&event);
                LOG_INFO("Menu: Short press - Next option");
            }
        }
        else
        {
            // 在正常状态下，短按DOWN浏览下一条消息
            if (pressDuration < LONG_PRESS_THRESHOLD && screen)
            {
                screen->showNextPacket();
                LOG_INFO("Normal: Short press - Next packet");
            }
        }
    }

}
