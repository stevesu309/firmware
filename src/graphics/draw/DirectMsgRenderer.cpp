#include "DirectMsgRenderer.h"

#include "NodeDB.h"
#include "graphics/Screen.h"
#include "main.h"
#include "graphics/TimeFormatters.h"
#include "graphics/draw/UIRenderer.h"
#include "graphics/ScreenFonts.h"
#include "graphics/fonts/OLEDDisplayFontsAR.h"
#include "OLEDDisplay.h"
#include "graphics/fonts/ChineseFont.h"
#include "PowerStatus.h"
#include "graphics/SharedUIDisplay.h"
#include "meshUtils.h"
#ifdef RED_BANK_S3
#include "red_bank_s3/RedBankController.h"
#endif

namespace graphics
{
  namespace DirectMsgRenderer
  {

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
        // 竖屏模式使用小字体
        selection.font = FONT_SMALL;
        selection.height = _fontHeight(FONT_SMALL);
      }
      else
      {
        selection.font = FONT_MEDIUM;
        selection.height = FONT_HEIGHT_MEDIUM;
        // selection.font = FONT_LARGE;
        // selection.height = _fontHeight(FONT_LARGE);
      }
      return selection;
    }

    void drawDirectMessageFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
    {
      // the max length of this buffer is much longer than we can possibly print
      static char tempBuf[237];
      int width = display->getWidth();
      int height = display->getHeight();
#ifdef RED_BANK_S3

#if HAS_SCREEN
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
#else
      FontSelection fontSel = selectFontForRotation(3);
#endif
      const uint8_t *selectedFont = fontSel.font;
      int selectedFontHeight = fontSel.height;

      display->setFont(selectedFont);

      NodeNum currentNode = redBankController->getCurrentDirectMessageNode();

      // 检查当前节点是否有私信
      if (redBankController->isDirectMessageListEmptyForNode(currentNode))
      {
        char title[20];
        if (currentRotation == 0)
          snprintf(title, sizeof(title), "DM");
        else
          snprintf(title, sizeof(title), "Direct Message");
        drawCommonHeader(display, x, y, title, false);
        const char *messageString = "No messages for this node";
        display->setTextAlignment(TEXT_ALIGN_LEFT);
        int center_text = (width / 2) - (display->getStringWidth(messageString) / 2);
        display->drawString(center_text, y + selectedFontHeight * 2, messageString);
        return;
      }

      // 获取当前节点的消息列表大小
      uint16_t directMsgListSize = redBankController->_getDirectMessageListSizeForNode(currentNode);
      uint8_t msgIndex = redBankController->getCurrentDirectMessageIndex();

      // 确保索引有效
      if (msgIndex >= directMsgListSize)
      {
        msgIndex = directMsgListSize - 1;
        redBankController->setCurrentDirectMessageIndex(msgIndex);
      }

      const meshtastic_MeshPacket &mp = redBankController->getRecentDirectMessage(msgIndex);

      // 获取节点信息用于显示标题
      meshtastic_NodeInfoLite *node = nodeDB->getMeshNode(currentNode);
      char nodeName[30];
      if (node && node->has_user && strlen(node->user.short_name) > 0)
      {
        snprintf(nodeName, sizeof(nodeName), "%s", node->user.short_name);
      }
      else
      {
        snprintf(nodeName, sizeof(nodeName), "%08x", currentNode);
      }

      char title[50];
      int nodeCount = redBankController->getDirectMessageNodeCount();
      snprintf(title, sizeof(title), "DM: %s (%d/%d)", nodeName, msgIndex + 1, directMsgListSize);

      drawCommonHeader(display, x, y, title, false);

      const int headerHeight = FONT_HEIGHT_SMALL + 1;

      // 获取发送者或接收者信息
      meshtastic_NodeInfoLite *fromNode = nodeDB->getMeshNode(getFrom(&mp));
      meshtastic_NodeInfoLite *toNode = nodeDB->getMeshNode(mp.to);

      char displayName[30];
      const char *displayNodeName = "???";

      // 判断是发送还是接收的私信
      bool isFromMe = (getFrom(&mp) == nodeDB->getNodeNum());

      if (isFromMe)
      {
        // 发送给对方的私信
        if (toNode && toNode->has_user)
        {
          displayNodeName = (strlen(toNode->user.short_name) > 0) ? toNode->user.short_name : "???";
          snprintf(displayName, sizeof(displayName), "To: %s", displayNodeName);
        }
        else
        {
          snprintf(displayName, sizeof(displayName), "To: %08x", mp.to);
        }
      }
      else
      {
        // 接收到的私信
        if (fromNode && fromNode->has_user)
        {
          displayNodeName = (strlen(fromNode->user.short_name) > 0) ? fromNode->user.short_name : "???";
          snprintf(displayName, sizeof(displayName), "From: %s", displayNodeName);
        }
        else
        {
          snprintf(displayName, sizeof(displayName), "From: %08x", getFrom(&mp));
        }
      }

      display->setTextAlignment(TEXT_ALIGN_LEFT);
      display->drawString(x + 5, y + selectedFontHeight, displayName);

      display->setTextAlignment(TEXT_ALIGN_RIGHT);
      display->drawLine(x, y + selectedFontHeight * 2, x + width, y + selectedFontHeight * 2);
      y += selectedFontHeight * 2;

      display->setTextAlignment(TEXT_ALIGN_LEFT);

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
      for (uint8_t xOff = 0; xOff <= (config.display.heading_bold ? 1 : 0); xOff++)
      {
        // Show a timestamp if received today, but longer than 15 minutes ago
        if (useTimestamp && minutes >= 15 && daysAgo == 0)
        {
          display->drawStringf(xOff + x, 0 + y, tempBuf, "At %02hu:%02hu %s",
                               timestampHours, timestampMinutes,
                               isFromMe ? "(sent)" : "(received)");
        }
        // Timestamp yesterday (if display is wide enough)
        else if (useTimestamp && daysAgo == 1 && display->width() >= 200)
        {
          display->drawStringf(xOff + x, 0 + y, tempBuf, "Yesterday %02hu:%02hu %s",
                               timestampHours, timestampMinutes,
                               isFromMe ? "(sent)" : "(received)");
        }
        // Otherwise, show a time delta
        else
        {
          display->drawStringf(xOff + x, 0 + y, tempBuf, "%s ago %s",
                               UIRenderer::drawTimeDelta(days, hours, minutes, seconds).c_str(),
                               isFromMe ? "(sent)" : "(received)");
        }
      }

      snprintf(tempBuf, sizeof(tempBuf), "%s", mp.decoded.payload.bytes);
      drawChineseStringWithLineBreak(display, 0 + x, 0 + y + selectedFontHeight, tempBuf);

#endif
    }
  }

}
