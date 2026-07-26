#ifndef CONTAINER_OS_GUI_H
#define CONTAINER_OS_GUI_H

#include <arch/gui.h>
#include "manager.h"
#include "image.h"
#include "volume.h"

#define CONTAINER_GUI_WIN_W  600
#define CONTAINER_GUI_WIN_H  420

typedef enum {
    TAB_CONTAINERS = 0,
    TAB_IMAGES,
    TAB_VOLUMES,
    TAB_LOGS
} gui_tab_t;

typedef struct {
    struct window main_win;
    struct widget btn_start;
    struct widget btn_stop;
    struct widget btn_create;
    struct widget btn_remove;
    struct widget tab_containers;
    struct widget tab_images;
    struct widget tab_volumes;
    struct widget tab_logs;
    gui_tab_t current_tab;
    int selected_index;
} container_gui_t;

void container_gui_init(container_gui_t *gui, int x, int y);
void container_gui_draw(container_gui_t *gui);
void container_gui_show(void);

#endif
