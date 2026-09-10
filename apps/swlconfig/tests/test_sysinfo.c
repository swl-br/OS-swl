/* Testes unitários dos parsers puros de sysinfo.c — não tocam em
 * arquivo real, usam texto de exemplo fixo (assim o teste não depende
 * de quanta RAM a máquina que roda o teste tem). */
#include "sysinfo.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_meminfo_modern_kernel(void) {
	printf("test_meminfo_modern_kernel... ");
	const char *fixture =
		"MemTotal:       16384000 kB\n"
		"MemFree:         2048000 kB\n"
		"MemAvailable:    8192000 kB\n"
		"Buffers:          512000 kB\n";

	long total, avail;
	bool ok = swlconfig_parse_meminfo(fixture, &total, &avail);
	assert(ok);
	assert(total == 16384000);
	assert(avail == 8192000);
	printf("ok\n");
}

static void test_meminfo_old_kernel_fallback_to_memfree(void) {
	printf("test_meminfo_old_kernel_fallback_to_memfree... ");
	/* kernel pré-3.14 não tem MemAvailable */
	const char *fixture =
		"MemTotal:       8192000 kB\n"
		"MemFree:        1024000 kB\n"
		"Buffers:         256000 kB\n";

	long total, avail;
	bool ok = swlconfig_parse_meminfo(fixture, &total, &avail);
	assert(ok);
	assert(total == 8192000);
	assert(avail == 1024000); /* caiu pro fallback MemFree */
	printf("ok\n");
}

static void test_meminfo_invalid_text(void) {
	printf("test_meminfo_invalid_text... ");
	const char *fixture = "isso nao eh um meminfo de verdade\nlixo\n";
	long total, avail;
	bool ok = swlconfig_parse_meminfo(fixture, &total, &avail);
	assert(!ok);
	printf("ok\n");
}

static void test_meminfo_empty_text(void) {
	printf("test_meminfo_empty_text... ");
	long total, avail;
	bool ok = swlconfig_parse_meminfo("", &total, &avail);
	assert(!ok);
	printf("ok\n");
}

static void test_uptime_valid(void) {
	printf("test_uptime_valid... ");
	long secs;
	bool ok = swlconfig_parse_uptime("12345.67 98765.43\n", &secs);
	assert(ok);
	assert(secs == 12345);
	printf("ok\n");
}

static void test_uptime_only_one_number(void) {
	printf("test_uptime_only_one_number... ");
	/* /proc/uptime sempre tem 2 números, mas o parser só precisa do
	 * primeiro — não deve exigir o segundo pra funcionar */
	long secs;
	bool ok = swlconfig_parse_uptime("42.5", &secs);
	assert(ok);
	assert(secs == 42);
	printf("ok\n");
}

static void test_uptime_invalid_text(void) {
	printf("test_uptime_invalid_text... ");
	long secs;
	bool ok = swlconfig_parse_uptime("nao eh numero nenhum", &secs);
	assert(!ok);
	printf("ok\n");
}

static void test_sysinfo_collect_real_system(void) {
	printf("test_sysinfo_collect_real_system... ");
	/* Único teste que toca o sistema real (não dá pra evitar pra essa
	 * função específica) — só confere que não quebra e que pelo menos
	 * uma fonte real respondeu, sem prever valores exatos (variam por
	 * máquina). */
	swlconfig_sysinfo_t info;
	bool ok = swlconfig_sysinfo_collect(&info);
	assert(ok);
	assert(strlen(info.kernel_release) > 0);
	assert(info.mem_total_kb > 0);
	assert(info.disk_total_gb > 0.0);
	printf("ok (kernel: %s, mem total: %ld kB, disco: %.1f GB)\n",
	       info.kernel_release, info.mem_total_kb, info.disk_total_gb);
}

int main(void) {
	test_meminfo_modern_kernel();
	test_meminfo_old_kernel_fallback_to_memfree();
	test_meminfo_invalid_text();
	test_meminfo_empty_text();
	test_uptime_valid();
	test_uptime_only_one_number();
	test_uptime_invalid_text();
	test_sysinfo_collect_real_system();
	printf("\nTodos os testes passaram.\n");
	return 0;
}
