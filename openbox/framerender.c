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
#include "sofia_actions.h"

#include <cairo/cairo.h>
#include <cairo/cairo-xlib.h>
#include <pango/pangocairo.h>
#include <X11/Xlib.h>

/* =========================================================
 * CONSTANTES DE LAYOUT
 * ========================================================= */
#define SOFIA_TITLE_H    40
#define SOFIA_BTN_W      32
#define SOFIA_SEP_PAD    10
#define SOFIA_FONT       "Inter Semi-Bold 10"
#define SOFIA_BTN_FONT   "Inter 14"

/* Helper: seta cor Cairo a partir de componentes RGB 0-255 */
static inline void set_rgb(cairo_t *cr, int r, int g, int b)
{
    cairo_set_source_rgb(cr, r/255.0, g/255.0, b/255.0);
}

/* =========================================================
 * DESENHO DE RETÂNGULO SÓLIDO
 * ========================================================= */
static void fill_rect(cairo_t *cr,
                       double x, double y, double w, double h,
                       int r, int g, int b)
{
    set_rgb(cr, r, g, b);
    cairo_rectangle(cr, x, y, w, h);
    cairo_fill(cr);
}

/* =========================================================
 * DESENHO DE SÍMBOLO CENTRALIZADO NUM BOTÃO
 * ========================================================= */
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

/* =========================================================
 * BOTÃO COMPLETO (fundo + símbolo)
 * ========================================================= */
static void draw_button(cairo_t    *cr,
                         double      x,
                         double      h,
                         const char *sym,
                         gboolean    hover,
                         gboolean    press,
                         gboolean    is_close,
                         gboolean    focused)
{
    /* Fundo */
    if (press)
        fill_rect(cr, x, 0, SOFIA_BTN_W, h, 0x33,0x33,0x33);
    else if (hover && is_close)
        fill_rect(cr, x, 0, SOFIA_BTN_W, h, 0xC4,0x2B,0x1C);
    else if (hover)
        fill_rect(cr, x, 0, SOFIA_BTN_W, h, 0x2A,0x2A,0x2A);
    else if (focused)
        fill_rect(cr, x, 0, SOFIA_BTN_W, h, 0x18,0x18,0x18);
    else
        fill_rect(cr, x, 0, SOFIA_BTN_W, h, 0x12,0x12,0x12);

    /* Símbolo */
    if (hover || press)
        draw_symbol(cr, sym, x, h, 0xFF,0xFF,0xFF);
    else
        draw_symbol(cr, sym, x, h, 0x77,0x77,0x77);
}

/* =========================================================
 * TÍTULO DA JANELA — alinhado à direita
 * ========================================================= */
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

    /* Letter spacing */
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

/* =========================================================
 * RENDER DA TITLEBAR — Cairo direto na title Window
 * ========================================================= */
static void render_titlebar(ObFrame *self)
{
    Display *dpy = obt_display;
    int w = self->width;
    gboolean focused = self->focused;

    cairo_surface_t *surf = cairo_xlib_surface_create(
        dpy, self->title,
        DefaultVisual(dpy, DefaultScreen(dpy)),
        w, SOFIA_TITLE_H);

    if (cairo_surface_status(surf) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(surf);
        return;
    }

    cairo_t *cr = cairo_create(surf);
    double h = SOFIA_TITLE_H;

    /* 1. Fundo */
    if (focused)
        fill_rect(cr, 0, 0, w, h, 0x18,0x18,0x18);
    else
        fill_rect(cr, 0, 0, w, h, 0x12,0x12,0x12);

    /* 2. Botões de janela — posições lidas do frame (sincronizadas com frame.c)
     * frame.c usa: x=12, btn_width=32, espacamento=6
     * close=12, iconify=50, max=88 */
    /* Fallback se frame.c não inicializou as posições */
    double x = (self->close_x > 0) ? self->close_x : 12.0;

    draw_button(cr, x, h, "✕",
                self->close_hover, self->close_press, TRUE, focused);

    x = (self->iconify_x > 0) ? self->iconify_x : 50.0;
    draw_button(cr, x, h, "─",
                self->iconify_hover, self->iconify_press, FALSE, focused);

    x = (self->max_x > 0) ? self->max_x : 88.0;
    gboolean is_max = self->client->max_vert || self->client->max_horz;
    draw_button(cr, x, h, is_max ? "❐" : "◻",
                self->max_hover, self->max_press, FALSE, focused);

    /* Posição após o último botão */
    x = self->max_x + 32 + 6;

    /* 3. Separador vertical (só focused) */
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

    /* 4. Botões de ação — dinâmicos via AppletManager */
    if (focused && sofia_current_actions.count > 0) {
        for (int k = 0; k < sofia_current_actions.count && k < 4; k++) {
            draw_button(cr, x, h, sofia_current_actions.actions[k].icon,
                        FALSE, FALSE, FALSE, focused);
            x += SOFIA_BTN_W;
        }
    } else if (!focused) {
        /* unfocused: ações neutras apagadas */
        const char *neutral[] = { "🗁", "＋", "✎", "↻" };
        for (int k = 0; k < 4; k++) {
            draw_button(cr, x, h, neutral[k],
                        FALSE, FALSE, FALSE, FALSE);
            x += SOFIA_BTN_W;
        }
    }
    /* focused sem ações: espaço vazio */

    /* 5. Título à direita — limpa área antes para evitar glitch */
    if (self->client && self->client->title && self->client->title[0]) {
        /* Mede o título para limpar a área exata */
        PangoLayout *lo_m = pango_cairo_create_layout(cr);
        PangoFontDescription *fd_m = pango_font_description_from_string(SOFIA_FONT);
        pango_layout_set_font_description(lo_m, fd_m);
        pango_layout_set_text(lo_m, self->client->title, -1);
        PangoAttrList *attrs_m = pango_attr_list_new();
        pango_attr_list_insert(attrs_m, pango_attr_letter_spacing_new(800));
        pango_layout_set_attributes(lo_m, attrs_m);
        pango_attr_list_unref(attrs_m);
        pango_font_description_free(fd_m);
        int tw_m, th_m;
        pango_layout_get_pixel_size(lo_m, &tw_m, &th_m);
        g_object_unref(lo_m);
        /* Limpa área do título antes de desenhar */
        double title_x = (double)(w - tw_m - 16);
        if (focused)
            fill_rect(cr, title_x - 4, 0, tw_m + 20, h, 0x18,0x18,0x18);
        else
            fill_rect(cr, title_x - 4, 0, tw_m + 20, h, 0x12,0x12,0x12);
        draw_title(cr, self->client->title, w, h, focused);
    }

    cairo_destroy(cr);
    cairo_surface_destroy(surf);
}

/* =========================================================
 * RENDER DAS BORDAS — X11 direto
 * ========================================================= */
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

/* =========================================================
 * OCULTAR SUBWINDOWS LEGADAS DO OBRENDER
 * Cairo pinta direto na self->title — as subwindows ficam por cima
 * se não forem ocultadas
 * ========================================================= */
static void hide_legacy_subwindows(ObFrame *self)
{
    /* IMPORTANTE: close, max, iconify NÃO são desmapados!
     * O event.c detecta hover/press nessas subwindows X11.
     * Tornamos transparentes para o Cairo pintar por baixo,
     * mas elas precisam existir para capturar eventos de mouse. */

    /* Torna os botões transparentes — sem background */
    if (self->close_on)
        XSetWindowBackgroundPixmap(obt_display, self->close, ParentRelative);
    if (self->max_on)
        XSetWindowBackgroundPixmap(obt_display, self->max, ParentRelative);
    if (self->iconify_on)
        XSetWindowBackgroundPixmap(obt_display, self->iconify, ParentRelative);

    /* Label também transparente — Cairo desenha o título */
    if (self->label_on)
        XSetWindowBackgroundPixmap(obt_display, self->label, ParentRelative);

    /* Estes sim podem ser desmapados — não usamos */
    if (self->desk_on)    XUnmapWindow(obt_display, self->desk);
    if (self->shade_on)   XUnmapWindow(obt_display, self->shade);
    if (self->icon_on)    XUnmapWindow(obt_display, self->icon);
}

/* =========================================================
 * PONTO DE ENTRADA
 * ========================================================= */
void framerender_frame(ObFrame *self)
{
    if (frame_iconify_animating(self)) return;
    if (!self->need_render)            return;
    if (!self->visible)                return;
    self->need_render = FALSE;

    /* Oculta subwindows legadas — Cairo pinta na title window diretamente */
    hide_legacy_subwindows(self);

    /* Titlebar Cairo */
    if (self->decorations & OB_FRAME_DECOR_TITLEBAR)
        render_titlebar(self);

    /* Bordas X11 */
    render_borders(self);

    /* Handle invisível mas funcional para resize */
    if (self->decorations & OB_FRAME_DECOR_HANDLE) {
        XSetWindowBackground(obt_display, self->handle, 0x121212);
        XClearWindow(obt_display, self->handle);
    }

    XFlush(obt_display);
}

/* Nada a liberar — Cairo não usa pool de cores */
void sofia_colors_free(void) { }
