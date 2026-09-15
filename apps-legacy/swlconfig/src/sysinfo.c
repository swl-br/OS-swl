#define _DEFAULT_SOURCE /* statvfs, uname — mesma justificativa de
                          * sempre neste projeto (c_std=c11 estrito
                          * esconde isso atrás de feature-test macros). */
#include "sysinfo.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/utsname.h>
#include <sys/statvfs.h>

bool swlconfig_parse_meminfo(const char *text, long *mem_total_kb, long *mem_available_kb) {
	*mem_total_kb = -1;
	*mem_available_kb = -1;
	long mem_free_kb = -1;

	const char *line = text;
	while (line && *line) {
		long value;
		if (sscanf(line, "MemTotal: %ld kB", &value) == 1) {
			*mem_total_kb = value;
		} else if (sscanf(line, "MemAvailable: %ld kB", &value) == 1) {
			*mem_available_kb = value;
		} else if (sscanf(line, "MemFree: %ld kB", &value) == 1) {
			mem_free_kb = value;
		}
		const char *nl = strchr(line, '\n');
		line = nl ? nl + 1 : NULL;
	}

	if (*mem_total_kb < 0) {
		return false; /* nem MemTotal achou: não é um meminfo válido */
	}
	if (*mem_available_kb < 0) {
		/* kernel antigo sem MemAvailable (pré-3.14): usa MemFree como
		 * aproximação por baixo (não conta cache/buffer reclamável). */
		*mem_available_kb = mem_free_kb;
	}
	return true;
}

bool swlconfig_parse_uptime(const char *text, long *uptime_seconds) {
	double secs;
	if (sscanf(text, "%lf", &secs) != 1) {
		*uptime_seconds = -1;
		return false;
	}
	*uptime_seconds = (long)secs;
	return true;
}

static char *read_whole_file(const char *path) {
	FILE *f = fopen(path, "rb");
	if (!f) return NULL;

	char *buf = NULL;
	size_t cap = 0, len = 0;
	char chunk[4096];
	size_t n;
	while ((n = fread(chunk, 1, sizeof(chunk), f)) > 0) {
		if (len + n + 1 > cap) {
			cap = (len + n + 1) * 2;
			buf = realloc(buf, cap);
		}
		memcpy(buf + len, chunk, n);
		len += n;
	}
	fclose(f);
	if (buf) buf[len] = '\0';
	return buf;
}

bool swlconfig_sysinfo_collect(swlconfig_sysinfo_t *out) {
	memset(out, 0, sizeof(*out));
	out->uptime_seconds = -1;
	out->mem_total_kb = -1;
	out->mem_available_kb = -1;
	out->disk_total_gb = -1.0;
	out->disk_free_gb = -1.0;

	bool got_anything = false;

	struct utsname uts;
	if (uname(&uts) == 0) {
		strncpy(out->kernel_release, uts.release, sizeof(out->kernel_release) - 1);
		strncpy(out->hostname, uts.nodename, sizeof(out->hostname) - 1);
		got_anything = true;
	}

	char *meminfo_text = read_whole_file("/proc/meminfo");
	if (meminfo_text) {
		if (swlconfig_parse_meminfo(meminfo_text, &out->mem_total_kb, &out->mem_available_kb)) {
			got_anything = true;
		}
		free(meminfo_text);
	}

	char *uptime_text = read_whole_file("/proc/uptime");
	if (uptime_text) {
		if (swlconfig_parse_uptime(uptime_text, &out->uptime_seconds)) {
			got_anything = true;
		}
		free(uptime_text);
	}

	struct statvfs vfs;
	if (statvfs("/", &vfs) == 0) {
		double block_gb = (double)vfs.f_frsize / (1024.0 * 1024.0 * 1024.0);
		out->disk_total_gb = (double)vfs.f_blocks * block_gb;
		out->disk_free_gb = (double)vfs.f_bavail * block_gb;
		got_anything = true;
	}

	return got_anything;
}
