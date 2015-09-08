#define UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#pragma comment(lib, "ws2_32.lib")
#include "pg.h"
#include "platform.h"
#include "util.h"


typedef struct {
    HANDLE file;
    HANDLE mapping;
    HANDLE view;
} Host;

void *_pgMapFile(void **hostp, const wchar_t *filename) {
    Host *host = *hostp = malloc(sizeof *host);
    host->file = CreateFile(filename,
        GENERIC_READ,
        FILE_SHARE_DELETE | FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        0,
        NULL);
    if (host->file == INVALID_HANDLE_VALUE)
        return NULL;
    size_t size = GetFileSize(host->file, NULL);
    
    host->mapping = CreateFileMapping(host->file,
        NULL,
        PAGE_READONLY,
        0, size,
        NULL);
    
    if (!host->mapping) {
        CloseHandle(host->file);
        return NULL;
    }
    
    host->view = MapViewOfFile(host->mapping, FILE_MAP_READ, 0, 0, 0);
    if (!host->view) {
        CloseHandle(host->mapping);
        CloseHandle(host->file);
        return NULL;
    }
    
    return host->view;
}

void _pgFreeFileMap(void *host) {
    UnmapViewOfFile(((Host*)host)->view);
    CloseHandle(((Host*)host)->mapping);
    CloseHandle(((Host*)host)->file);
    free(host);
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

void _pgScanDirectory(const wchar_t *dir, void perFile(const wchar_t *name, void *data)) {
    WIN32_FIND_DATA data;
    wchar_t search[MAX_PATH];
    if (wcslen(dir) >= MAX_PATH - 2)
        return;
    wcscpy(search, dir);
    wcscat(search, L"/*");
    
    HANDLE h = FindFirstFile(search, &data);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            wchar_t full[MAX_PATH*2];
            void *host;
            void *view;
            
            swprintf(full, MAX_PATH * 2, L"%ls\\%ls", dir, data.cFileName);
            if (view = _pgMapFile(&host, full)) {
                perFile(full, view);
                _pgFreeFileMap(host);
            }
        } while (FindNextFile(h, &data));
        FindClose(h);
    }
}
const wchar_t *_pgGetHomeDir() {
    static wchar_t dir[MAX_PATH];
    GetEnvironmentVariable(L"USERPROFILE", dir, MAX_PATH);
    return dir;
}
const wchar_t *_pgGetConfigDir() {
    static wchar_t dir[MAX_PATH];
    const wchar_t *home = _pgGetHomeDir();
    if (!home)
        return NULL;
    wcscpy(dir, home);
    wcscat(dir, L"/.pg");
    return dir;
}
static void scanPerFile(const wchar_t *filename, void *data) {
    int nfonts = 1;
    for (int index = 0; index < nfonts; index++) {
        PgFont *font = (PgFont*)pgLoadFontHeader(data, index);
        if (!font)
            continue;
        nfonts = font->nfonts;
        
        const wchar_t *family = pgGetFontFamilyName(font);
        int i, relation = 0;
        for (i = 0; i < PgNFontFamilies; i++) {
            relation = wcsicmp(family, PgFontFamilies[i].name);
            if (relation <= 0)
                break;
        }
        
        if (relation < 0 || i == PgNFontFamilies) {
            PgFontFamilies = realloc(PgFontFamilies, (PgNFontFamilies + 1) * sizeof *PgFontFamilies);
            for (int j = PgNFontFamilies; j > i; j--)
                PgFontFamilies[j] = PgFontFamilies[j - 1];
            memset(&PgFontFamilies[i], 0, sizeof *PgFontFamilies);
            PgFontFamilies[i].name = wcsdup(family);
            PgNFontFamilies++;
        }
        
        int weight = pgGetFontWeight(font) / 100;
        if (weight >= 0 && weight < 10) {
            const wchar_t **slot = pgIsFontItalic(font) ?
                &PgFontFamilies[i].italic[weight]:
                &PgFontFamilies[i].roman[weight];
            if (*slot)
                free((void*) *slot);
            *slot = wcsdup(filename);
            if (pgIsFontItalic(font))
                PgFontFamilies[i].italicIndex[weight] = index;
            else
                PgFontFamilies[i].romanIndex[weight] = index;
        }
        pgFreeFont(font);
    }
}
PgFontFamily *_pgScanFonts() {
    static wchar_t dir[MAX_PATH];
    static wchar_t path[MAX_PATH];
    GetEnvironmentVariable(L"WINDIR", dir, MAX_PATH);
    wcscat(dir, L"/Fonts");
    
    const wchar_t *config = _pgGetConfigDir();
    if (!config)
        return PgFontFamilies;
    wcscpy(path, config);
    CreateDirectory(path, NULL);
    wcscat(path, L"/fonts");
    
    FILE *file = _wfopen(path, L"r");
    if (file) {
        wchar_t line[65536];
        wchar_t buf[65536];
        if (fgetws(line, 65536, file))
            if (1 != swscanf(line, L"%d\n", &PgNFontFamilies))
                goto rebuild;
        PgFontFamilies = calloc(PgNFontFamilies, sizeof *PgFontFamilies);
        int i = -1;
        while (fgetws(line, 65536, file)) {
            int weight;
            int index;
            if (swscanf(line, L"[%[^]]]\n", buf) == 1)
                PgFontFamilies[++i].name = wcsdup(buf);
            else if (swscanf(line, L" Roman,%d,%[^,],%d\n", &weight, buf, &index) == 3) {
                PgFontFamilies[i].romanIndex[weight / 100] = index;
                PgFontFamilies[i].roman[weight / 100] = wcsdup(buf);
            } else if (swscanf(line, L" Italic,%d,%[^,],%d\n", &weight, buf, &index) == 3) {
                PgFontFamilies[i].italicIndex[weight / 100] = index;
                PgFontFamilies[i].italic[weight / 100] = wcsdup(buf);
            } else
                goto rebuild;
        }
        fclose(file);
        
        return PgFontFamilies;
        
        rebuild:
        fclose(file);
    }
    
    PgFontFamilies = NULL;
    PgNFontFamilies = 0;
    _pgScanDirectory(dir, scanPerFile);
    
    file = _wfopen(path, L"w");
    if (file) {
        fwprintf(file, L"%d\n", PgNFontFamilies);
        for (int i = 0; i < PgNFontFamilies; i++) {
            fwprintf(file, L"[%ls]\n", PgFontFamilies[i].name);
            for (int w = 0; w < 10; w++)
                if (PgFontFamilies[i].roman[w])
                    fwprintf(file, L"  Roman,%d,%ls,%d\n",
                        w * 100,
                        PgFontFamilies[i].roman[w],
                        PgFontFamilies[i].romanIndex[w]);
            for (int w = 0; w < 10; w++)
                if (PgFontFamilies[i].italic[w])
                    fwprintf(file, L"  Italic,%d,%ls,%d\n",
                        w * 100,
                        PgFontFamilies[i].italic[w],
                        PgFontFamilies[i].italicIndex[w]);
        }
        fclose(file);
    }
    return PgFontFamilies;
}
