#define UNICODE
#define WIN32_WINNT 0x0601
#define WIN32_LEAN_AND_MEAN
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "pg.h"
#pragma comment(lib, "gdi32")
#pragma comment(lib, "user32")
#pragma comment(lib, "pg")

uint32_t fg = 0x808080;
uint32_t bg = 0xffff80;
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

void repaint() {
    pgClearCanvas(G, bg);
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
        BitBlt(ps.hdc, 0, 0, G->width, G->height, cdc, 0, 0, SRCCOPY);
        DeleteDC(cdc);
        EndPaint(hwnd, &ps);
        return 0;
        }
    case WM_SIZE:
        pgResizeCanvas(G, LOWORD(lparam), HIWORD(lparam));
        repaint();
        return 0;
    case WM_CREATE:
        W = hwnd;
        puts("W");
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
	if (!GetProcessUserModeExceptionPolicy)
		FatalAppExit(0, L"XXX");
    WNDCLASS wc = { CS_HREDRAW|CS_VREDRAW, WndProc, 0, 0,
        GetModuleHandle(NULL), LoadIcon(NULL, IDI_APPLICATION),
        LoadCursor(NULL, IDC_ARROW), (HBRUSH)(COLOR_WINDOW + 1),
        NULL, L"GenericWindow" };
    RegisterClass(&wc);
    
    RECT r = { 0, 0, 800, 600 };
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
