#pragma once

#include "configuration.h"
#include <stdint.h>

namespace RedBankS3
{
enum class KeypadKey { NONE, ENTER, ESC, UP, LEFT, RIGHT, DOWN };

class RedBankController
{
  public:
    RedBankController();
    ~RedBankController();
    void setup();
    void loop();

    uint8_t getDirection(void);

#if HAS_SCREEN
    void scanAdcKeypad();
    KeypadKey getKey1();
    KeypadKey getKey2();

    // 屏幕旋转相关
    uint8_t currentRotation = 0;
    void rotateScreenLeft();
    void rotateScreenRight();
    void applyRotation();
    uint8_t getCurrentRotation() { return currentRotation; };

    // 菜单按钮相关
    void handleLeftButtonPress();
    void handleLeftButtonRelease();
    void handleRightButtonPress();
    void handleRightButtonRelease();
    void handleEnterButtonPress();
    void handleEnterButtonRelease();
    void handleEscButtonPress();
    void handleEscButtonRelease();
    void handleUpButtonPress();
    void handleUpButtonRelease();
    void handleDownButtonPress();
    void handleDownButtonRelease();
    bool isMenuActive();
    void setMenuActive(bool active);
#endif // HAS_SCREEN

  private:
    // 当前消息索引
    int m_currentMeshPacketIndex;
    uint8_t direction;

#if HAS_SCREEN
    KeypadKey mapKeyByRotation(KeypadKey physicalKey);

    // 按键状态管理
    bool leftButtonPressed = false;
    bool rightButtonPressed = false;
    bool enterButtonPressed = false;
    bool enterLongPressTriggered = false; // 标记长按是否已触发，防止重复触发
    bool escButtonPressed = false;
    bool upButtonPressed = false;
    bool downButtonPressed = false;
    uint32_t leftButtonPressTime = 0;
    uint32_t rightButtonPressTime = 0;
    uint32_t enterButtonPressTime = 0;
    uint32_t escButtonPressTime = 0;
    uint32_t upButtonPressTime = 0;
    uint32_t downButtonPressTime = 0;
    static const uint32_t LONG_PRESS_THRESHOLD = 2000;
    bool menuActive = false;
#endif // HAS_SCREEN
};
} // namespace RedBankS3