#pragma once

#include <vector>
#include "mesh/generated/meshtastic/mesh.pb.h"
#include "NodeDB.h"

namespace RedBankS3
{
    enum class KeypadKey
    {
        NONE,
        ENTER,
        ESC,
        UP,
        LEFT,
        RIGHT,
        DOWN
    };
    class RedBankController
    {

    public:
        RedBankController();
        ~RedBankController();
        void setup();
        void loop();

        bool isMeshPacketListEmpty(uint8_t channel);
        void saveMeshPacket(const meshtastic_MeshPacket &mp);
        meshtastic_MeshPacket getRecentMeshPacket(uint8_t channel, uint8_t recent_index);
        int _getMeshPacketListSize(uint8_t channel);
        uint8_t getDirection(void);

        void push_packet(uint8_t channel_index, const meshtastic_MeshPacket &mp);
        void restoreChannelPackets(void);
        void scanAdcKeypad();
        KeypadKey getKey1();
        KeypadKey getKey2();

        // 屏幕旋转相关
        uint8_t currentRotation = 0;                              // 当前旋转角度：0=竖屏, 1=左横屏, 3=右横屏
        void rotateScreenLeft();                                  // 向左旋转（ENTER按键）
        void rotateScreenRight();                                 // 向右旋转（ESC按键）
        void applyRotation();                                     // 使用旋转
        uint8_t getCurrentRotation() { return currentRotation; }; // 获取当前旋转角度
    private:
        // 消息列表容量
        static const int MESH_PACKET_LIST_CAPCITY = 10;
        // 消息列表
        // std::vector<meshtastic_MeshPacket> *m_meshPacketList;
        std::vector<meshtastic_MeshPacket> channelPackets[8];

        // 当前消息索引
        int m_currentMeshPacketIndex;

        uint8_t direction;

        void _previousMeshPacket();
        void _nextMeshPacket();
        void _handleShuttingDownButtonPress();
    };
}