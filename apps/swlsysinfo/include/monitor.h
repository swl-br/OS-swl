#ifndef SWLSYSINFO_MONITOR_H
#define SWLSYSINFO_MONITOR_H

#include <stdbool.h>
#include <stddef.h>

/*
 * monitor: coleta de métricas do sistema via /proc.
 *
 * Todas as funções são stateless (sem alocação entre chamadas) e
 * resilientes: se um arquivo não existir ou vier com formato inesperado,
 * os campos ficam em 0/desconhecido e a UI mostra "(indisponível)" em
 * vez de crachar.
 */

#define SWLSYSINFO_TOP_PROC 8  /* quantos processos no top-N por CPU */

struct swlsysinfo_cpu {
    double usage_pct;   /* 0..100 desde a última leitura (ou -1 se sem delta) */
    double load1, load5, load15;  /* do /proc/loadavg */
    int cores;        /* nº de núcleos lógicos (cpuinfo) */
    char model[128];   /* modelo do processador */
};

struct swlsysinfo_mem {
    unsigned long long total_kb, used_kb, avail_kb;
    unsigned long long swap_total_kb, swap_used_kb;
};

struct swlsysinfo_sys {
    char kernel[128];    /* /proc/version — "Linux version 7.2.1 ..." */
    char hostname[64];
    char uptime[32];    /* dd:hh:mm */
    int process_count;
};

struct swlsysinfo_proc {
    int pid;
    char comm[32];
    double cpu_pct;   /* desde a última leitura (pode ser -1 no   primeiro tick) */
unsigned long long utime_ticks, stime_ticks;   /* p/ delta de CPU */
    unsigned long rss_kb;
};

struct swlsysinfo_snapshot {
    struct swlsysinfo_cpu cpu;
    struct swlsysinfo_mem mem;
    struct swlsysinfo_sys sys;
    struct swlsysinfo_proc procs[SWLSYSINFO_TOP_PROC];
    int nprocs;
    double sample_time;   /* monotonic_secs() da leitura aktueller */
};

/* Preenche o snapshot. Só traduz o que leu, sem validação extra.
 * `prev_*` são opcionais (podem ser NULL) e servem pro delta de CPU;
 * na primeira chamada (ou se NULL) o usage fica -1. */
void swlsysinfo_snapshot(struct swlsysinfo_snapshot *out,
                          const struct swlsysinfo_snapshot *prev);

#endif /* SWLSYSINFO_MONITOR_H */