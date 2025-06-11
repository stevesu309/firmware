#pragma once

#include <vector>
#include <string>
#include <map>
#include "SafeFile.h"

constexpr uint8_t MAX_MESSAGES_PER_CHANNEL = 10;
constexpr uint32_t MAX_MESSAGE_SIZE = 200;

struct Message
{
  uint32_t timestamp;
  uint32_t sender;
  std::string text;
};

class ChannelMessageStore
{
private:
  std::map<int, std::vector<Message>> channelMessages; // 每个频道的消息队列
  std::string basePath = "/ChannelMessages";           // 基础路径

public:
  void saveToFlash();
  void loadFromFlash();
  const std::vector<Message> &getMessagesForChannel(int channelIndex);
  void addMessageToChannel(int channelIndex, const Message &message);
};