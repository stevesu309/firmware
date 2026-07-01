#include "Nodara.h"

#include "GpioButtonInput.h"
#include "McuDataInput.h"
#ifdef Nodara
#include "BurstTestModule.h"
#endif

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
#ifdef Nodara
    burstTestModule = new BurstTestModule();
#endif
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
