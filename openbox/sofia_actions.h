/* sofia_actions.h — SofiaWM Ações Contextuais
 *
 * Comunica com o AppletManager via Unix socket para obter
 * as ações disponíveis para a janela focada.
 */

#ifndef __sofia_actions_h
#define __sofia_actions_h

#include <glib.h>

#define SOFIA_MAX_ACTIONS     4
#define SOFIA_ICON_MAX       16   /* bytes UTF-8 */
#define SOFIA_LABEL_MAX      64
#define SOFIA_CMD_MAX      1024
#define SOFIA_ID_MAX         64

/* Uma ação contextual retornada pelo AppletManager */
typedef struct {
    char icon   [SOFIA_ICON_MAX];
    char id     [SOFIA_ID_MAX];
    char label  [SOFIA_LABEL_MAX];
    char command[SOFIA_CMD_MAX];
} SofiaAction;

/* Estado das ações da janela atual */
typedef struct {
    SofiaAction actions[SOFIA_MAX_ACTIONS];
    int         count;
    char        context_file[512];
    char        mimetype[128];
} SofiaWindowActions;

/* Notifica o AppletManager sobre a janela focada.
 * Preenche 'out' com as ações disponíveis.
 * Retorna TRUE se obteve resposta, FALSE se socket falhou. */
gboolean sofia_actions_query(const char       *wm_class,
                              const char       *title,
                              int               pid,
                              SofiaWindowActions *out);

/* Executa uma ação pelo índice (dispara o command em background) */
void sofia_actions_execute(const SofiaWindowActions *actions, int index);

/* Limpa a struct de ações */
void sofia_actions_clear(SofiaWindowActions *actions);

/* Variável global com as ações da janela atualmente focada.
 * Definida em sofia_actions.c, lida pelo framerender.c */
extern SofiaWindowActions sofia_current_actions;

#endif /* __sofia_actions_h */
