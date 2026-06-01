#include "FiveWayGpioInput.h"
#include "Arduino.h"
#include "graphics/EInkDynamicDisplay.h"
#include "graphics/Screen.h"
#include "main.h"
#include "input/InputBroker.h"

namespace redcoast915
{

  namespace
  {
    constexpr const char *kInputSource = "FiveWayGpioInput";

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

    // SW_F1=UP, SW_F2=LEFT, SW_F3=RIGHT, SW_F4=DOWN, SW_F5=CANCEL, SW_BUT=ENTER
    struct KeyState
    {
      bool up = false;
      bool left = false;
      bool right = false;
      bool down = false;
      bool cancel = false;
      bool enter = false;
    };

    KeyState readKeys()
    {
      KeyState keys;
      keys.up = digitalRead(SW_F1) == LOW;
      keys.left = digitalRead(SW_F2) == LOW;
      keys.right = digitalRead(SW_F3) == LOW;
      keys.down = digitalRead(SW_F4) == LOW;
      keys.cancel = digitalRead(SW_F5) == LOW;
      keys.enter = digitalRead(SW_BUT) == LOW;
      return keys;
    }

    void handleDirectionRepeat(bool pressed, bool &held, uint32_t &nextRepeatAt, input_broker_event eventType, const char *label)
    {
      if (pressed && !held)
      {
        held = true;
        nextRepeatAt = millis() + FiveWayGpioInput::DIRECTION_REPEAT_DELAY;
        injectInputEvent(eventType);
        LOG_INFO("%s: press", label);
      }
      else if (!pressed && held)
      {
        held = false;
      }
      else if (pressed && held)
      {
        uint32_t now = millis();
        if (now >= nextRepeatAt)
        {
          injectInputEvent(eventType);
          nextRepeatAt = now + FiveWayGpioInput::DIRECTION_REPEAT_INTERVAL;
          LOG_INFO("%s: repeat", label);
        }
      }
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
    pinMode(SW_F1, INPUT_PULLUP);  // UP
    pinMode(SW_F2, INPUT_PULLUP);  // LEFT
    pinMode(SW_F3, INPUT_PULLUP);  // RIGHT
    pinMode(SW_F4, INPUT_PULLUP);  // DOWN
    pinMode(SW_F5, INPUT_PULLUP);  // CANCEL
    pinMode(SW_BUT, INPUT_PULLUP); // ENTER
    pinMode(GPS_ONOFF_PIN, OUTPUT);
    digitalWrite(GPS_ONOFF_PIN, HIGH);
  }

  void FiveWayGpioInput::setMenuActive(bool active)
  {
    bool wasActive = menuActive;
    menuActive = active;

    if (wasActive && !active && screen && screen->getDisplayDevice())
    {
#ifdef USE_EINK
      EINK_ADD_FRAMEFLAG(screen->getDisplayDevice(), COSMETIC);
#endif
    }
  }

  void FiveWayGpioInput::handleEnterKey(bool enter, bool lastEnter, bool isOverlayActive)
  {
    const bool inMenuFlow = isOverlayActive || menuActive;

    if (enter && !lastEnter)
    {
      enterButtonPressed = true;
      enterButtonPressTime = millis();
      enterLongPressTriggered = false;
      return;
    }

    // Open menu as soon as threshold is reached; no release required
    if (enter && enterButtonPressed && !enterLongPressTriggered && !inMenuFlow)
    {
      if (millis() - enterButtonPressTime >= LONG_PRESS_THRESHOLD)
      {
        enterLongPressTriggered = true;
        injectInputEvent(INPUT_BROKER_SELECT);
        setMenuActive(true);
        LOG_INFO("ENTER: long press - open menu");
      }
      return;
    }

    if (!enter && lastEnter)
    {
      const bool menuOpenedWhileHeld = enterLongPressTriggered;
      enterButtonPressed = false;
      enterLongPressTriggered = false;

      if (menuOpenedWhileHeld)
        return;

      if (inMenuFlow)
      {
        if (millis() - enterButtonPressTime < LONG_PRESS_THRESHOLD)
        {
          setMenuActive(true);
          injectInputEvent(INPUT_BROKER_SELECT);
          LOG_INFO("ENTER: short press - select");
        }
        return;
      }

      if (screen)
        screen->setOn(true);
      LOG_INFO("ENTER: short press");
    }
  }

  void FiveWayGpioInput::handleCancelKey(bool cancel, bool lastCancel, bool isOverlayActive)
  {
    if (cancel && !lastCancel)
    {
      cancelButtonPressed = true;
      cancelButtonPressTime = millis();
      return;
    }

    if (!cancel && lastCancel)
    {
      const uint32_t pressDuration = millis() - cancelButtonPressTime;
      cancelButtonPressed = false;

      if (isOverlayActive || menuActive)
      {
        if (pressDuration < LONG_PRESS_THRESHOLD)
        {
          injectInputEvent(INPUT_BROKER_CANCEL);
          setMenuActive(false);
          LOG_INFO("CANCEL: short press - close menu");
        }
        return;
      }

      if (pressDuration >= SHUTDOWN_PRESS_THRESHOLD)
      {
        shutdownAtMsec = millis() + DEFAULT_SHUTDOWN_SECONDS * 1000;
        LOG_INFO("CANCEL: long press - shutdown");
      }
      else
      {
        LOG_INFO("CANCEL: short press");
      }
    }
  }

  void FiveWayGpioInput::loop()
  {
    static KeyState lastKeys{};
    static bool initialized = false;

    const KeyState keys = readKeys();

    if (!initialized)
    {
      lastKeys = keys;
      initialized = true;
      return;
    }

    if (screen && !screen->getScreenOn())
    {
      if (keys.enter && !lastKeys.enter)
      {
        screen->setOn(true);
        LOG_INFO("Screen off: ENTER - wake");
      }

      enterButtonPressed = false;
      enterLongPressTriggered = false;
      cancelButtonPressed = false;
      leftButtonPressed = false;
      rightButtonPressed = false;
      upButtonPressed = false;
      downButtonPressed = false;
      lastKeys = keys;
      return;
    }

    const bool isOverlayActive = screen && screen->isOverlayBannerShowing();

    if (isOverlayActive && !menuActive)
      setMenuActive(true);
    else if (menuActive && !isOverlayActive)
      setMenuActive(false);

    handleEnterKey(keys.enter, lastKeys.enter, isOverlayActive);
    handleCancelKey(keys.cancel, lastKeys.cancel, isOverlayActive);

    handleDirectionRepeat(keys.left, leftButtonPressed, leftButtonNextRepeatAt, INPUT_BROKER_LEFT, "LEFT");
    handleDirectionRepeat(keys.right, rightButtonPressed, rightButtonNextRepeatAt, INPUT_BROKER_RIGHT, "RIGHT");
    handleDirectionRepeat(keys.up, upButtonPressed, upButtonNextRepeatAt, INPUT_BROKER_UP, "UP");
    handleDirectionRepeat(keys.down, downButtonPressed, downButtonNextRepeatAt, INPUT_BROKER_DOWN, "DOWN");

    lastKeys = keys;
  }
#endif
} // namespace redcoast915
