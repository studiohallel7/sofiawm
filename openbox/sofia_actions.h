/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*- */
#ifndef __sofia_actions_h
#define __sofia_actions_h

#include <glib.h>
#include <X11/Xlib.h>

#define SOFIA_MAX_ACTIONS 10
#define SOFIA_CMD_MAX 256

typedef struct _SofiaAction {
    char icon[32];
    char id[64];
    char label[64];
    char command[SOFIA_CMD_MAX];
} SofiaAction;

typedef struct _SofiaWindowActions {
    int count;
    char context_file[512];
    char mimetype[64];
    SofiaAction actions[SOFIA_MAX_ACTIONS];
} SofiaWindowActions;

extern SofiaWindowActions sofia_current_actions;

/* Função atualizada para usar X11 Properties em vez de Socket */
gboolean sofia_actions_update(Window win, SofiaWindowActions *out);

void sofia_actions_execute(const SofiaWindowActions *actions, int index);
void sofia_actions_clear(SofiaWindowActions *actions);

/* Retorna o Atom em cache para evitar gargalo e logoff no X11 */
Atom sofia_actions_get_atom(void);

#endif
