#include "FiveWayGpioInput.h"
#include "Arduino.h"
#include "graphics/EInkDynamicDisplay.h"
#include "graphics/Screen.h"
#include "main.h"
#include "input/InputBroker.h"

namespace redcoast915
{

  // #define SW_F1 (0 + 6)
  // #define SW_F2 (0 + 8)
  // #define SW_F3 (0 + 9)
  // #define SW_F4 (0 + 10)
  // #define SW_F5 (0 + 11)

  // #define SW_BUT (32 + 10) // 取消键

  namespace
  {
    constexpr const char *kInputSource = "FiveWayGpioInput";

    // Helper to keep all synthesized key events consistent.
    void injectInputEvent(input_broker_event eventType)
    {
      if (!inputBroker)
        return;

      InputEvent event;
      event.source = kInputSource;
      event.inputEvent = eventType;
      event.kbchar = 0;
      event.touchX = 0;
      event.touchY = 0;
      inputBroker->injectInputEvent(&event);
    }
  } // namespace

  FiveWayGpioInput::FiveWayGpioInput()
  {
  }

  FiveWayGpioInput::~FiveWayGpioInput()
  {
  }
#ifdef REDCOAST_SOLO_915
  void FiveWayGpioInput::setup()
  {

    pinMode(SW_F1, INPUT_PULLUP);
    pinMode(SW_F2, INPUT_PULLUP);
    pinMode(SW_F3, INPUT_PULLUP);
    pinMode(SW_F4, INPUT_PULLUP);
    pinMode(SW_F5, INPUT_PULLUP);
    pinMode(SW_BUT, INPUT_PULLUP);
  }

  void FiveWayGpioInput::setMenuActive(bool active)
  {
    bool wasActive = menuActive;
    menuActive = active;

    // Force a cleanup refresh when leaving menu mode to avoid stale overlay
    // remnants on E-Ink during menu transitions.
    if (wasActive && !active && screen && screen->getDisplayDevice())
    {
#ifdef USE_EINK
      EINK_ADD_FRAMEFLAG(screen->getDisplayDevice(), COSMETIC);
#endif
    }
  }

  void FiveWayGpioInput::loop()
  {
    static bool lastF1 = false;
    static bool lastF2 = false;
    static bool lastF3 = false;
    static bool lastF4 = false;
    static bool lastF5 = false;
    static bool lastBut = false;
    static bool initialized = false;

    bool f1 = digitalRead(SW_F1) == LOW;
    bool f2 = digitalRead(SW_F2) == LOW;
    bool f3 = digitalRead(SW_F3) == LOW;
    bool f4 = digitalRead(SW_F4) == LOW;
    bool f5 = digitalRead(SW_F5) == LOW;
    bool but = digitalRead(SW_BUT) == LOW;

    // Seed edge-detection state from the current electrical level so we
    // don't generate fake presses immediately after boot.
    if (!initialized)
    {
      lastF1 = f1;
      lastF2 = f2;
      lastF3 = f3;
      lastF4 = f4;
      lastF5 = f5;
      lastBut = but;
      initialized = true;
      return;
    }

    if (screen && !screen->getScreenOn())
    {
      // Allow ENTER to wake the display even while all other key handling
      // is suppressed during screen-off mode.
      if (but && !lastBut)
      {
        screen->setOn(true);
        LOG_INFO("Screen off: ENTER short press - Wake screen");
      }

      enterButtonPressed = false;
      enterLongPressTriggered = false;
      escButtonPressed = false;
      leftButtonPressed = false;
      rightButtonPressed = false;
      upButtonPressed = false;
      downButtonPressed = false;
      lastF1 = f1;
      lastF2 = f2;
      lastF3 = f3;
      lastF4 = f4;
      lastF5 = f5;
      lastBut = but;
      return;
    }

    bool isOverlayActive = screen && screen->isOverlayBannerShowing();

    // Keep a sticky local menu flag so select/cancel still behave correctly
    // while the overlay renderer transitions between menu screens.
    if (isOverlayActive && !menuActive)
      setMenuActive(true);
    else if (menuActive && !isOverlayActive)
      setMenuActive(false);

    // Open the menu as soon as ENTER has
    // been held long enough, without waiting for button release.
    if (f5 && enterButtonPressed && !enterLongPressTriggered)
    {
      uint32_t pressDuration = millis() - enterButtonPressTime;
      if (!isOverlayActive && !menuActive && pressDuration >= LONG_PRESS_THRESHOLD)
      {
        enterLongPressTriggered = true;
        injectInputEvent(INPUT_BROKER_SELECT);
        setMenuActive(true);
        LOG_INFO("Normal: Long press detected - Open menu immediately");
      }
    }

    // ENTER still uses release handling for short press select/turn-on, and
    // to provide a fallback if the long-press threshold is crossed near release.
    if (but && !lastBut)
    {
      enterButtonPressed = true;
      enterButtonPressTime = millis();
      enterLongPressTriggered = false;
    }
    else if (!but && lastBut)
    {
      uint32_t pressDuration = millis() - enterButtonPressTime;
      bool wasLongPressTriggered = enterLongPressTriggered;
      enterButtonPressed = false;
      enterLongPressTriggered = false;

      if (isOverlayActive || menuActive)
      {
        if (pressDuration < LONG_PRESS_THRESHOLD)
        {
          setMenuActive(true);
          injectInputEvent(INPUT_BROKER_SELECT);
          LOG_INFO("Overlay/Menu: Short press - Select option");
        }
      }
      else
      {
        if (!wasLongPressTriggered && pressDuration >= LONG_PRESS_THRESHOLD)
        {
          injectInputEvent(INPUT_BROKER_SELECT);
          setMenuActive(true);
          LOG_INFO("Normal: Long press on release - Open menu");
        }
        else
        {
          if (screen)
            screen->setOn(true);
          LOG_INFO("Normal: ENTER short press");
        }
      }
    }

    // Direction keys send one event on initial press, then repeat at a fixed
    // cadence while held to support scrolling through menus and lists.
    if (f2 && !lastF2)
    {
      leftButtonPressed = true;
      leftButtonNextRepeatAt = millis() + DIRECTION_REPEAT_DELAY;
      injectInputEvent(INPUT_BROKER_LEFT);
      LOG_INFO("LEFT button: Inject INPUT_BROKER_LEFT event");
    }
    else if (!f2 && lastF2)
    {
      leftButtonPressed = false;
    }
    else if (f2 && leftButtonPressed)
    {
      uint32_t now = millis();
      if (now >= leftButtonNextRepeatAt)
      {
        injectInputEvent(INPUT_BROKER_LEFT);
        leftButtonNextRepeatAt = now + DIRECTION_REPEAT_INTERVAL;
        LOG_INFO("LEFT button: Repeat INPUT_BROKER_LEFT event");
      }
    }

    if (f1 && !lastF1)
    {
      upButtonPressed = true;
      upButtonPressTime = millis();
      upButtonNextRepeatAt = upButtonPressTime + DIRECTION_REPEAT_DELAY;
      injectInputEvent(INPUT_BROKER_UP);
      LOG_INFO("UP button: Inject INPUT_BROKER_UP event");
    }
    else if (!f1 && lastF1)
    {
      upButtonPressed = false;
    }
    else if (f1 && upButtonPressed)
    {
      uint32_t now = millis();
      if (now >= upButtonNextRepeatAt)
      {
        injectInputEvent(INPUT_BROKER_UP);
        upButtonNextRepeatAt = now + DIRECTION_REPEAT_INTERVAL;
        LOG_INFO("UP button: Repeat INPUT_BROKER_UP event");
      }
    }

    if (f4 && !lastF4)
    {
      downButtonPressed = true;
      downButtonPressTime = millis();
      downButtonNextRepeatAt = downButtonPressTime + DIRECTION_REPEAT_DELAY;
      injectInputEvent(INPUT_BROKER_DOWN);
      LOG_INFO("DOWN button: Inject INPUT_BROKER_DOWN event");
    }
    else if (!f4 && lastF4)
    {
      downButtonPressed = false;
    }
    else if (f4 && downButtonPressed)
    {
      uint32_t now = millis();
      if (now >= downButtonNextRepeatAt)
      {
        injectInputEvent(INPUT_BROKER_DOWN);
        downButtonNextRepeatAt = now + DIRECTION_REPEAT_INTERVAL;
        LOG_INFO("DOWN button: Repeat INPUT_BROKER_DOWN event");
      }
    }

    if (f3 && !lastF3)
    {
      rightButtonPressed = true;
      rightButtonNextRepeatAt = millis() + DIRECTION_REPEAT_DELAY;
      injectInputEvent(INPUT_BROKER_RIGHT);
      LOG_INFO("RIGHT button: Inject INPUT_BROKER_RIGHT event");
    }
    else if (!f3 && lastF3)
    {
      rightButtonPressed = false;
    }
    else if (f3 && rightButtonPressed)
    {
      uint32_t now = millis();
      if (now >= rightButtonNextRepeatAt)
      {
        injectInputEvent(INPUT_BROKER_RIGHT);
        rightButtonNextRepeatAt = now + DIRECTION_REPEAT_INTERVAL;
        LOG_INFO("RIGHT button: Repeat INPUT_BROKER_RIGHT event");
      }
    }

    if (f5 && !lastF5)
    {
      escButtonPressed = true;
      escButtonPressTime = millis();
    }
    else if (!f5 && lastF5)
    {
      uint32_t pressDuration = millis() - escButtonPressTime;
      escButtonPressed = false;

      if (isOverlayActive || menuActive)
      {
        if (pressDuration < LONG_PRESS_THRESHOLD)
        {
          injectInputEvent(INPUT_BROKER_CANCEL);
          setMenuActive(false);
          LOG_INFO("Menu: Short press - Close menu");
        }
      }
      else
      {
        if (pressDuration >= SHUTDOWN_PRESS_THRESHOLD)
        {
          shutdownAtMsec = millis() + DEFAULT_SHUTDOWN_SECONDS * 1000;
          LOG_INFO("Normal: Long press 6s - Shutdown triggered");
        }
        else
        {
          LOG_INFO("Normal: CANCEL short press");
        }
      }
    }

    lastF1 = f1;
    lastF2 = f2;
    lastF3 = f3;
    lastF4 = f4;
    lastF5 = f5;
    lastBut = but;
  }
#endif
}