#pragma once

namespace redcoast915
{
  class FiveWayGpioInput
  {
  public:
    FiveWayGpioInput();
    ~FiveWayGpioInput();

    void setup();
    void loop();

    bool isMenuActive() const { return menuActive; }
    void setMenuActive(bool active);

    static const uint32_t DIRECTION_REPEAT_DELAY = 500;
    static const uint32_t DIRECTION_REPEAT_INTERVAL = 150;

  private:
    void handleEnterKey(bool enter, bool lastEnter, bool isOverlayActive);
    void handleCancelKey(bool cancel, bool lastCancel, bool isOverlayActive);

    // SW_BUT (ENTER): long-press opens menu while held; short-press selects in menu
    bool enterButtonPressed = false;
    bool enterLongPressTriggered = false;
    uint32_t enterButtonPressTime = 0;

    // SW_F5 (CANCEL): short-press closes menu; long-press in normal mode triggers shutdown
    bool cancelButtonPressed = false;
    uint32_t cancelButtonPressTime = 0;

    // SW_F1-F4 direction keys with auto-repeat while held
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

    bool menuActive = false;
  };
} // namespace redcoast915
