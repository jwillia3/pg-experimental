#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "pg.h"
#include "pw.h"
#include "platform.h"

void pwClose(Pw *win) {
    win->close(win);
}
void pwResize(Pw *win, int width, int height) {
    win->resize(win, width, height);
}
void pwUpdate(Pw *win) {
    win->update(win);
}
void pwSetTitle(Pw *win, const wchar_t *title) {
    free(win->title);
    win->title = wcsdup(title);
    win->setTitle(win, win->title);
}
void pwLoop() {
    _pwLoop();
}
Pw *pwNew(int width, int height, const wchar_t *title, void (*onRepaint)(Pw *win)) {
    return _pwNew(width, height, title, onRepaint);
}