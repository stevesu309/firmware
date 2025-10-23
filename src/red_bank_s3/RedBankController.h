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

        // 菜单按钮相关
        void handleLeftButtonPress();    // 处理 LEFT 按键按下
        void handleLeftButtonRelease();  // 处理 LEFT 按键释放
        void handleRightButtonPress();   // 处理 RIGHT 按键按下
        void handleRightButtonRelease(); // 处理 RIGHT 按键释放
        void handleEnterButtonPress();   // 处理 ENTER 按键按下
        void handleEnterButtonRelease(); // 处理 ENTER 按键释放
        void handleEscButtonPress();     // 处理 ESC 按键按下
        void handleEscButtonRelease();   // 处理 ESC 按键释放
        void handleUpButtonPress();      // 处理 UP 按键按下
        void handleUpButtonRelease();    // 处理 UP 按键释放
        void handleDownButtonPress();    // 处理 DOWN 按键按下
        void handleDownButtonRelease();  // 处理 DOWN 按键释放
        bool isMenuActive();             // 检查菜单是否激活
        void setMenuActive(bool active); // 设置菜单激活状态
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
        KeypadKey mapKeyByRotation(KeypadKey physicalKey);

        // 按键状态管理
        bool leftButtonPressed = false;                   // LEFT 按键是否按下
        bool rightButtonPressed = false;                  // RIGHT 按键是否按下
        bool enterButtonPressed = false;                  // ENTER 按键是否按下
        bool escButtonPressed = false;                    // ESC 按键是否按下
        bool upButtonPressed = false;                     // UP 按键是否按下
        bool downButtonPressed = false;                   // DOWN 按键是否按下
        uint32_t leftButtonPressTime = 0;                 // LEFT 按键按下时间
        uint32_t rightButtonPressTime = 0;                // RIGHT 按键按下时间
        uint32_t enterButtonPressTime = 0;                // ENTER 按键按下时间
        uint32_t escButtonPressTime = 0;                  // ESC 按键按下时间
        uint32_t upButtonPressTime = 0;                   // UP 按键按下时间
        uint32_t downButtonPressTime = 0;                 // DOWN 按键按下时间
        static const uint32_t LONG_PRESS_THRESHOLD = 500; // 长按阈值（毫秒）
        bool menuActive = false;                          // 菜单是否激活
    };
}