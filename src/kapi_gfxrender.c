#include "kapi_gfxrender.h"
#include "kapi.h"
#include <string.h>

static kgr_context_t* g_ctx_head = NULL;
static uint32_t g_next_id = 1;

static inline uint32_t col32(kgr_color_t c) {
    return ((uint32_t)c.a << 24) | ((uint32_t)c.r << 16) | ((uint32_t)c.g << 8) | (uint32_t)c.b;
}

static inline kapi_color_t to_kapi_color(kgr_color_t c) {
    kapi_color_t kc;
    kc.r = c.r; kc.g = c.g; kc.b = c.b; kc.a = c.a;
    return kc;
}

int kapi_gfxrender_init(void) {
    g_ctx_head = NULL;
    g_next_id = 1;
    return 0;
}

void kapi_gfxrender_shutdown(void) {
    kgr_context_t* c = g_ctx_head;
    while (c) {
        kgr_context_t* n = c->next;
        if (c->back) {
            if (c->back->data) kapi_free(c->back->data);
            kapi_free(c->back);
        }
        if (c->window) kapi_window_destroy(c->window);
        kapi_free(c);
        c = n;
    }
    g_ctx_head = NULL;
}

kgr_context_t* kgr_create(const kgr_window_desc_t* desc) {
    if (!desc) return NULL;

    kgr_context_t* ctx = (kgr_context_t*)kapi_malloc(sizeof(kgr_context_t));
    if (!ctx) return NULL;
    memset(ctx, 0, sizeof(kgr_context_t));

    ctx->id = g_next_id++;
    ctx->width = desc->width > 0 ? (uint32_t)desc->width : KGR_DEFAULT_WIDTH;
    ctx->height = desc->height > 0 ? (uint32_t)desc->height : KGR_DEFAULT_HEIGHT;
    ctx->format = desc->format ? desc->format : KGR_FORMAT_RGBA8888;
    ctx->flags = desc->flags;
    ctx->clear_color = desc->background;
    ctx->active = true;
    ctx->should_close = false;
    ctx->needs_present = false;

    uint32_t wf = KAPI_WINDOW_FLAG_VISIBLE | KAPI_WINDOW_FLAG_BORDER;
    if (desc->flags & KGR_FLAG_RESIZABLE) wf |= KAPI_WINDOW_FLAG_RESIZABLE;
    if (desc->flags & KGR_FLAG_FULLSCREEN) wf |= KAPI_WINDOW_FLAG_FULLSCREEN;
    if (desc->flags & KGR_FLAG_HIDDEN) wf &= ~KAPI_WINDOW_FLAG_VISIBLE;

    ctx->window = kapi_window_create(desc->title, 0, 0, ctx->width, ctx->height,
                                     KAPI_WINDOW_TYPE_NORMAL, wf);
    if (!ctx->window) { kapi_free(ctx); return NULL; }

    ctx->front = kapi_window_get_surface(ctx->window);

    ctx->back = (kapi_surface_t*)kapi_malloc(sizeof(kapi_surface_t));
    if (!ctx->back) { kapi_window_destroy(ctx->window); kapi_free(ctx); return NULL; }
    memset(ctx->back, 0, sizeof(kapi_surface_t));
    ctx->back->width = ctx->width;
    ctx->back->height = ctx->height;
    ctx->back->stride = ctx->width * 4;
    ctx->back->format = KAPI_SURFACE_FORMAT_ARGB8888;
    size_t bsz = (size_t)ctx->width * ctx->height * 4;
    ctx->back->data = (uint8_t*)kapi_malloc(bsz);
    if (!ctx->back->data) {
        kapi_free(ctx->back);
        kapi_window_destroy(ctx->window);
        kapi_free(ctx);
        return NULL;
    }
    memset(ctx->back->data, 0, bsz);

    ctx->next = g_ctx_head;
    g_ctx_head = ctx;
    return ctx;
}

int kgr_destroy(kgr_context_t* ctx) {
    if (!ctx) return -1;
    if (g_ctx_head == ctx) {
        g_ctx_head = ctx->next;
    } else {
        kgr_context_t* p = g_ctx_head;
        while (p && p->next != ctx) p = p->next;
        if (p) p->next = ctx->next;
    }
    if (ctx->back) {
        if (ctx->back->data) kapi_free(ctx->back->data);
        kapi_free(ctx->back);
    }
    if (ctx->window) kapi_window_destroy(ctx->window);
    kapi_free(ctx);
    return 0;
}

kgr_context_t* kgr_find(uint32_t id) {
    kgr_context_t* c = g_ctx_head;
    while (c) { if (c->id == id) return c; c = c->next; }
    return NULL;
}

int kgr_set_event_handler(kgr_context_t* ctx, kgr_event_fn fn, void* userdata) {
    if (!ctx) return -1;
    ctx->on_event = fn;
    ctx->event_ud = userdata;
    return 0;
}

int kgr_poll_event(kgr_context_t* ctx, kgr_event_t* ev) {
    if (!ctx || !ev) return -1;
    if (!ctx->active) return 0;
    kapi_window_event_t we;
    int r = kapi_window_handle_event(ctx->window, &we);
    if (r != 0) return 0;
    memset(ev, 0, sizeof(kgr_event_t));
    ev->ctx = ctx;
    switch (we.type) {
        case KAPI_WINDOW_EVENT_CLOSE:
            ev->type = KGR_EVENT_CLOSE;
            ctx->should_close = true;
            break;
        case KAPI_WINDOW_EVENT_RESIZE:
            ev->type = KGR_EVENT_RESIZE;
            ev->data.resize.width = we.data.resize.width;
            ev->data.resize.height = we.data.resize.height;
            break;
        case KAPI_WINDOW_EVENT_MOVE:
            ev->type = KGR_EVENT_MOVE;
            ev->data.move.x = we.data.move.x;
            ev->data.move.y = we.data.move.y;
            break;
        case KAPI_WINDOW_EVENT_KEY:
            ev->type = (we.data.key.modifiers & 0x80) ? KGR_EVENT_KEY_UP : KGR_EVENT_KEY_DOWN;
            ev->data.key.key = we.data.key.key;
            ev->data.key.mods = we.data.key.modifiers;
            break;
        case KAPI_WINDOW_EVENT_MOUSE:
            ev->type = (we.data.mouse.dx || we.data.mouse.dy) ? KGR_EVENT_MOUSE_MOVE : KGR_EVENT_MOUSE_DOWN;
            ev->data.mouse.button = we.data.mouse.button;
            ev->data.mouse.x = we.data.mouse.x;
            ev->data.mouse.y = we.data.mouse.y;
            break;
        case KAPI_WINDOW_EVENT_PAINT:
            ev->type = KGR_EVENT_PAINT;
            break;
        default:
            ev->type = KGR_EVENT_NONE;
            break;
    }
    if (ctx->on_event && ev->type != KGR_EVENT_NONE)
        ctx->on_event(ctx, ev, ctx->event_ud);
    return ev->type != KGR_EVENT_NONE ? 1 : 0;
}

bool kgr_should_close(kgr_context_t* ctx) {
    return ctx ? ctx->should_close : true;
}

void kgr_request_close(kgr_context_t* ctx) {
    if (ctx) ctx->should_close = true;
}

int kgr_begin_frame(kgr_context_t* ctx) {
    if (!ctx || !ctx->back) return -1;
    return 0;
}

int kgr_end_frame(kgr_context_t* ctx) {
    if (!ctx || !ctx->back) return -1;
    ctx->needs_present = true;
    return 0;
}

int kgr_present(kgr_context_t* ctx) {
    if (!ctx || !ctx->back || !ctx->front) return -1;
    if (!ctx->needs_present) return 0;
    size_t sz = (size_t)ctx->width * ctx->height * 4;
    if (ctx->front->data && ctx->front->data != ctx->back->data)
        memcpy(ctx->front->data, ctx->back->data, sz);
    ctx->needs_present = false;
    return 0;
}

int kgr_clear(kgr_context_t* ctx, kgr_color_t color) {
    if (!ctx || !ctx->back || !ctx->back->data) return -1;
    uint32_t c = col32(color);
    uint32_t* fb = (uint32_t*)ctx->back->data;
    size_t n = (size_t)ctx->width * ctx->height;
    for (size_t i = 0; i < n; i++) fb[i] = c;
    return 0;
}

int kgr_draw_pixel(kgr_context_t* ctx, int32_t x, int32_t y, kgr_color_t color) {
    if (!ctx || !ctx->back || !ctx->back->data) return -1;
    if (x < 0 || x >= (int32_t)ctx->width || y < 0 || y >= (int32_t)ctx->height) return -1;
    ((uint32_t*)(ctx->back->data + y * ctx->back->stride))[x] = col32(color);
    return 0;
}

int kgr_draw_line(kgr_context_t* ctx, int32_t x0, int32_t y0, int32_t x1, int32_t y1, kgr_color_t color) {
    if (!ctx || !ctx->back) return -1;
    kapi_graphics_context_t gc;
    memset(&gc, 0, sizeof(gc));
    gc.surface = ctx->back;
    gc.stroke_color = to_kapi_color(color);
    kapi_graphics_context_draw_line(&gc, (float)x0, (float)y0, (float)x1, (float)y1);
    return 0;
}

int kgr_draw_rect(kgr_context_t* ctx, int32_t x, int32_t y, int32_t w, int32_t h, kgr_color_t color) {
    if (!ctx || !ctx->back) return -1;
    kapi_graphics_context_t gc;
    memset(&gc, 0, sizeof(gc));
    gc.surface = ctx->back;
    gc.stroke_color = to_kapi_color(color);
    kapi_graphics_context_draw_rect(&gc, (float)x, (float)y, (float)w, (float)h);
    return 0;
}

int kgr_fill_rect(kgr_context_t* ctx, int32_t x, int32_t y, int32_t w, int32_t h, kgr_color_t color) {
    if (!ctx || !ctx->back) return -1;
    kapi_graphics_context_t gc;
    memset(&gc, 0, sizeof(gc));
    gc.surface = ctx->back;
    gc.fill_color = to_kapi_color(color);
    gc.fill_mode = 1;
    kapi_graphics_context_fill_rect(&gc, (float)x, (float)y, (float)w, (float)h);
    return 0;
}

int kgr_draw_circle(kgr_context_t* ctx, int32_t cx, int32_t cy, int32_t r, kgr_color_t color) {
    if (!ctx || !ctx->back) return -1;
    kapi_graphics_context_t gc;
    memset(&gc, 0, sizeof(gc));
    gc.surface = ctx->back;
    gc.stroke_color = to_kapi_color(color);
    kapi_graphics_context_draw_ellipse(&gc, (float)cx, (float)cy, (float)r, (float)r);
    return 0;
}

int kgr_fill_circle(kgr_context_t* ctx, int32_t cx, int32_t cy, int32_t r, kgr_color_t color) {
    if (!ctx || !ctx->back) return -1;
    kapi_graphics_context_t gc;
    memset(&gc, 0, sizeof(gc));
    gc.surface = ctx->back;
    gc.fill_color = to_kapi_color(color);
    gc.fill_mode = 1;
    kapi_graphics_context_fill_ellipse(&gc, (float)cx, (float)cy, (float)r, (float)r);
    return 0;
}

int kgr_fill_round_rect(kgr_context_t* ctx, int32_t x, int32_t y, int32_t w, int32_t h, int32_t rad, kgr_color_t color) {
    if (!ctx || !ctx->back) return -1;
    kapi_graphics_context_t gc;
    memset(&gc, 0, sizeof(gc));
    gc.surface = ctx->back;
    gc.fill_color = to_kapi_color(color);
    gc.fill_mode = 1;
    kapi_graphics_context_fill_round_rect(&gc, (float)x, (float)y, (float)w, (float)h, (float)rad);
    return 0;
}

int kgr_draw_text(kgr_context_t* ctx, int32_t x, int32_t y, const char* text, kgr_color_t color) {
    if (!ctx || !ctx->back || !text) return -1;
    kapi_graphics_context_t gc;
    memset(&gc, 0, sizeof(gc));
    gc.surface = ctx->back;
    gc.fill_color = to_kapi_color(color);
    kapi_graphics_context_draw_text(&gc, text, (float)x, (float)y);
    return 0;
}

int kgr_draw_image(kgr_context_t* ctx, int32_t x, int32_t y, int32_t w, int32_t h, const uint8_t* data, size_t len) {
    if (!ctx || !ctx->back || !data) return -1;
    if (len < (size_t)(w * h * 4)) return -1;
    uint8_t* dst = ctx->back->data;
    if (!dst) return -1;
    int32_t sstride = w * 4;
    for (int32_t row = 0; row < h; row++) {
        int32_t dy = y + row;
        if (dy < 0 || dy >= (int32_t)ctx->height) continue;
        for (int32_t col = 0; col < w; col++) {
            int32_t dx = x + col;
            if (dx < 0 || dx >= (int32_t)ctx->width) continue;
            uint32_t sp = ((uint32_t*)(data + row * sstride))[col];
            ((uint32_t*)(dst + dy * ctx->back->stride))[dx] = sp;
        }
    }
    return 0;
}

int kgr_set_title(kgr_context_t* ctx, const char* title) {
    if (!ctx || !ctx->window || !title) return -1;
    return kapi_window_set_title(ctx->window, title);
}

int kgr_resize(kgr_context_t* ctx, int32_t w, int32_t h) {
    if (!ctx || w <= 0 || h <= 0) return -1;
    if (ctx->back && ctx->back->data) kapi_free(ctx->back->data);
    ctx->width = (uint32_t)w;
    ctx->height = (uint32_t)h;
    if (ctx->back) {
        ctx->back->width = ctx->width;
        ctx->back->height = ctx->height;
        ctx->back->stride = ctx->width * 4;
        size_t bsz = (size_t)w * h * 4;
        ctx->back->data = (uint8_t*)kapi_malloc(bsz);
        if (!ctx->back->data) return -1;
        memset(ctx->back->data, 0, bsz);
    }
    return kapi_window_resize(ctx->window, w, h);
}

int kgr_get_size(kgr_context_t* ctx, int32_t* w, int32_t* h) {
    if (!ctx) return -1;
    if (w) *w = (int32_t)ctx->width;
    if (h) *h = (int32_t)ctx->height;
    return 0;
}

uint32_t* kgr_fb(kgr_context_t* ctx, int32_t* stride) {
    if (!ctx || !ctx->back || !ctx->back->data) return NULL;
    if (stride) *stride = (int32_t)ctx->back->stride / 4;
    return (uint32_t*)ctx->back->data;
}

int kgr_blit(kgr_context_t* ctx, int32_t dx, int32_t dy,
             const uint8_t* src, int32_t sw, int32_t sh, int32_t sstride) {
    if (!ctx || !ctx->back || !ctx->back->data || !src) return -1;
    uint8_t* dst = ctx->back->data;
    for (int32_t row = 0; row < sh; row++) {
        int32_t y = dy + row;
        if (y < 0 || y >= (int32_t)ctx->height) continue;
        int32_t copy_w = sw;
        int32_t sx_start = 0;
        int32_t dx_start = dx;
        if (dx < 0) { sx_start = -dx; dx_start = 0; copy_w += dx; }
        if (dx_start + copy_w > (int32_t)ctx->width) copy_w = (int32_t)ctx->width - dx_start;
        if (copy_w <= 0) continue;
        memcpy(dst + y * ctx->back->stride + dx_start * 4,
               src + row * sstride + sx_start * 4,
               copy_w * 4);
    }
    return 0;
}