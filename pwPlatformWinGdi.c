#define UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "pg.h"
#include "pw.h"
#include "platform.h"
#pragma comment(lib, "gdi32")
#pragma comment(lib, "user32")

typedef struct PwGdiWindow PwGdiWindow;
struct PwGdiWindow {
    Pw _;
    HWND hwnd;
};
typedef struct {
    Pg g;
    HBITMAP dib;
    void (*oldResize)(Pg *g, int width, int height);
} GdiPg;

static PgFont *UiFont;

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
static void close(Pw *win) {
    CloseWindow(((PwGdiWindow*)win)->hwnd);
}
static void resize(Pw *win, int width, int height) {
    SetWindowPos(((PwGdiWindow*)win)->hwnd,
        NULL, 0, 0, width, height,
        SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOZORDER);
}
static void setTitle(Pw *win, const wchar_t *title) {
    SetWindowText(((PwGdiWindow*)win)->hwnd, title);
}
static void repaint(PwGdiWindow *gdi) {
    Pw *win = &gdi->_;
    Pg *g = win->g;
    pgIdentity(g);
    {
        if (!UiFont) {
            void *host;
            UiFont = (PgFont*)pgLoadOpenTypeFont(
                _pgMapFile(&host, L"C:/Windows/Fonts/SegoeUI.ttf"),
                0);
        }
            
        PgPath *path = pgNewPath();
        pgMove(path, pgPt(0, 0));
        pgLine(path, pgPt(g->width, 0));
        pgLine(path, pgPt(g->width, 31));
        pgLine(path, pgPt(0, 31));
        pgFillPath(g, path, 0x3377aa);
        
        pgScaleFont(UiFont, 24, 24);
        
        float width = pgGetStringWidth(UiFont, win->title, -1);
        pgFillString(g, UiFont,
            g->width / 2.0f - width / 2.0f, 8,
            win->title, -1, 0x444444);
        
        pgClearPath(path);
        pgTranslate(g, g->width - 24, 8);
        pgMove(path, pgPt(0, 0));
        pgLine(path, pgPt(16, 16));
        pgMove(path, pgPt(0, 16));
        pgLine(path, pgPt(16, 0));
        pgStrokePath(g, path, 5.0f, 0x444444);
        
        pgClearPath(path);
        pgTranslate(g, -32, 0);
        pgMove(path, pgPt(0, 0));
        pgLine(path, pgPt(16, 0));
        pgLine(path, pgPt(16, 16));
        pgLine(path, pgPt(0, 16));
        pgLine(path, pgPt(0, 0));
        pgStrokePath(g, path, 5.0f, 0x444444);
        
        pgClearPath(path);
        pgTranslate(g, -32, 0);
        pgMove(path, pgPt(0, 16));
        pgLine(path, pgPt(16, 16));
        pgStrokePath(g, path, 5.0f, 0x444444);
        
        pgFreePath(path);
    }
    g->height -= 32;
    g->bmp += g->width * 32;
    g->clip.y2 -= 32;
    pgIdentity(g);
    if (win->onRepaint)
        win->onRepaint(win);
    g->clip.y2 += 32;
    g->height += 32;
    g->bmp -= g->width * 32;
}
static void update(Pw *win) {
    PwGdiWindow *gdi = (PwGdiWindow*)win;
    repaint(gdi);
    Pg *g = gdi->_.g;
    HDC wdc = GetDC(gdi->hwnd);
    HDC cdc = CreateCompatibleDC(wdc);
    HGDIOBJ old = SelectObject(cdc, ((GdiPg*)g)->dib);
    UpdateLayeredWindow(gdi->hwnd, wdc, NULL,
        &(SIZE){ g->width, g->height },
        cdc, &(POINT){ 0, 0 }, 0, NULL, ULW_OPAQUE);
    SelectObject(cdc, old);
    ReleaseDC(gdi->hwnd, wdc);
    DeleteDC(cdc);
}
static uint32_t state() {
    return  (GetKeyState(VK_LBUTTON)? 0x01: 0) |
            (GetKeyState(VK_MBUTTON)? 0x02: 0) |
            (GetKeyState(VK_RBUTTON)? 0x04: 0) |
            (GetKeyState(VK_SHIFT)? 0x10: 0) |
            (GetKeyState(VK_MENU)? 0x20: 0) |
            (GetKeyState(VK_CONTROL)? 0x40: 0);
}
static LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    PwGdiWindow *gdi = GetProp(hwnd, L"pw");
    Pw *win = &gdi->_;
    PAINTSTRUCT ps;
    
    switch (msg) {
    case WM_KEYDOWN:
        return win->onKeyDown? win->onKeyDown(win, state()): 0;
    case WM_KEYUP:
        return win->onKeyUp? win->onKeyUp(win, state()): 0;
    case WM_CHAR:
        return win->onChar? win->onChar(win, state(), wparam): 0;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
        return win->onClick? win->onClick(win, state()): 0;
    
    case WM_NCHITTEST: {
            RECT r;
            Pg *g = win->g;
            GetWindowRect(hwnd, &r);
            int x = LOWORD(lparam) - r.left;
            int y = HIWORD(lparam) - r.top;
            
            if (x < 4)
                return y < 4? HTTOPLEFT:
                    g->height - y < 4? HTBOTTOMLEFT:
                    HTLEFT;
            if (g->width - x < 4)
                return y < 4? HTTOPRIGHT:
                    g->height - y < 4? HTBOTTOMRIGHT:
                    HTRIGHT;
            if (g->height - y < 4)
                return HTBOTTOM;
            if (y < 32) {
                if (x >= g->width - 32) return HTCLOSE;
                if (x >= g->width - 64) return HTMAXBUTTON;
                if (x >= g->width - 96) return HTMINBUTTON;
                if (y < 4) return HTTOP;
                return HTCAPTION;
            }
            return HTCLIENT;
        }
        
    case WM_SIZE:
        pgResizeCanvas(win->g, LOWORD(lparam), HIWORD(lparam));
        if (win->onResize)
            win->onResize(win, LOWORD(lparam), HIWORD(lparam));
        update(win);
        return 0;
    case WM_CREATE:
        gdi = ((CREATESTRUCT*)lparam)->lpCreateParams;
        win = &gdi->_;
        SetProp(hwnd, L"pw", gdi);
        gdi->hwnd = hwnd;
        win->g = pgNewGdiCanvas(0, 0);
        return 0;
    case WM_ERASEBKGND:
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wparam, lparam);
}

Pw *pwOpenGdiWindow(int width, int height, const wchar_t *title, void (*onRepaint)(Pw *win)) {
    RegisterClass(&(WNDCLASS){ CS_HREDRAW|CS_VREDRAW, WndProc, 0, 0,
        GetModuleHandle(NULL), LoadIcon(NULL, IDI_APPLICATION),
        LoadCursor(NULL, IDC_ARROW), (HBRUSH)(COLOR_WINDOW + 1),
        NULL, L"GenericWindow" });
    
    PwGdiWindow *gdi = calloc(1, sizeof *gdi);
    Pw *win = &gdi->_;
    win->title = wcsdup(title);
    win->onRepaint = onRepaint;
    win->update = update;
    win->setTitle = setTitle;
    win->resize = resize;
    win->close = close;
    gdi->hwnd = CreateWindowEx(
        WS_EX_LAYERED,
        L"GenericWindow", title,
        WS_OVERLAPPEDWINDOW|WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, width, height,
        NULL, NULL, GetModuleHandle(NULL), gdi);
    return win;
}
Pw *_pwNew(int width, int height, const wchar_t *text, void (*onRepaint)(Pw *win)) {
    return pwOpenGdiWindow(width, height, text, onRepaint);
}
void _pwLoop() {
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
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
    extern int main();
    return main();
}