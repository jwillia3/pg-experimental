#define UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <stdbool.h>
#include <stdint.h>
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
