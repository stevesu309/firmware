#include "Nodara.h"

#include "GpioButtonInput.h"
#include "McuDataInput.h"

namespace nodara
{
namespace
{
GpioButtonInput gpioButtonInput;
McuDataInput mcuDataInput;
} // namespace

void setup()
{
    gpioButtonInput.setup();
    mcuDataInput.setup();
}

void loop()
{
    gpioButtonInput.loop();
    mcuDataInput.loop();
}

bool isMenuActive()
{
    return gpioButtonInput.isMenuActive();
}

void setMenuActive(bool active)
{
    gpioButtonInput.setMenuActive(active);
}
} // namespace nodara
