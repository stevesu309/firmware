#ifndef EINKSCREEN_H
#define EINKSCREEN_H

#include <SPI.h>

// 定义 E-Ink 屏幕相关引脚
#define PIN_EINK_CS 15
#define PIN_EINK_BUSY 48
#define PIN_EINK_DC 16
#define PIN_EINK_RES 47
#define PIN_EINK_SCLK 14
#define PIN_EINK_MOSI 11
#define EINK_POWER_PIN 38
#define LCD_YSIZE 264 // 屏幕高度（像素）
#define LCD_XSIZE 176 // 屏幕宽度（像素）

namespace graphics
{
    class EInkScreen
    {
    public:
        EInkScreen();
        void setup();                                                   // 初始化引脚和 SPI
        void lightUp();                                                 // 屏幕上电
        void displayHello();                                            // 显示 "Hello" 文本
        void init();                                                    // 屏幕初始化
        void reset();                                                   // 软件复位
        void refresh();                                                 // 刷新显示
        void sleep();                                                   // 休眠
        void displayWhite();                                            // 全白
        void displayBlack();                                            // 全黑
        void displayRed();                                              // 全红
        void displayBMP(const uint8_t *BW_data, const uint8_t *R_data); // 显示图片（黑白+红）

    private:
        void sendCommand(uint8_t command);
        void sendData(uint8_t data);
        void waitUntilReady();
    };
}

#endif // EINKSCREEN_H