/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-
 *
 * framerender.h — SofiaWM Decorator
 *
 * ESTE HEADER DEVE SER INCLUÍDO ANTES DE frame.h em framerender.c
 * para que as macros SOFIA_* estejam disponíveis durante a compilação.
 */

#ifndef __framerender_h
#define __framerender_h

/* ---- Nenhuma macro de geometria hardcoded ----
 * Usamos ob_rr_theme->title_height e ob_rr_theme->button_size
 * para respeitar o que o frame.c já calculou.
 * Isso evita conflito com SOFIA_TITLEBAR_HEIGHT não definido. */

struct _ObFrame;

/* Renderiza o decorator completo */
void framerender_frame(struct _ObFrame *self);

/* Libera cores alocadas — chamar em openbox_shutdown() */
void sofia_colors_free(void);

#endif /* __framerender_h */
