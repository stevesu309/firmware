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
    void setup();
    void loop();

  private:
    void setMenuActive(bool active);
    bool enterButtonPressed = false;
    bool enterLongPressTriggered = false;
    bool escButtonPressed = false;
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
    static const uint32_t LONG_PRESS_THRESHOLD = 2000;
    static const uint32_t SHUTDOWN_PRESS_THRESHOLD = 6000;
    static const uint32_t DIRECTION_REPEAT_DELAY = 500;
    static const uint32_t DIRECTION_REPEAT_INTERVAL = 150;
    bool menuActive = false;
  };
}