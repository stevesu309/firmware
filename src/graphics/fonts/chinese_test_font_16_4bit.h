#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /* 单个字形的描述 */
  typedef struct
  {
    uint32_t bitmap_index; // 在字模数组中的偏移
    uint16_t adv_w;        // 逻辑宽度
    uint8_t box_w;         // 实际像素宽度
    uint8_t box_h;         // 实际像素高度
    int8_t ofs_x;          // X 偏移
    int8_t ofs_y;          // Y 偏移
  } chinese_font_glyph_dsc_t_4bit;

// cmap 类型定义
#define CHINESE_FONT_CMAP_FORMAT0_TINY 0
#define CHINESE_FONT_CMAP_SPARSE_TINY 1

  // Unicode 映射范围
  typedef struct
  {
    uint32_t range_start;              // Unicode 起始码点
    uint32_t range_length;             // 范围长度
    uint16_t glyph_id_start;           // 第一个字形 ID
    const uint16_t *unicode_list;      // 稀疏映射时的 Unicode 列表
    const uint16_t *glyph_id_ofs_list; // FULL 模式时的 offset 表
    uint16_t list_length;
    uint8_t type; // CHINESE_FONT_CMAP_FORMAT0_TINY / FULL / SPARSE_TINY
  } chinese_font_cmap_t_4bit;

  // 字距调整对
  typedef struct
  {
    const uint16_t *glyph_ids; // 字距调整对的字形ID
    const int8_t *values;      // 字距调整值
    uint16_t pair_cnt;         // 对的数量
    uint8_t glyph_ids_size;    // 字形ID的大小
  } chinese_font_kern_pair_t_4bit;

  // 字体描述结构
  typedef struct
  {
    const uint8_t *glyph_bitmap;                    // 字形位图数据
    const chinese_font_glyph_dsc_t_4bit *glyph_dsc; // 字形描述
    const chinese_font_cmap_t_4bit *cmaps;          // Unicode映射表
    const chinese_font_kern_pair_t_4bit *kern_dsc;  // 字距调整描述
    uint8_t kern_scale;                             // 字距调整缩放
    uint16_t cmap_num;                              // 映射表数量
    uint8_t bpp;                                    // 每像素位数
    uint8_t kern_classes;                           // 字距调整类
    uint8_t bitmap_format;                          // 位图格式
  } chinese_font_dsc_t_4bit;

  // 字体结构
  typedef struct
  {
    const chinese_font_dsc_t_4bit *dsc; // 字体描述
    uint16_t line_height;               // 行高
    uint16_t base_line;                 // 基线
    uint8_t subpx;                      // 子像素渲染
    int8_t underline_position;          // 下划线位置
    uint8_t underline_thickness;        // 下划线厚度
    const void *user_data;              // 用户数据
  } chinese_font_t_4bit;

  extern const uint8_t chinese_font_glyph_bitmap_4bit[];
  extern const chinese_font_glyph_dsc_t_4bit chinese_font_glyph_dsc_4bit[];
  extern const chinese_font_cmap_t_4bit chinese_font_cmaps_4bit[];
  extern const uint16_t chinese_font_cmap_num_4bit;
  extern const chinese_font_t_4bit chinese_font_16_4bit;

  // 查找函数
  extern const chinese_font_glyph_dsc_t_4bit *chinese_font_get_glyph_4bit(uint32_t unicode);

#ifdef __cplusplus
}
#endif
