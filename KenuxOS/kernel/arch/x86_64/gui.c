/*
 * gui.c - 仿 Windows GUI 实现
 * 分层架构: gfx(图形原语) -> widget(组件) -> desktop(桌面)
 */

#include <arch/gui.h>
#include <arch/framebuffer.h>
#include <string.h>

/* ===== 字符串长度 (避免依赖 stdlib) ===== */
static int gui_strlen(const char *s) {
    int n = 0;
    while (s && *s++) n++;
    return n;
}

/* ===== 图形原语层 (gui_gfx) ===== */

/* 绘制 3D 效果矩形 (仿 Windows 经典按钮边框) */
void gfx_draw_rect_3d(int x, int y, int w, int h, uint32_t face,
                      uint32_t highlight, uint32_t shadow, int raised)
{
    /* 填充主体 */
    fb_fill_rect(x, y, (uint32_t)w, (uint32_t)h, face);

    if (raised) {
        /* 凸起: 上/左 = 高光(白), 下/右 = 阴影(灰) */
        fb_draw_line(x, y, x + w - 1, y, highlight);           /* 上 */
        fb_draw_line(x, y, x, y + h - 1, highlight);           /* 左 */
        fb_draw_line(x, y + h - 1, x + w - 1, y + h - 1, shadow); /* 下 */
        fb_draw_line(x + w - 1, y, x + w - 1, y + h - 1, shadow); /* 右 */
    } else {
        /* 凹陷: 上/左 = 阴影(灰), 下/右 = 高光(白) */
        fb_draw_line(x, y, x + w - 1, y, shadow);              /* 上 */
        fb_draw_line(x, y, x, y + h - 1, shadow);              /* 左 */
        fb_draw_line(x, y + h - 1, x + w - 1, y + h - 1, highlight); /* 下 */
        fb_draw_line(x + w - 1, y, x + w - 1, y + h - 1, highlight); /* 右 */
    }
}

/* 在指定矩形内居中绘制文本 */
void gfx_draw_text_center(int x, int y, int w, int h,
                          const char *text, uint32_t fg, uint32_t bg)
{
    if (!text) return;
    int len = gui_strlen(text);
    int text_w = len * 8;   /* 8x16 字体 */
    int text_h = 16;

    int tx = x + (w - text_w) / 2;
    if (tx < x) tx = x;
    int ty = y + (h - text_h) / 2;
    if (ty < y) ty = y;

    fb_puts((uint32_t)tx, (uint32_t)ty, text, fg, bg);
}

/* 在指定位置绘制文本 */
void gfx_draw_text(int x, int y, const char *text, uint32_t fg, uint32_t bg)
{
    if (!text) return;
    fb_puts((uint32_t)x, (uint32_t)y, text, fg, bg);
}


/* ===== 组件层 (gui_widget) ===== */

void widget_init(struct widget *w, enum widget_type type,
                 int x, int y, int w_, int h_)
{
    memset(w, 0, sizeof(*w));
    w->type = type;
    w->r.x = x;
    w->r.y = y;
    w->r.w = (uint32_t)w_;
    w->r.h = (uint32_t)h_;
    w->visible = 1;
    w->bg_color = GUI_COLOR_BTN_FACE;
    w->fg_color = GUI_COLOR_TEXT;
}

void widget_set_text(struct widget *w, const char *text)
{
    w->text = text;
}

/* 绘制按钮 */
void button_draw(struct widget *btn)
{
    if (!btn || !btn->visible) return;

    uint32_t face = btn->bg_color;
    uint32_t hi = GUI_COLOR_BTN_HIGHLIGHT;
    uint32_t sh = GUI_COLOR_BTN_SHADOW;

    if (btn->pressed) {
        /* 按下: 凹陷效果 */
        gfx_draw_rect_3d(btn->r.x, btn->r.y, (int)btn->r.w, (int)btn->r.h,
                         face, sh, hi, 0);
        /* 文字偏移 1px 模拟按下 */
        gfx_draw_text_center(btn->r.x + 1, btn->r.y + 1,
                             (int)btn->r.w, (int)btn->r.h,
                             btn->text, btn->fg_color, face);
    } else {
        /* 正常: 凸起效果 */
        gfx_draw_rect_3d(btn->r.x, btn->r.y, (int)btn->r.w, (int)btn->r.h,
                         face, hi, sh, 1);
        gfx_draw_text_center(btn->r.x, btn->r.y,
                             (int)btn->r.w, (int)btn->r.h,
                             btn->text, btn->fg_color, face);
    }
}

/* 初始化窗口 */
void window_init(struct window *win, int x, int y, int w, int h,
                 const char *title)
{
    memset(win, 0, sizeof(*win));
    widget_init(&win->base, WIDGET_WINDOW, x, y, w, h);
    win->state = WIN_STATE_ACTIVE;
    win->title = title;
    win->base.bg_color = GUI_COLOR_WINDOW_BG;
}

/* 绘制窗口 */
void window_draw(struct window *win)
{
    if (!win || !win->base.visible) return;

    int x = win->base.r.x;
    int y = win->base.r.y;
    int w = (int)win->base.r.w;
    int h = (int)win->base.r.h;
    uint32_t title_color = (win->state == WIN_STATE_ACTIVE)
        ? GUI_COLOR_TITLEBAR : GUI_COLOR_TITLEBAR_INACTIVE;

    /* 外边框 (深灰) */
    fb_fill_rect(x, y, (uint32_t)w, (uint32_t)h, GUI_COLOR_WINDOW_FRAME);

    /* 标题栏 */
    fb_fill_rect(x + 1, y + 1, (uint32_t)(w - 2), GUI_TITLEBAR_HEIGHT,
                 title_color);

    /* 标题栏文字 */
    if (win->title) {
        gfx_draw_text(x + 6, y + 4, win->title,
                      GUI_COLOR_TEXT_WHITE, title_color);
    }

    /* 关闭按钮 (右上角) */
    int close_x = x + w - 18;
    int close_y = y + 3;
    gfx_draw_rect_3d(close_x, close_y, 16, 16,
                     GUI_COLOR_BTN_FACE,
                     GUI_COLOR_BTN_HIGHLIGHT, GUI_COLOR_BTN_SHADOW, 1);
    gfx_draw_text(close_x + 4, close_y + 1, "X",
                  GUI_COLOR_TEXT, GUI_COLOR_BTN_FACE);

    /* 窗口内容区 (白色背景) */
    fb_fill_rect(x + 1, y + 1 + GUI_TITLEBAR_HEIGHT,
                 (uint32_t)(w - 2), (uint32_t)(h - GUI_TITLEBAR_HEIGHT - 2),
                 GUI_COLOR_WINDOW_BG);

    /* 绘制子控件 */
    for (int i = 0; i < win->base.child_count; i++) {
        struct widget *child = win->base.children[i];
        if (!child || !child->visible) continue;
        if (child->type == WIDGET_BUTTON) {
            button_draw(child);
        }
    }
}


/* ===== 桌面层 (gui_desktop) ===== */

#define DESKTOP_MAX_WINDOWS 8

static struct {
    struct window *windows[DESKTOP_MAX_WINDOWS];
    int window_count;
    int screen_w;
    int screen_h;
    int initialized;
} desktop;

void desktop_init(void)
{
    fb_info_t *fb = fb_get_info();
    if (!fb) return;

    memset(&desktop, 0, sizeof(desktop));
    desktop.screen_w = (int)fb->width;
    desktop.screen_h = (int)fb->height;
    desktop.initialized = 1;
}

/* 绘制桌面背景 */
static void desktop_draw_background(void)
{
    /* 整屏填充桌面色 */
    fb_clear(GUI_COLOR_DESKTOP);
}

/* 绘制任务栏 */
static void desktop_draw_taskbar(void)
{
    int tb_y = desktop.screen_h - GUI_TASKBAR_HEIGHT;

    /* 任务栏主体 */
    fb_fill_rect(0, (uint32_t)tb_y,
                 (uint32_t)desktop.screen_w, GUI_TASKBAR_HEIGHT,
                 GUI_COLOR_TASKBAR);

    /* 任务栏上边线 (高光) */
    fb_draw_line(0, tb_y, desktop.screen_w - 1, tb_y,
                 GUI_COLOR_BTN_HIGHLIGHT);

    /* 开始按钮 */
    int btn_x = 2;
    int btn_y = tb_y + (GUI_TASKBAR_HEIGHT - GUI_START_BTN_HEIGHT) / 2;
    gfx_draw_rect_3d(btn_x, btn_y, GUI_START_BTN_WIDTH, GUI_START_BTN_HEIGHT,
                     GUI_COLOR_START_BTN,
                     GUI_COLOR_BTN_HIGHLIGHT, GUI_COLOR_BTN_SHADOW, 1);
    gfx_draw_text_center(btn_x, btn_y, GUI_START_BTN_WIDTH, GUI_START_BTN_HEIGHT,
                         "Start", GUI_COLOR_TEXT_WHITE, GUI_COLOR_START_BTN);

    /* 任务栏右侧时钟区域 */
    int clock_w = 80;
    int clock_x = desktop.screen_w - clock_w - 2;
    gfx_draw_rect_3d(clock_x, btn_y, clock_w, GUI_START_BTN_HEIGHT,
                     GUI_COLOR_TASKBAR,
                     GUI_COLOR_BTN_SHADOW, GUI_COLOR_BTN_HIGHLIGHT, 0);
    gfx_draw_text_center(clock_x, btn_y, clock_w, GUI_START_BTN_HEIGHT,
                         "12:00", GUI_COLOR_TEXT, GUI_COLOR_TASKBAR);
}

void desktop_draw(void)
{
    if (!desktop.initialized) return;

    /* 绘制桌面背景 */
    desktop_draw_background();

    /* 绘制所有窗口 */
    for (int i = 0; i < desktop.window_count; i++) {
        window_draw(desktop.windows[i]);
    }

    /* 绘制任务栏 (最上层) */
    desktop_draw_taskbar();
}

void desktop_add_window(struct window *win)
{
    if (!win || desktop.window_count >= DESKTOP_MAX_WINDOWS) return;
    desktop.windows[desktop.window_count++] = win;
}

/* 桌面主循环 (简单展示，后续可添加事件处理) */
void desktop_run(void)
{
    desktop_draw();
    /* 后续可添加鼠标/键盘事件处理循环 */
}
