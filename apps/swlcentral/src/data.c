#include <string.h>
#include <ctype.h>
#include "central.h"

const char *const cc_cat_names[CC_CAT_COUNT] = {
    "Rede", "Personalização", "Sistema", "Segurança",
    "Atualizações", "Diagnóstico", "Gaming", "Sobre",
};

const char *const cc_lang_opts[3] = { "Português (BR)", "English", "Español" };
const char *const cc_lock_opts[4] = { "Nunca", "1 minuto", "5 minutos", "15 minutos" };

typedef struct { const char *term; cc_cat_t cat; } search_entry_t;
static const search_entry_t SEARCH_INDEX[] = {
    {"wifi", CC_CAT_REDE}, {"wi-fi", CC_CAT_REDE}, {"rede", CC_CAT_REDE},
    {"ethernet", CC_CAT_REDE}, {"bluetooth", CC_CAT_REDE},
    {"tema", CC_CAT_PERSONALIZACAO}, {"cor", CC_CAT_PERSONALIZACAO}, {"idioma", CC_CAT_PERSONALIZACAO},
    {"transparencia", CC_CAT_PERSONALIZACAO}, {"animacoes", CC_CAT_PERSONALIZACAO},
    {"energia", CC_CAT_SISTEMA}, {"brilho", CC_CAT_SISTEMA}, {"bateria", CC_CAT_SISTEMA},
    {"hostname", CC_CAT_SISTEMA}, {"notificacoes", CC_CAT_SISTEMA},
    {"firewall", CC_CAT_SEGURANCA}, {"ssh", CC_CAT_SEGURANCA}, {"desenvolvedor", CC_CAT_SEGURANCA},
    {"bloqueio de tela", CC_CAT_SEGURANCA},
    {"atualizacao", CC_CAT_ATUALIZACOES}, {"canal", CC_CAT_ATUALIZACOES},
    {"diagnostico", CC_CAT_DIAGNOSTICO}, {"gpu", CC_CAT_DIAGNOSTICO}, {"disco", CC_CAT_DIAGNOSTICO},
    {"game mode", CC_CAT_GAMING}, {"fps", CC_CAT_GAMING}, {"jogos", CC_CAT_GAMING},
    {"versao", CC_CAT_SOBRE}, {"backup", CC_CAT_SOBRE},
};
#define SEARCH_N (int)(sizeof(SEARCH_INDEX) / sizeof(SEARCH_INDEX[0]))

static bool icontains(const char *hay, const char *needle) {
    if (!*needle) return false;
    size_t nlen = strlen(needle);
    for (const char *p = hay; *p; p++) {
        size_t i = 0;
        while (i < nlen && p[i] && tolower((unsigned char)p[i]) == tolower((unsigned char)needle[i])) i++;
        if (i == nlen) return true;
    }
    return false;
}

void cc_refresh_search(cc_app_t *a) {
    a->search_nhits = 0;
    if (!a->search[0]) return;
    for (int i = 0; i < SEARCH_N && a->search_nhits < 6; i++) {
        if (icontains(SEARCH_INDEX[i].term, a->search)) {
            a->search_hits[a->search_nhits].label = SEARCH_INDEX[i].term;
            a->search_hits[a->search_nhits].category = cc_cat_names[SEARCH_INDEX[i].cat];
            a->search_cats[a->search_nhits] = SEARCH_INDEX[i].cat;
            a->search_nhits++;
        }
    }
}
