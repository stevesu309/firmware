#pragma once

#include <vector>
#include <map>
#include "mesh/generated/meshtastic/mesh.pb.h"
#include "NodeDB.h"

namespace redcoast915
{
  class FiveWayGpioInput
  {
  public:
    FiveWayGpioInput();
    ~FiveWayGpioInput();
    // Configure GPIO modes for the five-way switch and cancel button.
    void setup();
    // Poll button states and translate them into InputBroker events.
    void loop();
    bool isMenuActive() const { return menuActive; }

    // Mirrors the current overlay/menu interaction state so key handling
    // can keep working during menu transitions.
    void setMenuActive(bool active);

  private:
    // ENTER uses separate flags so long-press can open the menu immediately
    // while suppressing an extra action on button release.
    bool enterButtonPressed = false;
    bool enterLongPressTriggered = false;

    // ESC/CANCEL keeps release-based handling because short press closes
    // menus while long press in normal mode requests shutdown.
    bool escButtonPressed = false;

    // Direction keys use press/repeat state to support auto-repeat scrolling.
    bool leftButtonPressed = false;
    bool rightButtonPressed = false;
    bool upButtonPressed = false;
    bool downButtonPressed = false;

    uint32_t enterButtonPressTime = 0;
    uint32_t escButtonPressTime = 0;
    uint32_t leftButtonNextRepeatAt = 0;
    uint32_t rightButtonNextRepeatAt = 0;
    uint32_t upButtonPressTime = 0;
    uint32_t downButtonPressTime = 0;
    uint32_t upButtonNextRepeatAt = 0;
    uint32_t downButtonNextRepeatAt = 0;

    // ENTER menu-open threshold and ESC shutdown threshold.
    static const uint32_t LONG_PRESS_THRESHOLD = 2000;
    static const uint32_t SHUTDOWN_PRESS_THRESHOLD = 6000;
    // Delay before directional auto-repeat starts, then repeat cadence.
    static const uint32_t DIRECTION_REPEAT_DELAY = 500;
    static const uint32_t DIRECTION_REPEAT_INTERVAL = 150;

    // Local view of whether the UI is currently in a menu/overlay flow.
    bool menuActive = false;
  };
}