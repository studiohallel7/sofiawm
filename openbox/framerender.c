/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-
 *
 * framerender.c — SofiaWM Decorator
 * Reescrito para reproduzir fielmente o visual do Sophia Gallery (interface.py)
 *
 * Paleta de referência (interface.py):
 *   BG focused:      #181818
 *   BG unfocused:    #121212
 *   Accent:          #D35400
 *   Hover close:     #C42B1C  (vermelho)
 *   Hover outros:    #2A2A2A  (cinza escuro)
 *   Press outros:    #333333
 *   Separator:       #222222  (linha inferior titlebar)
 *   Border:          #1E1E1E  (bordas laterais)
 *   Texto focused:   #CCCCCC
 *   Texto unfocused: #666666
 *   Símbolos botões: ✕  ─  ◻
 *   Ícones de ação:  🗁  ✎  ↻  (contextuais da janela ativa)
 *
 * Correções em relação à versão anterior:
 *   - Sem memory leaks: cores alocadas UMA vez em sofia_colors_init()
 *     e liberadas em sofia_colors_free()
 *   - framerender_max/iconify/close agora SÃO chamadas corretamente
 *   - Botões de ação contextuais adicionados (action_open, action_edit, action_rotate)
 *   - 'clear' removido (era resquício não utilizado)
 *   - Separador accent (#D35400) na base da titlebar quando focused
 *
 * Arquivos que precisam de ajuste paralelo:
 *   - frame.h:   adicionar campos Window action_open, action_edit, action_rotate
 *                e gboolean action_*_hover, action_*_press
 *   - frame.c:   criar as X11 Windows dos botões de ação e seus hit-zones
 *   - event.c:   mapear EnterNotify/LeaveNotify/ButtonPress para os novos botões
 */

#include "frame.h"
#include "openbox.h"
#include "screen.h"
#include "client.h"
#include "framerender.h"
#include "obrender/theme.h"

/* =========================================================
 * PALETA DE CORES SOFIA — alocada uma única vez
 * Evita o memory leak de chamar RrColorParse() a cada redraw
 * ========================================================= */
typedef struct {
    RrColor *bg_focused;        /* #181818 */
    RrColor *bg_unfocused;      /* #121212 */
    RrColor *accent;            /* #D35400 */
    RrColor *hover_close;       /* #C42B1C */
    RrColor *hover_btn;         /* #2A2A2A */
    RrColor *press_btn;         /* #333333 */
    RrColor *separator;         /* #222222 */
    RrColor *border;            /* #1E1E1E */
    RrColor *text_focused;      /* #CCCCCC */
    RrColor *text_unfocused;    /* #666666 */
    RrColor *text_action;       /* #777777 — ícones de ação normais */
    RrColor *text_action_hover; /* #FFFFFF — ícones de ação hover */
    gboolean initialized;
} SofiaColors;

static SofiaColors sofia_colors = { 0 };

/* Inicializa as cores uma única vez na primeira chamada */
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

/* Liberar cores — chamar no shutdown do WM (openbox.c ou frame.c cleanup) */
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
 * HELPERS DE DESENHO
 * ========================================================= */

/* Força um RrAppearance para cor sólida plana (sem gradiente) */
static inline void sofia_set_solid(RrAppearance *a, RrColor *color)
{
    a->surface.grad    = RR_SURFACE_SOLID;
    a->surface.primary = color;
}

/* Define a cor e fonte do texto em uma RrAppearance */
static inline void sofia_set_text(RrAppearance *a,
                                   const gchar *str,
                                   RrColor     *color)
{
    a->texture[0].type                   = RR_TEXTURE_TEXT;
    a->texture[0].data.text.string       = str;
    a->texture[0].data.text.color        = color;
    a->texture[0].data.text.justify      = RR_JUSTIFY_CENTER;
    a->texture[0].data.text.shadow_color = NULL; /* sem sombra */
    a->texture[0].data.text.shadow_alpha = 0;
    a->texture[0].data.text.shadow_offset_x = 0;
    a->texture[0].data.text.shadow_offset_y = 0;
}

/* =========================================================
 * DECLARAÇÕES ESTÁTICAS
 * ========================================================= */
static void framerender_label(ObFrame *self, RrAppearance *a);
static void framerender_icon(ObFrame *self);
static void framerender_desk(ObFrame *self);
static void framerender_shade(ObFrame *self);
static void framerender_close(ObFrame *self, RrAppearance *a);
static void framerender_max(ObFrame *self, RrAppearance *a);
static void framerender_iconify(ObFrame *self, RrAppearance *a);
static void framerender_action_buttons(ObFrame *self, RrAppearance *a);
static void framerender_separator(ObFrame *self);

/* =========================================================
 * FUNÇÃO PRINCIPAL
 * ========================================================= */
void framerender_frame(ObFrame *self)
{
    if (frame_iconify_animating(self)) return;
    if (!self->need_render)           return;
    if (!self->visible)               return;
    self->need_render = FALSE;

    /* Garante que as cores estão inicializadas */
    sofia_colors_init();

    /* --------------------------------------------------
     * 1. BORDAS E ÁREAS INTERNAS
     *    Pintadas com XSetWindowBackground — sem RrPaint
     *    pois são X11 Windows simples (sem textura)
     * -------------------------------------------------- */
    {
        gulong px_bg     = RrColorPixel(sofia_colors.bg_unfocused);
        gulong px_border = RrColorPixel(sofia_colors.border);
        gulong px_sep    = RrColorPixel(sofia_colors.separator);

        /* Fundo interno (atrás do cliente) */
        XSetWindowBackground(obt_display, self->backback, px_bg);
        XClearWindow(obt_display,         self->backback);

        /* Faixas internas (innerXxx são o espaço entre borda e cliente) */
        XSetWindowBackground(obt_display, self->innerleft,   px_bg);
        XClearWindow(obt_display,         self->innerleft);
        XSetWindowBackground(obt_display, self->innerright,  px_bg);
        XClearWindow(obt_display,         self->innerright);
        XSetWindowBackground(obt_display, self->innerbottom, px_bg);
        XClearWindow(obt_display,         self->innerbottom);

        /* Bordas externas (1px) */
        XSetWindowBackground(obt_display, self->left,  px_border);
        XClearWindow(obt_display,         self->left);
        XSetWindowBackground(obt_display, self->right, px_border);
        XClearWindow(obt_display,         self->right);

        /* Topo da titlebar (1px acima) */
        XSetWindowBackground(obt_display, self->titletop, px_border);
        XClearWindow(obt_display,         self->titletop);

        /* Linha separadora abaixo da titlebar
         * Quando focused: accent (#D35400) — fiel ao interface.py
         * Quando unfocused: separator (#222222) — discreto */
        gulong px_bottom = self->focused
            ? RrColorPixel(sofia_colors.accent)
            : px_sep;
        XSetWindowBackground(obt_display, self->titlebottom, px_bottom);
        XClearWindow(obt_display,         self->titlebottom);
    }

    /* --------------------------------------------------
     * 2. TITLEBAR
     * -------------------------------------------------- */
    if (self->decorations & OB_FRAME_DECOR_TITLEBAR)
    {
        /* Seleção das RrAppearances conforme estado focus */
        RrAppearance *t, *l, *m, *ic, *d, *s, *c;

        if (self->focused) {
            t  = ob_rr_theme->a_focused_title;
            l  = ob_rr_theme->a_focused_label;
            c  = (!(self->decorations & OB_FRAME_DECOR_CLOSE) ?
                  ob_rr_theme->btn_close->a_focused_disabled :
                  (self->close_press  ? ob_rr_theme->btn_close->a_focused_pressed  :
                  (self->close_hover  ? ob_rr_theme->btn_close->a_focused_hover    :
                                        ob_rr_theme->btn_close->a_focused_unpressed)));
            m  = (!(self->decorations & OB_FRAME_DECOR_MAXIMIZE) ?
                  ob_rr_theme->btn_max->a_focused_disabled :
                  (self->client->max_vert || self->client->max_horz ?
                   (self->max_press   ? ob_rr_theme->btn_max->a_focused_pressed_toggled   :
                   (self->max_hover   ? ob_rr_theme->btn_max->a_focused_hover_toggled     :
                                        ob_rr_theme->btn_max->a_focused_unpressed_toggled)) :
                   (self->max_press   ? ob_rr_theme->btn_max->a_focused_pressed   :
                   (self->max_hover   ? ob_rr_theme->btn_max->a_focused_hover     :
                                        ob_rr_theme->btn_max->a_focused_unpressed))));
            ic = (!(self->decorations & OB_FRAME_DECOR_ICONIFY) ?
                  ob_rr_theme->btn_iconify->a_focused_disabled :
                  (self->iconify_press ? ob_rr_theme->btn_iconify->a_focused_pressed  :
                  (self->iconify_hover ? ob_rr_theme->btn_iconify->a_focused_hover    :
                                         ob_rr_theme->btn_iconify->a_focused_unpressed)));
            d  = (!(self->decorations & OB_FRAME_DECOR_ALLDESKTOPS) ?
                  ob_rr_theme->btn_desk->a_focused_disabled :
                  (self->client->desktop == DESKTOP_ALL ?
                   (self->desk_press  ? ob_rr_theme->btn_desk->a_focused_pressed_toggled   :
                   (self->desk_hover  ? ob_rr_theme->btn_desk->a_focused_hover_toggled     :
                                        ob_rr_theme->btn_desk->a_focused_unpressed_toggled)) :
                   (self->desk_press  ? ob_rr_theme->btn_desk->a_focused_pressed   :
                   (self->desk_hover  ? ob_rr_theme->btn_desk->a_focused_hover     :
                                        ob_rr_theme->btn_desk->a_focused_unpressed))));
            s  = (!(self->decorations & OB_FRAME_DECOR_SHADE) ?
                  ob_rr_theme->btn_shade->a_focused_disabled :
                  (self->client->shaded ?
                   (self->shade_press ? ob_rr_theme->btn_shade->a_focused_pressed_toggled   :
                   (self->shade_hover ? ob_rr_theme->btn_shade->a_focused_hover_toggled     :
                                        ob_rr_theme->btn_shade->a_focused_unpressed_toggled)) :
                   (self->shade_press ? ob_rr_theme->btn_shade->a_focused_pressed   :
                   (self->shade_hover ? ob_rr_theme->btn_shade->a_focused_hover     :
                                        ob_rr_theme->btn_shade->a_focused_unpressed))));
        } else {
            t  = ob_rr_theme->a_unfocused_title;
            l  = ob_rr_theme->a_unfocused_label;
            c  = (!(self->decorations & OB_FRAME_DECOR_CLOSE) ?
                  ob_rr_theme->btn_close->a_unfocused_disabled :
                  (self->close_press  ? ob_rr_theme->btn_close->a_unfocused_pressed  :
                  (self->close_hover  ? ob_rr_theme->btn_close->a_unfocused_hover    :
                                        ob_rr_theme->btn_close->a_unfocused_unpressed)));
            m  = (!(self->decorations & OB_FRAME_DECOR_MAXIMIZE) ?
                  ob_rr_theme->btn_max->a_unfocused_disabled :
                  (self->client->max_vert || self->client->max_horz ?
                   (self->max_press   ? ob_rr_theme->btn_max->a_unfocused_pressed_toggled   :
                   (self->max_hover   ? ob_rr_theme->btn_max->a_unfocused_hover_toggled     :
                                        ob_rr_theme->btn_max->a_unfocused_unpressed_toggled)) :
                   (self->max_press   ? ob_rr_theme->btn_max->a_unfocused_pressed   :
                   (self->max_hover   ? ob_rr_theme->btn_max->a_unfocused_hover     :
                                        ob_rr_theme->btn_max->a_unfocused_unpressed))));
            ic = (!(self->decorations & OB_FRAME_DECOR_ICONIFY) ?
                  ob_rr_theme->btn_iconify->a_unfocused_disabled :
                  (self->iconify_press ? ob_rr_theme->btn_iconify->a_unfocused_pressed  :
                  (self->iconify_hover ? ob_rr_theme->btn_iconify->a_unfocused_hover    :
                                         ob_rr_theme->btn_iconify->a_unfocused_unpressed)));
            d  = (!(self->decorations & OB_FRAME_DECOR_ALLDESKTOPS) ?
                  ob_rr_theme->btn_desk->a_unfocused_disabled :
                  (self->client->desktop == DESKTOP_ALL ?
                   (self->desk_press  ? ob_rr_theme->btn_desk->a_unfocused_pressed_toggled   :
                   (self->desk_hover  ? ob_rr_theme->btn_desk->a_unfocused_hover_toggled     :
                                        ob_rr_theme->btn_desk->a_unfocused_unpressed_toggled)) :
                   (self->desk_press  ? ob_rr_theme->btn_desk->a_unfocused_pressed   :
                   (self->desk_hover  ? ob_rr_theme->btn_desk->a_unfocused_hover     :
                                        ob_rr_theme->btn_desk->a_unfocused_unpressed))));
            s  = (!(self->decorations & OB_FRAME_DECOR_SHADE) ?
                  ob_rr_theme->btn_shade->a_unfocused_disabled :
                  (self->client->shaded ?
                   (self->shade_press ? ob_rr_theme->btn_shade->a_unfocused_pressed_toggled   :
                   (self->shade_hover ? ob_rr_theme->btn_shade->a_unfocused_hover_toggled     :
                                        ob_rr_theme->btn_shade->a_unfocused_unpressed_toggled)) :
                   (self->shade_press ? ob_rr_theme->btn_shade->a_unfocused_pressed   :
                   (self->shade_hover ? ob_rr_theme->btn_shade->a_unfocused_hover     :
                                        ob_rr_theme->btn_shade->a_unfocused_unpressed))));
        }

        /* ---- 2a. FUNDO DA TITLEBAR (40px, flat) ---- */
        sofia_set_solid(t, self->focused ? sofia_colors.bg_focused
                                         : sofia_colors.bg_unfocused);
        RrPaint(t, self->title, self->width, SOFIA_TITLEBAR_HEIGHT);

        /* ---- 2b. BOTÃO FECHAR ✕ ---- */
        framerender_close(self, c);

        /* ---- 2c. BOTÃO MAXIMIZAR ◻ ---- */
        framerender_max(self, m);

        /* ---- 2d. BOTÃO MINIMIZAR ─ ---- */
        framerender_iconify(self, ic);

        /* ---- 2e. LABEL (título da janela) ---- */
        l->surface.parent  = t;
        l->surface.parentx = self->label_x;
        l->surface.parenty = 0;
        framerender_label(self, l);

        /* ---- 2f. BOTÕES DE AÇÃO CONTEXTUAIS (🗁 ✎ ↻) ---- */
        /* Visíveis apenas na janela ativa (focused) */
        framerender_action_buttons(self, t);

        /* ---- 2g. OCULTAR elementos Openbox não usados ---- */
        framerender_icon(self);
        framerender_desk(self);
        framerender_shade(self);
    }

    /* --------------------------------------------------
     * 3. HANDLE + GRIPS (barra inferior de resize)
     *    Mantida funcional, estilizada para flat
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

            /* grip herda do handle se for PARENTREL */
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
 * FUNÇÕES DE RENDERIZAÇÃO DE ELEMENTOS
 * ========================================================= */

/* ---- LABEL (título da janela) ----
 * Posicionado pelo frame.c via self->label_x
 * Texto centralizado entre os botões de ação e os botões de janela
 */
static void framerender_label(ObFrame *self, RrAppearance *a)
{
    if (!self->label_on) return;

    RrColor *text_color = self->focused
        ? sofia_colors.text_focused
        : sofia_colors.text_unfocused;

    sofia_set_text(a, self->client->title, text_color);

    /* Override fonte para Inter/Segoe UI se disponível — font size 13 */
    /* Nota: a fonte é controlada pelo themerc; aqui apenas garantimos
     * que a cor está correta independente do tema carregado */
    a->texture[0].data.text.font = ob_rr_theme->win_font_focused;

    RrPaint(a, self->label, self->label_width, SOFIA_TITLEBAR_HEIGHT);
}

/* ---- BOTÃO FECHAR ✕ ----
 * Hover: vermelho #C42B1C
 * Press: vermelho mais escuro (via press_btn como fallback)
 * Normal: invisível (fundo da titlebar)
 */
static void framerender_close(ObFrame *self, RrAppearance *a)
{
    if (!self->close_on) return;

    RrColor *bg;
    if (self->close_press)      bg = sofia_colors.press_btn;
    else if (self->close_hover) bg = sofia_colors.hover_close;
    else                        bg = self->focused
                                        ? sofia_colors.bg_focused
                                        : sofia_colors.bg_unfocused;

    sofia_set_solid(a, bg);
    sofia_set_text(a, "✕",
        (self->close_hover || self->close_press)
            ? sofia_colors.text_action_hover
            : sofia_colors.text_action);

    RrPaint(a, self->close, SOFIA_BTN_W, SOFIA_TITLEBAR_HEIGHT);
}

/* ---- BOTÃO MAXIMIZAR ◻ / ❐ ----
 * ◻ = janela normal  |  ❐ = janela maximizada (toggled)
 */
static void framerender_max(ObFrame *self, RrAppearance *a)
{
    if (!self->max_on) return;

    gboolean is_max = self->client->max_vert || self->client->max_horz;
    const gchar *symbol = is_max ? "❐" : "◻";

    RrColor *bg;
    if (self->max_press)      bg = sofia_colors.press_btn;
    else if (self->max_hover) bg = sofia_colors.hover_btn;
    else                      bg = self->focused
                                       ? sofia_colors.bg_focused
                                       : sofia_colors.bg_unfocused;

    sofia_set_solid(a, bg);
    sofia_set_text(a, symbol,
        (self->max_hover || self->max_press)
            ? sofia_colors.text_action_hover
            : sofia_colors.text_action);

    RrPaint(a, self->max, SOFIA_BTN_W, SOFIA_TITLEBAR_HEIGHT);
}

/* ---- BOTÃO MINIMIZAR ─ ---- */
static void framerender_iconify(ObFrame *self, RrAppearance *a)
{
    if (!self->iconify_on) return;

    RrColor *bg;
    if (self->iconify_press)      bg = sofia_colors.press_btn;
    else if (self->iconify_hover) bg = sofia_colors.hover_btn;
    else                          bg = self->focused
                                           ? sofia_colors.bg_focused
                                           : sofia_colors.bg_unfocused;

    sofia_set_solid(a, bg);
    sofia_set_text(a, "─",
        (self->iconify_hover || self->iconify_press)
            ? sofia_colors.text_action_hover
            : sofia_colors.text_action);

    RrPaint(a, self->iconify, SOFIA_BTN_W, SOFIA_TITLEBAR_HEIGHT);
}

/* ---- BOTÕES DE AÇÃO CONTEXTUAIS (🗁 ✎ ↻) ----
 *
 * Renderizados APENAS quando a janela está focused,
 * posicionados à esquerda dos botões de janela, após um separador vertical.
 *
 * Layout (esquerda → direita):
 *   [✕][─][◻] | sep | [🗁][✎][↻] | ............título............ |
 *
 * Os campos Window self->action_open / action_edit / action_rotate
 * e os flags hover/press correspondentes devem ser declarados em frame.h
 * e criados em frame.c (XCreateWindow com event mask adequada).
 *
 * Ação semântica (tratada em event.c):
 *   action_open   (🗁) → OB_USER_ACTION_OPEN   → abre file manager na pasta do app
 *   action_edit   (✎) → OB_USER_ACTION_EDIT   → abre editor com o processo do cliente
 *   action_rotate (↻) → OB_USER_ACTION_ROTATE → rotaciona/reposiciona janela (snap)
 */
static void framerender_action_buttons(ObFrame *self, RrAppearance *base)
{
    /* Somente na janela focused — igual ao interface.py que mostra
     * os ícones de ação somente quando a janela está ativa */
    if (!self->focused) {
        /* Oculta os botões de ação quando unfocused */
        if (self->action_open)   XUnmapWindow(obt_display, self->action_open);
        if (self->action_edit)   XUnmapWindow(obt_display, self->action_edit);
        if (self->action_rotate) XUnmapWindow(obt_display, self->action_rotate);
        return;
    }

    /* Garante que estão visíveis */
    if (self->action_open)   XMapWindow(obt_display, self->action_open);
    if (self->action_edit)   XMapWindow(obt_display, self->action_edit);
    if (self->action_rotate) XMapWindow(obt_display, self->action_rotate);

    /* Estrutura dos 3 botões de ação */
    struct {
        Window      win;
        const char *symbol;
        gboolean    hover;
        gboolean    press;
    } actions[] = {
        { self->action_open,   "🗁", self->action_open_hover,   self->action_open_press   },
        { self->action_edit,   "✎", self->action_edit_hover,   self->action_edit_press   },
        { self->action_rotate, "↻", self->action_rotate_hover, self->action_rotate_press },
    };

    for (int k = 0; k < 3; k++) {
        if (!actions[k].win) continue;

        RrColor *bg;
        if (actions[k].press)      bg = sofia_colors.press_btn;
        else if (actions[k].hover) bg = sofia_colors.hover_btn;
        else                       bg = sofia_colors.bg_focused;

        sofia_set_solid(base, bg);
        sofia_set_text(base, actions[k].symbol,
            (actions[k].hover || actions[k].press)
                ? sofia_colors.text_action_hover
                : sofia_colors.text_action);

        RrPaint(base, actions[k].win, SOFIA_BTN_W, SOFIA_TITLEBAR_HEIGHT);
    }
}

/* ---- ELEMENTOS OPENBOX NÃO USADOS NO SOFIA ----
 * Ocultados via XUnmapWindow para não ocupar espaço
 * mas mantidos na struct para compatibilidade com frame.c
 */
static void framerender_icon(ObFrame *self)
{
    if (self->icon_on)
        XUnmapWindow(obt_display, self->icon);
}

static void framerender_desk(ObFrame *self)
{
    if (self->desk_on)
        XUnmapWindow(obt_display, self->desk);
}

static void framerender_shade(ObFrame *self)
{
    if (self->shade_on)
        XUnmapWindow(obt_display, self->shade);
}
