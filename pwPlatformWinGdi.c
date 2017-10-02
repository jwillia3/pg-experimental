#define _WIN32_WINNT    0x0501
#define WIN32_LEAN_AND_MEAN
#define UNICODE
#include <iso646.h>
#include <windows.h>
#include <pg.h>
#include <pw.h>
#pragma comment(lib, "user32")
#pragma comment(lib, "gdi32")
#pragma comment(lib, "pg")

static HMODULE  shcore;
static HRESULT (WINAPI *GetDpiForMonitor)(HMONITOR, int, unsigned*, unsigned*);
static HWND     app_window;
float    PwDpi = 72.0f;

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    PAINTSTRUCT ps;
    Pg *gs = GetProp(hwnd, L"gs");
    
    switch (msg) {
    case WM_PAINT: {
        BeginPaint(hwnd, &ps);
        HDC cdc = CreateCompatibleDC(ps.hdc);
        SelectObject(cdc, GetProp(hwnd, L"bmp"));
        pwDrawPanel(PwApp, gs);
        BitBlt(ps.hdc, ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right, ps.rcPaint.bottom,
            cdc, ps.rcPaint.left, ps.rcPaint.top, SRCCOPY);
        DeleteDC(cdc);
        EndPaint(hwnd, &ps);
        return 0;
        }
    case WM_ERASEBKGND:
        return 0;
    case WM_SIZE: {
            int width = (short)LOWORD(lparam);
            int height = (short)HIWORD(lparam);
            HBITMAP bmp = GetProp(hwnd, L"bmp");
            DeleteObject(bmp);
            void *data = NULL;
            bmp = CreateDIBSection(NULL,
                &(BITMAPINFO){{sizeof(BITMAPINFOHEADER), width, -height,
                    1, 32, 0, width * height, PwDpi, PwDpi, 0xffffff, 0xffffff}},
                DIB_RGB_COLORS, &data, NULL, 0);
            SetProp(hwnd, L"bmp", bmp);
            gs->bmp = NULL;
            pgResizeCanvas(gs, width, height);
            gs->bmp = data;
            pwLayout(PwApp, width, height);
            return 0;
        }
    case 0x02E0: { // case WM_DPICHANGED: // Windows 8.1
			RECT r = *(RECT*)lparam;
			PwDpi = LOWORD(wparam) / 72.0f;
			SetWindowPos(hwnd, NULL,
				r.left,
				r.top,
				r.right - r.left,
				r.bottom - r.top,
				SWP_NOZORDER|SWP_NOACTIVATE);
            return 0;
		}
    case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_RBUTTONDOWN: {
            int x = (short)LOWORD(lparam);
            int y = (short)HIWORD(lparam);
            Pw *panel = pwTargetPanel(PwApp, x, y);
    		for (Pw *p = panel; p; p = p->parent)
    			if (p->clicked_down and p->clicked_down(p, x - p->win_x, y - p->win_y)) return 0;
    		return 0;
        }
    case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP: {
            int x = (short)LOWORD(lparam);
            int y = (short)HIWORD(lparam);
            Pw *panel = pwTargetPanel(PwApp, x, y);
    		for (Pw *p = panel; p; p = p->parent)
    			if (p->clicked and p->clicked(p, x - p->win_x, y - p->win_y)) {
                    if (p->focus == NULL or not p->focus(p))
                        pwSetActivePanel(p);
                    return 0;
                }
    		return 0;
        }
	case WM_MOUSEMOVE: {
            int x = (short)LOWORD(lparam);
            int y = (short)HIWORD(lparam);
            Pw *panel = pwTargetPanel(PwApp, x, y);
    		for (Pw *p = panel; p; p = p->parent)
    			if (p->mouse_moved and p->mouse_moved(p, x - p->win_x, y - p->win_y)) return 0;
    		return 0;
        }
    case WM_MOUSEWHEEL: {
			int delta = (short)HIWORD(wparam);
			POINT pt = {(short)LOWORD(lparam), (short)HIWORD(lparam)};
			ScreenToClient(hwnd, &pt);
			Pw *panel = pwTargetPanel(PwApp, pt.x, pt.y);
			for (Pw *p = panel; p; p = p->parent)
				if (p->wheel_rolled and p->wheel_rolled(p, delta)) return 0;
            return 0;
		}
    case WM_KEYDOWN:
        if (PwActive)
            for (Pw *p = PwActive; p; p = p->parent)
                if (p->key_pressed and p->key_pressed(p, wparam)) return 0;
        if (PwApp and PwApp->key_pressed) PwApp->key_pressed(PwApp, wparam);
        return 0;
    case WM_CHAR:
        if (PwActive)
            for (Pw *p = PwActive; p; p = p->parent)
                if (p->char_pressed and p->char_pressed(p, wparam)) return 0;
        if (PwApp and PwApp->char_pressed) PwApp->char_pressed(PwApp, wparam);
        return 0;
    case WM_CREATE:
		app_window = hwnd;
        gs = pgNewBitmapCanvas(0, 0);
        SetProp(hwnd, L"gs", gs);
        
        if (GetDpiForMonitor) {
            enum { MDT_EFFECTIVE_DPI = 0 };
            HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
            unsigned tmpDpi;
            if (GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &tmpDpi, &tmpDpi) == S_OK)
                PwDpi = tmpDpi / 72.0f;
            CloseHandle(monitor);
        } else {
            HDC hdc = GetDC(hwnd);
            PwDpi = GetDeviceCaps(hdc, LOGPIXELSY) / 72.0f;
            ReleaseDC(hwnd, hdc);
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wparam, lparam);
}
static void init() {
    if (shcore = LoadLibrary(L"shcore.dll")) {
        HRESULT (*SetProcessDpiAwareness)(INT) = (void*) GetProcAddress(shcore, "SetProcessDpiAwareness");
        GetDpiForMonitor = (void*) GetProcAddress(shcore, "GetDpiForMonitor");
        if (SetProcessDpiAwareness)
            SetProcessDpiAwareness(2);
    }
    RegisterClass(&(WNDCLASS){CS_DBLCLKS|CS_HREDRAW|CS_VREDRAW,
        WndProc, 0, 0, 0, LoadIcon(0, IDI_APPLICATION),
        LoadCursor(0, IDC_ARROW), (HBRUSH)(COLOR_WINDOW+1),
        0, L"Window"});
}
static bool update_app(Pw *app) {
	InvalidateRect(app_window, NULL, FALSE);
	return true;
}
Pw *pwNewAppPanel(int width, int height) {
    RECT rt;
    SetRect(&rt, 0, 0, width, height);
    AdjustWindowRect(&rt, WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0);
    CreateWindowEx(0, L"Window", L"",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT,CW_USEDEFAULT,rt.right-rt.left,rt.bottom-rt.top,
        0, 0, GetModuleHandle(0), 0);
	return PwApp = PW_NEW_PANEL(.width=width, .height=height, .update=update_app);
}
bool pwYield() {
    MSG msg;
    if (GetMessage(&msg,0,0,0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        return true;
    }
    return false;
}
int WinMain(HINSTANCE inst, HINSTANCE prev, char *cmd, int show) {
	extern int pwMain();
	extern void pwInit();
	pwInit();
    init();
	return pwMain();
}
