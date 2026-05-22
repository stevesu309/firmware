#include "ChatHistoryStore.h"
#include "DebugConfiguration.h"
#include "meshUtils.h"
#include <cstring>

#if defined(RED_BANK_S3) || defined(REDCOAST_SOLO_915)

ChatHistoryStore *chatHistoryStore = nullptr;

ChatHistoryStore::ChatHistoryStore()
{
    // 启动时先恢复磁盘中的短历史，后续 UI 直接从内存列表读取。
    restoreChannelPackets();
    restoreDirectMessages();
}

bool ChatHistoryStore::isMeshPacketListEmpty(uint8_t channel) const
{
    if (channel >= kMaxChannels)
    {
        LOG_INFO("ChatHistoryStore: incorrect channel num = 0x%x!", channel);
        return false;
    }

    return channelPackets[channel].empty();
}

void ChatHistoryStore::pushChannelPacket(uint8_t channelIndex, const meshtastic_MeshPacket &mp)
{
    if (channelIndex >= kMaxChannels)
        return;

    // 固定容量环形语义：满了就丢弃最旧的一条，保留最近消息。
    if (channelPackets[channelIndex].size() >= kChannelMessageCapacity)
        channelPackets[channelIndex].erase(channelPackets[channelIndex].begin());

    channelPackets[channelIndex].push_back(mp);
}

void ChatHistoryStore::saveMeshPacket(const meshtastic_MeshPacket &mp)
{
    if (mp.channel >= kMaxChannels)
    {
        LOG_INFO("ChatHistoryStore: incorrect mp.channel = 0x%x!", mp.channel);
        return;
    }

    // direct message 使用私信列表保存；广播消息按频道保存。
    if (mp.to != NODENUM_BROADCAST)
    {
        pushDirectMessage(mp);
        return;
    }

    pushChannelPacket(mp.channel, mp);

    // 频道历史文件按连续下标保存，内存列表变化后重写当前频道短历史。
    for (uint8_t i = 0; i < channelPackets[mp.channel].size(); ++i)
        nodeDB->saveChannelPacketToDisk(mp.channel, i, channelPackets[mp.channel].at(i));
}

void ChatHistoryStore::restoreChannelPackets()
{
    meshtastic_MeshPacket mp;
    for (uint8_t i = 0; i < kMaxChannels; ++i)
    {
        if (channelFile.channels[i].role != meshtastic_Channel_Role_PRIMARY &&
            channelFile.channels[i].role != meshtastic_Channel_Role_SECONDARY)
            continue;

        // 只恢复有效频道的历史，避免显示未启用频道的旧文件。
        LOG_INFO("restoring channel %d packets...", i);
        channelPackets[i].clear();
        LOG_INFO("channel %d packets size = %d", i, channelPackets[i].size());
        for (uint8_t j = 0; j < kChannelMessageCapacity; ++j)
        {
            if (nodeDB->restoreMeshPacket(i, j, mp))
                pushChannelPacket(i, mp);
        }
    }
}

void ChatHistoryStore::restoreDirectMessages()
{
    // 私信文件名包含对端节点号，因此需要先遍历已知节点再尝试恢复。
    for (size_t i = 0; i < nodeDB->getNumMeshNodes(); ++i)
    {
        meshtastic_NodeInfoLite *node = nodeDB->getMeshNodeByIndex(i);
        if (!node)
            continue;

        NodeNum nodeNum = node->num;
        meshtastic_MeshPacket mp;
        auto &nodeMessages = directMessagesByNode[nodeNum];
        nodeMessages.clear();

        for (uint8_t j = 0; j < kDirectMessageCapacity; ++j)
        {
            if (nodeDB->restoreDirectMessagePacket(nodeNum, j, mp))
                nodeMessages.push_back(mp);
        }

        if (!nodeMessages.empty())
            LOG_INFO("restored %zu direct messages for node 0x%08x", nodeMessages.size(), nodeNum);
    }

    if (currentDirectMessageNode != 0)
        return;

    // 默认选中第一个有历史的节点，进入 DM 页面时能直接显示最近会话。
    bool foundNodeWithMessages = false;
    for (const auto &pair : directMessagesByNode)
    {
        if (!pair.second.empty())
        {
            currentDirectMessageNode = pair.first;
            currentDirectMessageIndex = pair.second.size() - 1;
            LOG_INFO("Auto-selected first node with messages: 0x%08x", currentDirectMessageNode);
            foundNodeWithMessages = true;
            break;
        }
    }

    if (!foundNodeWithMessages && nodeDB->getNumMeshNodes() > 0)
    {
        meshtastic_NodeInfoLite *firstNode = nodeDB->getMeshNodeByIndex(0);
        if (firstNode)
        {
            currentDirectMessageNode = firstNode->num;
            currentDirectMessageIndex = 0;
            LOG_INFO("No nodes with messages, auto-selected first node: 0x%08x", currentDirectMessageNode);
        }
    }
}

meshtastic_MeshPacket ChatHistoryStore::getRecentMeshPacket(uint8_t channel, uint8_t recentIndex) const
{
    if ((channel >= kMaxChannels) || (recentIndex >= channelPackets[channel].size()))
    {
        LOG_INFO("ChatHistoryStore: invalid argument, channel num = 0x%x, recent_index = %d!", channel, recentIndex);
        meshtastic_MeshPacket emptyPacket;
        memset(&emptyPacket, 0, sizeof(emptyPacket));
        return emptyPacket;
    }

    return channelPackets[channel].at(recentIndex);
}

int ChatHistoryStore::getMeshPacketListSize(uint8_t channel) const
{
    if (channel >= kMaxChannels)
        return 0;
    return channelPackets[channel].size();
}

void ChatHistoryStore::pushDirectMessage(const meshtastic_MeshPacket &mp)
{
    NodeNum otherNode;
    NodeNum myNodeNum = nodeDB->getNodeNum();
    LOG_DEBUG("myNodeNum: 0x%08x", myNodeNum);
    LOG_DEBUG("getFrom:0x%08x", getFrom(&mp));

    // 私信列表按“对端节点”聚合：我发出的消息取 mp.to，收到的消息取 from。
    if (getFrom(&mp) == myNodeNum)
        otherNode = mp.to;
    else
        otherNode = getFrom(&mp);

    if (otherNode == 0 || otherNode == NODENUM_BROADCAST)
        return;

    auto &nodeMessages = directMessagesByNode[otherNode];
    // 每个对端只保留最近 kDirectMessageCapacity 条，避免长期占用过多 RAM/文件。
    if (nodeMessages.size() >= kDirectMessageCapacity)
        nodeMessages.erase(nodeMessages.begin());

    nodeMessages.push_back(mp);

    // 私信同样按连续下标保存，方便 UI 用浏览索引读取。
    for (uint8_t i = 0; i < nodeMessages.size(); ++i)
        nodeDB->saveDirectMessagePacketToDisk(otherNode, i, nodeMessages.at(i));

    if (currentDirectMessageNode == 0)
    {
        currentDirectMessageNode = otherNode;
        currentDirectMessageIndex = nodeMessages.size() - 1;
    }
    else if (currentDirectMessageNode == otherNode)
    {
        currentDirectMessageIndex = nodeMessages.size() - 1;
    }
}

bool ChatHistoryStore::isDirectMessageListEmpty() const
{
    return directMessagesByNode.empty();
}

bool ChatHistoryStore::isDirectMessageListEmptyForNode(NodeNum nodeNum) const
{
    auto it = directMessagesByNode.find(nodeNum);
    return (it == directMessagesByNode.end() || it->second.empty());
}

meshtastic_MeshPacket ChatHistoryStore::getRecentDirectMessage(uint8_t recentIndex) const
{
    if (currentDirectMessageNode == 0)
    {
        meshtastic_MeshPacket emptyPacket;
        memset(&emptyPacket, 0, sizeof(emptyPacket));
        return emptyPacket;
    }

    auto it = directMessagesByNode.find(currentDirectMessageNode);
    if (it == directMessagesByNode.end() || recentIndex >= it->second.size())
    {
        meshtastic_MeshPacket emptyPacket;
        memset(&emptyPacket, 0, sizeof(emptyPacket));
        return emptyPacket;
    }

    return it->second.at(recentIndex);
}

int ChatHistoryStore::getDirectMessageListSize() const
{
    if (currentDirectMessageNode == 0)
        return 0;

    auto it = directMessagesByNode.find(currentDirectMessageNode);
    if (it == directMessagesByNode.end())
        return 0;

    return it->second.size();
}

int ChatHistoryStore::getDirectMessageListSizeForNode(NodeNum nodeNum) const
{
    auto it = directMessagesByNode.find(nodeNum);
    if (it == directMessagesByNode.end())
        return 0;
    return it->second.size();
}

void ChatHistoryStore::setCurrentDirectMessageNode(NodeNum nodeNum)
{
    currentDirectMessageNode = nodeNum;
    auto it = directMessagesByNode.find(nodeNum);
    // 切换会话时默认定位到最新一条，UP/DOWN 再从这里开始浏览。
    if (it != directMessagesByNode.end() && !it->second.empty())
        currentDirectMessageIndex = it->second.size() - 1;
    else
        currentDirectMessageIndex = 0;
}

std::vector<NodeNum> ChatHistoryStore::getDirectMessageNodeList() const
{
    std::vector<NodeNum> nodeList;
    for (const auto &pair : directMessagesByNode)
    {
        if (!pair.second.empty())
            nodeList.push_back(pair.first);
    }
    return nodeList;
}

int ChatHistoryStore::getDirectMessageNodeCount() const
{
    int count = 0;
    for (const auto &pair : directMessagesByNode)
    {
        if (!pair.second.empty())
            count++;
    }
    return count;
}

void ChatHistoryStore::deleteCurrentDirectMessage()
{
    if (currentDirectMessageNode == 0)
        return;

    auto it = directMessagesByNode.find(currentDirectMessageNode);
    if (it == directMessagesByNode.end() || it->second.empty())
        return;

    auto &nodeMessages = it->second;
    if (currentDirectMessageIndex >= nodeMessages.size())
        return;

    nodeDB->deleteDirectMessagePacketFromDisk(currentDirectMessageNode, currentDirectMessageIndex);
    nodeMessages.erase(nodeMessages.begin() + currentDirectMessageIndex);

    // 删除中间消息后重新编号落盘，避免历史文件下标出现空洞。
    for (uint8_t i = 0; i < nodeMessages.size(); ++i)
        nodeDB->saveDirectMessagePacketToDisk(currentDirectMessageNode, i, nodeMessages.at(i));

    // 清理尾部旧文件，防止被 restoreDirectMessages() 当作残留历史读回来。
    for (uint8_t i = nodeMessages.size(); i < kDirectMessageCapacity; ++i)
        nodeDB->deleteDirectMessagePacketFromDisk(currentDirectMessageNode, i);

    if (currentDirectMessageIndex >= nodeMessages.size() && !nodeMessages.empty())
        currentDirectMessageIndex = nodeMessages.size() - 1;
    else if (nodeMessages.empty())
        currentDirectMessageIndex = 0;

    LOG_INFO("Deleted direct message at index %d for node 0x%08x", currentDirectMessageIndex, currentDirectMessageNode);
}

void ChatHistoryStore::deleteAllDirectMessagesForNode(NodeNum nodeNum)
{
    if (nodeNum == 0)
        return;

    auto it = directMessagesByNode.find(nodeNum);
    if (it == directMessagesByNode.end())
        return;

    nodeDB->deleteAllDirectMessagePacketsForNode(nodeNum);
    directMessagesByNode.erase(it);

    if (currentDirectMessageNode == nodeNum)
    {
        currentDirectMessageNode = 0;
        currentDirectMessageIndex = 0;
    }

    LOG_INFO("Deleted all direct messages for node 0x%08x", nodeNum);
}

void ChatHistoryStore::deleteCurrentChannelMessage(uint8_t channelIndex, uint16_t packetIndex)
{
    if (channelIndex >= kMaxChannels)
        return;

    auto &channelMessages = channelPackets[channelIndex];
    if (packetIndex >= channelMessages.size())
    {
        LOG_WARN("deleteCurrentChannelMessage: packet_index %d >= size %zu", packetIndex, channelMessages.size());
        return;
    }

    LOG_INFO("Deleting channel message: channel=%d, index=%d, total=%zu", channelIndex, packetIndex, channelMessages.size());

    // 先清空该频道所有历史文件，再按当前内存列表重新写入。
    for (uint8_t i = 0; i < kChannelMessageCapacity; ++i)
        nodeDB->deleteChannelPacketFromDisk(channelIndex, i);

    channelMessages.erase(channelMessages.begin() + packetIndex);
    LOG_INFO("After erase: channel %d has %zu messages", channelIndex, channelMessages.size());

    // 重新编号保存，保持 packetIndex 与 UI 浏览索引一致。
    for (uint8_t i = 0; i < channelMessages.size(); ++i)
    {
        if (!nodeDB->saveChannelPacketToDisk(channelIndex, i, channelMessages.at(i)))
            LOG_WARN("Failed to save channel %d packet %d to disk", channelIndex, i);
    }

    LOG_INFO("Deleted channel message at index %d for channel %d, remaining %zu messages", packetIndex, channelIndex,
             channelMessages.size());
}

void ChatHistoryStore::deleteAllChannelMessagesForChannel(uint8_t channelIndex)
{
    if (channelIndex >= kMaxChannels)
        return;

    nodeDB->deleteAllChannelPacketsForChannel(channelIndex);
    channelPackets[channelIndex].clear();
    LOG_INFO("Deleted all channel messages for channel %d", channelIndex);
}

#endif
