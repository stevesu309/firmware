#include "ChannelMessageStore.h"
#include "SafeFile.h"
#include "FSCommon.h"
// #include "Log.h"

void ChannelMessageStore::saveToFlash()
{
  FSCom.mkdir(basePath.c_str());

  for (const auto &entry : channelMessages)
  {
    int channelIndex = entry.first;
    const auto &messages = entry.second;

    // 构造文件路径
    std::string filename = basePath + "/channel_" + std::to_string(channelIndex) + ".msgs";

    // 打开文件
    auto f = SafeFile(filename.c_str(), false);

    LOG_INFO("Saving messages for channel %d", channelIndex);

    // 写入消息数量
    f.write(messages.size());

    // 写入每条消息
    for (const auto &message : messages)
    {
      f.write((uint8_t *)&message.timestamp, sizeof(message.timestamp));
      f.write((uint8_t *)&message.sender, sizeof(message.sender));
      f.write((uint8_t *)message.text.c_str(), std::min(MAX_MESSAGE_SIZE, message.text.size()));
      f.write('\0');
    }

    f.close();
  }
}

void ChannelMessageStore::loadFromFlash()
{
  FSCom.mkdir(basePath.c_str());

  for (int channelIndex = 0; channelIndex < MAX_NUM_CHANNELS; ++channelIndex)
  {
    // 构造文件路径
    std::string filename = basePath + "/channel_" + std::to_string(channelIndex) + ".msgs";

    if (!FSCom.exists(filename.c_str()))
    {
      continue; // 如果文件不存在，跳过
    }

    auto f = FSCom.open(filename.c_str(), FILE_O_READ);
    if (!f)
    {
      LOG_ERROR("Failed to open file %s", filename.c_str());
      continue;
    }

    size_t messageCount;
    f.read((uint8_t *)&messageCount, sizeof(messageCount));

    std::vector<Message> messages;
    for (size_t i = 0; i < messageCount; ++i)
    {
      Message message;
      f.read((uint8_t *)&message.timestamp, sizeof(message.timestamp));
      f.read((uint8_t *)&message.sender, sizeof(message.sender));

      char text[MAX_MESSAGE_SIZE];
      size_t textLength = f.read((uint8_t *)text, MAX_MESSAGE_SIZE);
      text[textLength] = '\0'; // 确保字符串以空字符结尾
      message.text = std::string(text);

      messages.push_back(message);
    }

    channelMessages[channelIndex] = messages;
    f.close();
  }
}

void ChannelMessageStore::addMessageToChannel(int channelIndex, const Message &message)
{
  auto &messages = channelMessages[channelIndex];

  // 如果消息队列已满，删除最早的消息
  if (messages.size() >= MAX_MESSAGES_PER_CHANNEL)
  {
    messages.erase(messages.begin());
  }

  messages.push_back(message);
}

const std::vector<Message> &ChannelMessageStore::getMessagesForChannel(int channelIndex)
{
  static std::vector<Message> emptyList;
  if (channelMessages.find(channelIndex) != channelMessages.end())
  {
    return channelMessages[channelIndex];
  }
  return emptyList;
}