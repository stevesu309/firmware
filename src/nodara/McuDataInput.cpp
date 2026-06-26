#include "McuDataInput.h"

#include "Arduino.h"
#include "main.h"
#include "MessageStore.h"
#include "mesh/Throttle.h"
#include "mesh/NodeDB.h"

#include <cstdio>
#include <cstring>

namespace nodara
{
namespace
{
#ifndef MCU_DATA_BAUD
#define MCU_DATA_BAUD 1200
#endif

#if defined(MCU_DATA_SERIAL_8E1)
constexpr uint16_t kMcuDataSerialConfig = SERIAL_8E1;
constexpr const char *kMcuDataSerialConfigName = "8E1";
#else
constexpr uint16_t kMcuDataSerialConfig = SERIAL_8N1;
constexpr const char *kMcuDataSerialConfigName = "8N1";
#endif

constexpr uint32_t kMcuDataBaud = MCU_DATA_BAUD;
constexpr uint32_t kMcuDataFrameGapMs = 100;
constexpr uint32_t kMcuDataPinLogIntervalMs = 1000;
constexpr uint32_t kPowerOffBannerMs = 5000;
constexpr uint32_t kLedBlinkMs = 100;
constexpr size_t kMcuDataMaxFrameSize = 32;
constexpr uint8_t kMcuDataAddress = 0x01;
constexpr uint16_t kMcuDataRegister = 0x1000;
constexpr size_t kMcuDataWriteSingleFrameSize = 8;
constexpr size_t kMcuDataWriteMultipleFrameSize = 13;
constexpr uint8_t kMcuDataWriteMultipleCommand03[kMcuDataWriteMultipleFrameSize] = {0x01, 0x10, 0x10, 0x00, 0x00,
                                                                                    0x02, 0x04, 0x00, 0x03, 0x00,
                                                                                    0x00, 0xCE, 0x6F};
constexpr uint8_t kMcuDataWriteMultipleCommand05[kMcuDataWriteMultipleFrameSize] = {0x01, 0x10, 0x10, 0x00, 0x00,
                                                                                    0x02, 0x04, 0x00, 0x05, 0x00,
                                                                                    0x00, 0x2E, 0x6E};

uint8_t frame[kMcuDataMaxFrameSize] = {};
size_t frameSize = 0;
uint32_t lastByteMs = 0;

uint16_t readBe16(const uint8_t *data)
{
    return (static_cast<uint16_t>(data[0]) << 8) | data[1];
}

uint16_t modbusCrc16(const uint8_t *data, size_t size)
{
    uint16_t crc = 0xFFFF;

    for (size_t i = 0; i < size; i++) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; bit++) {
            if ((crc & 0x0001) != 0)
                crc = (crc >> 1) ^ 0xA001;
            else
                crc >>= 1;
        }
    }

    return crc;
}

bool hasValidModbusCrc(const uint8_t *data, size_t size)
{
    if (size < 4)
        return false;

    const uint16_t expected = modbusCrc16(data, size - 2);
    const uint16_t received = static_cast<uint16_t>(data[size - 2]) | (static_cast<uint16_t>(data[size - 1]) << 8);
    return expected == received;
}

void logMcuDataFrame(const uint8_t *data, size_t size)
{
    char hex[kMcuDataMaxFrameSize * 3] = {};
    size_t offset = 0;

    for (size_t i = 0; i < size && offset < sizeof(hex); i++) {
        const int written = snprintf(hex + offset, sizeof(hex) - offset, "%02X%s", data[i], i + 1 < size ? " " : "");
        if (written <= 0)
            break;
        offset += static_cast<size_t>(written);
    }

    LOG_INFO("MCU_DATA RX (%u bytes): %s", static_cast<unsigned>(size), hex);
}

void blinkPowerOffLed()
{
#ifdef PIN_LED2
    for (uint8_t i = 0; i < 8; i++) {
        digitalWrite(PIN_LED2, LED_STATE_ON);
        pinMode(PIN_LED2, OUTPUT);
        delay(kLedBlinkMs);
        ledOff(PIN_LED2);
        delay(kLedBlinkMs);
    }
#endif
}

void persistBeforePowerCut()
{
#if defined(RED_BANK_S3) || defined(Nodara)
    if (chatHistoryStore)
        chatHistoryStore->persistToDisk();
#endif
    if (nodeDB)
        nodeDB->saveToDisk();
#if HAS_SCREEN
    messageStore.saveToFlash();
#endif
}

#if defined(MCU_DATA_PIN_DEBUG)
void processMcuDataPinDebug()
{
    static bool initialized = false;
    static bool lastLevel = true;
    static uint32_t lastLogMs = 0;
    static uint32_t samples = 0;
    static uint32_t lowSamples = 0;
    static uint32_t edges = 0;

    const bool level = digitalRead(MCU_DATA_PIN) == HIGH;
    if (!initialized) {
        initialized = true;
        lastLevel = level;
        lastLogMs = millis();
    }

    samples++;
    if (!level)
        lowSamples++;
    if (level != lastLevel) {
        edges++;
        lastLevel = level;
    }

    if (!Throttle::isWithinTimespanMs(lastLogMs, kMcuDataPinLogIntervalMs)) {
        if (edges > 0 || lowSamples > 0) {
            LOG_INFO("MCU_DATA pin level=%s samples=%u low=%u edges=%u", level ? "HIGH" : "LOW",
                     static_cast<unsigned>(samples), static_cast<unsigned>(lowSamples), static_cast<unsigned>(edges));
        }

        lastLogMs = millis();
        samples = 0;
        lowSamples = 0;
        edges = 0;
    }
}
#endif
} // namespace

void McuDataInput::setup()
{
#if defined(Nodara) && defined(PIN_SERIAL2_RX) && defined(PIN_SERIAL2_TX)
    pinMode(MCU_DATA_PIN, INPUT_PULLUP);
    Serial2.begin(kMcuDataBaud, kMcuDataSerialConfig);
    LOG_INFO("MCU_DATA serial RX initialized on pin %u at %u baud %s", static_cast<unsigned>(MCU_DATA_PIN),
             static_cast<unsigned>(kMcuDataBaud), kMcuDataSerialConfigName);
#endif
}

void McuDataInput::loop()
{
#if defined(Nodara) && defined(PIN_SERIAL2_RX) && defined(PIN_SERIAL2_TX)
    while (Serial2.available() > 0) {
        const int value = Serial2.read();
        if (value < 0)
            break;

        if (frameSize >= sizeof(frame)) {
            logMcuDataFrame(frame, frameSize);
            frameSize = 0;
        }

        frame[frameSize++] = static_cast<uint8_t>(value);
        lastByteMs = millis();
    }

    if (frameSize > 0 && !Throttle::isWithinTimespanMs(lastByteMs, kMcuDataFrameGapMs)) {
        logMcuDataFrame(frame, frameSize);
        if (!hasValidModbusCrc(frame, frameSize)) {
            LOG_WARN("MCU_DATA invalid CRC");
        } else if (frameSize == kMcuDataWriteSingleFrameSize && frame[0] == kMcuDataAddress && frame[1] == 0x06 &&
                   readBe16(&frame[2]) == kMcuDataRegister) {
            const uint16_t command = readBe16(&frame[4]);
            if (command == 0x0003)
                handleCommand03();
            else if (command == 0x0005)
                handleCommand05();
            else
                LOG_WARN("MCU_DATA unknown command value 0x%04X", command);
        } else if (frameSize == kMcuDataWriteMultipleFrameSize &&
                   memcmp(frame, kMcuDataWriteMultipleCommand03, frameSize) == 0) {
            handleCommand03();
        } else if (frameSize == kMcuDataWriteMultipleFrameSize &&
                   memcmp(frame, kMcuDataWriteMultipleCommand05, frameSize) == 0) {
            handleCommand05();
        } else {
            LOG_WARN("MCU_DATA unsupported Modbus frame");
        }

        frameSize = 0;
        lastByteMs = 0;
    }

#if defined(MCU_DATA_PIN_DEBUG)
    processMcuDataPinDebug();
#endif
#endif
}

void McuDataInput::handleCommand03()
{
    LOG_INFO("MCU_DATA command 0x0003 received");
    shutdownAtMsec = millis() + DEFAULT_SHUTDOWN_SECONDS * 1000;
    LOG_INFO("MCU_DATA command 0x0003 - shutdown in %u seconds", static_cast<unsigned>(DEFAULT_SHUTDOWN_SECONDS));
}

void McuDataInput::handleCommand05()
{
    LOG_INFO("MCU_DATA command 0x0005 received");
#if HAS_SCREEN
    if (screen)
        screen->showSimpleBanner("Power Off", kPowerOffBannerMs);
#endif
    blinkPowerOffLed();
    persistBeforePowerCut();
    LOG_INFO("MCU_DATA command 0x0005 - power off preparation complete");
}
} // namespace nodara
