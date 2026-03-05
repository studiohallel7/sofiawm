/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-
 *
 * framerender.c — SofiaWM Decorator
 * Cairo + Pango — zero dependência do obrender para a titlebar
 *
 *  FOCUSED:
 *  ┌──────────────────────────────────────────────────────┐ #1E1E1E
 *  │ [✕][─][◻] ┊ [🗁][＋][✎][↻]       Nome Da Janela  │ #181818
 *  ├──────────────────────────────────────────────────────┤ #D35400
 *
 *  UNFOCUSED:
 *  ┌──────────────────────────────────────────────────────┐ #1E1E1E
 *  │ [✕][─][◻]   [🗁][＋][✎][↻]       Nome Da Janela  │ #121212
 *  ├──────────────────────────────────────────────────────┤ #222222
 */

#include "framerender.h"
#include "frame.h"
#include "openbox.h"
#include "client.h"

#include <cairo/cairo.h>
#include <cairo/cairo-xlib.h>
#include <pango/pangocairo.h>
#include <X11/Xlib.h>

#define SOFIA_TITLE_H    40
#define SOFIA_BTN_W      32
#define SOFIA_SEP_PAD    10
#define SOFIA_FONT       "Inter Semi-Bold 10"
#define SOFIA_BTN_FONT   "Inter 14"

static inline void set_rgb(cairo_t *cr, int r, int g, int b)
{
    cairo_set_source_rgb(cr, r/255.0, g/255.0, b/255.0);
}

static void fill_rect(cairo_t *cr,
                       double x, double y, double w, double h,
                       int r, int g, int b)
{
    set_rgb(cr, r, g, b);
    cairo_rectangle(cr, x, y, w, h);
    cairo_fill(cr);
}

static void draw_symbol(cairo_t    *cr,
                         const char *sym,
                         double      btn_x,
                         double      btn_h,
                         int         r, int g, int b)
{
    PangoLayout *lo = pango_cairo_create_layout(cr);
    PangoFontDescription *fd = pango_font_description_from_string(SOFIA_BTN_FONT);
    pango_layout_set_font_description(lo, fd);
    pango_layout_set_text(lo, sym, -1);
    pango_font_description_free(fd);

    int tw, th;
    pango_layout_get_pixel_size(lo, &tw, &th);

    set_rgb(cr, r, g, b);
    cairo_move_to(cr,
        btn_x + (SOFIA_BTN_W - tw) / 2.0,
        (btn_h - th) / 2.0);
    pango_cairo_show_layout(cr, lo);
    g_object_unref(lo);
}

static void draw_title(cairo_t    *cr,
                        const char *text,
                        double      area_w,
                        double      h,
                        gboolean    focused)
{
    PangoLayout *lo = pango_cairo_create_layout(cr);
    PangoFontDescription *fd = pango_font_description_from_string(SOFIA_FONT);
    pango_layout_set_font_description(lo, fd);
    pango_layout_set_text(lo, text, -1);
    pango_font_description_free(fd);

    PangoAttrList *attrs = pango_attr_list_new();
    pango_attr_list_insert(attrs, pango_attr_letter_spacing_new(800));
    pango_layout_set_attributes(lo, attrs);
    pango_attr_list_unref(attrs);

    int tw, th;
    pango_layout_get_pixel_size(lo, &tw, &th);

    if (focused)
        set_rgb(cr, 0x55,0x55,0x55);
    else
        set_rgb(cr, 0x44,0x44,0x44);

    cairo_move_to(cr, area_w - tw - 16.0, (h - th) / 2.0);
    pango_cairo_show_layout(cr, lo);
    g_object_unref(lo);
}

/* Pinta um botão dentro da sua própria subwindow X11 */
static void render_button_window(Display    *dpy,
                                  Window      win,
                                  int         win_w,
                                  int         win_h,
                                  const char *sym,
                                  gboolean    hover,
                                  gboolean    press,
                                  gboolean    is_close,
                                  gboolean    focused)
{
    cairo_surface_t *surf = cairo_xlib_surface_create(
        dpy, win,
        DefaultVisual(dpy, DefaultScreen(dpy)),
        win_w, win_h);

    if (cairo_surface_status(surf) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(surf);
        return;
    }

    cairo_t *cr = cairo_create(surf);

    if (press)
        fill_rect(cr, 0, 0, win_w, win_h, 0x33,0x33,0x33);
    else if (hover && is_close)
        fill_rect(cr, 0, 0, win_w, win_h, 0xC4,0x2B,0x1C);
    else if (hover)
        fill_rect(cr, 0, 0, win_w, win_h, 0x2A,0x2A,0x2A);
    else if (focused)
        fill_rect(cr, 0, 0, win_w, win_h, 0x18,0x18,0x18);
    else
        fill_rect(cr, 0, 0, win_w, win_h, 0x12,0x12,0x12);

    if (hover || press)
        draw_symbol(cr, sym, 0, win_h, 0xFF,0xFF,0xFF);
    else
        draw_symbol(cr, sym, 0, win_h, 0x77,0x77,0x77);

    cairo_destroy(cr);
    cairo_surface_destroy(surf);
}

static void render_titlebar(ObFrame *self)
{
    Display *dpy = obt_display;
    int w = self->width;
    gboolean focused = self->focused;
    double h = SOFIA_TITLE_H;

    /* Botões — cada um pintado na sua própria subwindow */
    gboolean is_max = self->client->max_vert || self->client->max_horz;

    if (self->close_on)
        render_button_window(dpy, self->close, 32, 30, "✕",
                             self->close_hover, self->close_press,
                             TRUE, focused);
    if (self->iconify_on)
        render_button_window(dpy, self->iconify, 32, 30, "─",
                             self->iconify_hover, self->iconify_press,
                             FALSE, focused);
    if (self->max_on)
        render_button_window(dpy, self->max, 32, 30,
                             is_max ? "❐" : "◻",
                             self->max_hover, self->max_press,
                             FALSE, focused);

    /* Surface para fundo + separador + ações + título */
    cairo_surface_t *surf = cairo_xlib_surface_create(
        dpy, self->title,
        DefaultVisual(dpy, DefaultScreen(dpy)),
        w, SOFIA_TITLE_H);

    if (cairo_surface_status(surf) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(surf);
        return;
    }

    cairo_t *cr = cairo_create(surf);

    /* Fundo */
    if (focused)
        fill_rect(cr, 0, 0, w, h, 0x18,0x18,0x18);
    else
        fill_rect(cr, 0, 0, w, h, 0x12,0x12,0x12);

    /* Posição após último botão */
    double x = self->max_x + 32 + 6;

    /* Separador vertical (só focused) */
    if (focused) {
        x += SOFIA_SEP_PAD;
        set_rgb(cr, 0x33,0x33,0x33);
        cairo_set_line_width(cr, 1.0);
        cairo_move_to(cr, x + 0.5, 8);
        cairo_line_to(cr, x + 0.5, h - 8);
        cairo_stroke(cr);
        x += 1 + SOFIA_SEP_PAD;
    } else {
        x += SOFIA_SEP_PAD * 2 + 1;
    }

    /* Botões de ação — dinâmicos via AppletManager */
    extern SofiaWindowActions sofia_current_actions;
    if (sofia_current_actions.count > 0) {
        for (int k = 0; k < sofia_current_actions.count; k++) {
            fill_rect(cr, x, 0, SOFIA_BTN_W, h,
                      focused ? 0x18 : 0x12,
                      focused ? 0x18 : 0x12,
                      focused ? 0x18 : 0x12);
            draw_symbol(cr, sofia_current_actions.actions[k].icon,
                        x, h, 0x77,0x77,0x77);
            x += SOFIA_BTN_W;
        }
    } else {
        /* Sem ações contextuais — ícones neutros padrão */
        const char *defaults[] = { "🗁", "＋", "✎", "↻" };
        for (int k = 0; k < 4; k++) {
            fill_rect(cr, x, 0, SOFIA_BTN_W, h,
                      focused ? 0x18 : 0x12,
                      focused ? 0x18 : 0x12,
                      focused ? 0x18 : 0x12);
            draw_symbol(cr, defaults[k], x, h, 0x44,0x44,0x44);
            x += SOFIA_BTN_W;
        }
    }

    /* Título à direita */
    if (self->client && self->client->title)
        draw_title(cr, self->client->title, w, h, focused);

    cairo_destroy(cr);
    cairo_surface_destroy(surf);
}

static void render_borders(ObFrame *self)
{
    gulong px_border  = 0x1E1E1E;
    gulong px_bg      = 0x121212;
    gulong px_bottom  = self->focused ? 0xD35400 : 0x222222;

    XSetWindowBackground(obt_display, self->titlebottom, px_bottom);
    XClearWindow(obt_display,         self->titlebottom);
    XSetWindowBackground(obt_display, self->titletop,   px_border);
    XClearWindow(obt_display,         self->titletop);
    XSetWindowBackground(obt_display, self->left,       px_border);
    XClearWindow(obt_display,         self->left);
    XSetWindowBackground(obt_display, self->right,      px_border);
    XClearWindow(obt_display,         self->right);
    XSetWindowBackground(obt_display, self->backback,    px_bg);
    XClearWindow(obt_display,         self->backback);
    XSetWindowBackground(obt_display, self->innerleft,   px_bg);
    XClearWindow(obt_display,         self->innerleft);
    XSetWindowBackground(obt_display, self->innerright,  px_bg);
    XClearWindow(obt_display,         self->innerright);
    XSetWindowBackground(obt_display, self->innerbottom, px_bg);
    XClearWindow(obt_display,         self->innerbottom);
}

static void hide_legacy_subwindows(ObFrame *self)
{
    /* Botões ficam visíveis — captura eventos de mouse para hover/press
     * Cairo pinta dentro de cada subwindow individualmente */
    if (self->label_on)
        XSetWindowBackgroundPixmap(obt_display, self->label, ParentRelative);

    /* Estes não são usados no SofiaWM */
    if (self->desk_on)    XUnmapWindow(obt_display, self->desk);
    if (self->shade_on)   XUnmapWindow(obt_display, self->shade);
    if (self->icon_on)    XUnmapWindow(obt_display, self->icon);
}

void framerender_frame(ObFrame *self)
{
    if (frame_iconify_animating(self)) return;
    if (!self->need_render)            return;
    if (!self->visible)                return;
    self->need_render = FALSE;

    hide_legacy_subwindows(self);

    if (self->decorations & OB_FRAME_DECOR_TITLEBAR)
        render_titlebar(self);

    render_borders(self);

    if (self->decorations & OB_FRAME_DECOR_HANDLE) {
        XSetWindowBackground(obt_display, self->handle, 0x121212);
        XClearWindow(obt_display, self->handle);
    }

    XFlush(obt_display);
}

void sofia_colors_free(void) { }
