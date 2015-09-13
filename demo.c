#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <pg.h>
#include <pw.h>

PgFont *Font;

static void event(Pw *pw, int msg, PwEventArgs *e) {
    switch (msg) {
    case PWE_DRAW:
        Pg *g = e->draw.g;
        pgClearCanvas(g, PwConf.bg);
        pgFillUtf8(g, Font, 0, 0, "(nothing)", -1, PwConf.fg);
        break;
    case PWE_KEY_DOWN:
        if (e->key.key == 'W' && e->key.ctl)
            pwClose(pw);
        break;
    }
}

int main() {
    Font = pgOpenFont(L"Open Sans", 900, 1);
    pgScaleFont(Font, 96.0f, 0.0f);
    Pw *pw = pwNewWindow(1024, 800, event);
    while (pwWaitMessage());
    return 0;
}
#undef main
int main() {
    extern int WinMain();
    return WinMain();
}