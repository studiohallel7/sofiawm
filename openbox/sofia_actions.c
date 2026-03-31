/* sofia_actions.c — SofiaWM Ações Contextuais via X11 Properties (Com Cache) */

#include "sofia_actions.h"
#include "obt/display.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>

SofiaWindowActions sofia_current_actions = { 0 };

/* =========================================================
 * CACHE DO ATOM (Evita travamentos e logoffs no X11)
 * ========================================================= */
static Atom cached_sofia_atom = None;

Atom sofia_actions_get_atom(void) {
    if (cached_sofia_atom == None && obt_display != NULL) {
        cached_sofia_atom = XInternAtom(obt_display, "_SOFIA_WINDOW_ACTIONS", False);
    }
    return cached_sofia_atom;
}

/* =========================================================
 * HELPERS JSON MÍNIMO
 * ========================================================= */
static int json_get_string(const char *json, const char *key, char *out, int out_size) {
    char search[128];
    snprintf(search, sizeof(search), "\"%s\":", key);
    const char *p = strstr(json, search);
    if (!p) return 0;
    p += strlen(search);
    while (*p == ' ' || *p == '\t') p++;
    if (*p != '"') return 0;
    p++; 

    int len = 0;
    while (*p && len < out_size - 1) {
        if (*p == '\\') {
            p++;
            if (*p == '"') { out[len++] = '"'; p++; } 
            else if (*p == 'n') { out[len++] = '\n'; p++; } 
            else if (*p == 't') { out[len++] = '\t'; p++; } 
            else if (*p == 'u' && p[1] && p[2] && p[3] && p[4]) {
                unsigned int cp = 0;
                sscanf(p + 1, "%4x", &cp);
                p += 5;
                if (cp < 0x80) { out[len++] = (char)cp; } 
                else if (cp < 0x800) {
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
            break;
        } else {
            out[len++] = *p++;
        }
    }
    out[len] = '\0';
    return len > 0 ? 1 : 0;
}

static int parse_actions(const char *json, SofiaWindowActions *out) {
    out->count = 0;
    json_get_string(json, "context_file", out->context_file, sizeof(out->context_file));
    json_get_string(json, "mimetype", out->mimetype, sizeof(out->mimetype));

    const char *arr = strstr(json, "\"actions\"");
    if (!arr) return 0;
    arr = strchr(arr, '[');
    if (!arr) return 0;
    arr++; 

    while (*arr && *arr != ']' && out->count < SOFIA_MAX_ACTIONS) {
        while (*arr && *arr != '{' && *arr != ']') arr++;
        if (*arr != '{') break;

        const char *scan = arr + 1;
        int depth = 1;
        int in_str = 0;
        while (*scan && depth > 0) {
            if (in_str) {
                if (*scan == '\\') { scan++; if (*scan) scan++; continue; } 
                else if (*scan == '"') { in_str = 0; }
            } else {
                if (*scan == '"') { in_str = 1; }
                else if (*scan == '{') { depth++; }
                else if (*scan == '}') { depth--; }
            }
            if (depth > 0) scan++;
        }

        const char *obj_end = (*scan == '}') ? scan : NULL;
        if (!obj_end) break;

        int obj_len = (int)(obj_end - arr + 1);
        char obj[4096] = { 0 };
        if (obj_len >= (int)sizeof(obj)) obj_len = sizeof(obj) - 1;
        memcpy(obj, arr, obj_len);

        SofiaAction *a = &out->actions[out->count];
        memset(a, 0, sizeof(*a));

        json_get_string(obj, "icon", a->icon, sizeof(a->icon));
        json_get_string(obj, "id", a->id, sizeof(a->id));
        json_get_string(obj, "label", a->label, sizeof(a->label));
        json_get_string(obj, "command", a->command, sizeof(a->command));

        if (a->id[0] != '\0') out->count++;
        arr = obj_end + 1;
    }
    return out->count;
}

/* =========================================================
 * LER AÇÕES DO X11 PROPERTY (Agora com Cache Rápido)
 * ========================================================= */
gboolean sofia_actions_update(Window win, SofiaWindowActions *out) {
    sofia_actions_clear(out);
    if (!win || win == None) return FALSE;

    Atom prop = sofia_actions_get_atom();
    if (prop == None) return FALSE;

    Atom type_ret;
    int format_ret;
    unsigned long nitems_ret, bytes_after_ret;
    unsigned char *prop_data = NULL;

    if (XGetWindowProperty(obt_display, win, prop, 0, 8192, False,
                           AnyPropertyType, &type_ret, &format_ret,
                           &nitems_ret, &bytes_after_ret, &prop_data) == Success) {
        if (prop_data && nitems_ret > 0) {
            parse_actions((const char *)prop_data, out);
        }
        if (prop_data) XFree(prop_data);
    }
    return out->count > 0;
}

/* =========================================================
 * EXECUÇÃO E UTILITÁRIOS
 * ========================================================= */
void sofia_actions_execute(const SofiaWindowActions *actions, int index) {
    if (!actions || index < 0 || index >= actions->count) return;

    const SofiaAction *a = &actions->actions[index];
    if (a->command[0] == '\0') return;

    char cmd[SOFIA_CMD_MAX * 2];
    const char *cf = actions->context_file[0] ? actions->context_file : "";
    const char *f_pos = strstr(a->command, "%F");

    if (f_pos) {
        int pre_len = (int)(f_pos - a->command);
        snprintf(cmd, sizeof(cmd), "%.*s%s%s", pre_len, a->command, cf, f_pos + 2);
    } else {
        strncpy(cmd, a->command, sizeof(cmd) - 1);
    }

    char shell_cmd[SOFIA_CMD_MAX * 2 + 32];
    snprintf(shell_cmd, sizeof(shell_cmd), "sh -c '%s' &", cmd);
    system(shell_cmd);
}

void sofia_actions_clear(SofiaWindowActions *actions) {
    if (actions) memset(actions, 0, sizeof(*actions));
}
