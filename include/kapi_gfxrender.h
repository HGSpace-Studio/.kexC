#ifndef KAPI_GFXRENDER_H
#define KAPI_GFXRENDER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "kapi_window.h"
#include "kapi_graphics2d.h"

#ifdef __cplusplus
extern "C" {
#endif

#define KGR_MAX_CONTEXTS       32
#define KGR_DEFAULT_WIDTH      800
#define KGR_DEFAULT_HEIGHT     600

#define KGR_FORMAT_RGBA8888    0
#define KGR_FORMAT_RGBX8888    1
#define KGR_FORMAT_RGB565      2

#define KGR_FLAG_VSYNC         (1u << 0)
#define KGR_FLAG_DOUBLE_BUF    (1u << 1)
#define KGR_FLAG_RESIZABLE     (1u << 2)
#define KGR_FLAG_FULLSCREEN    (1u << 3)
#define KGR_FLAG_BORDERLESS    (1u << 4)
#define KGR_FLAG_HIDDEN        (1u << 5)

typedef struct kgr_context kgr_context_t;

typedef struct {
    uint8_t r, g, b, a;
} kgr_color_t;

typedef struct {
    char title[256];
    int32_t width;
    int32_t height;
    uint32_t format;
    uint32_t flags;
    kgr_color_t background;
} kgr_window_desc_t;

typedef struct {
    uint32_t type;
    uint32_t timestamp;
    kgr_context_t* ctx;
    union {
        struct { int32_t x, y; } move;
        struct { int32_t width, height; } resize;
        struct { uint32_t key; uint32_t mods; } key;
        struct { uint32_t button; int32_t x, y; } mouse;
        struct { int32_t dx, dy; } wheel;
    } data;
} kgr_event_t;

#define KGR_EVENT_NONE        0
#define KGR_EVENT_CLOSE       1
#define KGR_EVENT_RESIZE      2
#define KGR_EVENT_MOVE        3
#define KGR_EVENT_KEY_DOWN    4
#define KGR_EVENT_KEY_UP      5
#define KGR_EVENT_MOUSE_MOVE  6
#define KGR_EVENT_MOUSE_DOWN  7
#define KGR_EVENT_MOUSE_UP    8
#define KGR_EVENT_WHEEL       9
#define KGR_EVENT_FOCUS_IN    10
#define KGR_EVENT_FOCUS_OUT   11
#define KGR_EVENT_PAINT       12

typedef void (*kgr_event_fn)(kgr_context_t* ctx, const kgr_event_t* ev, void* userdata);

struct kgr_context {
    uint32_t id;
    kapi_window_t* window;
    kapi_surface_t* front;
    kapi_surface_t* back;
    uint32_t width;
    uint32_t height;
    uint32_t format;
    uint32_t flags;
    kgr_color_t clear_color;
    kgr_event_fn on_event;
    void* event_ud;
    bool active;
    bool should_close;
    bool needs_present;
    kgr_context_t* next;
};

int kapi_gfxrender_init(void);
void kapi_gfxrender_shutdown(void);

kgr_context_t* kgr_create(const kgr_window_desc_t* desc);
int kgr_destroy(kgr_context_t* ctx);
kgr_context_t* kgr_find(uint32_t id);

int kgr_set_event_handler(kgr_context_t* ctx, kgr_event_fn fn, void* userdata);
int kgr_poll_event(kgr_context_t* ctx, kgr_event_t* ev);
bool kgr_should_close(kgr_context_t* ctx);
void kgr_request_close(kgr_context_t* ctx);

int kgr_begin_frame(kgr_context_t* ctx);
int kgr_end_frame(kgr_context_t* ctx);
int kgr_present(kgr_context_t* ctx);

int kgr_clear(kgr_context_t* ctx, kgr_color_t color);
int kgr_draw_pixel(kgr_context_t* ctx, int32_t x, int32_t y, kgr_color_t color);
int kgr_draw_line(kgr_context_t* ctx, int32_t x0, int32_t y0, int32_t x1, int32_t y1, kgr_color_t color);
int kgr_draw_rect(kgr_context_t* ctx, int32_t x, int32_t y, int32_t w, int32_t h, kgr_color_t color);
int kgr_fill_rect(kgr_context_t* ctx, int32_t x, int32_t y, int32_t w, int32_t h, kgr_color_t color);
int kgr_draw_circle(kgr_context_t* ctx, int32_t cx, int32_t cy, int32_t r, kgr_color_t color);
int kgr_fill_circle(kgr_context_t* ctx, int32_t cx, int32_t cy, int32_t r, kgr_color_t color);
int kgr_fill_round_rect(kgr_context_t* ctx, int32_t x, int32_t y, int32_t w, int32_t h, int32_t rad, kgr_color_t color);
int kgr_draw_text(kgr_context_t* ctx, int32_t x, int32_t y, const char* text, kgr_color_t color);
int kgr_draw_image(kgr_context_t* ctx, int32_t x, int32_t y, int32_t w, int32_t h, const uint8_t* data, size_t len);

int kgr_set_title(kgr_context_t* ctx, const char* title);
int kgr_resize(kgr_context_t* ctx, int32_t w, int32_t h);
int kgr_get_size(kgr_context_t* ctx, int32_t* w, int32_t* h);

uint32_t* kgr_fb(kgr_context_t* ctx, int32_t* stride);
int kgr_blit(kgr_context_t* ctx, int32_t dx, int32_t dy,
             const uint8_t* src, int32_t sw, int32_t sh, int32_t sstride);

#ifdef __cplusplus
}
#endif

#endif