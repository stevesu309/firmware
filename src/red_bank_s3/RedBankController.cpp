#include "RedBankController.h"
#include "DebugConfiguration.h"
#include "FSCommon.h"
#include "graphics/EInkDisplay2.h"
#include "graphics/Screen.h"
#include "main.h"
#include "variant.h"
#include <Arduino.h>
// #include "GxEPD2_BW.h"
#include "AntennaManager.h"
#include "input/InputBroker.h"
#include "meshUtils.h"
#include <algorithm>
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

    switch (currentRotation) {
    case 0:
        return physicalKey;

    case 3:
        // 逆时针旋转90度
        switch (physicalKey) {
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
        switch (physicalKey) {
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
    if (anyKeyChanged) {
        uint32_t now = millis();
        // 如果距离上次按键变化时间太短（< 50ms），可能是抖动，忽略
        if (now - lastKeyChangeTime < 50) {
            return;
        }
        lastKeyChangeTime = now;
        key1_prev = key1_current;
        key2_prev = key2_current;
    }

    // 检查是否是组合按键（LEFT+UP 或 RIGHT+UP）
    bool isComboKey = (key1_current == KeypadKey::UP && (key2_current == KeypadKey::LEFT || key2_current == KeypadKey::RIGHT));

    if (!isComboKey) {
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
        if (leftPressed && !leftButtonPressed) {
            handleLeftButtonPress();
        } else if (!leftPressed && leftButtonPressed) {
            handleLeftButtonRelease();
        }

        // RIGHT 按键处理
        if (rightPressed && !rightButtonPressed) {
            handleRightButtonPress();
        } else if (!rightPressed && rightButtonPressed) {
            handleRightButtonRelease();
        }

        // ENTER 按键处理
        if (physicalEnterPressed && !enterButtonPressed) {
            handleEnterButtonPress();
        } else if (!physicalEnterPressed && enterButtonPressed) {
            handleEnterButtonRelease();
        }

        // ESC 按键处理
        if (physicalEscPressed && !escButtonPressed) {
            handleEscButtonPress();
        } else if (!physicalEscPressed && escButtonPressed) {
            handleEscButtonRelease();
        }

        // UP 按键处理
        if (upPressed && !upButtonPressed) {
            handleUpButtonPress();
        } else if (!upPressed && upButtonPressed) {
            handleUpButtonRelease();
        }

        // DOWN 按键处理
        if (downPressed && !downButtonPressed) {
            handleDownButtonPress();
        } else if (!downPressed && downButtonPressed) {
            handleDownButtonRelease();
        }

        // ENTER 按键长按检查（在按下期间持续检查）
        if (physicalEnterPressed && enterButtonPressed && !enterLongPressTriggered) {
            uint32_t pressDuration = millis() - enterButtonPressTime;
            bool isOverlayActive = screen && screen->isOverlayBannerShowing();

            // 在正常状态下，如果长按时间达到阈值，立即触发菜单
            if (!isOverlayActive && !menuActive && pressDuration >= LONG_PRESS_THRESHOLD) {
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

KeypadKey RedBankController::getKey1()
{
    return key1_last;
}
KeypadKey RedBankController::getKey2()
{
    return key2_last;
}
#endif
RedBankController::RedBankController()
{
    // m_meshPacketList = new std::vector<meshtastic_MeshPacket>();
    m_currentMeshPacketIndex = -1;
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
    switch (currentRotation) {
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
    switch (currentRotation) {
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

    if (screen && screen->getDisplayDevice()) {
#ifdef USE_EINK
        EInkDisplay *einkDisplay = static_cast<EInkDisplay *>(screen->getDisplayDevice());
        OLEDDisplay *oledDisplay = static_cast<OLEDDisplay *>(einkDisplay);

        // 先设置几何尺寸，再设置旋转
        // 根据旋转角度动态调整屏幕几何
        if (currentRotation == 1 || currentRotation == 3) {
            // 横屏模式：宽264，高176
            oledDisplay->setGeometry(GEOMETRY_RAWMODE, 264, 176);
            LOG_INFO("Set landscape geometry: 264x176");
        } else {
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
    if (key1_last == KeypadKey::UP && key2_last == KeypadKey::LEFT) {
        if (!leftUpCombo && (millis() - lastComboTime) > 200) { // 200ms防抖
            leftUpCombo = true;
            lastComboTime = millis();
            rotateScreenLeft();
            LOG_INFO("LEFT+UP combination triggered - Screen rotated LEFT");
        }
    } else if (leftUpCombo) {
        leftUpCombo = false;
    }

    // 检测RIGHT+UP组合（右旋）
    if (key1_last == KeypadKey::UP && key2_last == KeypadKey::RIGHT) {
        if (!rightUpCombo && (millis() - lastComboTime) > 200) { // 200ms防抖
            rightUpCombo = true;
            lastComboTime = millis();
            rotateScreenRight();
            LOG_INFO("RIGHT+UP combination triggered - Screen rotated RIGHT");
        }
    } else if (rightUpCombo) {
        rightUpCombo = false;
    }
#endif
#endif
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
    if (wasActive && !active && screen && screen->getDisplayDevice()) {
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

    if (pressDuration < LONG_PRESS_THRESHOLD) {
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

    if (pressDuration < LONG_PRESS_THRESHOLD) {
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
    if (menuActive && !isOverlayActive) {
        setMenuActive(false);
    }

    if (isOverlayActive || menuActive) {
        // 在overlay banner或菜单状态下，短按ENTER确定选择
        if (pressDuration < LONG_PRESS_THRESHOLD) {
            InputEvent event;
            event.inputEvent = INPUT_BROKER_SELECT;
            event.source = "RedBankController";
            event.kbchar = 0;
            event.touchX = 0;
            event.touchY = 0;
            inputBroker->injectInputEvent(&event);
            LOG_INFO("Overlay/Menu: Short press - Select option");
        }
    } else {
        // 在正常状态下，如果长按已经在按下期间触发，这里不需要重复触发
        // 如果还没有触发（可能时间刚好达到阈值），则在这里触发
        if (!wasLongPressTriggered && pressDuration >= LONG_PRESS_THRESHOLD) {
            InputEvent event;
            event.inputEvent = INPUT_BROKER_SELECT;
            event.source = "RedBankController";
            event.kbchar = 0;
            event.touchX = 0;
            event.touchY = 0;
            inputBroker->injectInputEvent(&event);
            menuActive = true;
            LOG_INFO("Normal: Long press on release - Open menu");
        } else if (pressDuration < LONG_PRESS_THRESHOLD) {
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

    if (isOverlayActive || menuActive) {
        // 在菜单状态下，短按ESC关闭菜单
        if (pressDuration < LONG_PRESS_THRESHOLD) {
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
    } else {
        // 在正常状态下，长按ESC 6秒触发关机
        if (pressDuration >= 6000) {
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

    if (pressDuration < LONG_PRESS_THRESHOLD) {
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

    if (pressDuration < LONG_PRESS_THRESHOLD) {
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
} // namespace RedBankS3
