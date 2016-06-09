#define UNICODE
#define WIN32_LEAN_AND_MEAN
#include <iso646.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <windows.h>
#include <pg.h>
#include <pw.h>
#include <platform.h>
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

typedef struct {
    HWND hwnd;
    Pg *client;
    Pg *chrome;
    HBITMAP dib;
} Window;

enum {
    MouseTolorance = 5
};

static void drawChrome(Pw *pw, Pg *g) {
    Window *win = pw->sys;
    
    int len = GetWindowTextLength(win->hwnd);
    wchar_t *title = malloc((len + 1) * sizeof (wchar_t));
    GetWindowText(win->hwnd, title, len + 1);
    
    float width = pgGetStringWidth(PwConf.font, title, -1);
    pgFillRect(g,
        pgPt(1.0f, 0.0f),
        pgPt(g->width, 32.5f),
        PwConf.titleBg);
    pgFillString(g, PwConf.font,
        g->width / 2.0f - width / 2.0f, 32.0f / 2.0f - pgGetFontEm(PwConf.font) / 2.0f,
        title, len, PwConf.titleFg);
    free(title);
    
    const float _1_4 = 32.0f * 3.0f / 8.0f;
    const float _3_4 = 32.0f * 5.0f / 8.0f;
    pgTranslate(g, g->width - 32.0f * 3, 0.0f);
    pgFillRect(g, pgPt(0.0f, 0.0f), pgPt(32.0f * 3.0f, 32.0f),
        pgBlend(PwConf.border, PwConf.titleBg, 128));
    pgStrokeLine(g, pgPt(_1_4, _3_4), pgPt(_3_4, _3_4), 5.0f, PwConf.border);
    pgTranslate(g, 32.0f, 0.0f);
    pgStrokeRect(g, pgPt(_1_4, _1_4), pgPt(_3_4, _3_4), 5.0f, PwConf.border);
    pgTranslate(g, 32.0f, 0.0f);
    pgStrokeLine(g, pgPt(_1_4, _1_4), pgPt(_3_4, _3_4), 5.0f, PwConf.border);
    pgStrokeLine(g, pgPt(_3_4, _1_4), pgPt(_1_4, _3_4), 5.0f, PwConf.border);
}

static void _event(Pw *pw, int msg, PwEventArgs *e) {
}
static void _exec(Pw *pw, int msg, PwEventArgs *e) {
    Window *win = pw->sys;
    switch (msg) {
    case PWE_DRAW:
        drawChrome(pw, ((Window*) pw->sys)->chrome);
        if (!e->draw.g)
            e->draw.g = win->client;
        pw->event(pw, msg, e);
        
        HDC dc = GetWindowDC(win->hwnd);
        HDC cdc = CreateCompatibleDC(dc);
        HGDIOBJ old = SelectObject(cdc, win->dib);
        UpdateLayeredWindow(win->hwnd, dc, NULL,
            &(SIZE) { win->chrome->width, win->chrome->height },
            cdc, &(POINT) { 0, 0 }, 0, NULL, ULW_OPAQUE);
        SelectObject(cdc, old);
        DeleteDC(cdc);
        ReleaseDC(win->hwnd, dc);
        break;
    case PWE_SIZE:
        SetWindowPos(win->hwnd, NULL, 0, 0, e->size.width, e->size.height,
            SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER);
        break;
    case PWE_CLOSE:
        DestroyWindow(win->hwnd);
        break;
    }
}
static void _sysEvent(Pw *pw, int msg, PwEventArgs *e) {
    Window *win = pw->sys;
    switch (msg) {
    case PWE_DRAW:
        drawChrome(pw, win->chrome);
        break;
    case PWE_KEY_DOWN:
        if (e->key.alt)
            if (e->key.key == VK_F4) {
                PostMessage(win->hwnd, WM_SYSCOMMAND, SC_CLOSE, 0);
                return;
            }
        pw->event(pw, msg, e);
        break;
    
    case PWE_SIZE:
        if (win->chrome)
            pgFreeCanvas(win->chrome),
            pgFreeCanvas(win->client),
            DeleteObject(win->dib);
        win->chrome = pgNewBitmapCanvas(e->size.width, e->size.height);
        free(win->chrome->bmp);
        win->chrome->bmp = NULL;
        win->chrome->borrowed = true;
        win->dib = CreateDIBSection(NULL,
            &(BITMAPINFO) {
                .bmiHeader = {
                    sizeof (BITMAPINFOHEADER),
                    e->size.width,
                    -e->size.height,
                    1, 32, 0, e->size.width * e->size.height * 4,
                    96, 96, -1, -1 }},
            DIB_RGB_COLORS,
            &win->chrome->bmp,
            NULL, 0);
        win->client = pgSubsectionCanvas(win->chrome,
            pgRect(pgPt(1.0f, 32.0f), pgPt(e->size.width - 1.0f, e->size.height - 1.0f)));
        pw->rect = pgRect(pgPt(0, 0), pgPt(e->size.width, e->size.height));
        pw->exec(pw, PWE_DRAW, &(PwEventArgs) { .draw.g = win->client });
        break;
    default:
        pw->event(pw, msg, e);
    }
}

LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    Pw *pw = GetProp(hwnd, L"Pw");
    Window *win = pw? pw->sys: NULL;
    switch (msg) {
    case WM_NCHITTEST:
        RECT r;
        GetWindowRect(hwnd, &r);
        int x = LOWORD(lparam) - r.left;
        int y = HIWORD(lparam) - r.top;
        int width = win->chrome->width;
        int height = win->chrome->height;
        
        if (x < MouseTolorance)
            return y < MouseTolorance? HTTOPLEFT:
                height - y < MouseTolorance? HTBOTTOMLEFT:
                HTLEFT;
        if (width - x < MouseTolorance)
            return y < MouseTolorance? HTTOPRIGHT:
                height - y < MouseTolorance? HTBOTTOMRIGHT:
                HTRIGHT;
        if (height - y < MouseTolorance)
            return HTBOTTOM;
        if (y < 32)
            if (abs(width - x) < 32)
                return HTCLOSE;
            else if (abs(width - x) < 32 * 2)
                return HTMAXBUTTON;
            else if (abs(width - x) < 32 * 3)
                return HTMINBUTTON;
            else if (y < MouseTolorance)
                return HTTOP;
            else return HTCAPTION;
        return HTCLIENT;
    case WM_SIZE:
        if (pw)
            pw->sysEvent(pw, PWE_SIZE, &(PwEventArgs) {
                .size = {
                    .width = LOWORD(lparam),
                    .height = HIWORD(lparam),
                }});
        return 0;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        unsigned key = MapVirtualKey(wparam, MAPVK_VK_TO_CHAR);
        bool shift = GetKeyState(VK_SHIFT) & 0x8000;
        bool capsLock = GetKeyState(VK_CAPITAL) & 1;
        
        if ((capsLock and shift) or (not capsLock and not shift))
            key = tolower(key);
        
        pw->sysEvent(pw, PWE_KEY_DOWN, &(PwEventArgs) {
            .key = {
                .alt = lparam & (1 << 29),
                .shift = shift,
                .ctl = GetKeyState(VK_CONTROL) & 0x8000,
                .key = key,
            }});
        return 0;
    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN:
        pw->sysEvent(pw, PWE_MOUSE_DOWN, &(PwEventArgs) {
            .mouse = {
                .alt = lparam & (1 << 29),
                .shift = GetKeyState(VK_SHIFT) & 0x8000,
                .ctl = GetKeyState(VK_CONTROL) & 0x8000,
                .at = pgPt(LOWORD(lparam), HIWORD(lparam))
            }});
        return 0;
    
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wparam, lparam);
}

Pw *pwNewWindow(int width, int height, void (*event)(Pw *pw, int msg, PwEventArgs *e)) {
    Pw *pw = calloc(1, sizeof *pw);
    pw->event = event? event: _event;
    pw->sysEvent = _sysEvent;
    pw->exec = _exec;
    Window *win = calloc(1, sizeof *win);
    pw->sys = win;
    win->hwnd = CreateWindowEx(
        WS_EX_LAYERED,
        L"Pw", L"Window",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 100, 100,
        NULL, NULL, GetModuleHandle(NULL), NULL);
    SetProp(win->hwnd, L"Pw", pw);
    pwSize(pw, width, height);
    return pw;
}

bool pwWaitMessage() {
    MSG msg;
    if (!GetMessage(&msg, NULL, 0, 0))
        return false;
    DispatchMessage(&msg);
    return true;
}

static void init() {
    WNDCLASS wc = { CS_HREDRAW|CS_VREDRAW, WndProc, 0, 0,
        GetModuleHandle(NULL), LoadIcon(NULL, IDI_APPLICATION),
        LoadCursor(NULL, IDC_ARROW), (HBRUSH)(COLOR_WINDOW + 1),
        NULL, L"Pw" };
    RegisterClass(&wc);
}

int WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show) {
    init();
    _pwInit();
    extern int pwMain();
    return pwMain();
}
