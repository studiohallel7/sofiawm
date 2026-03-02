/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   framerender.c for the Openbox window manager
   Copyright (c) 2006        Mikael Magnusson
   Copyright (c) 2003-2007   Dana Jansens

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   See the COPYING file for a copy of the GNU General Public License.
*/

#include "frame.h"
#include "openbox.h"
#include "screen.h"
#include "client.h"
#include "framerender.h"
#include "obrender/theme.h"

static void framerender_label(ObFrame *self, RrAppearance *a);
static void framerender_icon(ObFrame *self, RrAppearance *a);
static void framerender_max(ObFrame *self, RrAppearance *a);
static void framerender_iconify(ObFrame *self, RrAppearance *a);
static void framerender_desk(ObFrame *self, RrAppearance *a);
static void framerender_shade(ObFrame *self, RrAppearance *a);
static void framerender_close(ObFrame *self, RrAppearance *a);

void framerender_frame(ObFrame *self)
{
    if (frame_iconify_animating(self))
        return; /* delay redrawing until the animation is done */
    if (!self->need_render)
        return;
    if (!self->visible)
        return;
    self->need_render = FALSE;

    /* 1. Cores da Borda e Áreas Internas */
    {
        /* SofiaWM: Cores exatas do MesaSuite */
        gulong bg_inner = RrColorPixel(RrColorParse(ob_rr_inst, "#121212"));
        gulong border_fix = RrColorPixel(RrColorParse(ob_rr_inst, "#1E1E1E"));
        gulong separator_fix = RrColorPixel(RrColorParse(ob_rr_inst, "#222222")); /* Cor da linha inferior */

        /* Pintar o fundo das bordas internas e grips */
        XSetWindowBackground(obt_display, self->backback, bg_inner);
        XClearWindow(obt_display, self->backback);
        XSetWindowBackground(obt_display, self->innerleft, bg_inner);
        XClearWindow(obt_display, self->innerleft);
        XSetWindowBackground(obt_display, self->innerright, bg_inner);
        XClearWindow(obt_display, self->innerright);
        XSetWindowBackground(obt_display, self->innerbottom, bg_inner);
        XClearWindow(obt_display, self->innerbottom);

        /* A borda de 1px que definimos no frame.c */
        XSetWindowBackground(obt_display, self->left, border_fix);
        XClearWindow(obt_display, self->left);
        XSetWindowBackground(obt_display, self->right, border_fix);
        XClearWindow(obt_display, self->right);
        XSetWindowBackground(obt_display, self->titletop, border_fix);
        XClearWindow(obt_display, self->titletop);

        /* Aplica a cor exata da linha inferior da barra de título do interface.py */
        XSetWindowBackground(obt_display, self->titlebottom, separator_fix);
        XClearWindow(obt_display, self->titlebottom);
    }

    if (self->decorations & OB_FRAME_DECOR_TITLEBAR) {
        RrAppearance *t, *l, *m, *n, *i, *d, *s, *c, *clear;
        if (self->focused) {
            t = ob_rr_theme->a_focused_title;
            l = ob_rr_theme->a_focused_label;
            m = (!(self->decorations & OB_FRAME_DECOR_MAXIMIZE) ?
                 ob_rr_theme->btn_max->a_focused_disabled :
                 (self->client->max_vert || self->client->max_horz ?
                  (self->max_press ?
                   ob_rr_theme->btn_max->a_focused_pressed_toggled :
                   (self->max_hover ?
                    ob_rr_theme->btn_max->a_focused_hover_toggled :
                    ob_rr_theme->btn_max->a_focused_unpressed_toggled)) :
                  (self->max_press ?
                   ob_rr_theme->btn_max->a_focused_pressed :
                   (self->max_hover ?
                    ob_rr_theme->btn_max->a_focused_hover :
                    ob_rr_theme->btn_max->a_focused_unpressed))));
            n = ob_rr_theme->a_icon;
            i = (!(self->decorations & OB_FRAME_DECOR_ICONIFY) ?
                 ob_rr_theme->btn_iconify->a_focused_disabled :
                 (self->iconify_press ?
                  ob_rr_theme->btn_iconify->a_focused_pressed :
                  (self->iconify_hover ?
                   ob_rr_theme->btn_iconify->a_focused_hover :
                   ob_rr_theme->btn_iconify->a_focused_unpressed)));
            d = (!(self->decorations & OB_FRAME_DECOR_ALLDESKTOPS) ?
                 ob_rr_theme->btn_desk->a_focused_disabled :
                 (self->client->desktop == DESKTOP_ALL ?
                  (self->desk_press ?
                   ob_rr_theme->btn_desk->a_focused_pressed_toggled :
                   (self->desk_hover ?
                    ob_rr_theme->btn_desk->a_focused_hover_toggled :
                    ob_rr_theme->btn_desk->a_focused_unpressed_toggled)) :
                  (self->desk_press ?
                   ob_rr_theme->btn_desk->a_focused_pressed :
                   (self->desk_hover ?
                    ob_rr_theme->btn_desk->a_focused_hover :
                    ob_rr_theme->btn_desk->a_focused_unpressed))));
            s = (!(self->decorations & OB_FRAME_DECOR_SHADE) ?
                 ob_rr_theme->btn_shade->a_focused_disabled :
                 (self->client->shaded ?
                  (self->shade_press ?
                   ob_rr_theme->btn_shade->a_focused_pressed_toggled :
                   (self->shade_hover ?
                    ob_rr_theme->btn_shade->a_focused_hover_toggled :
                    ob_rr_theme->btn_shade->a_focused_unpressed_toggled)) :
                  (self->shade_press ?
                   ob_rr_theme->btn_shade->a_focused_pressed :
                   (self->shade_hover ?
                    ob_rr_theme->btn_shade->a_focused_hover :
                    ob_rr_theme->btn_shade->a_focused_unpressed))));
            c = (!(self->decorations & OB_FRAME_DECOR_CLOSE) ?
                 ob_rr_theme->btn_close->a_focused_disabled :
                 (self->close_press ?
                  ob_rr_theme->btn_close->a_focused_pressed :
                  (self->close_hover ?
                   ob_rr_theme->btn_close->a_focused_hover :
                   ob_rr_theme->btn_close->a_focused_unpressed)));
        } else {
            t = ob_rr_theme->a_unfocused_title;
            l = ob_rr_theme->a_unfocused_label;
            m = (!(self->decorations & OB_FRAME_DECOR_MAXIMIZE) ?
                 ob_rr_theme->btn_max->a_unfocused_disabled :
                 (self->client->max_vert || self->client->max_horz ?
                  (self->max_press ?
                   ob_rr_theme->btn_max->a_unfocused_pressed_toggled :
                   (self->max_hover ?
                    ob_rr_theme->btn_max->a_unfocused_hover_toggled :
                    ob_rr_theme->btn_max->a_unfocused_unpressed_toggled)) :
                  (self->max_press ?
                   ob_rr_theme->btn_max->a_unfocused_pressed :
                   (self->max_hover ?
                    ob_rr_theme->btn_max->a_unfocused_hover :
                    ob_rr_theme->btn_max->a_unfocused_unpressed))));
            n = ob_rr_theme->a_icon;
            i = (!(self->decorations & OB_FRAME_DECOR_ICONIFY) ?
                 ob_rr_theme->btn_iconify->a_unfocused_disabled :
                 (self->iconify_press ?
                  ob_rr_theme->btn_iconify->a_unfocused_pressed :
                  (self->iconify_hover ?
                   ob_rr_theme->btn_iconify->a_unfocused_hover :
                   ob_rr_theme->btn_iconify->a_unfocused_unpressed)));
            d = (!(self->decorations & OB_FRAME_DECOR_ALLDESKTOPS) ?
                 ob_rr_theme->btn_desk->a_unfocused_disabled :
                 (self->client->desktop == DESKTOP_ALL ?
                  (self->desk_press ?
                   ob_rr_theme->btn_desk->a_unfocused_pressed_toggled :
                   (self->desk_hover ?
                    ob_rr_theme->btn_desk->a_unfocused_hover_toggled :
                    ob_rr_theme->btn_desk->a_unfocused_unpressed_toggled)) :
                  (self->desk_press ?
                   ob_rr_theme->btn_desk->a_unfocused_pressed :
                   (self->desk_hover ?
                    ob_rr_theme->btn_desk->a_unfocused_hover :
                    ob_rr_theme->btn_desk->a_unfocused_unpressed))));
            s = (!(self->decorations & OB_FRAME_DECOR_SHADE) ?
                 ob_rr_theme->btn_shade->a_unfocused_disabled :
                 (self->client->shaded ?
                  (self->shade_press ?
                   ob_rr_theme->btn_shade->a_unfocused_pressed_toggled :
                   (self->shade_hover ?
                    ob_rr_theme->btn_shade->a_unfocused_hover_toggled :
                    ob_rr_theme->btn_shade->a_unfocused_unpressed_toggled)) :
                  (self->shade_press ?
                   ob_rr_theme->btn_shade->a_unfocused_pressed :
                   (self->shade_hover ?
                    ob_rr_theme->btn_shade->a_unfocused_hover :
                    ob_rr_theme->btn_shade->a_unfocused_unpressed))));
            c = (!(self->decorations & OB_FRAME_DECOR_CLOSE) ?
                 ob_rr_theme->btn_close->a_unfocused_disabled :
                 (self->close_press ?
                  ob_rr_theme->btn_close->a_unfocused_pressed :
                  (self->close_hover ?
                   ob_rr_theme->btn_close->a_unfocused_hover :
                   ob_rr_theme->btn_close->a_unfocused_unpressed)));
        }
        clear = ob_rr_theme->a_clear;

        /* 2. Pintura da Barra de Título e Botões */
        /* SofiaWM: Forçar estilo plano (Flat) na barra */
        t->surface.grad = RR_SURFACE_SOLID;
        t->surface.primary = self->focused ? 
            RrColorParse(ob_rr_inst, "#181818") : RrColorParse(ob_rr_inst, "#121212");
        
        /* Pintar o fundo da barra */
        RrPaint(t, self->title, self->width, 40);

        /* Botão Fechar: Hover Vermelho #C42B1C */
        c->surface.grad = RR_SURFACE_SOLID;
        c->surface.primary = self->close_hover ? 
            RrColorParse(ob_rr_inst, "#C42B1C") : RrColorParse(ob_rr_inst, "#181818");
        
        /* Botões Maximizar e Minimizar: Hover Cinza Escuro */
        m->surface.grad = i->surface.grad = RR_SURFACE_SOLID;
        m->surface.primary = self->max_hover ? 
            RrColorParse(ob_rr_inst, "#333333") : RrColorParse(ob_rr_inst, "#181818");
        i->surface.primary = self->iconify_hover ? 
            RrColorParse(ob_rr_inst, "#333333") : RrColorParse(ob_rr_inst, "#181818");

        /* Aplicar a pintura nos botões com os tamanhos que definimos */
        RrPaint(c, self->close, 32, 30);
        RrPaint(m, self->max, 32, 30);
        RrPaint(i, self->iconify, 32, 30);

        /* Pintar o Título (Label) no canto direito conforme o layout_title */
        l->surface.parent = t;
        l->surface.parentx = self->label_x;
        l->surface.parenty = 0;
        framerender_label(self, l);

        /* Oculta os elementos restantes não utilizados pelo layout nativo */
        framerender_icon(self, n);
        framerender_desk(self, d);
        framerender_shade(self, s);
    }

    if (self->decorations & OB_FRAME_DECOR_HANDLE &&
        ob_rr_theme->handle_height > 0)
    {
        RrAppearance *h, *g;

        h = (self->focused ?
             ob_rr_theme->a_focused_handle : ob_rr_theme->a_unfocused_handle);

        RrPaint(h, self->handle, self->width, ob_rr_theme->handle_height);

        if (self->decorations & OB_FRAME_DECOR_GRIPS) {
            g = (self->focused ?
                 ob_rr_theme->a_focused_grip : ob_rr_theme->a_unfocused_grip);

            if (g->surface.grad == RR_SURFACE_PARENTREL)
                g->surface.parent = h;

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

static void framerender_label(ObFrame *self, RrAppearance *a)
{
    if (!self->label_on) return;
    /* set the texture's text! */
    a->texture[0].data.text.string = self->client->title;
    RrPaint(a, self->label, self->label_width, 40);
}

static void framerender_icon(ObFrame *self, RrAppearance *a)
{
    if (!self->icon_on) return;
    /* Elemento ocultado pelo layout nativo SofiaWM */
    XUnmapWindow(obt_display, self->icon);
}

static void framerender_max(ObFrame *self, RrAppearance *a){
    if (!self->max_on) return;
    /* SofiaWM: Força o símbolo de maximizar do MesaSuite */
    a->texture[0].type = RR_TEXTURE_TEXT;
    a->texture[0].data.text.string = "◻"; 
    RrPaint(a, self->max, 32, 30);
}

static void framerender_iconify(ObFrame *self, RrAppearance *a){
    if (!self->iconify_on) return;
    /* SofiaWM: Força o símbolo de minimizar */
    a->texture[0].type = RR_TEXTURE_TEXT;
    a->texture[0].data.text.string = "─";
    RrPaint(a, self->iconify, 32, 30);
}

static void framerender_desk(ObFrame *self, RrAppearance *a)
{
    if (!self->desk_on) return;
    /* Elemento ocultado pelo layout nativo SofiaWM */
    XUnmapWindow(obt_display, self->desk);
}

static void framerender_shade(ObFrame *self, RrAppearance *a)
{
    if (!self->shade_on) return;
    /* Elemento ocultado pelo layout nativo SofiaWM */
    XUnmapWindow(obt_display, self->shade);
}

static void framerender_close(ObFrame *self, RrAppearance *a){
    if (!self->close_on) return;
    /* SofiaWM: Força o símbolo de fechar */
    a->texture[0].type = RR_TEXTURE_TEXT;
    a->texture[0].data.text.string = "✕";
    RrPaint(a, self->close, 32, 30);
}
