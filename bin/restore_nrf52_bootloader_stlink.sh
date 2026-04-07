#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OPENOCD_BIN_DEFAULT="$HOME/.platformio/packages/tool-openocd/bin/openocd"
OPENOCD_SCRIPT_DIR_DEFAULT="$HOME/.platformio/packages/tool-openocd/openocd/scripts"
BOOTLOADER_DEFAULT="$ROOT_DIR/bin/generic/Meshtastic_6.1.0_bootloader-0.9.2_s140_6.1.1.hex"
ENV_DEFAULT="t-echo"
ADAPTER_SPEED_DEFAULT="100"
INTERFACE_DEFAULT="stlink"

OPENOCD_BIN="${OPENOCD_BIN:-$OPENOCD_BIN_DEFAULT}"
OPENOCD_SCRIPT_DIR="${OPENOCD_SCRIPT_DIR:-$OPENOCD_SCRIPT_DIR_DEFAULT}"
BOOTLOADER_HEX="$BOOTLOADER_DEFAULT"
ENV_NAME="$ENV_DEFAULT"
ADAPTER_SPEED="$ADAPTER_SPEED_DEFAULT"
INTERFACE_NAME="$INTERFACE_DEFAULT"
DO_BUILD_AND_UPLOAD=0

usage() {
    cat <<EOF
用法:
  $(basename "$0") [选项]

功能:
  1) 使用 ST-Link + OpenOCD 对 nRF52 执行 mass_erase
  2) 烧录 bootloader hex 并 verify
  3) (可选) 编译并上传指定 PlatformIO env

选项:
  -b, --bootloader <path>   指定 bootloader hex 路径
  -e, --env <name>          指定 PlatformIO 环境名 (默认: $ENV_DEFAULT)
  -s, --speed <khz>         SWD 速度 kHz (默认: $ADAPTER_SPEED_DEFAULT)
  -i, --interface <name>    调试器接口: stlink 或 jlink (默认: $INTERFACE_DEFAULT)
  -u, --upload              烧完 bootloader 后执行 pio run -e <env> -t upload
  -h, --help                显示帮助

环境变量覆盖:
  OPENOCD_BIN, OPENOCD_SCRIPT_DIR

示例:
  # 仅恢复默认 bootloader
  $(basename "$0")

  # 指定 bootloader 并且后续上传 t-echo 固件
  $(basename "$0") -b "$ROOT_DIR/bin/generic/Meshtastic_6.1.0_bootloader-0.9.2_s140_6.1.1.hex" -e t-echo -u

  # 使用 J-Link 执行恢复
  $(basename "$0") -i jlink
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
    -b|--bootloader)
        BOOTLOADER_HEX="$2"
        shift 2
        ;;
    -e|--env)
        ENV_NAME="$2"
        shift 2
        ;;
    -s|--speed)
        ADAPTER_SPEED="$2"
        shift 2
        ;;
    -i|--interface)
        INTERFACE_NAME="$2"
        shift 2
        ;;
    -u|--upload)
        DO_BUILD_AND_UPLOAD=1
        shift
        ;;
    -h|--help)
        usage
        exit 0
        ;;
    *)
        echo "未知参数: $1" >&2
        usage
        exit 1
        ;;
    esac
done

if [[ ! -x "$OPENOCD_BIN" ]]; then
    echo "未找到 openocd 可执行文件: $OPENOCD_BIN" >&2
    echo "请先确认 ~/.platformio/packages/tool-openocd 已安装，或设置 OPENOCD_BIN 环境变量。" >&2
    exit 1
fi

if [[ ! -f "$OPENOCD_SCRIPT_DIR/interface/stlink.cfg" ]]; then
    :
fi

if [[ ! -f "$OPENOCD_SCRIPT_DIR/target/nrf52.cfg" ]]; then
    echo "未找到 nrf52 脚本: $OPENOCD_SCRIPT_DIR/target/nrf52.cfg" >&2
    exit 1
fi

if [[ ! -f "$BOOTLOADER_HEX" ]]; then
    echo "bootloader 文件不存在: $BOOTLOADER_HEX" >&2
    exit 1
fi

echo "==> OpenOCD: $OPENOCD_BIN"
echo "==> Scripts: $OPENOCD_SCRIPT_DIR"
echo "==> Bootloader: $BOOTLOADER_HEX"
echo "==> Speed(kHz): $ADAPTER_SPEED"
echo "==> Interface: $INTERFACE_NAME"

case "$INTERFACE_NAME" in
stlink|jlink)
    ;;
*)
    echo "不支持的 interface: $INTERFACE_NAME (仅支持 stlink 或 jlink)" >&2
    exit 1
    ;;
esac

if [[ ! -f "$OPENOCD_SCRIPT_DIR/interface/$INTERFACE_NAME.cfg" ]]; then
    echo "未找到 interface 脚本: $OPENOCD_SCRIPT_DIR/interface/$INTERFACE_NAME.cfg" >&2
    exit 1
fi

echo "==> [1/2] mass_erase (会清空整片 Flash)"
"$OPENOCD_BIN" \
    -f "$OPENOCD_SCRIPT_DIR/interface/$INTERFACE_NAME.cfg" \
    -c "transport select swd" \
    -f "$OPENOCD_SCRIPT_DIR/target/nrf52.cfg" \
    -c "adapter speed $ADAPTER_SPEED; init; reset halt; nrf5 mass_erase; reset; shutdown"

echo "==> [2/2] program + verify bootloader"
"$OPENOCD_BIN" \
    -f "$OPENOCD_SCRIPT_DIR/interface/$INTERFACE_NAME.cfg" \
    -c "transport select swd" \
    -f "$OPENOCD_SCRIPT_DIR/target/nrf52.cfg" \
    -c "adapter speed $ADAPTER_SPEED; init; reset halt; program $BOOTLOADER_HEX verify; reset; shutdown"

echo "==> Bootloader 恢复完成"
echo "提示: 断开 ST-Link 后，插 USB 并尝试双击 Reset 进入 bootloader。"

if [[ "$DO_BUILD_AND_UPLOAD" -eq 1 ]]; then
    echo "==> 继续执行: pio run -e $ENV_NAME -t upload"
    cd "$ROOT_DIR"
    pio run -e "$ENV_NAME" -t upload
fi

