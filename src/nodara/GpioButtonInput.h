#pragma once

namespace nodara
{
class GpioButtonInput
{
  public:
    GpioButtonInput();
    ~GpioButtonInput();

    void setup();
    void loop();

    bool isMenuActive() const { return menuActive; }
    void setMenuActive(bool active);

    static const uint32_t NAVIGATION_REPEAT_DELAY = 500;
    static const uint32_t NAVIGATION_REPEAT_INTERVAL = 150;

  private:
    void handleEnterKey(bool enter, bool lastEnter, bool isOverlayActive);
    void handleCancelKey(bool cancel, bool lastCancel, bool isOverlayActive);

    // SW_BUT (ENTER): long-press opens menu while held; short-press selects in menu
    bool enterButtonPressed = false;
    bool enterLongPressTriggered = false;
    uint32_t enterButtonPressTime = 0;

    // SW_F5 (CANCEL): short-press closes menu; double-tap toggles BurstTest; long-press triggers shutdown
    bool cancelButtonPressed = false;
    uint32_t cancelButtonPressTime = 0;
    uint32_t cancelLastShortReleaseMs = 0;
    uint8_t cancelTapCount = 0;

    // SW_F1-F4 GPIO navigation buttons with auto-repeat while held
    bool leftButtonPressed = false;
    bool rightButtonPressed = false;
    bool upButtonPressed = false;
    bool downButtonPressed = false;
    uint32_t leftButtonNextRepeatAt = 0;
    uint32_t rightButtonNextRepeatAt = 0;
    uint32_t upButtonNextRepeatAt = 0;
    uint32_t downButtonNextRepeatAt = 0;

    static const uint32_t LONG_PRESS_THRESHOLD = 2000;
    static const uint32_t SHUTDOWN_PRESS_THRESHOLD = 6000;
    static const uint32_t DOUBLE_TAP_WINDOW_MS = 400;

    bool menuActive = false;
};
} // namespace nodara
