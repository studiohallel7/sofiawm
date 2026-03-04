/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-
 *
 * framerender.c — SofiaWM Decorator
 * Visual fiel ao Sophia Gallery (interface.py)
 *
 * Paleta:
 *   BG focused    #181818   BG unfocused  #121212
 *   Accent        #D35400   Hover close   #C42B1C
 *   Hover btn     #2A2A2A   Press btn     #333333
 *   Separator     #222222   Border        #1E1E1E
 *   Text focused  #CCCCCC   Text unfocus  #666666
 *   Action icons  #777777   Action hover  #FFFFFF
 *
 * NOTA: Botões de ação (🗁 ✎ ↻) estão preparados mas aguardam
 * adição dos campos action_* em frame.h + frame.c (Fase 2).
 */

/* framerender.h DEVE vir primeiro — define as macros SOFIA_* */
#include "framerender.h"
#include "frame.h"
#include "openbox.h"
#include "screen.h"
#include "client.h"
#include "obrender/theme.h"

/* =========================================================
 * PALETA DE CORES — alocada UMA vez, liberada no shutdown
 * ========================================================= */
typedef struct {
    RrColor *bg_focused;
    RrColor *bg_unfocused;
    RrColor *accent;
    RrColor *hover_close;
    RrColor *hover_btn;
    RrColor *press_btn;
    RrColor *separator;
    RrColor *border;
    RrColor *text_focused;
    RrColor *text_unfocused;
    RrColor *text_action;
    RrColor *text_action_hover;
    gboolean initialized;
} SofiaColors;

static SofiaColors sofia_colors = { 0 };

static void sofia_colors_init(void)
{
    if (sofia_colors.initialized) return;
    sofia_colors.bg_focused        = RrColorParse(ob_rr_inst, "#181818");
    sofia_colors.bg_unfocused      = RrColorParse(ob_rr_inst, "#121212");
    sofia_colors.accent            = RrColorParse(ob_rr_inst, "#D35400");
    sofia_colors.hover_close       = RrColorParse(ob_rr_inst, "#C42B1C");
    sofia_colors.hover_btn         = RrColorParse(ob_rr_inst, "#2A2A2A");
    sofia_colors.press_btn         = RrColorParse(ob_rr_inst, "#333333");
    sofia_colors.separator         = RrColorParse(ob_rr_inst, "#222222");
    sofia_colors.border            = RrColorParse(ob_rr_inst, "#1E1E1E");
    sofia_colors.text_focused      = RrColorParse(ob_rr_inst, "#CCCCCC");
    sofia_colors.text_unfocused    = RrColorParse(ob_rr_inst, "#666666");
    sofia_colors.text_action       = RrColorParse(ob_rr_inst, "#777777");
    sofia_colors.text_action_hover = RrColorParse(ob_rr_inst, "#FFFFFF");
    sofia_colors.initialized = TRUE;
}

void sofia_colors_free(void)
{
    if (!sofia_colors.initialized) return;
    RrColorFree(sofia_colors.bg_focused);
    RrColorFree(sofia_colors.bg_unfocused);
    RrColorFree(sofia_colors.accent);
    RrColorFree(sofia_colors.hover_close);
    RrColorFree(sofia_colors.hover_btn);
    RrColorFree(sofia_colors.press_btn);
    RrColorFree(sofia_colors.separator);
    RrColorFree(sofia_colors.border);
    RrColorFree(sofia_colors.text_focused);
    RrColorFree(sofia_colors.text_unfocused);
    RrColorFree(sofia_colors.text_action);
    RrColorFree(sofia_colors.text_action_hover);
    sofia_colors.initialized = FALSE;
}

/* =========================================================
 * HELPERS INLINE
 * ========================================================= */
static inline void sofia_set_solid(RrAppearance *a, RrColor *color)
{
    a->surface.grad    = RR_SURFACE_SOLID;
    a->surface.primary = color;
}

static inline void sofia_set_text(RrAppearance *a,
                                   const gchar  *str,
                                   RrColor      *color)
{
    a->texture[0].type                      = RR_TEXTURE_TEXT;
    a->texture[0].data.text.string          = (gchar *)str;
    a->texture[0].data.text.color           = color;
    a->texture[0].data.text.justify         = RR_JUSTIFY_CENTER;
    a->texture[0].data.text.shadow_color    = NULL;
    a->texture[0].data.text.shadow_alpha    = 0;
    a->texture[0].data.text.shadow_offset_x = 0;
    a->texture[0].data.text.shadow_offset_y = 0;
}

/* =========================================================
 * PROTÓTIPOS
 * ========================================================= */
static void framerender_label(ObFrame *self, RrAppearance *a);
static void framerender_close(ObFrame *self, RrAppearance *a);
static void framerender_max(ObFrame *self, RrAppearance *a);
static void framerender_iconify(ObFrame *self, RrAppearance *a);
static void framerender_hide_unused(ObFrame *self);

/* =========================================================
 * FUNÇÃO PRINCIPAL
 * ========================================================= */
void framerender_frame(ObFrame *self)
{
    if (frame_iconify_animating(self)) return;
    if (!self->need_render)            return;
    if (!self->visible)                return;
    self->need_render = FALSE;

    sofia_colors_init();

    /* --------------------------------------------------
     * 1. BORDAS E ÁREAS INTERNAS
     * -------------------------------------------------- */
    {
        gulong px_bg     = RrColorPixel(sofia_colors.bg_unfocused);
        gulong px_border = RrColorPixel(sofia_colors.border);
        gulong px_bottom = self->focused
            ? RrColorPixel(sofia_colors.accent)      /* #D35400 focused */
            : RrColorPixel(sofia_colors.separator);  /* #222222 unfocused */

        XSetWindowBackground(obt_display, self->backback,    px_bg);     XClearWindow(obt_display, self->backback);
        XSetWindowBackground(obt_display, self->innerleft,   px_bg);     XClearWindow(obt_display, self->innerleft);
        XSetWindowBackground(obt_display, self->innerright,  px_bg);     XClearWindow(obt_display, self->innerright);
        XSetWindowBackground(obt_display, self->innerbottom, px_bg);     XClearWindow(obt_display, self->innerbottom);
        XSetWindowBackground(obt_display, self->left,        px_border); XClearWindow(obt_display, self->left);
        XSetWindowBackground(obt_display, self->right,       px_border); XClearWindow(obt_display, self->right);
        XSetWindowBackground(obt_display, self->titletop,    px_border); XClearWindow(obt_display, self->titletop);
        XSetWindowBackground(obt_display, self->titlebottom, px_bottom); XClearWindow(obt_display, self->titlebottom);
    }

    /* --------------------------------------------------
     * 2. TITLEBAR
     * -------------------------------------------------- */
    if (self->decorations & OB_FRAME_DECOR_TITLEBAR)
    {
        RrAppearance *t, *l, *m, *ic, *d, *s, *c;

        if (self->focused) {
            t  = ob_rr_theme->a_focused_title;
            l  = ob_rr_theme->a_focused_label;
            c  = (!(self->decorations & OB_FRAME_DECOR_CLOSE)
                  ? ob_rr_theme->btn_close->a_focused_disabled
                  : self->close_press ? ob_rr_theme->btn_close->a_focused_pressed
                  : self->close_hover ? ob_rr_theme->btn_close->a_focused_hover
                  :                     ob_rr_theme->btn_close->a_focused_unpressed);
            m  = (!(self->decorations & OB_FRAME_DECOR_MAXIMIZE)
                  ? ob_rr_theme->btn_max->a_focused_disabled
                  : (self->client->max_vert || self->client->max_horz)
                    ? (self->max_press  ? ob_rr_theme->btn_max->a_focused_pressed_toggled
                       : self->max_hover? ob_rr_theme->btn_max->a_focused_hover_toggled
                       :                  ob_rr_theme->btn_max->a_focused_unpressed_toggled)
                    : (self->max_press  ? ob_rr_theme->btn_max->a_focused_pressed
                       : self->max_hover? ob_rr_theme->btn_max->a_focused_hover
                       :                  ob_rr_theme->btn_max->a_focused_unpressed));
            ic = (!(self->decorations & OB_FRAME_DECOR_ICONIFY)
                  ? ob_rr_theme->btn_iconify->a_focused_disabled
                  : self->iconify_press ? ob_rr_theme->btn_iconify->a_focused_pressed
                  : self->iconify_hover ? ob_rr_theme->btn_iconify->a_focused_hover
                  :                       ob_rr_theme->btn_iconify->a_focused_unpressed);
            d  = (!(self->decorations & OB_FRAME_DECOR_ALLDESKTOPS)
                  ? ob_rr_theme->btn_desk->a_focused_disabled
                  : (self->client->desktop == DESKTOP_ALL)
                    ? (self->desk_press  ? ob_rr_theme->btn_desk->a_focused_pressed_toggled
                       : self->desk_hover? ob_rr_theme->btn_desk->a_focused_hover_toggled
                       :                   ob_rr_theme->btn_desk->a_focused_unpressed_toggled)
                    : (self->desk_press  ? ob_rr_theme->btn_desk->a_focused_pressed
                       : self->desk_hover? ob_rr_theme->btn_desk->a_focused_hover
                       :                   ob_rr_theme->btn_desk->a_focused_unpressed));
            s  = (!(self->decorations & OB_FRAME_DECOR_SHADE)
                  ? ob_rr_theme->btn_shade->a_focused_disabled
                  : self->client->shaded
                    ? (self->shade_press  ? ob_rr_theme->btn_shade->a_focused_pressed_toggled
                       : self->shade_hover? ob_rr_theme->btn_shade->a_focused_hover_toggled
                       :                    ob_rr_theme->btn_shade->a_focused_unpressed_toggled)
                    : (self->shade_press  ? ob_rr_theme->btn_shade->a_focused_pressed
                       : self->shade_hover? ob_rr_theme->btn_shade->a_focused_hover
                       :                    ob_rr_theme->btn_shade->a_focused_unpressed));
        } else {
            t  = ob_rr_theme->a_unfocused_title;
            l  = ob_rr_theme->a_unfocused_label;
            c  = (!(self->decorations & OB_FRAME_DECOR_CLOSE)
                  ? ob_rr_theme->btn_close->a_unfocused_disabled
                  : self->close_press ? ob_rr_theme->btn_close->a_unfocused_pressed
                  : self->close_hover ? ob_rr_theme->btn_close->a_unfocused_hover
                  :                     ob_rr_theme->btn_close->a_unfocused_unpressed);
            m  = (!(self->decorations & OB_FRAME_DECOR_MAXIMIZE)
                  ? ob_rr_theme->btn_max->a_unfocused_disabled
                  : (self->client->max_vert || self->client->max_horz)
                    ? (self->max_press  ? ob_rr_theme->btn_max->a_unfocused_pressed_toggled
                       : self->max_hover? ob_rr_theme->btn_max->a_unfocused_hover_toggled
                       :                  ob_rr_theme->btn_max->a_unfocused_unpressed_toggled)
                    : (self->max_press  ? ob_rr_theme->btn_max->a_unfocused_pressed
                       : self->max_hover? ob_rr_theme->btn_max->a_unfocused_hover
                       :                  ob_rr_theme->btn_max->a_unfocused_unpressed));
            ic = (!(self->decorations & OB_FRAME_DECOR_ICONIFY)
                  ? ob_rr_theme->btn_iconify->a_unfocused_disabled
                  : self->iconify_press ? ob_rr_theme->btn_iconify->a_unfocused_pressed
                  : self->iconify_hover ? ob_rr_theme->btn_iconify->a_unfocused_hover
                  :                       ob_rr_theme->btn_iconify->a_unfocused_unpressed);
            d  = (!(self->decorations & OB_FRAME_DECOR_ALLDESKTOPS)
                  ? ob_rr_theme->btn_desk->a_unfocused_disabled
                  : (self->client->desktop == DESKTOP_ALL)
                    ? (self->desk_press  ? ob_rr_theme->btn_desk->a_unfocused_pressed_toggled
                       : self->desk_hover? ob_rr_theme->btn_desk->a_unfocused_hover_toggled
                       :                   ob_rr_theme->btn_desk->a_unfocused_unpressed_toggled)
                    : (self->desk_press  ? ob_rr_theme->btn_desk->a_unfocused_pressed
                       : self->desk_hover? ob_rr_theme->btn_desk->a_unfocused_hover
                       :                   ob_rr_theme->btn_desk->a_unfocused_unpressed));
            s  = (!(self->decorations & OB_FRAME_DECOR_SHADE)
                  ? ob_rr_theme->btn_shade->a_unfocused_disabled
                  : self->client->shaded
                    ? (self->shade_press  ? ob_rr_theme->btn_shade->a_unfocused_pressed_toggled
                       : self->shade_hover? ob_rr_theme->btn_shade->a_unfocused_hover_toggled
                       :                    ob_rr_theme->btn_shade->a_unfocused_unpressed_toggled)
                    : (self->shade_press  ? ob_rr_theme->btn_shade->a_unfocused_pressed
                       : self->shade_hover? ob_rr_theme->btn_shade->a_unfocused_hover
                       :                    ob_rr_theme->btn_shade->a_unfocused_unpressed));
        }

        /* Silencia warning de variável não usada (desk/shade são ocultados) */
        (void)d; (void)s;

        /* 2a. Fundo flat da titlebar */
        sofia_set_solid(t, self->focused ? sofia_colors.bg_focused
                                         : sofia_colors.bg_unfocused);
        RrPaint(t, self->title, self->width, ob_rr_theme->title_height);

        /* 2b. Botões de janela — cada um com seu símbolo e cor */
        framerender_close(self, c);
        framerender_max(self, m);
        framerender_iconify(self, ic);

        /* 2c. Label */
        l->surface.parent  = t;
        l->surface.parentx = self->label_x;
        l->surface.parenty = 0;
        framerender_label(self, l);

        /* 2d. Ocultar desk / shade / icon */
        framerender_hide_unused(self);
    }

    /* --------------------------------------------------
     * 3. HANDLE + GRIPS
     * -------------------------------------------------- */
    if (self->decorations & OB_FRAME_DECOR_HANDLE &&
        ob_rr_theme->handle_height > 0)
    {
        RrAppearance *h = self->focused
            ? ob_rr_theme->a_focused_handle
            : ob_rr_theme->a_unfocused_handle;

        sofia_set_solid(h, sofia_colors.bg_unfocused);
        RrPaint(h, self->handle, self->width, ob_rr_theme->handle_height);

        if (self->decorations & OB_FRAME_DECOR_GRIPS) {
            RrAppearance *g = self->focused
                ? ob_rr_theme->a_focused_grip
                : ob_rr_theme->a_unfocused_grip;

            if (g->surface.grad == RR_SURFACE_PARENTREL)
                g->surface.parent = h;
            else
                sofia_set_solid(g, sofia_colors.border);

            g->surface.parentx = 0;
            g->surface.parenty = 0;
            RrPaint(g, self->lgrip,
                    ob_rr_theme->grip_width, ob_rr_theme->handle_height);

            g->surface.parentx = self->width - ob_rr_theme->grip_width;
            g->surface.parenty = 0;
            RrPaint(g, self->rgrip,
                    ob_rr_theme->grip_width, ob_rr_theme->handle_height);
        }
    }

    XFlush(obt_display);
}

/* =========================================================
 * ELEMENTOS — renderização individual
 * ========================================================= */

static void framerender_label(ObFrame *self, RrAppearance *a)
{
    if (!self->label_on) return;
    sofia_set_text(a, self->client->title,
        self->focused ? sofia_colors.text_focused
                      : sofia_colors.text_unfocused);
    a->texture[0].data.text.font = self->focused
        ? ob_rr_theme->win_font_focused
        : ob_rr_theme->win_font_unfocused;
    RrPaint(a, self->label, self->label_width, ob_rr_theme->title_height);
}

static void framerender_close(ObFrame *self, RrAppearance *a)
{
    if (!self->close_on) return;
    RrColor *bg = self->close_press ? sofia_colors.press_btn
                : self->close_hover ? sofia_colors.hover_close
                : self->focused     ? sofia_colors.bg_focused
                :                     sofia_colors.bg_unfocused;
    RrColor *fg = (self->close_hover || self->close_press)
                ? sofia_colors.text_action_hover : sofia_colors.text_action;
    sofia_set_solid(a, bg);
    sofia_set_text(a, "\xe2\x9c\x95", fg); /* ✕ */
    RrPaint(a, self->close,
            ob_rr_theme->button_size + 2, ob_rr_theme->title_height);
}

static void framerender_max(ObFrame *self, RrAppearance *a)
{
    if (!self->max_on) return;
    gboolean is_max = self->client->max_vert || self->client->max_horz;
    RrColor *bg = self->max_press  ? sofia_colors.press_btn
                : self->max_hover  ? sofia_colors.hover_btn
                : self->focused    ? sofia_colors.bg_focused
                :                    sofia_colors.bg_unfocused;
    RrColor *fg = (self->max_hover || self->max_press)
                ? sofia_colors.text_action_hover : sofia_colors.text_action;
    /* ◻ = \xe2\x97\xbb  |  ❐ = \xe2\x9d\x90 */
    sofia_set_solid(a, bg);
    sofia_set_text(a, is_max ? "\xe2\x9d\x90" : "\xe2\x97\xbb", fg);
    RrPaint(a, self->max,
            ob_rr_theme->button_size + 2, ob_rr_theme->title_height);
}

static void framerender_iconify(ObFrame *self, RrAppearance *a)
{
    if (!self->iconify_on) return;
    RrColor *bg = self->iconify_press ? sofia_colors.press_btn
                : self->iconify_hover ? sofia_colors.hover_btn
                : self->focused       ? sofia_colors.bg_focused
                :                       sofia_colors.bg_unfocused;
    RrColor *fg = (self->iconify_hover || self->iconify_press)
                ? sofia_colors.text_action_hover : sofia_colors.text_action;
    sofia_set_solid(a, bg);
    sofia_set_text(a, "\xe2\x94\x80", fg); /* ─ */
    RrPaint(a, self->iconify,
            ob_rr_theme->button_size + 2, ob_rr_theme->title_height);
}

static void framerender_hide_unused(ObFrame *self)
{
    if (self->desk_on)  XUnmapWindow(obt_display, self->desk);
    if (self->shade_on) XUnmapWindow(obt_display, self->shade);
    if (self->icon_on)  XUnmapWindow(obt_display, self->icon);
}
