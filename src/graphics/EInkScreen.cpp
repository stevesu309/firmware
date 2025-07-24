#include "EInkScreen.h"
#include <Arduino.h>
#include <SPI.h>

namespace graphics
{
    EInkScreen::EInkScreen() {}

    void EInkScreen::setup()
    {
        pinMode(PIN_EINK_CS, OUTPUT);
        pinMode(PIN_EINK_DC, OUTPUT);
        pinMode(PIN_EINK_RES, OUTPUT);
        pinMode(PIN_EINK_BUSY, INPUT);
        pinMode(EINK_POWER_PIN, OUTPUT);
        digitalWrite(EINK_POWER_PIN, HIGH);
        SPI.begin(PIN_EINK_SCLK, -1, PIN_EINK_MOSI, PIN_EINK_CS);
        printf("E-Ink screen setup complete.\n");
    }

        void EInkScreen::lightUp()
    {
        digitalWrite(EINK_POWER_PIN, HIGH);
        delay(10);
        reset();
        init();
        displayWhite();
        printf("E-Ink screen initialized and powered up.\n");
    }

    // 等待屏幕就绪
    void EInkScreen::waitUntilReady()
    {
        while (digitalRead(PIN_EINK_BUSY) == HIGH) // BUSY高表示忙
        {
            delay(10);
        }
    }

    // 发送命令
    void EInkScreen::sendCommand(uint8_t command)
    {
        digitalWrite(PIN_EINK_DC, LOW);
        digitalWrite(PIN_EINK_CS, LOW);
        SPI.transfer(command);
        digitalWrite(PIN_EINK_CS, HIGH);
    }

    // 发送数据
    void EInkScreen::sendData(uint8_t data)
    {
        digitalWrite(PIN_EINK_DC, HIGH);
        digitalWrite(PIN_EINK_CS, LOW);
        SPI.transfer(data);
        digitalWrite(PIN_EINK_CS, HIGH);
    }

    // 软件复位
    void EInkScreen::reset()
    {
        digitalWrite(PIN_EINK_RES, LOW);
        delay(50);
        digitalWrite(PIN_EINK_RES, HIGH);
        delay(50);
        waitUntilReady();
    }

    // 初始化屏幕
    void EInkScreen::init()
    {
        reset();
        sendCommand(0x12); // 软件复位
        waitUntilReady();

        sendCommand(0x01); // Driver output control
        sendData(LCD_YSIZE - 1);
        sendData((LCD_YSIZE - 1) >> 8);
        sendData(0x00);

        sendCommand(0x11); // 数据输入模式设置
        sendData(0x03);

        sendCommand(0x44); // 设置Source范围
        sendData(0x00);
        sendData((LCD_XSIZE / 8) - 1);

        sendCommand(0x45); // 设置Gate范围
        sendData(0x00);
        sendData(0x00);
        sendData(LCD_YSIZE - 1);
        sendData((LCD_YSIZE - 1) >> 8);

        sendCommand(0x3C); // 边界控制
        sendData(0x05);

        sendCommand(0x4E); // Set RAM X address counter
        sendData(0x00);
        sendCommand(0x4F); // Set RAM Y address counter
        sendData(0x00);
        sendData(0x00);

        waitUntilReady();
    }

    // 刷新显示
    void EInkScreen::refresh()
    {
        sendCommand(0x18); // 温度传感器选择
        sendData(0x80);    // 内部
        sendCommand(0x22);
        sendData(0xF7);
        sendCommand(0x20);
        delay(10);
        waitUntilReady();
    }

    // 休眠
    void EInkScreen::sleep()
    {
        sendCommand(0x10);
        sendData(0x01);
        delay(100);
    }

    // 显示全白
    void EInkScreen::displayWhite()
    {
        sendCommand(0x24);
        for (int y = 0; y < LCD_YSIZE; y++)
            for (int x = 0; x < (LCD_XSIZE / 8); x++)
                sendData(0xFF);

        sendCommand(0x26);
        for (int y = 0; y < LCD_YSIZE; y++)
            for (int x = 0; x < (LCD_XSIZE / 8); x++)
                sendData(0x00);

        refresh();
        sleep();
        printf("E-Ink screen displayed white.\n");
    }

    // 显示全黑
    void EInkScreen::displayBlack()
    {
        sendCommand(0x24);
        for (int y = 0; y < LCD_YSIZE; y++)
            for (int x = 0; x < (LCD_XSIZE / 8); x++)
                sendData(0x00);

        sendCommand(0x26);
        for (int y = 0; y < LCD_YSIZE; y++)
            for (int x = 0; x < (LCD_XSIZE / 8); x++)
                sendData(0x00);

        refresh();
        sleep();
    }

    // 显示全红
    void EInkScreen::displayRed()
    {
        sendCommand(0x24);
        for (int y = 0; y < LCD_YSIZE; y++)
            for (int x = 0; x < (LCD_XSIZE / 8); x++)
                sendData(0x00);

        sendCommand(0x26);
        for (int y = 0; y < LCD_YSIZE; y++)
            for (int x = 0; x < (LCD_XSIZE / 8); x++)
                sendData(0xFF);

        refresh();
        sleep();
    }

    // 显示图片
    void EInkScreen::displayBMP(const uint8_t *BW_data, const uint8_t *R_data)
    {
        sendCommand(0x24);
        for (int y = 0; y < LCD_YSIZE; y++)
            for (int x = 0; x < (LCD_XSIZE / 8); x++)
                sendData(*BW_data++);

        sendCommand(0x26);
        for (int y = 0; y < LCD_YSIZE; y++)
            for (int x = 0; x < (LCD_XSIZE / 8); x++)
                sendData(*R_data++);

        refresh();
        sleep();
    }

    void EInkScreen::displayHello()
    {
        printf("Displaying 'HELLO' on the E-Ink screen...\n");
        // 简单 8x16 字体，每个字母占 8x16 像素
        // 这里只显示 "HELLO" 五个字母，居中显示
        const int charWidth = 8;
        const int charHeight = 16;
        const int numChars = 5;
        const int startX = (LCD_XSIZE - charWidth * numChars) / 2;
        const int startY = (LCD_YSIZE - charHeight) / 2;

        // 这里用全黑底
        sendCommand(0x24);
        for (int y = 0; y < LCD_YSIZE; y++)
        {
            for (int x = 0; x < (LCD_XSIZE / 8); x++)
            {
                sendData(0x00);
            }
        }

        // 这里用全黑底，红色层不用
        sendCommand(0x26);
        for (int y = 0; y < LCD_YSIZE; y++)
        {
            for (int x = 0; x < (LCD_XSIZE / 8); x++)
            {
                sendData(0x00);
            }
        }

        // 简单字模（只包含 H E L O，L 用两次）
        const uint8_t font[5][16] = {
            // H
            {0x81, 0x81, 0x81, 0x81, 0xFF, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81},
            // E
            {0xFF, 0x80, 0x80, 0x80, 0x80, 0xFF, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0xFF, 0x00},
            // L
            {0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0xFF, 0x00},
            // L
            {0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0xFF, 0x00},
            // O
            {0x7E, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x7E, 0x00}};

        // 在缓冲区生成 "HELLO" 的点阵
        uint8_t bwBuffer[LCD_YSIZE * (LCD_XSIZE / 8)] = {0};
        for (int c = 0; c < numChars; c++)
        {
            for (int row = 0; row < charHeight; row++)
            {
                int y = startY + row;
                if (y < 0 || y >= LCD_YSIZE)
                    continue;
                int xByte = (startX + c * charWidth) / 8;
                int xBit = (startX + c * charWidth) % 8;
                for (int col = 0; col < charWidth; col++)
                {
                    if (font[c][row] & (1 << (7 - col)))
                    {
                        int x = startX + c * charWidth + col;
                        if (x < 0 || x >= LCD_XSIZE)
                            continue;
                        int byteIdx = y * (LCD_XSIZE / 8) + (x / 8);
                        bwBuffer[byteIdx] |= (0x80 >> (x % 8));
                    }
                }
            }
        }

        // 写入黑白层
        sendCommand(0x24);
        for (int y = 0; y < LCD_YSIZE; y++)
        {
            for (int x = 0; x < (LCD_XSIZE / 8); x++)
            {
                sendData(bwBuffer[y * (LCD_XSIZE / 8) + x]);
            }
        }

        // 红色层全黑
        sendCommand(0x26);
        for (int y = 0; y < LCD_YSIZE; y++)
        {
            for (int x = 0; x < (LCD_XSIZE / 8); x++)
            {
                sendData(0x00);
            }
        }

        refresh();
        sleep();
        printf("E-Ink screen displayed 'HELLO'.\n");
    }
}