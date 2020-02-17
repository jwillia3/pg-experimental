#include <wctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "pg.h"
#include "platform.h"

struct host {
    void *data;
    int len;
};

void *_pgMapFile(void **hostp, const wchar_t *filename) {
    char *utf8Path = pgToUtf8(filename);
    int fd = open(utf8Path, O_RDONLY);
    struct stat stat;
    if (fd < 0)
        return NULL;
    fstat(fd, &stat);
    void *data = mmap(NULL, stat.st_size, PROT_READ, MAP_SHARED, fd, 0);
    close(fd); // file remains open as long as map is
    if (!data)
        return NULL;
    struct host *host = malloc(sizeof *host);
    *host = (struct host) { data, stat.st_size };
    *hostp = host;
    return data;
}

void _pgFreeFileMap(void *host) {
    munmap(((struct host *) host)->data, ((struct host *) host)->len);
    free(host);
}

static int compare(const void *a, const void *b) {
    return wcscmp(((PgFontFamily*) a)->name, ((PgFontFamily*) b)->name);
}

static PgFontFamily* findFamily(wchar_t *family) {
    for (int i = 0; i < PgNFontFamilies; i++)
        if (!wcsicmp(family, PgFontFamilies[i].name))
            return PgFontFamilies + i;

    PgFontFamilies = realloc(PgFontFamilies,
        ++PgNFontFamilies * sizeof *PgFontFamilies);

    PgFontFamilies[PgNFontFamilies - 1] = (PgFontFamily) {
        .name = wcsdup(family)
    };
    return PgFontFamilies + PgNFontFamilies - 1;
}

PgFontFamily *_pgScanFonts() {
    if (PgNFontFamilies)
        return PgFontFamilies;

    // This just calls `fc-list` to enumerate the fonts.
    // Otherwise, fc's XML configuration files would have to be parsed
    // to find the directories that fonts can be in.
    FILE *pipe = popen(
        // The space before ' %{spacing}' is important.
        // The field can be blank and wcstok() drops empty fields
        "fc-list -f '%{family[0]}:%{weight}: %{spacing}:%{slant}:%{width}:%{fontformat}:%{file}:%{index}\n'",
        "r");

    PgFontFamily *all = NULL;
    int n;

    size_t len = 0;
    char *buf = 0;
    while (getline(&buf, &len, pipe) > 0) {
        wchar_t *line = pgFromUtf8(buf);
        wchar_t *state;
        wchar_t *familyName = wcstok(line, L":\n", &state);
        int weight = wcstol(wcstok(NULL, L":\n", &state), NULL, 0);
        int spacing = wcstol(wcstok(NULL, L":\n", &state), NULL, 0);
        int slant = wcstol(wcstok(NULL, L":\n", &state), NULL, 0);
        int width = wcstol(wcstok(NULL, L":\n", &state), NULL, 0);
        wchar_t *format = wcstok(NULL, L":\n", &state);
        wchar_t *path = wcstok(NULL, L":\n", &state);
        int index = wcstol(wcstok(NULL, L":\n", &state), NULL, 0);

        width =
            width == 50? 0: // ultracondensed
            width == 63? 0: // extracondensed
            width == 75? 0: // condensed
            width == 86? 0: // semicondensed
            width == 100? 1: // normal
            width == 113? 2: // semiexpanded
            width == 125? 2: // expanded
            width == 150? 2: // extraexpanded
            width == 200? 2: // ultraexpanded
            1;

        weight =
            weight == 0? 100:   // thin
            weight == 40? 200:  // extralight
            weight == 50? 300:  // light
            weight == 55? 300:  // demilight
            weight == 75? 400:  // book
            weight == 80? 400:  // regular
            weight == 100? 500:  // medium
            weight == 180? 600:  // demibold
            weight == 200? 700:  // bold
            weight == 205? 800:  // extrabold
            weight == 210? 900:  // black
            400;

        // `fc-list` does not reliably give a good family name. e.g. it does not contain width
        wchar_t preferredName[128];
        swprintf(preferredName, 128,
            width == 0 && !wcsstr(familyName, L"Condensed")? L"%ls Condensed":
            width == 1? L"%ls":
            width == 2 && !wcsstr(familyName, L"Expanded")? L"%ls Expanded":
            L"%ls",
            familyName);
        familyName = preferredName;

        if (wcscmp(L"TrueType", format))
            continue;

        PgFontFamily *family = findFamily(familyName);

        struct pgFontDesc *desc = slant > 0
            ? family->italic
            : family->roman;

        // Do not overwrite description
        if (desc->path == NULL)
            desc[weight / 100] = (struct pgFontDesc) {
                .path = wcsdup(path),
                .index = index,
                .isFixedPitched = spacing == 100
            };
        family->hasFixedPitched |= spacing == 100;
        free(line);
    }
    free(buf);
    pclose(pipe);

    qsort(PgFontFamilies, PgNFontFamilies, sizeof *PgFontFamilies, compare);
    return PgFontFamilies;
}

int16_t native16(int16_t x) {
    return ntohs(x);
}
int32_t native32(int32_t x) {
    return ntohl(x);
}
uint16_t nativeu16(uint16_t x) {
    return ntohs(x);
}
uint32_t nativeu32(uint32_t x) {
    return ntohl(x);
}

int wcsicmp(const wchar_t *s1, const wchar_t *s2) {
    for ( ; *s1 && towlower(*s1) == towlower(*s2); s1++, s2++);
    return  towlower(*s1) < towlower(*s2)? -1:
            towlower(*s1) > towlower(*s2)? 1:
            0;
}
