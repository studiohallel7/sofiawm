/* sofia_actions.c — SofiaWM Ações Contextuais
 *
 * Cliente Unix socket que comunica com o AppletManager (Python)
 * para obter ações contextuais da janela focada.
 *
 * Fluxo:
 *   1. Janela recebe foco → event.c chama sofia_actions_query()
 *   2. Envia JSON para ~/.config/grande_vigia.socket
 *   3. AppletManager responde com lista de ações
 *   4. framerender.c lê sofia_current_actions e renderiza botões
 */

#include "sofia_actions.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <glib.h>

/* Caminho do socket do AppletManager */
#define VIGIA_SOCKET_PATH "/.config/grande_vigia.socket"
#define SOCKET_TIMEOUT_MS  500   /* timeout em ms — não travar o WM */
#define RECV_BUF_SIZE     4096

/* Estado global das ações da janela atual
 * Lido pelo framerender.c para renderizar os botões */
SofiaWindowActions sofia_current_actions = { 0 };

/* =========================================================
 * HELPERS JSON MÍNIMO — sem dependência de jansson/json-c
 * ========================================================= */

/* Extrai string de um JSON simples: {"key":"value"}
 * Suporta \uXXXX e \" escapados dentro do valor */
static int json_get_string(const char *json,
                            const char *key,
                            char       *out,
                            int         out_size)
{
    char search[128];
    snprintf(search, sizeof(search), "\"%s\":\"", key);
    const char *p = strstr(json, search);
    if (!p) return 0;
    p += strlen(search);

    /* Copia até aspas não-escapadas, decodificando \uXXXX simples */
    int len = 0;
    while (*p && len < out_size - 1) {
        if (*p == '\\') {
            p++;
            if (*p == '"') {
                /* aspas escapadas — inclui no valor */
                out[len++] = '"';
                p++;
            } else if (*p == 'n') {
                out[len++] = '\n'; p++;
            } else if (*p == 't') {
                out[len++] = '\t'; p++;
            } else if (*p == 'u' && p[1] && p[2] && p[3] && p[4]) {
                /* \uXXXX — decodifica para UTF-8 */
                unsigned int cp = 0;
                sscanf(p + 1, "%4x", &cp);
                p += 5;
                if (cp < 0x80) {
                    out[len++] = (char)cp;
                } else if (cp < 0x800) {
                    if (len + 2 < out_size) {
                        out[len++] = (char)(0xC0 | (cp >> 6));
                        out[len++] = (char)(0x80 | (cp & 0x3F));
                    }
                } else {
                    if (len + 3 < out_size) {
                        out[len++] = (char)(0xE0 | (cp >> 12));
                        out[len++] = (char)(0x80 | ((cp >> 6) & 0x3F));
                        out[len++] = (char)(0x80 | (cp & 0x3F));
                    }
                }
            } else {
                out[len++] = '\\';
                if (*p) out[len++] = *p++;
            }
        } else if (*p == '"') {
            break; /* fim da string JSON */
        } else {
            out[len++] = *p++;
        }
    }
    out[len] = '\0';
    return len > 0 ? 1 : 0;
}

/* Extrai int de um JSON: {"key":123} */
static int json_get_int(const char *json, const char *key, int *out)
{
    char search[128];
    snprintf(search, sizeof(search), "\"%s\":", key);
    const char *p = strstr(json, search);
    if (!p) return 0;
    p += strlen(search);
    *out = atoi(p);
    return 1;
}

/* Parser mínimo do array "actions" retornado pelo AppletManager.
 * Formato esperado:
 * {"actions":[{"icon":"↻","id":"rotate_cw","label":"Girar 90°","command":"..."},...]}
 */
static int parse_actions(const char         *json,
                          SofiaWindowActions *out)
{
    out->count = 0;

    /* Extrai context_file e mimetype */
    json_get_string(json, "context_file", out->context_file,
                    sizeof(out->context_file));
    json_get_string(json, "mimetype", out->mimetype,
                    sizeof(out->mimetype));

    /* Encontra o array actions */
    const char *arr = strstr(json, "\"actions\":[");
    if (!arr) return 0;
    arr = strchr(arr, '[');
    if (!arr) return 0;
    arr++; /* pula o '[' */

    while (*arr && *arr != ']' && out->count < SOFIA_MAX_ACTIONS) {
        /* Pula até próximo objeto */
        while (*arr && *arr != '{') arr++;
        if (*arr != '{') break;

        /* Encontra o '}' correto — ignorando os que estão dentro de strings */
        const char *scan = arr + 1;
        int depth = 1;
        int in_str = 0;
        while (*scan && depth > 0) {
            if (in_str) {
                if (*scan == '\\') { scan++; } /* pula escaped char */
                else if (*scan == '"') { in_str = 0; }
            } else {
                if (*scan == '"')      { in_str = 1; }
                else if (*scan == '{') { depth++; }
                else if (*scan == '}') { depth--; }
            }
            if (depth > 0) scan++;
        }
        const char *obj_end = (*scan == '}') ? scan : NULL;
        if (!obj_end) break;

        /* Copia o objeto para buffer temporário */
        int obj_len = (int)(obj_end - arr + 1);
        char obj[2048] = { 0 };
        if (obj_len >= (int)sizeof(obj)) obj_len = sizeof(obj) - 1;
        memcpy(obj, arr, obj_len);

        SofiaAction *a = &out->actions[out->count];
        memset(a, 0, sizeof(*a));

        json_get_string(obj, "icon",    a->icon,    sizeof(a->icon));
        json_get_string(obj, "id",      a->id,      sizeof(a->id));
        json_get_string(obj, "label",   a->label,   sizeof(a->label));
        json_get_string(obj, "command", a->command, sizeof(a->command));

        if (a->id[0] != '\0')
            out->count++;

        arr = obj_end + 1;
    }

    return out->count;
}

/* =========================================================
 * COMUNICAÇÃO COM O APPLETMANAGER
 * ========================================================= */
gboolean sofia_actions_query(const char         *wm_class,
                              const char         *title,
                              int                 pid,
                              SofiaWindowActions *out)
{
    sofia_actions_clear(out);

    /* Monta caminho do socket */
    const char *home = g_get_home_dir();
    if (!home) return FALSE;

    char socket_path[512];
    snprintf(socket_path, sizeof(socket_path), "%s%s", home, VIGIA_SOCKET_PATH);

    /* Cria socket Unix */
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        g_debug("[SOFIA_ACTIONS] Falha ao criar socket: %s", strerror(errno));
        return FALSE;
    }

    /* Timeout para não travar o WM */
    struct timeval tv;
    tv.tv_sec  = 0;
    tv.tv_usec = SOCKET_TIMEOUT_MS * 1000;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    /* Conecta */
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        g_debug("[SOFIA_ACTIONS] AppletManager não disponível: %s", strerror(errno));
        close(fd);
        return FALSE;
    }

    /* Monta JSON da requisição
     * Escapa aspas no título para não quebrar o JSON */
    char safe_title[512] = { 0 };
    int j = 0;
    for (int i = 0; title && title[i] && j < (int)sizeof(safe_title) - 2; i++) {
        if (title[i] == '"' || title[i] == '\\') safe_title[j++] = '\\';
        safe_title[j++] = title[i];
    }

    char request[1024];
    snprintf(request, sizeof(request),
        "{\"command\":\"window_focused\","
        "\"wm_class\":\"%s\","
        "\"title\":\"%s\","
        "\"pid\":%d}",
        wm_class ? wm_class : "",
        safe_title,
        pid);

    /* Envia */
    if (send(fd, request, strlen(request), 0) < 0) {
        g_debug("[SOFIA_ACTIONS] Falha ao enviar: %s", strerror(errno));
        close(fd);
        return FALSE;
    }

    /* Recebe resposta */
    char buf[RECV_BUF_SIZE] = { 0 };
    int total = 0;
    int n;
    while ((n = recv(fd, buf + total, sizeof(buf) - total - 1, 0)) > 0)
        total += n;
    buf[total] = '\0';
    close(fd);

    if (total == 0) {
        g_debug("[SOFIA_ACTIONS] Resposta vazia do AppletManager");
        return FALSE;
    }

    g_debug("[SOFIA_ACTIONS] Resposta: %s", buf);

    /* Verifica status */
    if (!strstr(buf, "\"status\"") || (!strstr(buf, "\"ok\"") && !strstr(buf, "ok_"))) {
        g_debug("[SOFIA_ACTIONS] AppletManager retornou erro");
        return FALSE;
    }

    parse_actions(buf, out);
    g_debug("[SOFIA_ACTIONS] %d ações para '%s'", out->count, wm_class);
    return TRUE;
}

/* =========================================================
 * EXECUÇÃO DE AÇÃO
 * ========================================================= */
void sofia_actions_execute(const SofiaWindowActions *actions, int index)
{
    if (!actions || index < 0 || index >= actions->count) return;

    const SofiaAction *a = &actions->actions[index];
    if (a->command[0] == '\0') {
        g_warning("[SOFIA_ACTIONS] Ação '%s' sem comando definido", a->id);
        return;
    }

    g_info("[SOFIA_ACTIONS] Executando: %s → %s", a->label, a->command);

    /* Substitui %F pelo context_file se presente */
    char cmd[SOFIA_CMD_MAX * 2];
    const char *cf = actions->context_file[0]
        ? actions->context_file : "";

    const char *f_pos = strstr(a->command, "%F");
    if (f_pos) {
        int pre_len = (int)(f_pos - a->command);
        snprintf(cmd, sizeof(cmd), "%.*s%s%s",
                 pre_len, a->command, cf, f_pos + 2);
    } else {
        strncpy(cmd, a->command, sizeof(cmd) - 1);
    }

    /* Executa em background — não bloqueia o WM */
    char shell_cmd[SOFIA_CMD_MAX * 2 + 32];
    snprintf(shell_cmd, sizeof(shell_cmd), "sh -c '%s' &", cmd);
    system(shell_cmd);
}

/* =========================================================
 * UTILITÁRIOS
 * ========================================================= */
void sofia_actions_clear(SofiaWindowActions *actions)
{
    if (actions) memset(actions, 0, sizeof(*actions));
}
