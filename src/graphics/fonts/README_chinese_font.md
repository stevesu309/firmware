# 中文字库使用说明

## 概述

这个字库是从LVGL格式转换而来的独立中文字库，支持ASCII字符和中文字符显示。

## 文件结构

- `chinese_test_font_16.h` - 头文件，包含所有结构体定义和函数声明
- `chinese_test_font_16.cpp` - 实现文件，包含字库数据、函数实现和测试函数
- `chinese_font_test.h` - 测试函数声明头文件
- `chinese_font_example.h` - 示例函数声明头文件

## 主要结构体

### chinese_font_glyph_dsc_t
字形描述结构体，包含：
- `bitmap_index`: 在位图数组中的偏移
- `adv_w`: 逻辑宽度
- `box_w`: 实际像素宽度
- `box_h`: 实际像素高度
- `ofs_x`: X偏移
- `ofs_y`: Y偏移

### chinese_font_t
字体结构体，包含：
- `dsc`: 字体描述指针
- `line_height`: 行高
- `base_line`: 基线
- `subpx`: 子像素渲染
- `underline_position`: 下划线位置（有符号，可为负值）
- `underline_thickness`: 下划线厚度

## 使用方法

### 1. 包含头文件
```c
#include "chinese_test_font_16.h"
```

### 2. 查找字形
```c
const chinese_font_glyph_dsc_t *glyph = chinese_font_get_glyph(unicode);
if (glyph) {
    // 使用字形信息进行渲染
    // glyph->box_w, glyph->box_h 是字形尺寸
    // glyph->ofs_x, glyph->ofs_y 是偏移
    // glyph->bitmap_index 是位图数据索引
}
```

### 3. 获取字体信息
```c
// 获取行高
uint16_t line_height = chinese_font_16.line_height;

// 获取基线
uint16_t base_line = chinese_font_16.base_line;
```

### 4. 渲染字符
```c
void render_char(uint32_t unicode, int x, int y) {
    const chinese_font_glyph_dsc_t *glyph = chinese_font_get_glyph(unicode);
    if (!glyph) return;
    
    // 计算实际绘制位置
    int draw_x = x + glyph->ofs_x;
    int draw_y = y + glyph->ofs_y;
    
    // 从位图数据中获取像素数据
    const uint8_t *bitmap = &chinese_font_glyph_bitmap[glyph->bitmap_index];
    
    // 渲染字形 (需要根据具体显示设备实现)
    // ...
}
```

## 支持的字符范围

- ASCII字符: 32-126 (空格到~)
- 中文字符: 19968-40857 (常用汉字范围)

## 注意事项

1. 字库使用1bpp格式，每个像素用1位表示
2. 位图数据按行存储，需要根据字形尺寸正确解析
3. 字距调整功能已包含，可根据需要启用
4. 字库大小约16KB，适合嵌入式系统使用

## 编译

### 在您的项目中使用字库
只需要包含一个文件：
- `chinese_test_font_16.cpp` - 包含字库实现和所有测试函数

### 在您的代码中调用测试函数
```cpp
#include "chinese_font_test.h"
#include "chinese_font_example.h"

// 在您的代码中调用
test_chinese_font();
test_chinese_characters();
render_text_example("测试文本", 0, 0);
```

**注意**：
- 所有函数都包含在`chinese_test_font_16.cpp`中，确保编译时包含此文件
- 头文件已经包含了C++兼容性支持，可以在C++项目中直接使用
- `underline_position`字段使用`int8_t`类型以支持负值，这在C++的严格类型检查下是必要的
- 测试函数使用`extern "C"`声明，确保C++兼容性

## 测试

### 在您的代码中测试
```cpp
#include "chinese_font_test.h"
#include "chinese_font_example.h"

void setup() {
    // 测试字库功能
    test_chinese_font();
    test_chinese_characters();
    render_text_example("测试中文字符", 0, 0);
}
```

### 独立测试程序
如果您想创建独立的测试程序：
```bash
# 创建简单的测试程序
g++ -o test_chinese_font test_main.cpp chinese_test_font_16.cpp
./test_chinese_font
```