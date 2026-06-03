// #define RED_BANK_S3

// #define I2C_SDA 5
// #define I2C_SCL 4
#define USE_SX1262
// #define USE_SX1268

#define HAS_WIRE 0 // 没有iic设备

// Leave undefined to disable our PMU IRQ handler.  DO NOT ENABLE THIS because the pmuirq can cause sperious interrupts
// and waking from light sleep
// #define PMU_IRQ 40
// #define HAS_AXP2101

// #define HAS_RTC 1

#define GPS_RX_PIN 18
#define GPS_TX_PIN 17

// Display (E-Ink)
#define PIN_EINK_CS 15
#define PIN_EINK_BUSY 13
#define PIN_EINK_DC 16
#define PIN_EINK_RES 12
#define PIN_EINK_SCLK 14
#define PIN_EINK_MOSI 11

#define LORA_DIO0 -1 // a No connect on the SX1262 module
#define LORA_RESET 38

#define LORA_DIO3 // Not connected on PCB, if DIO3 is high the TXCO is enabled
#define LORA_SCK 5
#define LORA_MISO 3
#define LORA_MOSI 6

// 双模块单独引脚：
// - 900MHz 模块：CS 接在 GPIO7
// - 433MHz 模块：CS 接在 GPIO4
// 默认的 LORA_CS 仍然指向 900MHz 的 CS（GPIO7），
// 实际选择哪个模块由 main.cpp 中的运行时逻辑根据 config.lora.region 决定。
#define LORA_CS_900 7
#define LORA_CS_433 4
#define LORA_BUSY_433 47 // LoRa 模块电源控制引脚 // 收发模式切换
#define LORA_DIO1_433 48
#define LORA_CS LORA_CS_900
#define LORA_BUSY_900 9
#define LORA_DIO1_900 10 // SX1262 IRQ
#define LORA_BUSY LORA_BUSY_900

#define SX126X_CS LORA_CS         // 作为默认值，实际 cs 在 RED_BANK_S3 中会根据区域动态选择
#define SX126X_DIO1 LORA_DIO1_900 // LORA_DIO1
#define SX126X_BUSY LORA_BUSY
#define SX126X_RESET LORA_RESET // LORA_RESET
#define SX126X_DIO2_AS_RF_SWITCH

#define PIN_GPS_RESET (39) // GNSS模块复位引脚

#define PIN_LORA_EN 42 // LoRa 模块电源控制引脚
#define SX126X_POWER_EN 42
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

#define BATTERY_PIN 8
#define ADC_CHANNEL ADC1_GPIO8_CHANNEL