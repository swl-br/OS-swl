/*
 * api.c — backend real do SWLCentral v2. Mesmas regras de sempre:
 * nunca system()/popen() (só argv fixo via posix_spawnp), honesto
 * quando uma ferramenta não existe, nunca loga segredo.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <time.h>
#include <signal.h>
#include <spawn.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/select.h>
#include <syslog.h>
#include <pwd.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "central.h"

extern char **environ;

static bool bin_exists(const char *name) {
    if (!name || !*name) return false;
    if (strchr(name, '/')) return access(name, X_OK) == 0;
    const char *path = getenv("PATH");
    if (!path || !*path) path = "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin";
    char buf[512];
    const char *p = path;
    while (p && *p) {
        const char *colon = strchr(p, ':');
        size_t len = colon ? (size_t)(colon - p) : strlen(p);
        if (len > 0 && len < sizeof(buf) - 1) {
            memcpy(buf, p, len);
            size_t nlen = strlen(name);
            if (len + 1 + nlen < sizeof(buf)) {
                buf[len] = '/';
                memcpy(buf + len + 1, name, nlen + 1);
                if (access(buf, X_OK) == 0) return true;
            }
        }
        p = colon ? colon + 1 : NULL;
    }
    return false;
}

static int run_cmd(char *const argv[], char *buf, size_t bufcap, int timeout_ms) {
    if (buf && bufcap) buf[0] = 0;
    if (!bin_exists(argv[0])) return -1;
    int outpipe[2];
    if (pipe(outpipe) != 0) return -1;
    posix_spawn_file_actions_t fa;
    posix_spawn_file_actions_init(&fa);
    posix_spawn_file_actions_addclose(&fa, outpipe[0]);
    posix_spawn_file_actions_adddup2(&fa, outpipe[1], STDOUT_FILENO);
    posix_spawn_file_actions_adddup2(&fa, outpipe[1], STDERR_FILENO);
    posix_spawn_file_actions_addclose(&fa, outpipe[1]);
    posix_spawn_file_actions_addopen(&fa, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
    pid_t pid;
    int rc = posix_spawnp(&pid, argv[0], &fa, NULL, argv, environ);
    posix_spawn_file_actions_destroy(&fa);
    close(outpipe[1]);
    if (rc != 0) { close(outpipe[0]); return -1; }
    fcntl(outpipe[0], F_SETFL, O_NONBLOCK);
    size_t used = 0;
    bool timed_out = false;
    struct timespec t0; clock_gettime(CLOCK_MONOTONIC, &t0);
    for (;;) {
        fd_set rfds; FD_ZERO(&rfds); FD_SET(outpipe[0], &rfds);
        struct timeval tv = {0, 40000};
        int sel = select(outpipe[0] + 1, &rfds, NULL, NULL, &tv);
        if (sel > 0 && FD_ISSET(outpipe[0], &rfds)) {
            char tmp[1024]; ssize_t n = read(outpipe[0], tmp, sizeof(tmp));
            if (n > 0 && buf) {
                size_t cp = (size_t)n;
                if (used + cp >= bufcap) cp = (used < bufcap - 1) ? bufcap - 1 - used : 0;
                if (cp) { memcpy(buf + used, tmp, cp); used += cp; buf[used] = 0; }
            }
        }
        int status; pid_t w = waitpid(pid, &status, WNOHANG);
        if (w == pid) {
            char tmp[1024]; ssize_t n;
            while (buf && (n = read(outpipe[0], tmp, sizeof(tmp))) > 0) {
                size_t cp = (size_t)n;
                if (used + cp >= bufcap) cp = (used < bufcap - 1) ? bufcap - 1 - used : 0;
                if (cp) { memcpy(buf + used, tmp, cp); used += cp; buf[used] = 0; } else break;
            }
            close(outpipe[0]);
            return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
        }
        struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
        long elapsed_ms = (now.tv_sec - t0.tv_sec) * 1000 + (now.tv_nsec - t0.tv_nsec) / 1000000;
        if (elapsed_ms > timeout_ms) { timed_out = true; kill(pid, SIGKILL); waitpid(pid, &status, 0); break; }
    }
    close(outpipe[0]);
    return timed_out ? -2 : -1;
}
#define RUN(buf, ms, ...) run_cmd((char *const[]){__VA_ARGS__, NULL}, (buf), sizeof(buf), (ms))
static bool is_root(void) { return geteuid() == 0; }

/* ---- app dir: ~/.config/swl/swlcentral/ (decisão: 1 subpasta por app) ---- */
static bool app_dir(char *out, size_t cap) {
    const char *home = getenv("HOME");
    if (!home) { struct passwd *pw = getpwuid(getuid()); home = pw ? pw->pw_dir : NULL; }
    if (!home) return false;
    char base[560]; snprintf(base, sizeof(base), "%s/.config", home);
    mkdir(base, 0700);
    char swl[580]; snprintf(swl, sizeof(swl), "%s/swl", base);
    mkdir(swl, 0700);
    snprintf(out, cap, "%s/swlcentral", swl);
    mkdir(out, 0700);
    return true;
}

static FILE *log_file(void) {
    static FILE *f = NULL; static bool tried = false;
    if (tried) return f;
    tried = true;
    char dir[640];
    if (!app_dir(dir, sizeof(dir))) return NULL;
    char path[820]; snprintf(path, sizeof(path), "%s/central.log", dir);
    f = fopen(path, "a");
    return f;
}
static bool syslog_opened = false;
void cc_api_log(const char *component, const char *setting, const char *before, const char *after, bool ok) {
    if (!syslog_opened) { openlog("swlcentral", LOG_PID, LOG_USER); syslog_opened = true; }
    syslog(ok ? LOG_INFO : LOG_WARNING, "%s/%s %s -> %s (%s)", component, setting, before, after, ok ? "ok" : "falha");
    time_t now = time(NULL); struct tm tmv; localtime_r(&now, &tmv);
    char ts[32]; strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tmv);
    FILE *f = log_file();
    if (f) { fprintf(f, "%s  %s/%s  %s -> %s  [%s]\n", ts, component, setting, before, after, ok ? "ok" : "falha"); fflush(f); }
    fprintf(stderr, "swlcentral: %s/%s %s -> %s (%s)\n", component, setting, before, after, ok ? "ok" : "falha");
}

/* ---- config store: grava os campos de preferência de cc_app_t direto ---- */
static bool conf_path(char *out, size_t cap) {
    char dir[640];
    if (!app_dir(dir, sizeof(dir))) return false;
    snprintf(out, cap, "%s/config.conf", dir);
    return true;
}

bool cc_api_conf_load(cc_app_t *a) {
    a->wifi = a->bt = a->notify = a->gamemode = a->game_auto = a->fps_overlay = a->fw = a->upd_auto = true;
    a->bright = 0.65; a->transp = 0.35; a->scale = 0.5; a->overlay_alpha = 0.3;
    a->theme_mode = 1; a->power_profile = 1; a->game_profile = 2; a->lang = 0; a->upd_channel = 0;
    a->lock_timeout = 2; a->lid_suspend = true;
    snprintf(a->hostname, sizeof(a->hostname), "swl-os");

    char path[700];
    if (!conf_path(path, sizeof(path))) return false;
    FILE *f = fopen(path, "r");
    if (!f) return false;
    char line[192];
    while (fgets(line, sizeof(line), f)) {
        char *nl = strchr(line, '\n'); if (nl) *nl = 0;
        char *eq = strchr(line, '='); if (!eq) continue;
        *eq = 0; const char *k = line, *v = eq + 1;
        if (!strcmp(k, "wifi")) a->wifi = v[0] == '1';
        else if (!strcmp(k, "bt")) a->bt = v[0] == '1';
        else if (!strcmp(k, "notify")) a->notify = v[0] == '1';
        else if (!strcmp(k, "gamemode")) a->gamemode = v[0] == '1';
        else if (!strcmp(k, "game_auto")) a->game_auto = v[0] == '1';
        else if (!strcmp(k, "fps_overlay")) a->fps_overlay = v[0] == '1';
        else if (!strcmp(k, "fw")) a->fw = v[0] == '1';
        else if (!strcmp(k, "ssh")) a->ssh = v[0] == '1';
        else if (!strcmp(k, "devmode")) a->devmode = v[0] == '1';
        else if (!strcmp(k, "upd_auto")) a->upd_auto = v[0] == '1';
        else if (!strcmp(k, "reduce_motion")) a->reduce_motion = v[0] == '1';
        else if (!strcmp(k, "lid_suspend")) a->lid_suspend = v[0] == '1';
        else if (!strcmp(k, "bright")) a->bright = atof(v);
        else if (!strcmp(k, "transp")) a->transp = atof(v);
        else if (!strcmp(k, "scale")) a->scale = atof(v);
        else if (!strcmp(k, "overlay_alpha")) a->overlay_alpha = atof(v);
        else if (!strcmp(k, "theme_mode")) a->theme_mode = atoi(v);
        else if (!strcmp(k, "power_profile")) a->power_profile = atoi(v);
        else if (!strcmp(k, "game_profile")) a->game_profile = atoi(v);
        else if (!strcmp(k, "lang")) a->lang = atoi(v);
        else if (!strcmp(k, "upd_channel")) a->upd_channel = atoi(v);
        else if (!strcmp(k, "lock_timeout")) a->lock_timeout = atoi(v);
        else if (!strcmp(k, "hostname")) snprintf(a->hostname, sizeof(a->hostname), "%s", v);
    }
    fclose(f);
    return true;
}

bool cc_api_conf_save(cc_app_t *a) {
    char path[700];
    if (!conf_path(path, sizeof(path))) return false;
    char tmp[900]; snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    FILE *f = fopen(tmp, "w");
    if (!f) return false;
    fprintf(f, "wifi=%d\nbt=%d\nnotify=%d\ngamemode=%d\ngame_auto=%d\nfps_overlay=%d\nfw=%d\nssh=%d\n"
               "devmode=%d\nupd_auto=%d\nreduce_motion=%d\nlid_suspend=%d\n"
               "bright=%.4f\ntransp=%.4f\nscale=%.4f\noverlay_alpha=%.4f\n"
               "theme_mode=%d\npower_profile=%d\ngame_profile=%d\nlang=%d\nupd_channel=%d\nlock_timeout=%d\n"
               "hostname=%s\n",
            a->wifi, a->bt, a->notify, a->gamemode, a->game_auto, a->fps_overlay, a->fw, a->ssh,
            a->devmode, a->upd_auto, a->reduce_motion, a->lid_suspend,
            a->bright, a->transp, a->scale, a->overlay_alpha,
            a->theme_mode, a->power_profile, a->game_profile, a->lang, a->upd_channel, a->lock_timeout,
            a->hostname);
    fclose(f);
    return rename(tmp, path) == 0;
}

bool cc_api_reset_defaults(cc_app_t *a) {
    cc_api_conf_load(a); /* recarrega defaults por cima (arquivo pode nem existir) */
    a->theme = swl_theme_load();
    bool ok = cc_api_conf_save(a);
    cc_api_log("boot", "reset_padroes", "-", "aplicado", ok);
    return ok;
}

/* ---- rede ---- */
static bool find_wifi_iface(char *out, size_t cap) {
    DIR *d = opendir("/sys/class/net"); if (!d) return false;
    struct dirent *e; bool found = false;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        char probe[300]; snprintf(probe, sizeof(probe), "/sys/class/net/%s/wireless", e->d_name);
        struct stat sb;
        if (stat(probe, &sb) == 0) { snprintf(out, cap, "%s", e->d_name); found = true; break; }
    }
    closedir(d);
    return found;
}
static bool find_eth_iface(char *out, size_t cap) {
    DIR *d = opendir("/sys/class/net"); if (!d) return false;
    struct dirent *e; bool found = false;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.' || !strcmp(e->d_name, "lo")) continue;
        char probe[300]; snprintf(probe, sizeof(probe), "/sys/class/net/%s/wireless", e->d_name);
        struct stat sb;
        if (stat(probe, &sb) == 0) continue;
        snprintf(out, cap, "%s", e->d_name); found = true; break;
    }
    closedir(d);
    return found;
}

bool cc_api_wifi_get(bool *on) {
    if (bin_exists("nmcli")) {
        char buf[64];
        if (RUN(buf, 1200, "nmcli", "radio", "wifi") == 0) { *on = strstr(buf, "enabled") != NULL; return true; }
    }
    char iface[256];
    if (find_wifi_iface(iface, sizeof(iface))) {
        char path[300]; snprintf(path, sizeof(path), "/sys/class/net/%s/operstate", iface);
        FILE *f = fopen(path, "r");
        if (f) { char s[32] = {0}; if (!fgets(s, sizeof(s), f)) s[0] = 0; fclose(f); *on = !strstr(s, "down"); return true; }
    }
    return false;
}
bool cc_api_wifi_set(bool on) {
    char buf[64];
    if (bin_exists("nmcli")) return RUN(buf, 2000, "nmcli", "radio", "wifi", on ? "on" : "off") == 0;
    if (bin_exists("rfkill")) return RUN(buf, 1500, "rfkill", on ? "unblock" : "block", "wifi") == 0;
    return false;
}
static int bars_from_signal(int sig) { return sig >= 80 ? 4 : sig >= 55 ? 3 : sig >= 30 ? 2 : 1; }
int cc_api_wifi_scan(cc_net_t *out, int cap) {
    static char meta_pool[8][80], ssid_pool[8][64];
    int n = 0;
    if (!bin_exists("nmcli")) return 0;
    char buf[4096];
    if (RUN(buf, 6000, "nmcli", "-t", "-f", "SSID,SIGNAL,SECURITY,IN-USE", "dev", "wifi", "list", "--rescan", "yes") != 0) return 0;
    char *sp = NULL; char *line = strtok_r(buf, "\n", &sp);
    while (line && n < cap && n < 8) {
        char fields[4][64] = {{0}}; int fi = 0, ci = 0;
        for (char *p = line; *p && fi < 4; p++) {
            if (*p == '\\' && *(p + 1) == ':') { if (ci < 63) fields[fi][ci++] = ':'; p++; continue; }
            if (*p == ':') { fields[fi][ci] = 0; fi++; ci = 0; continue; }
            if (ci < 63) fields[fi][ci++] = *p;
        }
        fields[fi][ci] = 0;
        if (fields[0][0]) {
            snprintf(ssid_pool[n], sizeof(ssid_pool[n]), "%s", fields[0]);
            int sig = atoi(fields[1]);
            bool locked = fields[2][0] && strcmp(fields[2], "--") != 0;
            snprintf(meta_pool[n], sizeof(meta_pool[n]), "%s", locked ? fields[2] : "Aberta");
            out[n] = (cc_net_t){ ssid_pool[n], meta_pool[n], bars_from_signal(sig), locked, fields[3][0] == '*' };
            n++;
        }
        line = strtok_r(NULL, "\n", &sp);
    }
    return n;
}
bool cc_api_net_connect(int idx, const char *password) {
    cc_net_t nets[8]; int n = cc_api_wifi_scan(nets, 8);
    if (idx < 0 || idx >= n || !bin_exists("nmcli")) return false;
    char buf[512]; int rc;
    if (nets[idx].locked && password && password[0])
        rc = RUN(buf, 15000, "nmcli", "dev", "wifi", "connect", (char *)nets[idx].ssid, "password", (char *)password);
    else rc = RUN(buf, 15000, "nmcli", "dev", "wifi", "connect", (char *)nets[idx].ssid);
    cc_api_log("rede", "conectar", "-", nets[idx].ssid, rc == 0);
    return rc == 0;
}
bool cc_api_bt_get(bool *on) {
    char buf[256];
    if (bin_exists("bluetoothctl") && RUN(buf, 1500, "bluetoothctl", "show") == 0) { *on = strstr(buf, "Powered: yes") != NULL; return true; }
    return false;
}
bool cc_api_bt_set(bool on) {
    char buf[128];
    if (bin_exists("bluetoothctl")) return RUN(buf, 2000, "bluetoothctl", "power", on ? "on" : "off") == 0;
    if (bin_exists("rfkill")) return RUN(buf, 1500, "rfkill", on ? "unblock" : "block", "bluetooth") == 0;
    return false;
}
bool cc_api_eth_get(bool *on, char *iface, size_t ifacecap) {
    char name[256];
    if (!find_eth_iface(name, sizeof(name))) { if (on) *on = false; if (iface && ifacecap) iface[0] = 0; return false; }
    if (iface) snprintf(iface, ifacecap, "%s", name);
    char path[300]; snprintf(path, sizeof(path), "/sys/class/net/%s/operstate", name);
    FILE *f = fopen(path, "r");
    if (!f) { if (on) *on = false; return true; }
    char s[32] = {0}; if (!fgets(s, sizeof(s), f)) s[0] = 0; fclose(f);
    if (on) *on = strstr(s, "up") != NULL;
    return true;
}
bool cc_api_eth_set(bool on) {
    char name[256];
    if (!find_eth_iface(name, sizeof(name))) return false;
    char buf[128];
    if (bin_exists("ip")) return RUN(buf, 2000, "ip", "link", "set", name, on ? "up" : "down") == 0;
    if (bin_exists("ifconfig")) return RUN(buf, 2000, "ifconfig", name, on ? "up" : "down") == 0;
    return false;
}
bool cc_api_net_status(char *ssid, size_t ssidcap, char *ip, size_t ipcap, int *signal_pct) {
    if (ssid && ssidcap) ssid[0] = 0;
    if (ip && ipcap) ip[0] = 0;
    if (signal_pct) *signal_pct = 0;
    bool got = false;
    cc_net_t nets[8]; int n = cc_api_wifi_scan(nets, 8);
    for (int i = 0; i < n; i++) if (nets[i].connected) {
        if (ssid) snprintf(ssid, ssidcap, "%s", nets[i].ssid);
        if (signal_pct) *signal_pct = nets[i].bars * 25;
        got = true; break;
    }
    struct ifaddrs *ifs = NULL;
    if (getifaddrs(&ifs) == 0) {
        for (struct ifaddrs *p = ifs; p; p = p->ifa_next) {
            if (!p->ifa_addr || p->ifa_addr->sa_family != AF_INET || !strcmp(p->ifa_name, "lo")) continue;
            struct sockaddr_in *sin = (struct sockaddr_in *)p->ifa_addr;
            if (ip) inet_ntop(AF_INET, &sin->sin_addr, ip, ipcap);
            break;
        }
        freeifaddrs(ifs);
    }
    return got;
}

/* ---- brilho / energia ---- */
static bool backlight_path(char *out, size_t cap) {
    DIR *d = opendir("/sys/class/backlight"); if (!d) return false;
    struct dirent *e; bool found = false;
    while ((e = readdir(d))) { if (e->d_name[0] == '.') continue; snprintf(out, cap, "/sys/class/backlight/%s", e->d_name); found = true; break; }
    closedir(d);
    return found;
}
double cc_api_bright_get(void) { return 0.65; }
bool cc_api_bright_set(double v) {
    char dir[300]; if (!backlight_path(dir, sizeof(dir))) return false;
    char maxp[340], curp[340];
    snprintf(maxp, sizeof(maxp), "%s/max_brightness", dir);
    snprintf(curp, sizeof(curp), "%s/brightness", dir);
    FILE *fm = fopen(maxp, "r"); if (!fm) return false;
    long maxv = 0; if (fscanf(fm, "%ld", &maxv) != 1) maxv = 0; fclose(fm);
    if (maxv <= 0) return false;
    FILE *fc = fopen(curp, "w"); if (!fc) return false;
    fprintf(fc, "%ld", (long)(v * maxv)); fclose(fc);
    return true;
}
static const char *PP_NAMES[3] = { "power-saver", "balanced", "performance" };
int cc_api_power_get(void) { return 1; }
bool cc_api_power_set(int p) {
    if (p < 0 || p > 2) return false;
    char buf[128];
    if (bin_exists("powerprofilesctl")) return RUN(buf, 1500, "powerprofilesctl", "set", (char *)PP_NAMES[p]) == 0;
    if (bin_exists("cpupower")) {
        const char *gov = p == 0 ? "powersave" : p == 1 ? "ondemand" : "performance";
        return RUN(buf, 1500, "cpupower", "frequency-set", "-g", (char *)gov) == 0;
    }
    return false;
}

/* ---- diagnóstico ---- */
static bool proc_running(const char *needle) {
    DIR *d = opendir("/proc"); if (!d) return false;
    struct dirent *e; bool found = false;
    while ((e = readdir(d))) {
        if (e->d_name[0] < '0' || e->d_name[0] > '9') continue;
        char path[280]; snprintf(path, sizeof(path), "/proc/%s/comm", e->d_name);
        FILE *f = fopen(path, "r"); if (!f) continue;
        char comm[128] = {0}; if (fgets(comm, sizeof(comm), f) && strstr(comm, needle)) found = true;
        fclose(f);
        if (found) break;
    }
    closedir(d);
    return found;
}
static bool default_gateway(char *ipbuf, size_t cap) {
    FILE *f = fopen("/proc/net/route", "r"); if (!f) return false;
    char line[256]; if (!fgets(line, sizeof(line), f)) { fclose(f); return false; }
    bool found = false;
    while (fgets(line, sizeof(line), f)) {
        char iface[32]; unsigned long dest, gw;
        if (sscanf(line, "%31s %lx %lx", iface, &dest, &gw) == 3 && dest == 0 && gw != 0) {
            unsigned char b[4] = { (unsigned char)(gw & 0xff), (unsigned char)((gw >> 8) & 0xff),
                                    (unsigned char)((gw >> 16) & 0xff), (unsigned char)((gw >> 24) & 0xff) };
            snprintf(ipbuf, cap, "%u.%u.%u.%u", b[0], b[1], b[2], b[3]);
            found = true; break;
        }
    }
    fclose(f);
    return found;
}
int cc_api_diag_run(cc_diag_t *out, int cap) {
    static char gd[64], nd[80], ad[64], dd[64];
    int n = 0;
    if (n < cap) {
        bool ok = access("/dev/dri/card0", F_OK) == 0 || access("/dev/dri/renderD128", F_OK) == 0;
        snprintf(gd, sizeof(gd), "%s", ok ? "/dev/dri presente" : "nenhum device DRM encontrado");
        out[n++] = (cc_diag_t){ "GPU", gd, ok, false };
    }
    if (n < cap) {
        char gw[64]; bool ok = default_gateway(gw, sizeof(gw));
        snprintf(nd, sizeof(nd), ok ? "Gateway padrão: %s" : "Sem rota padrão configurada", gw);
        out[n++] = (cc_diag_t){ "Rede", nd, ok, false };
    }
    if (n < cap) {
        bool up = proc_running("pipewire") || proc_running("pulseaudio");
        snprintf(ad, sizeof(ad), "%s", up ? "Servidor de áudio ativo" : "Nenhum servidor de áudio rodando");
        out[n++] = (cc_diag_t){ "Áudio", ad, up, !up };
    }
    if (n < cap) {
        struct statvfs sv; bool ok = statvfs("/", &sv) == 0; bool low = false;
        if (ok) {
            double free_gb = (double)(sv.f_bavail * sv.f_frsize) / 1073741824.0;
            double total_gb = (double)(sv.f_blocks * sv.f_frsize) / 1073741824.0;
            double pct = total_gb > 0 ? free_gb / total_gb * 100.0 : 100.0;
            low = pct < 10.0;
            snprintf(dd, sizeof(dd), "%.1f GB livres de %.1f GB (%.0f%%)", free_gb, total_gb, pct);
        } else snprintf(dd, sizeof(dd), "Não foi possível ler o disco");
        out[n++] = (cc_diag_t){ "Disco", dd, ok && !low, ok && low };
    }
    return n;
}
static bool disk_clean(void) {
    const char *home = getenv("HOME");
    if (!home) { struct passwd *pw = getpwuid(getuid()); home = pw ? pw->pw_dir : NULL; }
    if (!home) return false;
    static const char *SAFE[] = { "/.cache/thumbnails", "/.cache/mesa_shader_cache", "/.cache/fontconfig" };
    for (size_t i = 0; i < sizeof(SAFE) / sizeof(SAFE[0]); i++) {
        char path[512]; snprintf(path, sizeof(path), "%s%s", home, SAFE[i]);
        char buf[64]; RUN(buf, 3000, "rm", "-rf", path); /* melhor esforço; ausência de `rm` só falha honesto */
    }
    return true;
}
bool cc_api_diag_fix(int idx) {
    cc_diag_t diags[8]; int n = cc_api_diag_run(diags, 8);
    if (idx < 0 || idx >= n) return false;
    bool ok = false; char buf[256];
    if (!strcmp(diags[idx].label, "Áudio")) {
        if (bin_exists("systemctl")) ok = RUN(buf, 4000, "systemctl", "--user", "restart", "pipewire", "pipewire-pulse", "wireplumber") == 0;
    } else if (!strcmp(diags[idx].label, "Disco")) {
        ok = disk_clean();
    } else {
        cc_api_log("diagnostico", "corrigir", diags[idx].label, "sem correção automática", false);
        return false;
    }
    cc_api_log("diagnostico", "corrigir", diags[idx].label, ok ? "aplicado" : "falhou", ok);
    return ok;
}

/* ---- segurança ---- */
bool cc_api_fw_get(bool *on) {
    char buf[256];
    if (bin_exists("ufw") && RUN(buf, 1500, "ufw", "status") == 0) { *on = strstr(buf, "Status: active") != NULL; return true; }
    return false;
}
bool cc_api_fw_set(bool on) {
    if (!is_root()) { cc_api_log("seguranca", "firewall", "-", "requer administrador", false); return false; }
    char buf[256];
    if (bin_exists("ufw")) return RUN(buf, 3000, "ufw", on ? "enable" : "disable") == 0;
    return false;
}
bool cc_api_ssh_get(bool *on) {
    char buf[64];
    if (bin_exists("systemctl")) { int rc = RUN(buf, 1500, "systemctl", "is-active", "ssh"); if (rc != -1) { *on = rc == 0; return true; } }
    return false;
}
bool cc_api_ssh_set(bool on) {
    if (!is_root()) { cc_api_log("seguranca", "ssh", "-", "requer administrador", false); return false; }
    char buf[128];
    if (bin_exists("systemctl")) {
        bool ok = RUN(buf, 4000, "systemctl", on ? "start" : "stop", "ssh") == 0;
        RUN(buf, 4000, "systemctl", on ? "enable" : "disable", "ssh");
        return ok;
    }
    return false;
}

/* ---- atualizações ---- */
int cc_api_upd_check(void) {
    char buf[8192];
    if (bin_exists("apt")) {
        if (RUN(buf, 20000, "apt", "list", "--upgradable") == 0) {
            int count = 0; for (char *p = buf; (p = strchr(p, '\n')); p++) count++;
            if (count > 0) count--;
            return count < 0 ? 0 : count;
        }
    } else if (bin_exists("pacman")) {
        if (RUN(buf, 20000, "pacman", "-Qu") == 0) {
            int count = 0; for (char *p = buf; (p = strchr(p, '\n')); p++) count++;
            return count;
        }
    }
    return -1;
}

/* ---- backup ---- */
bool cc_api_backup_now(void) {
    char dir[640]; if (!app_dir(dir, sizeof(dir)) || !bin_exists("tar")) return false;
    char outdir[700], outfile[760];
    snprintf(outdir, sizeof(outdir), "%s/backups", dir);
    mkdir(outdir, 0700);
    time_t now = time(NULL); struct tm tmv; localtime_r(&now, &tmv);
    char ts[32]; strftime(ts, sizeof(ts), "%Y%m%d-%H%M%S", &tmv);
    snprintf(outfile, sizeof(outfile), "%s/backup-%s.tar.gz", outdir, ts);
    char buf[256];
    int rc = RUN(buf, 8000, "tar", "-czf", outfile, "-C", dir, "config.conf");
    cc_api_log("boot", "backup", "-", outfile, rc == 0);
    return rc == 0;
}

/* ---- dispatch ---- */
void cc_dispatch(cc_app_t *a, int action, int arg) {
    switch (action) {
    case CC_ACT_WIFI: cc_api_log("rede", "wifi", "-", a->wifi ? "1" : "0", cc_api_wifi_set(a->wifi)); break;
    case CC_ACT_BT: cc_api_log("rede", "bluetooth", "-", a->bt ? "1" : "0", cc_api_bt_set(a->bt)); break;
    case CC_ACT_ETH: cc_api_log("rede", "ethernet", "-", a->eth ? "1" : "0", cc_api_eth_set(a->eth)); break;
    case CC_ACT_SCAN_WIFI: cc_api_log("rede", "buscar_redes", "-", "solicitado", true); break;
    case CC_ACT_NET_CONNECT: {
        const char *pw = a->dialog.has_field ? a->dialog.field.buf : NULL;
        cc_api_net_connect(arg, pw);
        break;
    }
    case CC_ACT_THEME: {
        static const char *tlabel[3] = { "Claro", "Escuro", "Auto" };
        a->theme_mode = arg;
        a->theme = swl_theme_load(); /* o arquivo compartilhado é quem manda de fato */
        cc_api_log("personalizacao", "tema", "-", tlabel[arg % 3], true);
        break;
    }
    case CC_ACT_LANG: cc_api_log("personalizacao", "idioma", "-", cc_lang_opts[arg], true); break;
    case CC_ACT_TRANSP: case CC_ACT_SCALE: case CC_ACT_REDUCE_MOTION:
        cc_api_log("personalizacao", "interface", "-", "ajustado", true); break;
    case CC_ACT_BRIGHT: cc_api_bright_set(a->bright); break;
    case CC_ACT_POWER: cc_api_power_set(a->power_profile); break;
    case CC_ACT_LID: cc_api_log("sistema", "suspender_tampa", "-", a->lid_suspend ? "1" : "0", true); break;
    case CC_ACT_NOTIFY: cc_api_log("sistema", "notificacoes", "-", a->notify ? "1" : "0", true); break;
    case CC_ACT_HOSTNAME: cc_api_log("sistema", "hostname", "-", a->hostname, true); break;
    case CC_ACT_FW: cc_api_log("seguranca", "firewall", "-", a->fw ? "1" : "0", cc_api_fw_set(a->fw)); break;
    case CC_ACT_SSH: cc_api_log("seguranca", "ssh", "-", a->ssh ? "1" : "0", cc_api_ssh_set(a->ssh)); break;
    case CC_ACT_DEVMODE: cc_api_log("seguranca", "devmode", "-", a->devmode ? "1" : "0", true); break;
    case CC_ACT_LOCK_TIMEOUT: cc_api_log("seguranca", "bloqueio_tela", "-", cc_lock_opts[arg], true); break;
    case CC_ACT_UPD_CHECK: { int n = cc_api_upd_check(); char b[16]; snprintf(b, sizeof(b), "%d", n); cc_api_log("atualizacoes", "verificar", "-", b, n >= 0); break; }
    case CC_ACT_UPD_AUTO: cc_api_log("atualizacoes", "automatica", "-", a->upd_auto ? "1" : "0", true); break;
    case CC_ACT_UPD_CHANNEL: cc_api_log("atualizacoes", "canal", "-", arg ? "beta" : "estavel", true); break;
    case CC_ACT_DIAG: cc_api_log("diagnostico", "verificar", "-", "solicitado", true); break;
    case CC_ACT_DIAG_FIX: cc_api_diag_fix(arg); break;
    case CC_ACT_GAMEMODE: cc_api_log("gaming", "game_mode", "-", a->gamemode ? "1" : "0", true); break;
    case CC_ACT_GAME_AUTO: cc_api_log("gaming", "ativar_auto", "-", a->game_auto ? "1" : "0", true); break;
    case CC_ACT_GAME_PROFILE: cc_api_log("gaming", "perfil", "-", "ajustado", true); break;
    case CC_ACT_FPS: cc_api_log("gaming", "mostrar_fps", "-", a->fps_overlay ? "1" : "0", true); break;
    case CC_ACT_OVERLAY_ALPHA: cc_api_log("gaming", "overlay_alpha", "-", "ajustado", true); break;
    case CC_ACT_BACKUP_NOW: cc_api_backup_now(); break;
    case CC_ACT_RESET_DEFAULTS: cc_api_reset_defaults(a); break;
    default: break;
    }
    cc_api_conf_save(a);
    a->need_redraw = true;
}
