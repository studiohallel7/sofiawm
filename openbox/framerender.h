/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-
 *
 * framerender.h — SofiaWM Decorator
 *
 * Constantes de layout (fiel ao interface.py):
 *   SOFIA_TITLEBAR_HEIGHT  40px  — altura da barra de título
 *   SOFIA_BTN_W            32px  — largura dos botões (✕ ─ ◻)
 *   SOFIA_BTN_H            40px  — altura dos botões (= titlebar)
 *   SOFIA_SEP_W            1px   — largura do separador vertical
 *   SOFIA_SEP_PAD          10px  — padding lateral do separador
 *   SOFIA_ACTION_BTN_W     32px  — largura dos botões de ação (🗁 ✎ ↻)
 *
 * Layout da titlebar (esq → dir):
 *
 *   ┌─────────────────────────────────────────────────────────────────┐
 *   │ [✕][─][◻] │sep│ [🗁][✎][↻] │ ........título........ │ SOFIA   │
 *   └─────────────────────────────────────────────────────────────────┘
 *    ←─ 3×32 ─→  10  ←── 3×32 ──→  ←────── label_width ──────────→
 *
 * Campos adicionais necessários em frame.h/_ObFrame:
 *
 *   Window  action_open;            // botão 🗁
 *   Window  action_edit;            // botão ✎
 *   Window  action_rotate;          // botão ↻
 *   gboolean action_open_hover;
 *   gboolean action_open_press;
 *   gboolean action_edit_hover;
 *   gboolean action_edit_press;
 *   gboolean action_rotate_hover;
 *   gboolean action_rotate_press;
 *
 * Campos adicionais em event.c:
 *   Tratar EnterNotify/LeaveNotify → action_*_hover
 *   Tratar ButtonPress/Release     → action_*_press + disparar ação
 */

#ifndef __framerender_h
#define __framerender_h

/* ---- Constantes de geometria ---- */
#define SOFIA_TITLEBAR_HEIGHT   40
#define SOFIA_BTN_W             32
#define SOFIA_BTN_H             SOFIA_TITLEBAR_HEIGHT
#define SOFIA_SEP_W              1
#define SOFIA_SEP_PAD           10
#define SOFIA_ACTION_BTN_W      32

/*
 * Offset X onde começa o bloco de botões de ação (após os 3 botões de janela
 * + separador). Calculado como:
 *   3 botões × 32px  +  (SEP_PAD × 2 + SEP_W)  +  12px margin esq
 *   = 96 + 21 + 12 = 129px
 * Mas frame.c deve calcular isso dinamicamente; esta macro é só referência.
 */
#define SOFIA_ACTION_BLOCK_X  (12 + 3*SOFIA_BTN_W + SOFIA_SEP_PAD*2 + SOFIA_SEP_W)

struct _ObFrame;

/* Renderiza o decorator completo da janela */
void framerender_frame(struct _ObFrame *self);

/* Libera as cores alocadas — chamar no shutdown do WM */
void sofia_colors_free(void);

#endif /* __framerender_h */
