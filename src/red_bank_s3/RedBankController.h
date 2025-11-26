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

#if HAS_SCREEN
        void scanAdcKeypad();
        KeypadKey getKey1();
        KeypadKey getKey2();

        // 屏幕旋转相关
        uint8_t currentRotation = 1;
        void rotateScreenLeft();
        void rotateScreenRight();
        void applyRotation();
        uint8_t getCurrentRotation() { return currentRotation; };

        // 菜单按钮相关
        void handleLeftButtonPress();
        void handleLeftButtonRelease();
        void handleRightButtonPress();
        void handleRightButtonRelease();
        void handleEnterButtonPress();
        void handleEnterButtonRelease();
        void handleEscButtonPress();
        void handleEscButtonRelease();
        void handleUpButtonPress();
        void handleUpButtonRelease();
        void handleDownButtonPress();
        void handleDownButtonRelease();
        bool isMenuActive();
        void setMenuActive(bool active);
#endif // HAS_SCREEN

    private:
        // 消息列表容量
        static const int MESH_PACKET_LIST_CAPCITY = 10;
        // 消息列表
        std::vector<meshtastic_MeshPacket> channelPackets[8];
        // 当前消息索引
        int m_currentMeshPacketIndex;
        uint8_t direction;

        // 移除或添加条件编译保护到这些私有函数
#if HAS_SCREEN
        void _previousMeshPacket();
        void _nextMeshPacket();
        KeypadKey mapKeyByRotation(KeypadKey physicalKey); // 只保留一个声明

        // 按键状态管理
        bool leftButtonPressed = false;
        bool rightButtonPressed = false;
        bool enterButtonPressed = false;
        bool escButtonPressed = false;
        bool upButtonPressed = false;
        bool downButtonPressed = false;
        uint32_t leftButtonPressTime = 0;
        uint32_t rightButtonPressTime = 0;
        uint32_t enterButtonPressTime = 0;
        uint32_t escButtonPressTime = 0;
        uint32_t upButtonPressTime = 0;
        uint32_t downButtonPressTime = 0;
        static const uint32_t LONG_PRESS_THRESHOLD = 500;
        bool menuActive = false;
#endif // HAS_SCREEN
    };
}