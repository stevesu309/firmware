#include "ChannelMessageRenderer.h"
#include "NodeDB.h"
#include "graphics/Screen.h"
#include "main.h"
#include "graphics/TimeFormatters.h"
#include "graphics/draw/UIRenderer.h"
#include "graphics/ScreenFonts.h"
#include "graphics/fonts/OLEDDisplayFontsAR.h"
#define MAX_VALID_CHANNELS 8
int validChannelIndices[MAX_VALID_CHANNELS];
int validChannelCount = 0;
size_t channelIndex = 0;
uint16_t channelFrameBeginIndex = 0;
uint16_t channelPacketBrowseIndex = 0;
namespace graphics
{

  namespace ChannelMessageRenderer
  {

    // 辅助函数：根据屏幕旋转角度选择合适的字体
    struct FontSelection
    {
      const uint8_t *font;
      int height;
    };

    FontSelection selectFontForRotation(uint8_t rotation)
    {
      FontSelection selection;
      if (rotation == 0)
      {
        // 竖屏模式使用最小字体 (Arimo_Regular_14, 高度14像素)
        selection.font = FONT_SMALL;
        selection.height = _fontHeight(FONT_SMALL);
      }
      else
      {
        // 横屏模式使用标准小字体 (Arimo_Regular_16, 高度16像素)
        selection.font = FONT_MEDIUM;
        selection.height = FONT_HEIGHT_MEDIUM;
      }
      return selection;
    }

    bool getFrameIndexByChannelIndex(uint8_t channelIndex, uint8_t *frameIndex)
    {
      uint8_t i = 0;
      uint8_t channelCnt = sizeof(validChannelIndices) / sizeof(validChannelIndices[0]);

      for (i = 0; i < channelCnt; ++i)
      {
        if (validChannelIndices[i] == channelIndex)
        {
          break;
        }
      }

      if (i >= channelCnt)
      {
        return (false);
      }

      *frameIndex = i;
      return (true);
    }

    bool isBrowsingChannelPacketFrame(uint8_t currentFrame)
    {
      if (validChannelCount == 0)
      {
        return (false);
      }

      if ((currentFrame >= channelFrameBeginIndex) &&
          (currentFrame < (channelFrameBeginIndex + validChannelCount)))
      {
        return (true);
      }

      return (false);
    }

    uint8_t getBrowsingChannelIndex(uint8_t currentFrame)
    {
      return (validChannelIndices[currentFrame - channelFrameBeginIndex]);
    }

    /// Draw the last text message we received
    void drawChannelTextMessageFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
    {
      // the max length of this buffer is much longer than we can possibly print
      static char tempBuf[237];
      int width = display->getWidth();
      int height = display->getHeight();

      if (redBankController->getCurrentRotation() == 0 && width > height)
      {
        std::swap(width, height); // 调换宽高
      }
      else if (redBankController->getCurrentRotation() != 0 && width < height)
      {
        std::swap(width, height);
      }

      display->setTextAlignment(TEXT_ALIGN_LEFT);

      // 根据屏幕旋转角度选择合适的字体大小
      uint8_t currentRotation = redBankController->getCurrentRotation();
      FontSelection fontSel = selectFontForRotation(currentRotation);
      const uint8_t *selectedFont = fontSel.font;
      int selectedFontHeight = fontSel.height;

      display->setFont(selectedFont);
      if (config.display.displaymode == meshtastic_Config_DisplayConfig_DisplayMode_INVERTED)
      {
        display->fillRect(0 + x, 0 + y, x + display->getWidth(), y + selectedFontHeight);
        display->setColor(BLACK);
      }

      // added by QCB begin
      if (validChannelCount == 0)
      {
        LOG_INFO("validChannelCount1 = %d\n", validChannelCount);
        return;
      }

      int localActualChannelIndex = getBrowsingChannelIndex(state->currentFrame);
      if (localActualChannelIndex >= 8)
      {
        LOG_INFO("Error localActualChannelIndex = %d >=  validChannelCount = %d\n",
                 localActualChannelIndex, validChannelCount);
        return;
      }
      char tetle[23];
      if (currentRotation == 0)
      {
        // 竖屏模式使用更短的标题
        snprintf(tetle, sizeof(tetle), "Ch Msg: %d/%d", localActualChannelIndex + 1, validChannelCount);
      }
      else
      {
        // 横屏模式使用完整标题
        snprintf(tetle, sizeof(tetle), "Channel Message : %d/%d", localActualChannelIndex + 1, validChannelCount);
      }

      display->setTextAlignment(TEXT_ALIGN_CENTER);
      display->setFont(selectedFont); // 确保标题也使用选中的字体
      display->drawString(x + width / 2, y, tetle);

      // display channel name
      const char *name = channelFile.channels[localActualChannelIndex].settings.name;
      char displayName[25];

      if (strlen(name) > 0)
      {
        if (currentRotation == 0)
        {
          // 竖屏模式使用更短的前缀
          if (channelFile.channels[localActualChannelIndex].role == meshtastic_Channel_Role_PRIMARY)
            snprintf(displayName, sizeof(displayName), "P: %s", name);
          else
            snprintf(displayName, sizeof(displayName), "S: %s", name);
        }
        else
        {
          // 横屏模式使用完整前缀
          if (channelFile.channels[localActualChannelIndex].role == meshtastic_Channel_Role_PRIMARY)
            snprintf(displayName, sizeof(displayName), "Pri Ch: %s", name);
          else
            snprintf(displayName, sizeof(displayName), "Sec Ch: %s", name);
        }
      }
      else
      {
        char preset = config.lora.modem_preset;
        switch (preset)
        {
        case meshtastic_Config_LoRaConfig_ModemPreset_LONG_FAST:
          name = "Long_fast";
          break;
        case meshtastic_Config_LoRaConfig_ModemPreset_LONG_SLOW:
          name = "Long_slow";
          break;
        case meshtastic_Config_LoRaConfig_ModemPreset_VERY_LONG_SLOW:
          name = "Very_long_slow";
          break;
        case meshtastic_Config_LoRaConfig_ModemPreset_MEDIUM_SLOW:
          name = "Medium_slow";
          break;
        case meshtastic_Config_LoRaConfig_ModemPreset_MEDIUM_FAST:
          name = "Medium_fast";
          break;
        case meshtastic_Config_LoRaConfig_ModemPreset_SHORT_SLOW:
          name = "Short_slow";
          break;
        case meshtastic_Config_LoRaConfig_ModemPreset_SHORT_FAST:
          name = "Short_fast";
          break;
        default:
          name = "Long_fast";
          break;
        }
        if (currentRotation == 0)
        {
          // 竖屏模式使用更短的前缀
          if (channelFile.channels[localActualChannelIndex].role == meshtastic_Channel_Role_PRIMARY)
            snprintf(displayName, sizeof(displayName), "P: %s", name);
          else
            snprintf(displayName, sizeof(displayName), "S: %s", name);
        }
        else
        {
          // 横屏模式使用完整前缀
          if (channelFile.channels[localActualChannelIndex].role == meshtastic_Channel_Role_PRIMARY)
            snprintf(displayName, sizeof(displayName), "Pri Ch: %s", name);
          else
            snprintf(displayName, sizeof(displayName), "Sec Ch: %s", name);
        }
      }
      // display->fillRect(x, y, 200, FONT_HEIGHT_SMALL * 2);
      // EINK_ADD_FRAMEFLAG(display, BACKGROUND); // Take the opportunity for a full-refresh
      display->setTextAlignment(TEXT_ALIGN_LEFT);
      display->setFont(selectedFont); // 确保channel名称也使用选中的字体

      display->drawString(x + 5, y + selectedFontHeight, displayName);
      display->setTextAlignment(TEXT_ALIGN_RIGHT);

      display->drawLine(x, y + selectedFontHeight * 2, x + width, y + selectedFontHeight * 2);
      y += selectedFontHeight * 2;

      display->setTextAlignment(TEXT_ALIGN_LEFT);

      // display packet info
      // uint8_t direction = 0;
      uint16_t packetListSize = redBankController->_getMeshPacketListSize(localActualChannelIndex);

      if (packetListSize == 0)
      {
        LOG_INFO("packetListSize = %d\n", packetListSize);
        return;
      }

      if (channelPacketBrowseIndex >= packetListSize)
      {
        channelPacketBrowseIndex = packetListSize - 1;
      }

      const meshtastic_MeshPacket &mp = redBankController->getRecentMeshPacket(localActualChannelIndex, channelPacketBrowseIndex);

      // added by QCB end
      meshtastic_NodeInfoLite *node = nodeDB->getMeshNode(getFrom(&mp));

      // For time delta
      uint32_t seconds = sinceReceived(&mp);
      uint32_t minutes = seconds / 60;
      uint32_t hours = minutes / 60;
      uint32_t days = hours / 24;

      // For timestamp
      uint8_t timestampHours, timestampMinutes;
      int32_t daysAgo;
      bool useTimestamp = deltaToTimestamp(seconds, &timestampHours, &timestampMinutes, &daysAgo);

      // If bold, draw twice, shifting right by one pixel
      display->setFont(selectedFont); // 确保时间戳也使用选中的字体
      for (uint8_t xOff = 0; xOff <= (config.display.heading_bold ? 1 : 0); xOff++)
      {
        // Show a timestamp if received today, but longer than 15 minutes ago
        if (useTimestamp && minutes >= 15 && daysAgo == 0)
        {
          display->drawStringf(xOff + x, 0 + y, tempBuf, "At %02hu:%02hu from %s", timestampHours, timestampMinutes,
                               (node && node->has_user) ? node->user.short_name : "???");
        }
        // Timestamp yesterday (if display is wide enough)
        else if (useTimestamp && daysAgo == 1 && display->width() >= 200)
        {
          display->drawStringf(xOff + x, 0 + y, tempBuf, "Yesterday %02hu:%02hu from %s", timestampHours, timestampMinutes,
                               (node && node->has_user) ? node->user.short_name : "???");
        }
        // Otherwise, show a time delta
        else
        {
          display->drawStringf(xOff + x, 0 + y, tempBuf, "%s ago from %s",
                               UIRenderer::drawTimeDelta(days, hours, minutes, seconds).c_str(),
                               (node && node->has_user) ? node->user.short_name : "???");
        }
      }

      display->setColor(WHITE);
      display->setFont(selectedFont); // 确保消息内容也使用选中的字体

      snprintf(tempBuf, sizeof(tempBuf), "%s", mp.decoded.payload.bytes);
      display->drawStringMaxWidth(0 + x, 0 + y + selectedFontHeight, x + display->getWidth(), tempBuf);
    }
  }
}
