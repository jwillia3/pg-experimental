#define UNICODE
#define WIN32_WINNT 0x0601
#define WIN32_LEAN_AND_MEAN
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "pg.h"
#include "demo.h"
#pragma comment(lib, "gdi32")
#pragma comment(lib, "user32")
#pragma comment(lib, "pg")

uint32_t bg = 0x404040;
uint32_t fg = 0xf09050;
HWND W;
Pg  *G;

typedef struct {
    Pg g;
    HBITMAP dib;
    void (*oldResize)(Pg *g, int width, int height);
} GdiPg;

static void gdi_resize(Pg *_g, int width, int height) {
    GdiPg *g = (void*)_g;
    g->g.bmp = NULL;
    g->oldResize(&g->g, width, height);
    free(g->g.bmp);
    DeleteObject(g->dib);
    g->dib = CreateDIBSection(NULL,
        (BITMAPINFO*)&(BITMAPINFOHEADER){
            sizeof(BITMAPINFOHEADER),
            width, -height, 1, 32, 0, width * height * 4,
            96, 96, -1, -1
        },
        DIB_RGB_COLORS,
        &(void*)g->g.bmp,
        NULL, 0);
}

Pg *pgNewGdiCanvas(int width, int height) {
    GdiPg *g = (GdiPg*)pgNewBitmapCanvas(0, 0);
    g = realloc(g, sizeof *g);
    g->oldResize = g->g.resize;
    g->g.resize = gdi_resize;
    g->dib = NULL;
    g->g.resize(&g->g, width, height);
    return &g->g;
}

PgPath *pgGetSvgPath(const char *svg) {
    static char params[256];
    if (!params['m']) {
        char *name = "mzlhvcsqt";
        int n[] = { 2, 0, 2, 1, 1, 6, 4, 4, 2 };
        memset(params, -1, 256);
        for (int i = 0; name[i]; i++)
            params[name[i]] = params[toupper(name[i])] = n[i];
    }
    
    PgPath     *path = pgNewPath();
    PgPt       cur = {0,0};
    PgPt       start = {0,0};
    PgPt       reflect = {0,0};
    PgPt       b, c;
    int         cmd;
    float       a[6];
    while (*svg) {
        while (isspace(*svg) || *svg==',') svg++;
        if (params[*svg] >= 0) // continue last command
            cmd = *svg++;
        
        for (int i = 0; i < params[cmd]; i++) {
            while (isspace(*svg) || *svg==',') svg++;
            a[i] = strtof(svg, (char**)&svg);
        }

        switch (cmd) {
        case 'm':
            start = cur = pgPt(cur.x + a[0], cur.y + a[1]);
            pgMove(path, cur);
            break;
        case 'M':
            start = cur = pgPt(a[0], a[1]);
            pgMove(path, cur);
            break;
        case 'Z':
        case 'z':
            cur = start;
            break;
        case 'L':
            cur = pgPt(a[0], a[1]);
            pgLine(path, cur);
            break;
        case 'l':
            cur = pgPt(cur.x + a[0], cur.y + a[1]);
            pgLine(path, cur);
            break;
        case 'h':
            cur = pgPt(cur.x + a[0], cur.y);
            pgLine(path, cur);
            break;
        case 'H':
            cur = pgPt(a[0], cur.y);
            pgLine(path, cur);
            break;
        case 'v':
            cur = pgPt(cur.x, cur.y + a[0]);
            pgLine(path, cur);
            break;
        case 'V':
            cur = pgPt(cur.x, a[0]);
            pgLine(path, cur);
            break;
        case 'c':
            b = pgPt(cur.x + a[0], cur.y + a[1]);
            reflect = pgPt(cur.x + a[2], cur.y + a[3]);
            cur = pgPt(cur.x + a[4], cur.y + a[5]);
            pgCubic(path, b, reflect, cur);
            break;
        case 'C':
            b = pgPt(a[0], a[1]);
            reflect = pgPt(a[2], a[3]);
            cur = pgPt(a[4], a[5]);
            pgCubic(path, b, reflect, cur);
            break;
        case 's':
            b = pgPt( cur.x + (cur.x - reflect.x),
                    cur.y + (cur.y - reflect.y));
            reflect = pgPt(cur.x + a[0], cur.y + a[1]);
            cur = pgPt(cur.x + a[2], cur.y + a[3]);
            pgCubic(path, b, reflect, cur);
            break;
        case 'S':
            b = pgPt( cur.x + (cur.x - reflect.x),
                    cur.y + (cur.y - reflect.y));
            reflect = pgPt(a[0], a[1]);
            cur = pgPt(a[2], a[3]);
            pgCubic(path, b, reflect, cur);
            break;
        case 'q':
            reflect = pgPt(cur.x + a[0], cur.y + a[1]);
            cur = pgPt(cur.x + a[2], cur.y + a[3]);
            pgQuad(path, reflect, cur);
            break;
        case 'Q':
            reflect = pgPt(a[0], a[1]);
            cur = pgPt(a[2], a[3]);
            pgQuad(path, reflect, cur);
            break;
        case 't':
            reflect = pgPt(cur.x + (cur.x - reflect.x),
                         cur.y + (cur.y - reflect.y));
            cur = pgPt(cur.x + a[0], cur.y + a[1]);
            pgQuad(path, reflect, cur);
            break;
        case 'T':
            reflect = pgPt(cur.x + (cur.x - reflect.x),
                         cur.y + (cur.y - reflect.y));
            cur = pgPt(a[0], a[1]);
            pgQuad(path, reflect, cur);
            break;
        }
    }
    return path;
}

void save() {
    #pragma pack(push)
    #pragma pack(1)
    struct {
        struct { uint8_t sig[2]; uint32_t total, resv, offs; };
        struct { uint32_t sz, width, height;
            uint16_t planes, bits;
            uint32_t compression, dataSize, xres, yres, used, important;
        } bmp;
    } head = {
        { "BM", sizeof head + G->width * G->height * 4, 0, sizeof head },
        { sizeof head.bmp, G->width, -G->height, 1, 32, 0,
          G->width * G->height * 4, 96, 96, -1, -1 }
    };
    #pragma pack(pop)
    FILE *file = fopen("out.bmp", "wb");
    fwrite(&head, 1, sizeof head, file);
    fwrite(G->bmp, 1, G->width * G->height * 4, file);
    fclose(file);
}

int fps;
int tick;
PgPath *SvgPath[sizeof TestSVG / sizeof *TestSVG];
void setup() {
    if (SvgPath[0]) return;
    for (int i = 0; TestSVG[i]; i++)
        SvgPath[i] = pgGetSvgPath(TestSVG[i]);
}
void repaint() {
    static int oldFps;
    
    pgClearCanvas(G, bg);
    pgIdentity(G);
    pgTranslate(G, -200, -250);
    pgRotate(G, tick / 180.0f * 8.0f);
    pgTranslate(G, G->width / 2.0f, G->height / 2.0f);

    setup();
    for (int i = 0; SvgPath[i]; i++)
        pgStrokePath(G, SvgPath[i], 10.0f, ~fg);
    for (int i = 0; SvgPath[i]; i++)
        pgFillPath(G, SvgPath[i], fg);
        
//    pgIdentity(G);
//    pgRotate(G, tick / 180.0f * 8.0f);
//    pgTranslate(G, G->width / 2.0f, G->height / 2.0f);
//    pgStrokePath(G, pgGetSvgPath(
//        "M0,0"
//        " v100"
//        ), 20.0f, fg);
//    fps = 10;
    fps = 60;
    
    if (!fps && oldFps) KillTimer(W, 0);
    if (fps) SetTimer(W, 0, 1000 / fps, NULL);
    oldFps = fps;
}
void create() {
    G = pgNewGdiCanvas(0, 0);
}
void resize(int width, int height) {
    pgResizeCanvas(G, width, height);
    repaint();
}

LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    PAINTSTRUCT ps;
    switch (msg) {
    case WM_PAINT: {
        BeginPaint(hwnd, &ps);
        HDC cdc = CreateCompatibleDC(ps.hdc);
        SelectObject(cdc, ((GdiPg*)G)->dib);
        BitBlt(ps.hdc,
            ps.rcPaint.left, ps.rcPaint.top,
            ps.rcPaint.right, ps.rcPaint.bottom, 
            cdc, ps.rcPaint.left, ps.rcPaint.top, SRCCOPY);
        DeleteDC(cdc);
        EndPaint(hwnd, &ps);
        return 0;
        }
    case WM_CHAR:
        if (wparam == 3) { // ^C
            save();
            system("start mspaint out.bmp");
            PostQuitMessage(0);
        }
        return 0;
    case WM_KEYDOWN:
        if (wparam == VK_LEFT && tick > 0) { tick--; repaint(); InvalidateRect(hwnd, NULL, false); }
        if (wparam == VK_RIGHT) { tick++; repaint(); InvalidateRect(hwnd, NULL, false); }
        return 0;
        
    case WM_SIZE:
        pgResizeCanvas(G, LOWORD(lparam), HIWORD(lparam));
        repaint();
        return 0;
    case WM_TIMER:
        tick++;
        repaint();
        InvalidateRect(hwnd, NULL, false);
        return 0;
    case WM_CREATE:
        W = hwnd;
        create();
        return 0;
    case WM_ERASEBKGND:
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wparam, lparam);
}

int WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show) {
    HINSTANCE kernel32 = LoadLibrary(L"kernel32.dll");
    BOOL (*GetProcessUserModeExceptionPolicy)(DWORD*) = kernel32? (void*)GetProcAddress(kernel32, "GetProcessUserModeExceptionPolicy"): 0;
    BOOL (*SetProcessUserModeExceptionPolicy)(DWORD) = kernel32? (void*)GetProcAddress(kernel32, "SetProcessUserModeExceptionPolicy"): 0;
    DWORD dwFlags;
    if (GetProcessUserModeExceptionPolicy) {
        GetProcessUserModeExceptionPolicy(&dwFlags);
        SetProcessUserModeExceptionPolicy(dwFlags & ~1);
    }
        
    WNDCLASS wc = { CS_HREDRAW|CS_VREDRAW, WndProc, 0, 0,
        GetModuleHandle(NULL), LoadIcon(NULL, IDI_APPLICATION),
        LoadCursor(NULL, IDC_ARROW), (HBRUSH)(COLOR_WINDOW + 1),
        NULL, L"GenericWindow" };
    RegisterClass(&wc);
    
    RECT r = { 0, 0, 1280, 1024 };
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, false);
    CreateWindow(L"GenericWindow", L"Window",
        WS_OVERLAPPEDWINDOW|WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, r.right-r.left, r.bottom-r.top,
        NULL, NULL, GetModuleHandle(NULL), NULL);
    
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return msg.wParam;
}
int main() { return WinMain(0,0,0,0); }
