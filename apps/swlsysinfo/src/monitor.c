/* monitor.c — coleta de métricas via /proc. */
#include "monitor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>

static unsigned long long read_proc_stat_totals(unsigned long long *idle_out)
{
    FILE *f = fopen("/proc/stat", "r");
    if (!f) return 0;
    unsigned long long user,nice,system,idle,iowait,irq,softirq,steal;
    int n = fscanf(f, "%*s %llu %llu %llu %llu %llu %llu %llu %llu"
                     , &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal);
    fclose(f);
    if (n < 8) return 0;
    if (idle_out) *idle_out = idle + iowait;
    return user + nice + system + idle + iowait + irq + softirq + steal;
}

static void read_cpuinfo(struct swlsysinfo_cpu *cpu)
{
    FILE *f = fopen("/proc/cpuinfo", "r");
    if (!f) return;
    char line[256];
    int cores_seen =0;
    while (fgets(line, sizeof(line), f)) {
        unsigned long long n;
        if (sscanf(line, "model name: %255[^\\n]", cpu->model) ==1) {
        } else if (sscanf(line, "processor: %llu", &n) ==1) {
            cores_seen++;
        }
    }
    fclose(f);
    cpu->cores = cores_seen > 0 ? cores_seen : 1;
}

static void read_loadavg(struct swlsysinfo_cpu *cpu)
{
    FILE *f = fopen("/proc/loadavg", "r");
    if (!f) return;
    fscanf(f, "%lf %lf %lf", &cpu->load1, &cpu->load5, &cpu->load15);
    fclose(f);
}

static void read_cpu(struct swlsysinfo_cpu *cpu, const struct swlsysinfo_cpu *prev)
{
    unsigned long long prev_idle =0, prev_total =0;
    unsigned long long idle =0, total =0;
    if (prev) {
        prev_total = read_proc_stat_totals(&prev_idle);
    }
    total = read_proc_stat_totals(&idle);
    if (prev && prev_total > 0 && total > prev_total && idle >= prev_idle) {
        unsigned long long dt_total = total - prev_total;
        unsigned long long dt_idle = idle - prev_idle;
        if (dt_total > 0) {
            cpu->usage_pct =100.0 * (double)(dt_total - dt_idle) / (double)dt_total;
        } else {
            cpu->usage_pct =0.0;
        }
    } else {
        cpu->usage_pct = -1.0;
    }
    read_loadavg(cpu);
    read_cpuinfo(cpu);
}

/* --- memória --- */

static void read_mem(struct swlsysinfo_mem *mem)
{
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) return;
    char line[256];
    unsigned long long total =0, avail =0, free_ =0, buffers =0, cached =0;
    unsigned long long swap_total =0, swap_free =0;
    bool has_avail = false, has_swap_total = false;
    while (fgets(line, sizeof(line), f)) {
        unsigned long long v;
        if (sscanf(line, "MemTotal: %llu kB", &v) ==1) total = v;
        else if (sscanf(line, "MemAvailable: %llu kB", &v) ==1) { avail = v; has_avail = true; }
        else if (sscanf(line, "MemFree: %llu kB", &v) ==1) free_ = v;
        else if (sscanf(line, "Buffers: %llu kB", &v) ==1) buffers = v;
        else if (sscanf(line, "Cached: %llu kB", &v) ==1) cached = v;
        else if (sscanf(line, "SwapTotal: %llu kB", &v) ==1) { swap_total = v; has_swap_total = true; }
        else if (sscanf(line, "SwapFree: %llu kB", &v) ==1) { swap_free = v; }
    }
    fclose(f);
    mem->total_kb = total;
    mem->avail_kb = has_avail ? avail : (free_ + buffers + cached);
    mem->used_kb = total > mem->avail_kb ? total - mem->avail_kb : 0;
    mem->swap_total_kb = has_swap_total ? swap_total :  0;
    mem->swap_used_kb = mem->swap_total_kb > swap_free ? mem->swap_total_kb - swap_free :  0;
}

/* --- sistema --- */

static void read_version(struct swlsysinfo_sys *sys)
{
    FILE *f = fopen("/proc/version", "r");
    if (!f) return;
    char buf[160];
    if (fgets(buf, sizeof(buf), f)) {
        char *paren = strchr(buf, '(');
        if (paren) *paren = '\0';
        buf[strcspn(buf, "\n")] = '\0';
        snprintf(sys->kernel, sizeof(sys->kernel), "%s", buf);
    }
    fclose(f);
}

static void read_hostname(struct swlsysinfo_sys *sys)
{
    FILE *f = fopen("/proc/sys/kernel/hostname", "r");
    if (!f) return;
    char buf[64];
    if (fgets(buf, sizeof(buf), f)) {
        buf[strcspn(buf, "\n")] = '\0';
        snprintf(sys->hostname, sizeof(sys->hostname), "%s", buf);
    }
    fclose(f);
}

static void read_uptime(struct swlsysinfo_sys *sys)
{
    FILE *f = fopen("/proc/uptime", "r");
    if (!f) return;
    double up;
    if (fscanf(f, "%lf", &up) ==1) {
        long days = (long)(up / 86400.0);
        long hours = (long)((up - days * 86400.0) / 3600.0);
        long mins = (long)((up - days * 86400.0 - hours * 3600.0) / 60.0);
        snprintf(sys->uptime, sizeof(sys->uptime), "%02ldd:%02ld:%02ld", days, hours, mins);
    }
    fclose(f);
}

static void count_processes(struct swlsysinfo_sys *sys)
{
    DIR *d = opendir("/proc");
    if (!d) return;
    struct dirent *e;
    int n =0;
    while ((e = readdir(d))) {
        if (e->d_name[0] >= '0' && e->d_name[0] <= '9') n++;
    }
    closedir(d);
    sys->process_count = n;
}

/* --- processos --- */

static bool pid_stat_times(int pid, unsigned long long *utime_out, unsigned long long *stime_out)
{
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    FILE *f = fopen(path, "r");
    if (!f) return false;
    char buf[1024];
    size_t n = fread(buf,1, sizeof(buf) - 1, f);
    fclose(f);
    if (n ==0) return false;
    buf[n] = '\0';
    char *close = strrchr(buf, ')');
    if (!close || close[1] != ' ') return false;
    char *rest = close + 2;
    unsigned long long utime, stime;
    int m = sscanf(rest,
            "%*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %llu %llu",
            &utime, &stime);
    if (m !=2) return false;
    *utime_out = utime;
    *stime_out = stime;
    return true;
}

static long clock_ticks_per_sec(void)
{
    long t = sysconf(_SC_CLK_TCK);
    return t > 0 ? t : 100;
}

static double monotonic_secs(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) ==0)
        return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
    return 0.0;
}

static int cmp_proc_cpu(const void *a, const void *b)
{
    const struct swlsysinfo_proc *pa = a, *pb = b;
    if (pb->cpu_pct > pa->cpu_pct) return 1;
    if (pa->cpu_pct > pb->cpu_pct) return -1;
    return 0;
}

static void read_comm(int pid, char *out, size_t outsz)
{
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/comm", pid);
    FILE *f = fopen(path, "r");
    if (!f) { out[0] = '\0'; return; }
    if (fgets(out, (int)outsz, f)) {
        out[strcspn(out, "\n")] = '\0';
    }
    fclose(f);
}

static void read_top_procs(struct swlsysinfo_snapshot *snap,
                             const struct swlsysinfo_snapshot *prev)
{
    DIR *d = opendir("/proc");
    if (!d) return;
    struct swlsysinfo_proc tmp[1024];
    int n =0;
    long hz = clock_ticks_per_sec();
    struct dirent *e;
    while ((e = readdir(d)) && n < 1024) {
        const char *name = e->d_name;
        if (name[0] < '0' || name[0] > '9') continue;
        int pid = atoi(name);
        struct swlsysinfo_proc *p = &tmp[n++];
        memset(p, 0, sizeof(*p));
        p->pid = pid;
        read_comm(pid, p->comm, sizeof(p->comm));
        unsigned long long ut =0, st =0;
        if (pid_stat_times(pid, &ut, &st)) {
            p->utime_ticks = ut;
            p->stime_ticks = st;
            p->cpu_pct = -1.0;
            if (prev) {
                int i;
                for (i =0; i < prev->nprocs; i++) {
                    if (prev->procs[i].pid == pid) {
                        double dt_secs = snap->sample_time - prev->sample_time;
                        if (dt_secs > 0.0) {
                            unsigned long long dt = (ut + st) - (prev->procs[i].utime_ticks + prev->procs[i].stime_ticks);
                            p->cpu_pct =100.0 * (double)dt / (double)hz / dt_secs;
                        }
                        break;
                    }
                }
            }
        }
        char spath[64];
        snprintf(spath, sizeof(spath), "/proc/%d/statm", pid);
        FILE *sf = fopen(spath, "r");
        if (sf) {
            unsigned long long pages =0;
            if (fscanf(sf, "%*s %llu", &pages) ==1)
                p->rss_kb = (unsigned long)(pages * (unsigned long long)sysconf(_SC_PAGESIZE) / 1024);
            fclose(sf);
        }
    }
    closedir(d);
    qsort(tmp, (size_t)n, sizeof(tmp[0]), cmp_proc_cpu);
    int top = n < SWLSYSINFO_TOP_PROC ? n : SWLSYSINFO_TOP_PROC;
    memcpy(snap->procs, tmp, (size_t)top * sizeof(tmp[0]));
    snap->nprocs = top;
}

void swlsysinfo_snapshot(struct swlsysinfo_snapshot *out,
                          const struct swlsysinfo_snapshot *prev)
{
    memset(out, 0, sizeof(*out));
    out->sample_time = monotonic_secs();
    read_cpu(&out->cpu, prev ? &prev->cpu : NULL);
    read_mem(&out->mem);
    read_version(&out->sys);
    read_hostname(&out->sys);
    read_uptime(&out->sys);
    count_processes(&out->sys);
    read_top_procs(out, prev);
}
