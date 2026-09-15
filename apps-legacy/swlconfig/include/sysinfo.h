/* sysinfo — coleta de informações reais do sistema pra seção "Sobre"
 * do swlconfig. Separado do resto do app (sem Wayland/Cairo aqui) pra
 * poder ser testado sem precisar de compositor — mesmo princípio do
 * editor.c do SWLPad (lógica pura separada da UI).
 *
 * As funções de parse (swlconfig_parse_meminfo/parse_uptime) recebem
 * o texto já lido como string, não abrem arquivo sozinhas — isso é o
 * que permite testar com um texto de exemplo fixo em vez de depender
 * do /proc de verdade (que varia de máquina pra máquina). */
#ifndef SWLCONFIG_SYSINFO_H
#define SWLCONFIG_SYSINFO_H

#include <stdbool.h>

typedef struct {
	char kernel_release[128]; /* uname -r, ex.: "6.8.0-generic" */
	char hostname[256];
	long uptime_seconds;      /* -1 se não conseguiu ler */
	long mem_total_kb;        /* -1 se não conseguiu ler */
	long mem_available_kb;    /* -1 se não conseguiu ler (ver nota no .c
	                            * sobre fallback em kernels sem MemAvailable) */
	double disk_total_gb;     /* -1.0 se não conseguiu ler */
	double disk_free_gb;      /* -1.0 se não conseguiu ler */
} swlconfig_sysinfo_t;

/* Preenche `out` com dados reais do sistema (uname, /proc/meminfo,
 * /proc/uptime, statvfs("/")). Campos individuais ficam com valor
 * sentinela (-1 / -1.0 / string vazia) se a fonte específica falhar —
 * uma falha parcial não derruba as outras informações. Retorna false
 * só se TODAS as fontes falharem (situação anormal). */
bool swlconfig_sysinfo_collect(swlconfig_sysinfo_t *out);

/* --- Helpers de parse puro (testáveis sem tocar em arquivo real) --- */

/* Extrai MemTotal e MemAvailable (em kB) do texto de /proc/meminfo.
 * Se "MemAvailable" não existir no texto (kernels antigos, pré-3.14),
 * usa "MemFree" como aproximação (documentado: é uma aproximação por
 * baixo, não conta cache reclamável — mas é melhor que nada). Retorna
 * false se nem MemTotal for encontrado (texto não é um meminfo válido). */
bool swlconfig_parse_meminfo(const char *text, long *mem_total_kb, long *mem_available_kb);

/* Extrai os segundos de uptime do texto de /proc/uptime (primeiro
 * número da linha, formato "1234.56 78.90"). Retorna false se o
 * texto não começar com um número válido. */
bool swlconfig_parse_uptime(const char *text, long *uptime_seconds);

#endif /* SWLCONFIG_SYSINFO_H */
