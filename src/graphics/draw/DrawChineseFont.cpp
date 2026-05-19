#include "DrawChineseFont.h"
#include <string.h>
#include "red_bank_s3/Flash.h"
#include "DebugConfiguration.h"
#include <new>

#ifndef CNFONT_EMBED_INTERNAL_TABLE
#define CNFONT_EMBED_INTERNAL_TABLE 0
#endif

namespace
{
    static constexpr uint32_t kChineseFontMagic = CNFONT_CFG_MAGIC;
    static constexpr uint32_t kChineseFontVersion = CNFONT_CFG_VERSION;
    static constexpr uint32_t kChineseFontBaseAddr = CNFONT_CFG_EXT_ADDR;
    static constexpr uint32_t kChineseFontMaxBytes = CNFONT_CFG_MAX_BYTES;
    static constexpr uint32_t kUtf8KeySize = CNFONT_CFG_KEY_SIZE;
    static constexpr uint32_t kBitmapSize = CNFONT_CFG_BITMAP_SIZE;
    static constexpr uint32_t kGlyphWidth = CNFONT_CFG_GLYPH_WIDTH;
    static constexpr uint32_t kGlyphHeight = CNFONT_CFG_GLYPH_HEIGHT;
    static constexpr uint32_t kBytesPerRow = (kGlyphWidth + 7U) / 8U;
    static constexpr int16_t kLineHeight = CNFONT_CFG_LINE_HEIGHT;
    static constexpr int16_t kGlyphYOffset = CNFONT_CFG_Y_OFFSET;

    struct ChineseFontFileHeader
    {
        uint32_t magic;
        uint32_t version;
        uint32_t count;
        uint32_t reserved;
    };

    static bool gExternalFontTriedInit = false;
    static bool gExternalFontReady = false;
    static uint32_t gExternalFontCount = 0;
    static uint8_t *gExternalKeys = nullptr; // count * 4 bytes
    static bool gExternalFontLoggedReady = false;
    static bool gExternalFontLoggedHit = false;
    static bool gExternalFontLoggedFallback = false;

    static void makeUtf8Key(const char *utf8, uint8_t out[kUtf8KeySize])
    {
        memset(out, 0, kUtf8KeySize);
        if (!utf8)
            return;
        for (uint32_t i = 0; i < (kUtf8KeySize - 1U) && utf8[i] != '\0'; ++i)
        {
            out[i] = static_cast<uint8_t>(utf8[i]);
        }
    }

    static bool exportChineseFontToExternal()
    {
#if !CNFONT_EMBED_INTERNAL_TABLE
        LOG_WARN("[CNFONT][EXT] embedded table disabled, skip export");
        return false;
#else
        ChineseFontFileHeader header = {kChineseFontMagic, kChineseFontVersion, chineseFontCount, 0};
        const uint32_t keyBytes = chineseFontCount * kUtf8KeySize;
        const uint32_t bitmapBytes = chineseFontCount * kBitmapSize;
        const uint32_t totalBytes = sizeof(header) + keyBytes + bitmapBytes;
        if (totalBytes > kChineseFontMaxBytes)
        {
            LOG_WARN("[CNFONT][EXT] layout too large: need=%lu limit=%lu",
                     (unsigned long)totalBytes, (unsigned long)kChineseFontMaxBytes);
            return false;
        }

        if (!Esp32PowerLog::ExtFlashRawErase(kChineseFontBaseAddr, totalBytes))
        {
            LOG_WARN("[CNFONT][EXT] erase failed base=0x%08lx len=%lu",
                     (unsigned long)kChineseFontBaseAddr, (unsigned long)totalBytes);
            return false;
        }

        if (!Esp32PowerLog::ExtFlashRawWrite(kChineseFontBaseAddr, &header, sizeof(header)))
        {
            LOG_WARN("[CNFONT][EXT] write header failed");
            return false;
        }

        uint8_t *keyTable = new (std::nothrow) uint8_t[keyBytes];
        if (!keyTable)
        {
            LOG_WARN("[CNFONT][EXT] alloc key table failed: %lu bytes", (unsigned long)keyBytes);
            return false;
        }

        for (uint32_t i = 0; i < chineseFontCount; ++i)
        {
            makeUtf8Key(chineseFont[i].utf8, keyTable + i * kUtf8KeySize);
        }

        if (!Esp32PowerLog::ExtFlashRawWrite(kChineseFontBaseAddr + sizeof(header), keyTable, keyBytes))
        {
            delete[] keyTable;
            LOG_WARN("[CNFONT][EXT] write key table failed");
            return false;
        }
        delete[] keyTable;

        uint32_t bitmapAddr = kChineseFontBaseAddr + sizeof(header) + keyBytes;
        for (uint32_t i = 0; i < chineseFontCount; ++i)
        {
            if (!Esp32PowerLog::ExtFlashRawWrite(bitmapAddr, chineseFont[i].bitmap, kBitmapSize))
            {
                LOG_WARN("[CNFONT][EXT] write bitmap failed at index=%lu", (unsigned long)i);
                return false;
            }
            bitmapAddr += kBitmapSize;
        }

        LOG_INFO("[CNFONT][EXT] exported count=%lu bytes=%lu base=0x%08lx",
                 (unsigned long)chineseFontCount, (unsigned long)totalBytes, (unsigned long)kChineseFontBaseAddr);
        return true;
#endif
    }

    static bool ensureExternalChineseFont()
    {
        if (gExternalFontTriedInit)
        {
            return gExternalFontReady;
        }
        gExternalFontTriedInit = true;

        if (!Esp32PowerLog::ExtFlashRawReady())
        {
            LOG_WARN("[CNFONT][EXT] raw flash not ready");
            return false;
        }

        ChineseFontFileHeader header;
        if (!Esp32PowerLog::ExtFlashRawRead(kChineseFontBaseAddr, &header, sizeof(header)))
        {
            LOG_WARN("[CNFONT][EXT] read header failed");
            return false;
        }
        const bool headerBasicInvalid = (header.magic != kChineseFontMagic || header.version != kChineseFontVersion || header.count == 0);
#if CNFONT_EMBED_INTERNAL_TABLE
        const bool headerCountMismatch = (header.count != chineseFontCount);
#else
        const bool headerCountMismatch = false;
#endif

        if (headerBasicInvalid || headerCountMismatch)
        {
#if CNFONT_EMBED_INTERNAL_TABLE
            LOG_INFO("[CNFONT][EXT] header mismatch, try rebuild (magic=0x%08lx ver=%lu count=%lu)",
                     (unsigned long)header.magic, (unsigned long)header.version, (unsigned long)header.count);
            if (!exportChineseFontToExternal())
            {
                LOG_WARN("[CNFONT][EXT] rebuild failed");
                return false;
            }
            if (!Esp32PowerLog::ExtFlashRawRead(kChineseFontBaseAddr, &header, sizeof(header)))
            {
                LOG_WARN("[CNFONT][EXT] read header after rebuild failed");
                return false;
            }
#else
            LOG_WARN("[CNFONT][EXT] invalid external header and embedded table disabled");
            return false;
#endif
        }

        if (header.magic != kChineseFontMagic || header.version != kChineseFontVersion || header.count == 0)
        {
            LOG_WARN("[CNFONT][EXT] invalid header after rebuild");
            return false;
        }

        const uint32_t keyBytes = header.count * kUtf8KeySize;
        if ((sizeof(header) + keyBytes + header.count * kBitmapSize) > kChineseFontMaxBytes)
        {
            LOG_WARN("[CNFONT][EXT] invalid size after header check");
            return false;
        }

        gExternalKeys = new (std::nothrow) uint8_t[keyBytes];
        if (!gExternalKeys)
        {
            LOG_WARN("[CNFONT][EXT] alloc runtime key table failed: %lu bytes", (unsigned long)keyBytes);
            return false;
        }

        if (!Esp32PowerLog::ExtFlashRawRead(kChineseFontBaseAddr + sizeof(header), gExternalKeys, keyBytes))
        {
            delete[] gExternalKeys;
            gExternalKeys = nullptr;
            LOG_WARN("[CNFONT][EXT] read key table failed");
            return false;
        }

        gExternalFontCount = header.count;
        gExternalFontReady = true;
        if (!gExternalFontLoggedReady)
        {
            LOG_INFO("[CNFONT][EXT] ready count=%lu base=0x%08lx",
                     (unsigned long)gExternalFontCount, (unsigned long)kChineseFontBaseAddr);
            gExternalFontLoggedReady = true;
        }
        return true;
    }

    static bool lookupExternalChineseBitmap(const char *utf8, uint8_t outBitmap[kBitmapSize])
    {
        if (!ensureExternalChineseFont())
        {
            return false;
        }

        uint8_t key[kUtf8KeySize];
        makeUtf8Key(utf8, key);

        int32_t foundIndex = -1;
        for (uint32_t i = 0; i < gExternalFontCount; ++i)
        {
            if (memcmp(gExternalKeys + (i * kUtf8KeySize), key, kUtf8KeySize) == 0)
            {
                foundIndex = static_cast<int32_t>(i);
                break;
            }
        }
        if (foundIndex < 0)
        {
            return false;
        }

        const uint32_t keyBytes = gExternalFontCount * kUtf8KeySize;
        const uint32_t bitmapOffset = sizeof(ChineseFontFileHeader) + keyBytes + (static_cast<uint32_t>(foundIndex) * kBitmapSize);
        return Esp32PowerLog::ExtFlashRawRead(kChineseFontBaseAddr + bitmapOffset, outBitmap, kBitmapSize);
    }

    static void drawGlyphBitmap(OLEDDisplay *display, int16_t x, int16_t y, const uint8_t *bitmap)
    {
        for (uint32_t row = 0; row < kGlyphHeight; ++row)
        {
            for (uint32_t col = 0; col < kGlyphWidth; ++col)
            {
                const uint32_t byteIndex = row * kBytesPerRow + (col / 8U);
                const uint32_t bitIndex = 7U - (col % 8U);
                if (bitmap[byteIndex] & (1U << bitIndex))
                {
                    display->setPixel(x + static_cast<int16_t>(col), y + static_cast<int16_t>(row) + kGlyphYOffset);
                }
            }
        }
    }
}

// 返回值：true = 找到并绘制了对应汉字；false = 未找到（调用方可做降级处理）
bool drawChineseChar(OLEDDisplay *display, int16_t x, int16_t y, const char *utf8)
{
    uint8_t externalBitmap[kBitmapSize];
    if (lookupExternalChineseBitmap(utf8, externalBitmap))
    {
        if (!gExternalFontLoggedHit)
        {
            LOG_INFO("[CNFONT][EXT] first external glyph hit");
            gExternalFontLoggedHit = true;
        }
        drawGlyphBitmap(display, x, y, externalBitmap);
        return true;
    }

    if (!gExternalFontLoggedFallback)
    {
        LOG_WARN("[CNFONT][EXT] fallback to internal glyph table");
        gExternalFontLoggedFallback = true;
    }

#if CNFONT_EMBED_INTERNAL_TABLE
    for (unsigned int i = 0; i < chineseFontCount; i++)
    {
        if (strcmp(chineseFont[i].utf8, utf8) == 0)
        {
            drawGlyphBitmap(display, x, y, chineseFont[i].bitmap);
            return true;
        }
    }
#endif

    return false;
}

void drawChineseStringWithLineBreak(OLEDDisplay *display, int16_t x, int16_t y, const char *str)
{
    int offset = 0;
    int16_t currentX = x;
    int16_t currentY = y;
    const int16_t lineHeight = kLineHeight;
    int16_t screenWidth = display->getWidth();

    while (str[offset])
    {
        // 检查换行符
        if (str[offset] == '\n')
        {
            currentX = x;
            currentY += lineHeight;
            offset++;
            continue;
        }

        // 检查是否超出屏幕宽度
        if (currentX >= screenWidth)
        {
            currentX = x;
            currentY += lineHeight;
        }

        unsigned char c = static_cast<unsigned char>(str[offset]);

        // 1 字节 ASCII
        if (c < 0x80)
        {
            char buf[2] = {static_cast<char>(c), 0};

            int16_t w = display->getStringWidth(buf);
            if (currentX + w > screenWidth)
            {
                currentX = x;
                currentY += lineHeight;
            }

            display->drawString(currentX, currentY, buf);
            currentX += w;
            offset += 1;
        }
        // 2 字节 UTF-8（欧洲语言常见：é、ö 等）
        else if ((c & 0xE0) == 0xC0)
        {
            if (!str[offset + 1])
                break; // 防止越界

            char buf[3] = {str[offset], str[offset + 1], 0};

            // 注意：OLEDDisplay 的字体实现通常按“单字节字符”计算宽度，
            // 但这里的 UTF-8 特殊字符占用 2 个字节。
            // 如果直接用 getStringWidth(buf)，会按两个字符的宽度计算，导致我们手动推进的 X 位置比真实视觉宽度大一倍，
            // 看起来就像多出一个字符的空白。
            //
            // 为了在逻辑上把它当作“一个字符”处理，这里用一个普通 ASCII 字符的宽度作为近似值。
            int16_t w = display->getStringWidth("A");
            if (currentX + w > screenWidth)
            {
                currentX = x;
                currentY += lineHeight;
            }

            display->drawString(currentX, currentY, buf);
            currentX += w;
            offset += 2;
        }
        // 3 字节 UTF-8（中文等）
        else if ((c & 0xF0) == 0xE0)
        {
            if (!str[offset + 1] || !str[offset + 2])
                break; // 防止越界

            char buf[4] = {str[offset], str[offset + 1], str[offset + 2], 0};

            // 先尝试按内置汉字点阵绘制
            if (currentX + static_cast<int16_t>(kGlyphWidth) > screenWidth)
            {
                currentX = x;
                currentY += lineHeight;
            }

            bool drawn = drawChineseChar(display, currentX, currentY, buf);
            if (drawn)
            {
                currentX += static_cast<int16_t>(kGlyphWidth);
            }
            else
            {
                // 未找到对应汉字，退回到字体库绘制（比如其它 3 字节字符）
                // int16_t w = display->getStringWidth(buf);
                // if (currentX + w > screenWidth)
                // {
                //     currentX = x;
                //     currentY += lineHeight;
                // }
                // display->drawString(currentX, currentY, buf);
                // currentX += w;
                currentX += static_cast<int16_t>(kGlyphWidth); // 按汉字宽度占位
            }

            offset += 3;
        }
        else
        {
            // 其他情况（4 字节 emoji 等），简单跳过或按单字节回退
            // 这里按单字节占位显示，避免死循环
            char buf[2] = {static_cast<char>(c), 0};
            int16_t w = display->getStringWidth(buf);
            if (currentX + w > screenWidth)
            {
                currentX = x;
                currentY += lineHeight;
            }
            display->drawString(currentX, currentY, buf);
            currentX += w;
            offset += 1;
        }
    }
}
