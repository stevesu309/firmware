#pragma once

#include "concurrency/OSThread.h"

#ifdef Nodara

// Temporary mesh test: double-tap SW_F5 toggles TEXT_MESSAGE packets to BURST_TEST_DM_TARGET (burst N in app).
static constexpr uint32_t BURST_TEST_DM_TARGET = 0xffffffff; // device B; change before flash

class BurstTestModule : private concurrency::OSThread
{
  public:
    BurstTestModule();

    void toggle();
    bool isEnabled() const { return sendingEnabled; }

  protected:
    virtual int32_t runOnce() override;

  private:
    void sendOnePacket();

    bool sendingEnabled = false;
    uint32_t packetsSent = 0;
    uint32_t enabledAtMs = 0;
};

extern BurstTestModule *burstTestModule;

#endif
