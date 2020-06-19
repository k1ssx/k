#define _POSIX_C_SOURCE 200809L
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include <unistd.h>
#include <sys/stat.h>

#include "sha.h"
#include "log.h"
#include "checksum.h"
#include "util.h"
#include "pkg.h"

void pkg_checksums(package *pkg) {
    FILE *src;
    unsigned char buf[1000];
    unsigned char shasum[32];
    char *base;
    sha256_ctx ctx;
    int i;
    int j;

    pkg->sums = xmalloc(sizeof(char *) * pkg->src_len + 1);

    for (i = 0; i < pkg->src_len; i++) {
        src  = xfopen(pkg->src[i], "rb");
        base = basename(pkg->src[i]);
        pkg->sums[i] = xmalloc(67 + strlen(base));

        sha256_init(&ctx);
        while ((j = fread(buf, 1, sizeof(buf), src)) > 0)
            sha256_update(&ctx, buf, j);
        sha256_final(&ctx, shasum);

        snprintf(pkg->sums[i], 67 + strlen(base), \
"%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\
%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x  %s",  
            shasum[0],  shasum[1],  shasum[2],  shasum[3],
            shasum[4],  shasum[5],  shasum[6],  shasum[7],
            shasum[8],  shasum[9],  shasum[10], shasum[11],
            shasum[12], shasum[13], shasum[14], shasum[15],
            shasum[16], shasum[17], shasum[18], shasum[19],
            shasum[20], shasum[21], shasum[22], shasum[23],
            shasum[24], shasum[25], shasum[26], shasum[27],
            shasum[28], shasum[29], shasum[30], shasum[31],
            base
        );

        printf("%s\n", pkg->sums[i]);
        fclose(src);
    }

    checksum_to_file(pkg);
}

void pkg_verify(package *pkg) {
    FILE *file;
    char *buf = 0;
    int i = 0;

    msg("[%s] Verifying checksums", pkg->name);
    pkg_checksums(pkg);

    if (pkg->src_len == 0)
        die("Sources file does not exist");

    xchdir(*pkg->path);
    file = xfopen("checksums", "r");

    while ((getline(&buf, &(size_t){0}, file) != -1)) {
        buf[strcspn(buf, "\n")] = 0;

        if (strcmp(pkg->sums[i], buf) != 0 || i > pkg->src_len)
            die("Checksums mismatch'");

        i++;
    }

    fclose(file);
    free(buf);
    msg("[%s] Verified checksums", pkg->name);
}

void checksum_to_file(package *pkg) {
    FILE *file;

    xchdir(pkg->path[0]);
    file = xfopen("checksums", "w");

    for (int i = 0; i < pkg->src_len; i++)
        fprintf(file, "%s\n", pkg->sums[i]);

    fclose(file);
    msg("[%s] Generated checksums", pkg->name);
}