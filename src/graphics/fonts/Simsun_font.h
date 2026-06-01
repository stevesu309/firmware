#pragma once
#include <stdint.h>

/* 单个字形的描述 */
typedef struct
{
  uint32_t bitmap_index; // 在字模数组中的偏移
  uint16_t adv_w;        // 逻辑宽度
  uint8_t box_w;         // 实际像素宽度
  uint8_t box_h;         // 实际像素高度
  int8_t ofs_x;          // X 偏移
  int8_t ofs_y;          // Y 偏移
} simsun_glyph_dsc_t;

// cmap 类型定义（替代 LVGL 宏）
#define SIMSUN_CMAP_FORMAT0_TINY 0
#define SIMSUN_CMAP_FORMAT0_FULL 1
#define SIMSUN_CMAP_SPARSE_TINY 2

// Unicode 映射范围
typedef struct
{
  uint32_t range_start;              // Unicode 起始码点
  uint32_t range_length;             // 范围长度
  uint16_t glyph_id_start;           // 第一个字形 ID
  const uint16_t *unicode_list;      // 稀疏映射时的 Unicode 列表
  const uint16_t *glyph_id_ofs_list; // FULL 模式时的 offset 表
  uint16_t list_length;
  uint8_t type; // SIMSUN_CMAP_FORMAT0_TINY / FULL / SPARSE_TINY
} simsun_cmap_t;

extern const uint8_t simsun_glyph_bitmap[];
extern const simsun_glyph_dsc_t simsun_glyph_dsc[];
extern const simsun_cmap_t simsun_cmaps[];
extern const uint16_t simsun_cmap_num;

// 查找函数
const simsun_glyph_dsc_t *simsun_get_glyph(uint32_t unicode);
