#include "BurstTestModule.h"

#ifdef Nodara

#include "Default.h"
#include "MeshService.h"
#include "Router.h"
#include "configuration.h"
#include "mesh/generated/meshtastic/mesh.pb.h"

BurstTestModule *burstTestModule = nullptr;

static constexpr size_t BURST_TEST_PAYLOAD_LEN = 150;

BurstTestModule::BurstTestModule() : concurrency::OSThread("BurstTest") {}

void BurstTestModule::toggle()
{
    if (sendingEnabled) {
        sendingEnabled = false;
        LOG_INFO("BurstTest OFF: sent %u pkts in %u ms", packetsSent, millis() - enabledAtMs);
        setIntervalFromNow(INT32_MAX);
        return;
    }

    sendingEnabled = true;
    packetsSent = 0;
    enabledAtMs = millis();
    LOG_INFO("BurstTest ON: DM to 0x%08x, %u-byte TEXT_MESSAGE packets", BURST_TEST_DM_TARGET, BURST_TEST_PAYLOAD_LEN);
    enabled = true;
    setIntervalFromNow(0);
}

void BurstTestModule::sendOnePacket()
{
    meshtastic_MeshPacket *p = router->allocForSending();
    p->decoded.portnum = meshtastic_PortNum_TEXT_MESSAGE_APP;
    p->to = BURST_TEST_DM_TARGET;
    p->want_ack = false;
    p->hop_limit = Default::getConfiguredOrDefaultHopLimit(config.lora.hop_limit);
    p->priority = meshtastic_MeshPacket_Priority_BACKGROUND;

    memset(p->decoded.payload.bytes, 'A' + (packetsSent % 26), BURST_TEST_PAYLOAD_LEN);
    int prefixLen = snprintf((char *)p->decoded.payload.bytes, BURST_TEST_PAYLOAD_LEN, "burst %u ", packetsSent);
    if (prefixLen > 0 && (size_t)prefixLen < BURST_TEST_PAYLOAD_LEN) {
        p->decoded.payload.bytes[prefixLen] = ' ';
    }
    p->decoded.payload.size = BURST_TEST_PAYLOAD_LEN;

    service->sendToMesh(p);
    packetsSent++;

    if ((packetsSent % 10) == 0) {
        LOG_INFO("BurstTest: sent %u pkts, elapsed %u ms", packetsSent, millis() - enabledAtMs);
    }
}

int32_t BurstTestModule::runOnce()
{
    if (!sendingEnabled) {
        return INT32_MAX;
    }

    meshtastic_QueueStatus qs = router->getQueueStatus();
    if (qs.free != qs.maxlen) {
        return 50;
    }

    sendOnePacket();
    return 3000;
}

#endif
