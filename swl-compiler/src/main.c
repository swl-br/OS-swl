/*
 * main.c -- swlc driver: lex + parse + check + codegen.
 *
 * Usage: swlc [-o out.asm] file.swl
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "swlc.h"

static char *read_file(const char *path, long *len_out)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        fprintf(stderr, "swlc: cannot open '%s'\n", path);
        exit(1);
    }
    fseek(fp, 0, SEEK_END);
    long n = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char *buf = xmalloc((size_t)n + 1);
    if (fread(buf, 1, (size_t)n, fp) != (size_t)n) {
        fprintf(stderr, "swlc: cannot read '%s'\n", path);
        exit(1);
    }
    fclose(fp);
    buf[n] = '\0';
    *len_out = n;
    return buf;
}

static char *default_out(const char *in)
{
    /* replace the extension of the input path with ".asm" */
    const char *slash = strrchr(in, '/');
    const char *dot = strrchr(in, '.');
    size_t base;
    if (dot && (!slash || dot > slash))
        base = (size_t)(dot - in);
    else
        base = strlen(in);
    char *out = xmalloc(base + 5);
    memcpy(out, in, base);
    strcpy(out + base, ".asm");
    return out;
}

static void usage(void)
{
    fprintf(stderr,
            "usage: swlc [-o out.asm] file.swl\n"
            "  compiles a SWL MVP module to x86 32-bit NASM assembly\n"
            "  (default output: <file>.asm)\n");
    exit(2);
}

int main(int argc, char **argv)
{
    const char *infile = NULL;
    const char *outfile = NULL;
    int i;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 >= argc)
                usage();
            outfile = argv[++i];
        } else if (argv[i][0] == '-' && argv[i][1] != '\0') {
            usage();
        } else {
            if (infile)
                usage();
            infile = argv[i];
        }
    }
    if (!infile)
        usage();

    g_filename = infile;

    long len;
    char *src = read_file(infile, &len);
    (void)len;

    int ntok;
    Token *toks = lex(src, &ntok);

    Program prog;
    memset(&prog, 0, sizeof(prog));
    parse_program(toks, ntok, &prog);
    sema_check(&prog);

    if (!outfile)
        outfile = default_out(infile);
    codegen(&prog, outfile);

    free(ptrs);
    free(arrays);
    return 0;
}
