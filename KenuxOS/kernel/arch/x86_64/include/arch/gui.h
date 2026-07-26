#ifndef ARCH_X86_64_GUI_H
#define ARCH_X86_64_GUI_H

#include <stdint.h>
#include <arch/framebuffer.h>

/* ===== 颜色定义 (仿 Windows 经典主题) ===== */
#define GUI_COLOR_DESKTOP      0xFF008080  /* 桌面背景 teal */
#define GUI_COLOR_TASKBAR      0xFFC0C0C0  /* 任务栏 silver */
#define GUI_COLOR_TASKBAR_DARK 0xFF808080  /* 任务栏阴影 */
#define GUI_COLOR_TITLEBAR     0xFF000080  /* 标题栏 navy */
#define GUI_COLOR_TITLEBAR_INACTIVE 0xFF808080
#define GUI_COLOR_WINDOW_BG    0xFFFFFFFF  /* 窗口内容区白色 */
#define GUI_COLOR_WINDOW_FRAME 0xFF808080  /* 窗口边框 */
#define GUI_COLOR_BTN_FACE     0xFFC0C0C0  /* 按钮表面 */
#define GUI_COLOR_BTN_HIGHLIGHT 0xFFFFFFFF /* 按钮高光(上左) */
#define GUI_COLOR_BTN_SHADOW   0xFF808080  /* 按钮阴影(下右) */
#define GUI_COLOR_BTN_DARK     0xFF000000  /* 按钮暗边 */
#define GUI_COLOR_TEXT         0xFF000000  /* 黑色文字 */
#define GUI_COLOR_TEXT_WHITE   0xFFFFFFFF  /* 白色文字 */
#define GUI_COLOR_START_BTN    0xFF000080  /* 开始按钮 */

/* ===== 布局常量 ===== */
#define GUI_TASKBAR_HEIGHT     32
#define GUI_START_BTN_WIDTH    70
#define GUI_START_BTN_HEIGHT   24
#define GUI_TITLEBAR_HEIGHT    22
#define GUI_WINDOW_MIN_W       80
#define GUI_WINDOW_MIN_H       60
#define GUI_WINDOW_BORDER      2

/* ===== 控件类型 ===== */
enum widget_type {
    WIDGET_NONE = 0,
    WIDGET_WINDOW,
    WIDGET_BUTTON,
    WIDGET_LABEL,
    WIDGET_MAX
};

/* ===== 窗口状态 ===== */
enum window_state {
    WIN_STATE_NORMAL = 0,
    WIN_STATE_ACTIVE,
    WIN_STATE_INACTIVE
};

/* ===== 矩形 ===== */
struct rect {
    int32_t x, y;
    uint32_t w, h;
};

/* ===== 控件基类 ===== */
struct widget {
    enum widget_type type;
    struct rect r;
    uint32_t bg_color;
    uint32_t fg_color;
    const char *text;
    int visible;
    int pressed;
    struct widget *parent;
    struct widget *children[8];
    int child_count;
};

/* ===== 窗口 ===== */
struct window {
    struct widget base;
    enum window_state state;
    const char *title;
};

/* ===== 图形原语 (gui_gfx) ===== */
void gfx_draw_rect_3d(int x, int y, int w, int h, uint32_t face,
                      uint32_t highlight, uint32_t shadow, int raised);
void gfx_draw_text_center(int x, int y, int w, int h,
                          const char *text, uint32_t fg, uint32_t bg);
void gfx_draw_text(int x, int y, const char *text, uint32_t fg, uint32_t bg);

/* ===== 组件 (gui_widget) ===== */
void widget_init(struct widget *w, enum widget_type type,
                 int x, int y, int w_, int h_);
void widget_set_text(struct widget *w, const char *text);
void button_draw(struct widget *btn);
void window_draw(struct window *win);
void window_init(struct window *win, int x, int y, int w, int h, const char *title);

/* ===== 桌面 (gui_desktop) ===== */
void desktop_init(void);
void desktop_draw(void);
void desktop_add_window(struct window *win);
void desktop_run(void);

#endif
