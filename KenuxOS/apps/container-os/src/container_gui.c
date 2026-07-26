#include "container_gui.h"
#include <string.h>

static void draw_tab_bar(container_gui_t *gui, int x, int y, int w)
{
    int tab_w = w / 4;
    const char *labels[] = { "Containers", "Images", "Volumes", "Logs" };

    for (int i = 0; i < 4; i++) {
        int tx = x + i * tab_w;
        int active = (gui->current_tab == i);
        uint32_t face = active ? GUI_COLOR_WINDOW_BG : GUI_COLOR_TASKBAR;
        uint32_t fg = active ? GUI_COLOR_TEXT : GUI_COLOR_TEXT;
        uint32_t hi = GUI_COLOR_BTN_HIGHLIGHT;
        uint32_t sh = GUI_COLOR_BTN_SHADOW;

        gfx_draw_rect_3d(tx, y, tab_w - 2, 22, face, hi, sh, active ? 0 : 1);
        gfx_draw_text_center(tx, y, tab_w - 2, 22, labels[i], fg, face);
    }
}

static void draw_container_list(container_gui_t *gui, int x, int y, int w, int h)
{
    int count = manager_count();
    gfx_draw_text(x + 4, y + 4, "ID              Name             Image          Status",
                  GUI_COLOR_TEXT, GUI_COLOR_WINDOW_BG);

    fb_draw_line(x, y + 22, x + w - 2, y + 22, GUI_COLOR_WINDOW_FRAME);

    for (int i = 0; i < count && i < 10; i++) {
        container_t *c = manager_get(i);
        if (!c) continue;

        int row_y = y + 26 + i * 24;
        uint32_t bg = (i == gui->selected_index) ? 0xFF000080 : GUI_COLOR_WINDOW_BG;
        uint32_t fg = (i == gui->selected_index) ? GUI_COLOR_TEXT_WHITE : GUI_COLOR_TEXT;

        fb_fill_rect(x, (uint32_t)row_y, (uint32_t)(w - 2), 22, bg);

        char line[256];
        const char *status = container_status_str(c->status);
        int pos = 0;

        for (int j = 0; c->id[j] && pos < 16; j++)
            line[pos++] = c->id[j];
        while (pos < 16) line[pos++] = ' ';

        for (int j = 0; c->name[j] && pos < 33; j++)
            line[pos++] = c->name[j];
        while (pos < 33) line[pos++] = ' ';

        for (int j = 0; c->image[j] && pos < 50; j++)
            line[pos++] = c->image[j];
        while (pos < 50) line[pos++] = ' ';

        for (int j = 0; status[j] && pos < 65; j++)
            line[pos++] = status[j];
        line[pos] = '\0';

        gfx_draw_text(x + 4, row_y + 4, line, fg, bg);
    }

    if (count == 0) {
        gfx_draw_text(x + 4, y + 30, "No containers. Click Create to add one.",
                      GUI_COLOR_WINDOW_FRAME, GUI_COLOR_WINDOW_BG);
    }
}

static void draw_image_list(container_gui_t *gui, int x, int y, int w, int h)
{
    (void)gui;
    int count = image_count();
    gfx_draw_text(x + 4, y + 4, "Repository       Tag      OS       Size",
                  GUI_COLOR_TEXT, GUI_COLOR_WINDOW_BG);
    fb_draw_line(x, y + 22, x + w - 2, y + 22, GUI_COLOR_WINDOW_FRAME);

    container_image_t images[MAX_IMAGES];
    int n = image_list(images, MAX_IMAGES);

    for (int i = 0; i < n && i < 10; i++) {
        int row_y = y + 26 + i * 24;
        char line[256];
        int pos = 0;

        for (int j = 0; images[i].name[j] && pos < 17; j++)
            line[pos++] = images[i].name[j];
        while (pos < 17) line[pos++] = ' ';

        for (int j = 0; images[i].tag[j] && pos < 26; j++)
            line[pos++] = images[i].tag[j];
        while (pos < 26) line[pos++] = ' ';

        for (int j = 0; images[i].os[j] && pos < 35; j++)
            line[pos++] = images[i].os[j];
        while (pos < 35) line[pos++] = ' ';

        if (images[i].size >= 1024 * 1024) {
            char buf[16];
            int sz = (int)(images[i].size / (1024 * 1024));
            int p = 0;
            if (sz == 0) buf[p++] = '0';
            else {
                char tmp[16]; int t = 0;
                while (sz > 0) { tmp[t++] = '0' + sz % 10; sz /= 10; }
                while (t > 0) buf[p++] = tmp[--t];
            }
            buf[p++] = 'M'; buf[p++] = 'B'; buf[p] = '\0';
            for (int j = 0; buf[j] && pos < 44; j++)
                line[pos++] = buf[j];
        }
        line[pos] = '\0';

        gfx_draw_text(x + 4, row_y + 4, line, GUI_COLOR_TEXT, GUI_COLOR_WINDOW_BG);
    }
}

static void draw_volume_list(container_gui_t *gui, int x, int y, int w, int h)
{
    (void)gui;
    int count = volume_count();
    gfx_draw_text(x + 4, y + 4, "Name            Mount Path    Size      Used",
                  GUI_COLOR_TEXT, GUI_COLOR_WINDOW_BG);
    fb_draw_line(x, y + 22, x + w - 2, y + 22, GUI_COLOR_WINDOW_FRAME);

    volume_t vols[MAX_VOLUMES];
    int n = volume_list(vols, MAX_VOLUMES);

    for (int i = 0; i < n && i < 10; i++) {
        int row_y = y + 26 + i * 24;
        char line[256];
        int pos = 0;

        for (int j = 0; vols[i].name[j] && pos < 17; j++)
            line[pos++] = vols[i].name[j];
        while (pos < 17) line[pos++] = ' ';

        for (int j = 0; vols[i].mount_path[j] && pos < 31; j++)
            line[pos++] = vols[i].mount_path[j];
        while (pos < 31) line[pos++] = ' ';

        int sz_mb = (int)(vols[i].size / (1024 * 1024));
        char tmp[16]; int t = 0;
        if (sz_mb == 0) tmp[t++] = '0';
        else while (sz_mb > 0) { tmp[t++] = '0' + sz_mb % 10; sz_mb /= 10; }
        while (t > 0 && pos < 41) line[pos++] = tmp[--t];
        while (pos < 46) line[pos++] = ' ';

        int used_mb = (int)(vols[i].used / (1024 * 1024));
        t = 0;
        if (used_mb == 0) tmp[t++] = '0';
        else while (used_mb > 0) { tmp[t++] = '0' + used_mb % 10; used_mb /= 10; }
        while (t > 0 && pos < 55) line[pos++] = tmp[--t];
        line[pos] = '\0';

        gfx_draw_text(x + 4, row_y + 4, line, GUI_COLOR_TEXT, GUI_COLOR_WINDOW_BG);
    }
}

static void draw_log_view(container_gui_t *gui, int x, int y, int w, int h)
{
    container_t *c = manager_get(gui->selected_index);
    if (!c) {
        gfx_draw_text(x + 4, y + 4, "Select a container to view logs.",
                      GUI_COLOR_WINDOW_FRAME, GUI_COLOR_WINDOW_BG);
        return;
    }

    char title[128];
    int pos = 0;
    const char *prefix = "Logs for ";
    for (int i = 0; prefix[i]; i++) title[pos++] = prefix[i];
    for (int i = 0; c->name[i] && pos < 60; i++) title[pos++] = c->name[i];
    title[pos] = '\0';

    gfx_draw_text(x + 4, y + 4, title, GUI_COLOR_TEXT, GUI_COLOR_WINDOW_BG);
    fb_draw_line(x, y + 22, x + w - 2, y + 22, GUI_COLOR_WINDOW_FRAME);

    int line_y = y + 26;
    int log_x = 0;
    int line_start = 0;

    for (int i = 0; i < c->log_len && line_y < y + h - 30; i++) {
        if (c->logs[i] == '\n' || c->logs[i] == '\0') {
            char line[256];
            int len = i - line_start;
            if (len > 100) len = 100;
            for (int j = 0; j < len; j++)
                line[j] = c->logs[line_start + j];
            line[len] = '\0';
            gfx_draw_text(x + 4, line_y, line, GUI_COLOR_TEXT, GUI_COLOR_WINDOW_BG);
            line_y += 16;
            line_start = i + 1;
            log_x = 0;
        }
    }
}

static void draw_content(container_gui_t *gui)
{
    int x = gui->main_win.base.r.x + 2;
    int y = gui->main_win.base.r.y + GUI_TITLEBAR_HEIGHT + 28;
    int w = (int)gui->main_win.base.r.w - 4;
    int h = (int)gui->main_win.base.r.h - GUI_TITLEBAR_HEIGHT - 80;

    fb_fill_rect(x, (uint32_t)y, (uint32_t)w, (uint32_t)h, GUI_COLOR_WINDOW_BG);
    gfx_draw_rect_3d(x, y, w, h, GUI_COLOR_WINDOW_BG,
                     GUI_COLOR_BTN_SHADOW, GUI_COLOR_BTN_HIGHLIGHT, 0);

    switch (gui->current_tab) {
        case TAB_CONTAINERS:
            draw_container_list(gui, x + 2, y + 2, w - 4, h - 4);
            break;
        case TAB_IMAGES:
            draw_image_list(gui, x + 2, y + 2, w - 4, h - 4);
            break;
        case TAB_VOLUMES:
            draw_volume_list(gui, x + 2, y + 2, w - 4, h - 4);
            break;
        case TAB_LOGS:
            draw_log_view(gui, x + 2, y + 2, w - 4, h - 4);
            break;
    }
}

void container_gui_init(container_gui_t *gui, int x, int y)
{
    memset(gui, 0, sizeof(*gui));
    window_init(&gui->main_win, x, y, CONTAINER_GUI_WIN_W, CONTAINER_GUI_WIN_H,
                "Container Manager");
    gui->main_win.state = WIN_STATE_INACTIVE;
    gui->current_tab = TAB_CONTAINERS;
    gui->selected_index = 0;

    widget_init(&gui->btn_create, WIDGET_BUTTON, x + 10,
                y + CONTAINER_GUI_WIN_H - 38, 70, 26);
    widget_set_text(&gui->btn_create, "Create");
    gui->btn_create.parent = &gui->main_win.base;
    gui->main_win.base.children[gui->main_win.base.child_count++] = &gui->btn_create;

    widget_init(&gui->btn_start, WIDGET_BUTTON, x + 90,
                y + CONTAINER_GUI_WIN_H - 38, 70, 26);
    widget_set_text(&gui->btn_start, "Start");
    gui->btn_start.parent = &gui->main_win.base;
    gui->main_win.base.children[gui->main_win.base.child_count++] = &gui->btn_start;

    widget_init(&gui->btn_stop, WIDGET_BUTTON, x + 170,
                y + CONTAINER_GUI_WIN_H - 38, 70, 26);
    widget_set_text(&gui->btn_stop, "Stop");
    gui->btn_stop.parent = &gui->main_win.base;
    gui->main_win.base.children[gui->main_win.base.child_count++] = &gui->btn_stop;

    widget_init(&gui->btn_remove, WIDGET_BUTTON, x + 250,
                y + CONTAINER_GUI_WIN_H - 38, 70, 26);
    widget_set_text(&gui->btn_remove, "Remove");
    gui->btn_remove.parent = &gui->main_win.base;
    gui->main_win.base.children[gui->main_win.base.child_count++] = &gui->btn_remove;
}

void container_gui_draw(container_gui_t *gui)
{
    if (!gui) return;
    window_draw(&gui->main_win);

    int x = gui->main_win.base.r.x + 2;
    int y = gui->main_win.base.r.y + GUI_TITLEBAR_HEIGHT + 2;
    int w = (int)gui->main_win.base.r.w - 4;

    draw_tab_bar(gui, x, y, w);
    draw_content(gui);
}

static container_gui_t s_gui;

void container_gui_show(void)
{
    manager_init();
    volume_init();

    if (manager_count() == 0) {
        manager_create("web-server", "busybox", "httpd -f");
        manager_create("db-service", "alpine", "mysqld");
        manager_create("cache-app", "busybox", "redis-server");
    }

    container_gui_init(&s_gui, 120, 50);
    desktop_add_window(&s_gui.main_win);
    container_gui_draw(&s_gui);
}
