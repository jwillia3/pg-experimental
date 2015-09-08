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
#include "util.h"
#pragma comment(lib, "gdi32")
#pragma comment(lib, "user32")
enum { Chrome = 32 };

const float MouseTolorance = 6;

typedef struct PwGdiWindow PwGdiWindow;
struct PwGdiWindow {
    Pw _;
    HWND hwnd;
};
typedef struct {
    Pg g;
    HBITMAP dib;
    void (*oldResize)(Pg *g, int width, int height);
    void (*oldFree)(Pg *g);
} PgDibBitmap;

static PgFont *UiFont;
static int nOpenWindows;


static void dib_resize(Pg *_g, int width, int height) {
    PgDibBitmap *g = (void*)_g;
    if (!_g->borrowed) {
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
}
static void dib_free(Pg *_g) {
    PgDibBitmap *g = (void*)_g;
    if (!_g->borrowed) {
        g->g.bmp = NULL;
        g->oldFree(&g->g);
        DeleteObject(g->dib);
    }
    free(g);
}
static Pg *dib_subsection(Pg *original, PgRect rect) {
    PgDibBitmap *dib = malloc(sizeof *dib);
    Pg *g = &dib->g;
    *dib = *(PgDibBitmap*)original;
    rect.a.x = clamp(0, rect.a.x, g->width);
    rect.b.x = clamp(0, rect.b.x, g->width);
    g->width = clamp(0, rect.b.x - rect.a.x, g->width);
    g->height = clamp(0, rect.b.y - rect.a.y, g->height);
    g->clip.b.x = min(g->clip.b.x, g->width);
    g->clip.b.y = min(g->clip.b.y, g->height);
    g->borrowed = true;
    g->bmp += (int)rect.a.x + (int)rect.a.y * g->stride;
    return g;
}


Pg *pgNewGdiCanvas(int width, int height) {
    PgDibBitmap *g = (PgDibBitmap*)pgNewBitmapCanvas(0, 0);
    g = realloc(g, sizeof *g);
    g->oldResize = g->g.resize;
    g->oldFree = g->g.free;
    g->g.resize = dib_resize;
    g->g.free = dib_free;
    g->g.subsection = dib_subsection;
    g->dib = NULL;
    g->g.resize(&g->g, width, height);
    return &g->g;
}
static void close(Pw *win) {
    DestroyWindow(((PwGdiWindow*)win)->hwnd);
}
static void resize(Pw *win, int width, int height) {
    SetWindowPos(((PwGdiWindow*)win)->hwnd,
        NULL, 0, 0, width, height,
        SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOZORDER);
}
static void setTitle(Pw *win, const wchar_t *title) {
    SetWindowText(((PwGdiWindow*)win)->hwnd, title);
}
static void repaintChrome(Pw *win, Pg *g) {
    const uint32_t titleBarColor = 0xffeeeeee;
    const uint32_t titleBarTextColor = 0xff666666;
    const uint32_t borderColor = 0xff666666;
    const uint32_t titleButtonColor = 0xffdddddd;
    const float titleSize = Chrome / 2.0f;
    
    if (!UiFont) {
        void *host;
        UiFont = (PgFont*)pgLoadOpenTypeFont(
            _pgMapFile(&host, L"C:/Windows/Fonts/Arial.ttf"),
            0);
    }
    
    pgIdentity(g);
    
    pgFillRect(g, pgPt(-1, -1), pgPt(g->width, Chrome), titleBarColor);
    
    pgScaleFont(UiFont, titleSize, 0);
    float width = pgGetStringWidth(UiFont, win->title, -1);
    pgFillString(g, UiFont,
        g->width / 2.0f - width / 2.0f,
        Chrome / 2.0f - titleSize / 2.0f,
        win->title, -1,
        titleBarTextColor);
    
    const float Chrome1_4 = Chrome * 3.0f / 8.0f;
    const float Chrome3_4 = Chrome * 5.0f / 8.0f;
    pgTranslate(g, g->width - Chrome * 3, 0);
    pgFillRect(g, pgPt(0, 0), pgPt(Chrome * 3, Chrome), titleButtonColor);
    pgStrokeLine(g, pgPt(Chrome1_4, Chrome3_4), pgPt(Chrome3_4, Chrome3_4), 5.0f, titleBarTextColor);
    pgTranslate(g, Chrome, 0);
    pgStrokeRect(g, pgPt(Chrome1_4, Chrome1_4), pgPt(Chrome3_4, Chrome3_4), 5.0f, titleBarTextColor);
    pgTranslate(g, Chrome, 0);
    pgStrokeLine(g, pgPt(Chrome1_4, Chrome1_4), pgPt(Chrome3_4, Chrome3_4), 5.0f, titleBarTextColor);
    pgStrokeLine(g, pgPt(Chrome3_4, Chrome1_4), pgPt(Chrome1_4, Chrome3_4), 5.0f, titleBarTextColor);
    pgIdentity(g);
    pgStrokeLine(g, pgPt(0.5f, 0.5f), pgPt(0.5f, g->height - 0.5f), 1.0f, borderColor);
    pgStrokeLine(g, pgPt(g->width - 0.5f, 0.5f), pgPt(g->width - 0.5f, g->height - 0.5f), 1.0f, borderColor);
    pgStrokeLine(g, pgPt(0.5f, 0.5f), pgPt(g->width - 0.5f, 0.5f), 1.0f, borderColor);
    pgStrokeLine(g, pgPt(0.5f, g->height - 0.5f), pgPt(g->width - 0.5f, g->height - 0.5f), 1.0f, borderColor);
}
static void repaint(PwGdiWindow *gdi) {
    Pw *win = &gdi->_;
    repaintChrome(win, win->chromeCanvas);
    if (win->onRepaint)
        win->onRepaint(win);
}
static void update(Pw *win) {
    PwGdiWindow *gdi = (PwGdiWindow*)win;
    repaint(gdi);
    Pg *g = gdi->_.chromeCanvas;
    HDC wdc = GetDC(gdi->hwnd);
    HDC cdc = CreateCompatibleDC(wdc);
    HGDIOBJ old = SelectObject(cdc, ((PgDibBitmap*)g)->dib);
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
            (GetAsyncKeyState(VK_MENU)? 0x20: 0) |
            (GetKeyState(VK_CONTROL)? 0x40: 0);
}
static LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    PwGdiWindow *gdi = GetProp(hwnd, L"pw");
    Pw *win = &gdi->_;
    PAINTSTRUCT ps;
    
    switch (msg) {
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        return win->onKeyDown? win->onKeyDown(win, state(), wparam): true;
    case WM_KEYUP:
    case WM_SYSKEYUP:
        return win->onKeyUp? win->onKeyUp(win, state(), wparam): true;
    case WM_CHAR:
    case WM_SYSCHAR:
        if (win->onChar? win->onChar(win, state(), wparam): true)
            break;
        return 0;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
        return win->onClick? win->onClick(win, state()): true;
    
    case WM_NCHITTEST: {
            RECT r;
            Pg *g = win->chromeCanvas;
            GetWindowRect(hwnd, &r);
            int x = LOWORD(lparam) - r.left;
            int y = HIWORD(lparam) - r.top;
            
            if (x < MouseTolorance)
                return y < MouseTolorance? HTTOPLEFT:
                    g->height - y < MouseTolorance? HTBOTTOMLEFT:
                    HTLEFT;
            if (g->width - x < MouseTolorance)
                return y < MouseTolorance? HTTOPRIGHT:
                    g->height - y < MouseTolorance? HTBOTTOMRIGHT:
                    HTRIGHT;
            if (g->height - y < MouseTolorance)
                return HTBOTTOM;
            if (y < Chrome) {
                if (x >= g->width - Chrome) return HTCLOSE;
                if (x >= g->width - Chrome * 2) return HTMAXBUTTON;
                if (x >= g->width - Chrome * 3) return HTMINBUTTON;
                if (y < MouseTolorance) return HTTOP;
                return HTCAPTION;
            }
            return HTCLIENT;
        }
        
    case WM_SIZE: {
            RECT r;
            GetWindowRect(hwnd, &r);
            pgResizeCanvas(win->chromeCanvas, r.right - r.left, r.bottom - r.top);
            pgFreeCanvas(win->g);
            win->g = pgSubsectionCanvas(win->chromeCanvas,
                pgRect(
                    pgPt(1, Chrome),
                    pgPt(win->chromeCanvas->width - 1, win->chromeCanvas->height - 1)));
            if (win->onResize)
                win->onResize(win, win->g->width, win->g->height);
            update(win);
            return 0;
        }
    case WM_CREATE:
        nOpenWindows++;
        gdi = ((void**)((CREATESTRUCT*)lparam)->lpCreateParams)[0];
        void (*setup)(Pw*,void*), *etc;
        setup = ((void**)((CREATESTRUCT*)lparam)->lpCreateParams)[1];
        etc = ((void**)((CREATESTRUCT*)lparam)->lpCreateParams)[2];
        win = &gdi->_;
        SetProp(hwnd, L"pw", gdi);
        gdi->hwnd = hwnd;
        win->chromeCanvas = pgNewGdiCanvas(0, 0);
        win->g = pgSubsectionCanvas(win->chromeCanvas, (PgRect){ 0,0,0,0 });
        if (setup)
            setup(win, etc);
        return 0;
    case WM_ERASEBKGND:
        return 0;
    case WM_DESTROY:
        if (!--nOpenWindows)
            PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wparam, lparam);
}

Pw *pwOpenGdiWindow(int width, int height, const wchar_t *title, void (*onSetup)(Pw *win, void *etc), void *etc) {
    RegisterClass(&(WNDCLASS){ CS_HREDRAW|CS_VREDRAW, WndProc, 0, 0,
        GetModuleHandle(NULL), LoadIcon(NULL, IDI_APPLICATION),
        LoadCursor(NULL, IDC_ARROW), (HBRUSH)(COLOR_WINDOW + 1),
        NULL, L"GenericWindow" });
    
    PwGdiWindow *gdi = calloc(1, sizeof *gdi);
    Pw *win = &gdi->_;
    win->title = wcsdup(title);
    win->update = update;
    win->setTitle = setTitle;
    win->resize = resize;
    win->close = close;
    gdi->hwnd = CreateWindowEx(
        WS_EX_LAYERED,
        L"GenericWindow", title,
        WS_OVERLAPPEDWINDOW|WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, width, height + Chrome,
        NULL, NULL, GetModuleHandle(NULL), (void*[]){ gdi, onSetup, etc });
    return win;
}
Pw *_pwNew(int width, int height, const wchar_t *title, void (*onSetup)(Pw *win, void *etc), void *etc) {
    return pwOpenGdiWindow(width, height, title, onSetup, etc);
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