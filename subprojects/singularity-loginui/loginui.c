#define _GNU_SOURCE
#include "loginui.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <pango/pangocairo.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

/* ── image helpers ──────────────────────────────────────────────────────── */

static GdkPixbuf *box_blur(GdkPixbuf *src, int radius) {
    int w = gdk_pixbuf_get_width(src), h = gdk_pixbuf_get_height(src);
    int nch = gdk_pixbuf_get_n_channels(src);
    int srs = gdk_pixbuf_get_rowstride(src);
    const guint8 *s = gdk_pixbuf_get_pixels(src);
    int count = 2 * radius + 1;

    GdkPixbuf *tmp = gdk_pixbuf_new(GDK_COLORSPACE_RGB, gdk_pixbuf_get_has_alpha(src), 8, w, h);
    int trs = gdk_pixbuf_get_rowstride(tmp);
    guint8 *t = gdk_pixbuf_get_pixels(tmp);
    for (int y = 0; y < h; y++) {
        const guint8 *srow = s + y * srs;
        guint8 *trow = t + y * trs;
        for (int c = 0; c < nch; c++) {
            int sum = 0;
            for (int k = -radius; k <= radius; k++) {
                int xx = k < 0 ? 0 : (k >= w ? w - 1 : k);
                sum += srow[xx * nch + c];
            }
            for (int x = 0; x < w; x++) {
                trow[x * nch + c] = (guint8)(sum / count);
                int xout = x - radius; if (xout < 0) xout = 0;
                int xin = x + radius + 1; if (xin >= w) xin = w - 1;
                sum += srow[xin * nch + c] - srow[xout * nch + c];
            }
        }
    }
    GdkPixbuf *dst = gdk_pixbuf_new(GDK_COLORSPACE_RGB, gdk_pixbuf_get_has_alpha(src), 8, w, h);
    int drs = gdk_pixbuf_get_rowstride(dst);
    guint8 *d = gdk_pixbuf_get_pixels(dst);
    for (int x = 0; x < w; x++) {
        for (int c = 0; c < nch; c++) {
            int sum = 0;
            for (int k = -radius; k <= radius; k++) {
                int yy = k < 0 ? 0 : (k >= h ? h - 1 : k);
                sum += t[yy * trs + x * nch + c];
            }
            for (int y = 0; y < h; y++) {
                d[y * drs + x * nch + c] = (guint8)(sum / count);
                int yout = y - radius; if (yout < 0) yout = 0;
                int yin = y + radius + 1; if (yin >= h) yin = h - 1;
                sum += t[yin * trs + x * nch + c] - t[yout * trs + x * nch + c];
            }
        }
    }
    g_object_unref(tmp);
    return dst;
}

static cairo_surface_t *pixbuf_to_surface(GdkPixbuf *pb) {
    int w = gdk_pixbuf_get_width(pb), h = gdk_pixbuf_get_height(pb);
    int nch = gdk_pixbuf_get_n_channels(pb);
    int prs = gdk_pixbuf_get_rowstride(pb);
    const guint8 *pix = gdk_pixbuf_get_pixels(pb);
    cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_RGB24, w, h);
    unsigned char *data = cairo_image_surface_get_data(s);
    int crs = cairo_image_surface_get_stride(s);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            const guint8 *p = pix + y * prs + x * nch;
            uint32_t *dp = (uint32_t *)(data + y * crs + x * 4);
            *dp = ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | (uint32_t)p[2];
        }
    }
    cairo_surface_mark_dirty(s);
    return s;
}

cairo_surface_t *loginui_load_wallpaper(const char *path, int target_w) {
    if (!path || path[0] == '\0') return NULL;
    if (g_str_has_prefix(path, "file://")) path += 7;
    GdkPixbuf *pb = gdk_pixbuf_new_from_file_at_scale(path, target_w > 0 ? target_w : 960, -1, TRUE, NULL);
    if (!pb) return NULL;
    GdkPixbuf *b1 = box_blur(pb, 16);
    GdkPixbuf *b2 = box_blur(b1, 16);
    g_object_unref(pb);
    g_object_unref(b1);
    cairo_surface_t *s = pixbuf_to_surface(b2);
    g_object_unref(b2);
    return s;
}

cairo_surface_t *loginui_load_avatar(const char *path, int size) {
    if (!path || path[0] == '\0') return NULL;
    GdkPixbuf *pb = gdk_pixbuf_new_from_file_at_scale(path, size, size, FALSE, NULL);
    if (!pb) return NULL;
    cairo_surface_t *s = pixbuf_to_surface(pb);
    g_object_unref(pb);
    return s;
}

cairo_surface_t *loginui_load_image(const char *path, int max_w, int max_h) {
    if (!path || path[0] == '\0') return NULL;
    GdkPixbuf *pb = gdk_pixbuf_new_from_file_at_scale(path, max_w, max_h, TRUE, NULL);
    if (!pb) return NULL;
    int w = gdk_pixbuf_get_width(pb), h = gdk_pixbuf_get_height(pb);
    int nch = gdk_pixbuf_get_n_channels(pb);
    gboolean alpha = gdk_pixbuf_get_has_alpha(pb);
    int prs = gdk_pixbuf_get_rowstride(pb);
    const guint8 *pix = gdk_pixbuf_get_pixels(pb);
    cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
    unsigned char *data = cairo_image_surface_get_data(s);
    int crs = cairo_image_surface_get_stride(s);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            const guint8 *p = pix + y * prs + x * nch;
            uint32_t a = alpha ? p[3] : 255;
            uint32_t r = p[0] * a / 255, g = p[1] * a / 255, b = p[2] * a / 255;
            uint32_t *dp = (uint32_t *)(data + y * crs + x * 4);
            *dp = (a << 24) | (r << 16) | (g << 8) | b;
        }
    }
    cairo_surface_mark_dirty(s);
    g_object_unref(pb);
    return s;
}

/* ── shm buffer ─────────────────────────────────────────────────────────── */

static void buffer_release(void *data, struct wl_buffer *wl_buffer) {
    struct loginui_buffer *b = data;
    wl_buffer_destroy(b->wl_buffer);
    munmap(b->data, b->size);
    free(b);
}
static const struct wl_buffer_listener buffer_listener = { buffer_release };

struct loginui_buffer *loginui_create_buffer(struct wl_shm *shm,
                                             uint32_t w, uint32_t h,
                                             cairo_t **cr_out) {
    uint32_t stride = w * 4;
    size_t size = (size_t)stride * h;
    int fd = memfd_create("singularity-loginui", MFD_CLOEXEC);
    if (fd < 0) return NULL;
    if (ftruncate(fd, (off_t)size) < 0) { close(fd); return NULL; }
    void *data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) { close(fd); return NULL; }
    struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, (int32_t)size);
    struct wl_buffer *wl_buffer = wl_shm_pool_create_buffer(
        pool, 0, (int32_t)w, (int32_t)h, (int32_t)stride, WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    close(fd);

    struct loginui_buffer *b = calloc(1, sizeof(*b));
    b->data = data; b->size = size; b->wl_buffer = wl_buffer;
    wl_buffer_add_listener(wl_buffer, &buffer_listener, b);

    cairo_surface_t *cs = cairo_image_surface_create_for_data(
        data, CAIRO_FORMAT_ARGB32, (int)w, (int)h, (int)stride);
    *cr_out = cairo_create(cs);
    cairo_surface_destroy(cs);
    return b;
}

/* ── cairo helpers ──────────────────────────────────────────────────────── */

static void draw_text(cairo_t *cr, const char *desc, const char *text,
                      double x, double y, int align, double r, double g, double b) {
    PangoLayout *layout = pango_cairo_create_layout(cr);
    PangoFontDescription *fd = pango_font_description_from_string(desc);
    pango_layout_set_font_description(layout, fd);
    pango_font_description_free(fd);
    pango_layout_set_text(layout, text, -1);
    int tw, th;
    pango_layout_get_pixel_size(layout, &tw, &th);
    double tx = x;
    if (align == 1) tx = x - tw / 2.0;
    else if (align == 2) tx = x - tw;
    cairo_set_source_rgb(cr, r, g, b);
    cairo_move_to(cr, tx, y);
    pango_cairo_show_layout(cr, layout);
    g_object_unref(layout);
}

static void text_size(cairo_t *cr, const char *desc, const char *text, int *w, int *h) {
    PangoLayout *layout = pango_cairo_create_layout(cr);
    PangoFontDescription *fd = pango_font_description_from_string(desc);
    pango_layout_set_font_description(layout, fd);
    pango_font_description_free(fd);
    pango_layout_set_text(layout, text, -1);
    pango_layout_get_pixel_size(layout, w, h);
    g_object_unref(layout);
}

static void rounded_rect(cairo_t *cr, double x, double y, double w, double h, double r) {
    double deg = 3.14159265 / 180.0;
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + w - r, y + r,     r, -90 * deg,   0);
    cairo_arc(cr, x + w - r, y + h - r, r,   0,        90 * deg);
    cairo_arc(cr, x + r,     y + h - r, r,  90 * deg,  180 * deg);
    cairo_arc(cr, x + r,     y + r,     r, 180 * deg,  270 * deg);
    cairo_close_path(cr);
}

/* ── render ─────────────────────────────────────────────────────────────── */

void loginui_render(cairo_t *cr, const LoginUiState *st,
                    double *card_x_out, double *card_y_out,
                    double *card_w_out, double *card_h_out) {
    double w = st->width, h = st->height;

    if (st->background) {
        int bw = cairo_image_surface_get_width(st->background);
        int bh = cairo_image_surface_get_height(st->background);
        double scale = w / bw; if (h / bh > scale) scale = h / bh;
        cairo_save(cr);
        cairo_translate(cr, (w - bw * scale) / 2.0, (h - bh * scale) / 2.0);
        cairo_scale(cr, scale, scale);
        cairo_set_source_surface(cr, st->background, 0, 0);
        cairo_paint(cr);
        cairo_restore(cr);
    } else {
        cairo_set_source_rgb(cr, 0.10, 0.10, 0.11);
        cairo_paint(cr);
    }

    const char *tbuf = st->time_str ? st->time_str : "";
    const char *dbuf = st->date_str ? st->date_str : "";
    const char *uname = (st->username && st->username[0]) ? st->username : "user";

    int tw, th, dw, dh;
    text_size(cr, "Sans Bold 60", tbuf, &tw, &th);
    text_size(cr, "Sans 18", dbuf, &dw, &dh);
    int clock_w = tw > dw ? tw : dw;
    double gap = 40;
    double card_w = 300, card_pad = 14;
    double avatar = 46, field_h = 56;
    double card_h = card_pad + avatar + 14 + field_h + card_pad;
    if (st->status_text && st->status_text[0]) card_h += 38;

    double group_w = clock_w + gap + card_w;
    double group_x = (w - group_w) / 2.0;
    double clock_right = group_x + clock_w;
    double card_x = group_x + clock_w + gap;
    double card_y = (h - card_h) / 2.0;

    if (card_x_out) *card_x_out = card_x;
    if (card_y_out) *card_y_out = card_y;
    if (card_w_out) *card_w_out = card_w;
    if (card_h_out) *card_h_out = card_h;

    double clock_block_h = th + 6 + dh;
    double clock_y = (h - clock_block_h) / 2.0;
    draw_text(cr, "Sans Bold 60", tbuf, clock_right, clock_y, 2, 1, 1, 1);
    draw_text(cr, "Sans 18", dbuf, clock_right, clock_y + th + 6, 2, 0.85, 0.85, 0.88);

    rounded_rect(cr, card_x, card_y, card_w, card_h, 24);
    cairo_set_source_rgba(cr, 0.176, 0.176, 0.176, 0.97);
    cairo_fill(cr);
    rounded_rect(cr, card_x + 0.5, card_y + 0.5, card_w - 1, card_h - 1, 24);
    cairo_set_source_rgba(cr, 1, 1, 1, 0.08);
    cairo_set_line_width(cr, 1);
    cairo_stroke(cr);

    double row_cy = card_y + card_pad + avatar / 2.0;
    double name_x = card_x + card_pad;
    if (st->avatar) {
        double ax = card_x + card_pad + avatar / 2.0;
        cairo_save(cr);
        cairo_arc(cr, ax, row_cy, avatar / 2.0, 0, 2 * 3.14159265);
        cairo_clip(cr);
        int aw = cairo_image_surface_get_width(st->avatar);
        int ah = cairo_image_surface_get_height(st->avatar);
        double sc = avatar / (double)(aw < ah ? aw : ah);
        cairo_translate(cr, ax - avatar / 2.0 + (avatar - aw * sc) / 2.0,
                            row_cy - avatar / 2.0 + (avatar - ah * sc) / 2.0);
        cairo_scale(cr, sc, sc);
        cairo_set_source_surface(cr, st->avatar, 0, 0);
        cairo_paint(cr);
        cairo_restore(cr);
        name_x = card_x + card_pad + avatar + 12;
    }
    {
        int nw, nh;
        text_size(cr, "Sans Bold 18", uname, &nw, &nh);
        draw_text(cr, "Sans Bold 18", uname, name_x, row_cy - nh / 2.0, 0, 0.96, 0.96, 0.98);
    }

    double fy = card_y + card_pad + avatar + 14;
    double fx = card_x + card_pad;
    double fw = card_w - 2 * card_pad;
    rounded_rect(cr, fx, fy, fw, field_h, 14);
    cairo_set_source_rgba(cr, 1, 1, 1, 0.08);
    cairo_fill(cr);
    draw_text(cr, "Sans Bold 12", "Password", fx + 14, fy + 8, 0, 0.96, 0.96, 0.98);
    double vcy = fy + 40;
    if (st->password_dots <= 0) {
        int pw, ph;
        text_size(cr, "Sans 12", "Password", &pw, &ph);
        draw_text(cr, "Sans 12", "Password", fx + 14, vcy - ph / 2.0, 0, 0.6, 0.6, 0.62);
    } else {
        int dots = st->password_dots; if (dots > 20) dots = 20;
        cairo_set_source_rgb(cr, 0.9, 0.9, 0.92);
        for (int i = 0; i < dots; i++) {
            cairo_arc(cr, fx + 18 + i * 16, vcy, 4, 0, 2 * 3.14159265);
            cairo_fill(cr);
        }
    }

    if (st->status_text && st->status_text[0]) {
        double sy = fy + field_h + 8;
        double sh = 30;
        rounded_rect(cr, fx, sy, fw, sh, 9);
        if (st->status_error) cairo_set_source_rgba(cr, 0.90, 0.30, 0.28, 0.16);
        else cairo_set_source_rgba(cr, 1, 1, 1, 0.07);
        cairo_fill(cr);
        int sw, sht;
        text_size(cr, "Sans 12", st->status_text, &sw, &sht);
        if (st->status_error)
            draw_text(cr, "Sans 12", st->status_text, fx + fw / 2.0, sy + (sh - sht) / 2.0, 1, 0.97, 0.52, 0.48);
        else
            draw_text(cr, "Sans 12", st->status_text, fx + fw / 2.0, sy + (sh - sht) / 2.0, 1, 0.82, 0.82, 0.85);
    }
}

/* ── public drawing helpers (for the greeter's chrome) ──────────────────── */

void loginui_rounded_rect(cairo_t *cr, double x, double y, double w, double h, double r) {
    rounded_rect(cr, x, y, w, h, r);
}

void loginui_text(cairo_t *cr, const char *font, const char *text,
                  double x, double y, int align, double r, double g, double b) {
    draw_text(cr, font, text, x, y, align, r, g, b);
}

void loginui_text_size(cairo_t *cr, const char *font, const char *text, int *w, int *h) {
    text_size(cr, font, text, w, h);
}

void loginui_paint_background(cairo_t *cr, cairo_surface_t *bg, int width, int height) {
    double w = width, h = height;
    if (bg) {
        int bw = cairo_image_surface_get_width(bg);
        int bh = cairo_image_surface_get_height(bg);
        double scale = w / bw; if (h / bh > scale) scale = h / bh;
        cairo_save(cr);
        cairo_translate(cr, (w - bw * scale) / 2.0, (h - bh * scale) / 2.0);
        cairo_scale(cr, scale, scale);
        cairo_set_source_surface(cr, bg, 0, 0);
        cairo_paint(cr);
        cairo_restore(cr);
    } else {
        cairo_set_source_rgb(cr, 0.10, 0.10, 0.11);
        cairo_paint(cr);
    }
}
