// #define RED_BANK_S3

// #define I2C_SDA 5
// #define I2C_SCL 4
#define USE_SX1262
#define HAS_WIRE 0 // 没有iic设备

// Leave undefined to disable our PMU IRQ handler.  DO NOT ENABLE THIS because the pmuirq can cause sperious interrupts
// and waking from light sleep
// #define PMU_IRQ 40
// #define HAS_AXP2101

// #define HAS_RTC 1

#define GPS_RX_PIN 18
#define GPS_TX_PIN 17
// 这是v7版的引脚定义
#if 0

// Display (E-Ink)
#define PIN_EINK_BS1 4
#define PIN_EINK_CS 15
#define PIN_EINK_BUSY 48
#define PIN_EINK_DC 16
#define PIN_EINK_RES 47
#define PIN_EINK_SCLK 14
#define PIN_EINK_MOSI 11
#define EINK_POWER_PIN 38

#define LORA_DIO0 -1 // a No connect on the SX1262 module
#define LORA_RESET 8
#define LORA_DIO1 10    // SX1262 IRQ
#define LORA_DIO2 9     // SX1262 BUSY
#define LORA_DIO3       // Not connected on PCB, but internally on the TTGO SX1262, if DIO3 is high the TXCO is enabled
#define LORA_ANT_900 12 // 天线选择引脚 US United States 902.0 - 928.0
#define LORA_ANT_413 13 // 天线选择引脚 CN China 470.0 - 510.0
#ifdef USE_SX1262
#define SX126X_CS 7    // FIXME - we really should define LORA_CS instead
#define SX126X_DIO1 10 // LORA_DIO1
#define SX126X_BUSY 9  // LORA_DIO2
#define SX126X_RESET 8 // LORA_RESET
#define SX126X_DIO2_AS_RF_SWITCH
// #define SX126X_DIO3_TCXO_VOLTAGE 1.8
#endif
#define PIN_GPS_RESET (41) // GNSS模块复位引脚

#define LORA_SCK 5
#define LORA_MISO 3
#define LORA_MOSI 6
#define LORA_CS 7

#endif
// 这是v8版的引脚定义
#if 1

// Display (E-Ink)
// #define PIN_EINK_BS1 4
#define PIN_EINK_CS 15
#define PIN_EINK_BUSY 13
#define PIN_EINK_DC 16
#define PIN_EINK_RES 12
#define PIN_EINK_SCLK 14
#define PIN_EINK_MOSI 11
#define EINK_POWER_PIN 38

#define LORA_DIO0 -1 // a No connect on the SX1262 module
#define LORA_RESET 8
#define LORA_DIO1 10 // SX1262 IRQ
#define LORA_DIO2 9  // SX1262 BUSY
#define LORA_DIO3    // Not connected on PCB, but internally on the TTGO SX1262, if DIO3 is high the TXCO is enabled
#define LORA_ANT 47  // 天线选择引脚 LOW：900MHz HIGH：432MHz
// #define LORA_ANT_413 48 // 天线选择引脚 CN China 470.0 - 510.0
#ifdef USE_SX1262
#define SX126X_CS 7    // FIXME - we really should define LORA_CS instead
#define SX126X_DIO1 10 // LORA_DIO1
#define SX126X_BUSY 9  // LORA_DIO2
#define SX126X_RESET 8 // LORA_RESET
#define SX126X_DIO2_AS_RF_SWITCH
// #define SX126X_DIO3_TCXO_VOLTAGE 1.8
#endif
#define PIN_GPS_RESET (39) // GNSS模块复位引脚

#define LORA_SCK 5
#define LORA_MISO 3
#define LORA_MOSI 6
#define LORA_CS 7

#endif

#define PIN_LORA_EN 42 // LoRa 模块电源控制引脚
// #define HAS_SDCARD // Have SPI interface SD card slot
// #define SDCARD_USE_SPI1

// #define HAS_GPS
#define PIN_GPS_EN (21)       // GNSS模块电源控制引脚
#define GPS_EN_ACTIVE (HIGH)  // 输出使能为高电平
#define GPS_RESET_MODE (HIGH) // 复位为低电平，但是这个引脚接了一个NPN，高电平导通，导通就是低电平复位，所以是高电平复位

// Button configuration for RED_BANK_S3
#define BUTTON_PIN 1
#define BUTTON_ACTIVE_LOW true
#define BUTTON_ACTIVE_PULLUP true
#define BUTTON_NEED_PULLUP
#define HAS_BUTTON 1