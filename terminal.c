#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "pg.h"
#include "pw.h"
#undef min
#undef max
#define min(a, b) ((a) < (b)? (a): (b))
#define max(a, b) ((a) > (b)? (a): (b))
#define clamp(a, b, c) (max(a, min(b, c)))

typedef struct {
    uint8_t fg, bg;
    uint16_t c;
} Cell;
typedef struct {
    Pw *win;
    PgFont *font;
    int nrows, ncols;
    PgPt fs;
    Cell **rows;
    int hist;
    Cell def, attr;
    int cr, cc;
    bool echo;
    bool inEsc;
    int blockCursor;
    bool origin;
    
    struct {
        int cr, cc;
        Cell attr;
        bool origin;
    } saved;
    
    uint32_t color[16];
    int ncmd;
    wchar_t cmd[64];
} Term;

void clearAll(Term *term) {
    for (int i = 0; i < term->hist; i++)
        free(term->rows[i]);
    
    term->rows = realloc(term->rows, sizeof *term->rows);
    for (int r = 0; r < term->nrows; r++) {
        term->rows[r] = malloc(term->ncols * sizeof *term->rows[r]);
        for (int c = 0; c < term->ncols; c++)
            term->rows[r][c] = term->def;
    }
    term->hist = term->nrows;
}
void scrollUp(Term *term) {
}
void scrollDown(Term *term) {
}

void cuu(Term *term, int n) {
    while (n-->0)
        if (term->cr > 0)
            term->cr--;
}
void cud(Term *term, int n) {
    while (n-->0)
        if (term->cr < term->nrows - 1)
            term->cr++;
}
void cuf(Term *term, int n) {
    while (n-->0)
        if (term->cc < term->ncols - 1)
            term->cc++;
}
void cub(Term *term, int n) {
    while (n-->0)
        if (term->cc > 0)
            term->cc--;
}
void cha(Term *term, int n) {
    term->cc = clamp(1, n, term->ncols) - 1;
}
void cup(Term *term, int r, int c) {
    term->cr = clamp(1, r, term->nrows) - 1;
    term->cc = clamp(1, c, term->ncols) - 1;
}
void ed(Term *term, int n) {
    if (n == 1) {
        for (int r = 0; r < term->cr; r++)
            for (int c = 0; c < term->ncols; c++)
                term->rows[r][c] = term->attr;
        for (int c = 0; c < term->cc; c++)
            term->rows[term->cr][c] = term->attr;
    } else if (n == 2 || n == 3) {
        for (int r = 0; r < term->nrows; r++)
            for (int c = 0; c < term->ncols; c++)
                term->rows[r][c] = term->attr;
    }
}
void el(Term *term, int n) {
    if (n == 1)
        for (int i = term->cc; i < term->ncols; i++)
            term->rows[term->cr][i] = term->attr;
    else if (n == 2)
        for (int i = 0; i < term->ncols; i++)
            term->rows[term->cr][i] = term->attr;
}

void ri(Term *term) {
    if (term->cr > 0)
        term->cr--;
    else
        scrollDown(term);
}
void ind(Term *term) {
    if (term->cr < term->nrows - 1)
        term->cr++;
    else
        scrollUp(term);
}
void nel(Term *term) {
    ind(term);
    term->cc = 0;
}
void ris(Term *term) {
    clearAll(term);
}
void decsc(Term *term) {
    term->saved.cr = term->cr;
    term->saved.cc = term->cc;
    term->saved.attr = term->attr;
    term->saved.origin = term->origin;
}
void decrc(Term *term) {
    term->cr = term->saved.cr;
    term->cc = term->saved.cc;
    term->attr = term->saved.attr;
    term->origin = term->saved.origin;
}
void bel(Term *term) {
    extern int Beep(int, int);
    Beep(750, 300);
}
void bs(Term *term) {
    cub(term, 1);
}
void ht(Term *term) {
    term->cc = term->ncols - 1;
}
void cr(Term *term) {
    term->cc = 0;
}
void lf(Term *term) {
    ind(term);
}
void crlf(Term *term) {
    cr(term);
    lf(term);
}

void csiCuu(Term *term, int *par, int n, int ic) { cuu(term, par[0]); }
void csiCud(Term *term, int *par, int n, int ic) { cud(term, par[0]); }
void csiCuf(Term *term, int *par, int n, int ic) { cuf(term, par[0]); }
void csiCub(Term *term, int *par, int n, int ic) { cub(term, par[0]); }
void csiCnl(Term *term, int *par, int n, int ic) { cuu(term, par[0]); term->cc = 0; }
void csiCpl(Term *term, int *par, int n, int ic) { cud(term, par[0]); term->cc = 0; }
void csiCha(Term *term, int *par, int n, int ic) { cha(term, par[0]); }
void csiCup(Term *term, int *par, int n, int ic) { cup(term, par[0], par[1]); }
void csiEd(Term *term, int *par, int n, int ic) { ed(term, par[0]); }
void csiEl(Term *term, int *par, int n, int ic) { el(term, par[0]); }
void csiSgr(Term *term, int *par, int n, int ic) {
    if (!n)
        term->attr = term->def;
    for (int i = 0; i < n; i++)
        switch (par[i]) {
        case 0:
            term->attr = term->def;
            break;
        case 30: case 31: case 32: case 33: case 34: case 35: case 36: case 37:
            term->attr.fg = par[i] - 30;
            break;
        case 90: case 91: case 92: case 93: case 94: case 95: case 96: case 97:
            term->attr.fg = par[i] - 90 + 8;
            break;
        case 39:
            term->attr.fg = term->def.fg;
            break;
        case 40: case 41: case 42: case 43: case 44: case 45: case 46: case 47:
            term->attr.bg = par[i] - 40;
            break;
        case 100: case 101: case 102: case 103: case 104: case 105: case 106: case 107:
            term->attr.bg = par[i] - 100 + 8;
            break;
        case 49:
            term->attr.bg = term->def.bg;
            break;
        }
}

void (*esc[256])(Term*) = {
    ['7'] = decsc,
    ['8'] = decrc,
    ['D'] = ind,
    ['M'] = ri,
    ['E'] = nel,
    ['c'] = ris,
};
struct {
    void (*exec)(Term*,int*,int,int);
    uint16_t def;
} csi[256] = {
    ['A'] = { csiCuu, 0001 },
    ['B'] = { csiCud, 0001 },
    ['C'] = { csiCuf, 0001 },
    ['D'] = { csiCub, 0001 },
    ['E'] = { csiCnl, 0001 },
    ['F'] = { csiCpl, 0001 },
    ['G'] = { csiCha, 0001 },
    ['H'] = { csiCup, 0003 },
    ['J'] = { csiEd, 0003 },
    ['K'] = { csiEl, 0003 },
    ['f'] = { csiCup, 0003 },
    ['m'] = { csiSgr, 0000 },
};
void receiver(Term *term, int c) {
    if (c < 32) {
        switch (c) {
        case 007: // BEL
            bel(term);
            break;
        case 010: // BS
            bs(term);
            break;
        case 011: // HT
            ht(term);
            break;
        case 012: // LF
        case 013: // VT
        case 014: // FF
        case 015: // CR
            crlf(term);
            break;
        case 030: // CAN
        case 032: // sub
            goto normal;
        case 033: // ESC
            term->ncmd = 0;
            term->inEsc = true;
            break;
        }
    } else if (term->inEsc) {
        if (term->ncmd >= sizeof term->cmd / sizeof *term->cmd)
            goto badEsc;
        
        if (term->ncmd == 0) {
            if (c == '[') // Control Sequence Introducer
                term->cmd[term->ncmd++] = c;
            else if (c > 255)
                goto badEsc;
            else {
                if (esc[c])
                    esc[c](term);
                term->inEsc = false;
            }
        } else if (term->cmd[0] == '[') {
            if ((c >= 040 && c <= 057) || (c >= 060 && c <= 077))
                term->cmd[term->ncmd++] = c;
            else if (c >= 0100 && c <= 0176) { // Final character of CSI
                term->inEsc = false;
                term->cmd[term->ncmd] = 0;
                if (c > 255)
                    goto badEsc;
                void (*exec)(Term*,int*,int,int) = csi[c].exec;
                uint16_t def = csi[c].def;
                
                if (exec) {
                    int par[16];
                    int n = 0;
                    int ic = (term->cmd[1] >= 040 && term->cmd[1] <= 057)?
                        term->cmd[1]: 0;
                    wchar_t *start = term->cmd + (ic? 2: 1);
                    wchar_t *end = start;
                    for (int i = 0; i < 16; i++) {
                        par[i] = wcstol(start, &end, 10);
                        if (start == end)
                            par[i] = (def >> i) & 1;
                        else
                            n++;
                        start = end + (*end == ';');
                    }
                    exec(term, par, n, ic);
                }
            }
        }
        return;
badEsc:
        bel(term);
        term->inEsc = false;
    } else {
normal:
        term->rows[term->cr][term->cc] = term->attr;
        term->rows[term->cr][term->cc].c = c;
        cuf(term, 1);
    }
}
void keyboard(Term *term, int c) {
    if (term->echo)
        receiver(term, c);
}
void keyboardString(Term *term, char *seq) {
    for (int i = 0; seq[i]; i++)
        keyboard(term, seq[i]);
}
static bool onKeyDown(Pw *win, uint32_t state, int key) {
    Term *term = win->etc;
    if (key == 045) keyboardString(term, "\033[D");
    else if (key == 046) keyboardString(term, "\033[A");
    else if (key == 047) keyboardString(term, "\033[C");
    else if (key == 050) keyboardString(term, "\033[B");
    else return true;
    pwUpdate(term->win);
    return false;
}
static bool onChar(Pw *win, uint32_t state, int c) {
    Term *term = win->etc;
    if (state & 0x20 && c == 32)
        return true;
    if (state & 0x20)
        keyboard(term, 033);
    keyboard(term, c);
    pwUpdate(term->win);
    return ~state & 0x20;
}
static void onRepaint(Pw *win) {
    Term *term = win->etc;
    Pg *g = win->g;
    for (int r = 0; r < term->nrows; r++) {
        for (int i = 0, j; i < term->ncols; i = j) {
            for (j = i; j < term->ncols && term->rows[r][i].bg == term->rows[r][j].bg; j++);
            pgFillRect(g,
                pgPt(i * term->fs.x, r * term->fs.y),
                pgPt(j * term->fs.x, (r + 1) * term->fs.y + 0.5f),
                term->color[term->rows[r][i].bg]);
        }
        if (term->cr == r)
            pgFillRect(g,
                pgPt(term->cc * term->fs.x, term->cr * term->fs.y),
                pgPt((term->cc + 1) * term->fs.x, (term->cr + 1) * term->fs.y + 0.5f),
                ~term->color[term->rows[term->cr][term->cc].bg]);
        
        for (int c = 0; c < term->ncols; c++)
            if (c != ' ')
                pgFillChar(g, term->font,
                    c * term->fs.x, r * term->fs.y,
                    term->rows[r][c].c,
                    term->color[term->rows[r][c].fg]);
    }
        
    if (term->inEsc) {
        pgFillRect(g,
            pgPt(0, (term->nrows - 1) * term->fs.y),
            pgPt(term->ncols * term->fs.x, term->nrows * term->fs.y + 0.5f),
            term->color[9]);
        pgFillString(g, term->font,
            0,
            (term->nrows - 1) * term->fs.y,
            term->cmd, term->ncmd, term->color[3]);
    }
}
static void onResize(Pw *win, int width, int height) {
    Term *term = win->etc;
    term->ncols = width / term->fs.x;
    term->nrows = height / term->fs.y;
    clearAll(term);
}
static void onSetup(Pw *win, void *etc) {
    Term *term = etc;
    win->etc = term;
    win->onResize = onResize;
    win->onRepaint = onRepaint;
    win->onKeyDown = onKeyDown;
    win->onChar = onChar;
}
Term *newTerm() {
    Term *term = calloc(1, sizeof *term);
    term->nrows = 25;
    term->ncols = 80;
    term->fs = (PgPt){ 20, 20 };
    term->def = (Cell){ .fg = 8, .bg = 0, .c = ' ' };
    term->attr = term->def;
    term->echo = true;
    term->blockCursor = true;
//    wcscpy(term->cmd,L"test");
//    term->ncmd = 4;
    memmove(term->color, (uint32_t[16]){
            0x212121, 0xc23621, 0x25bc24, 0xadad27,
            0x492ee1, 0xd338d3, 0x33bbc8, 0xcbcccd,
            0x808080, 0xfc391f, 0x31e722, 0xeaec23,
            0x5833ff, 0xf935f8, 0x14f0f0, 0xe9ebeb,
        }, sizeof term->color);
    term->font = pgLoadFontFromFile(L"c:/windows/fonts/consola.ttf", 0);
    pgScaleFont(term->font, term->fs.x, term->fs.y);
    term->fs.x = pgGetCharWidth(term->font, 'M');
    term->win = pwNew(
        term->ncols * term->fs.x,
        term->nrows * term->fs.y,
        L"Terminal",
        onSetup, term);
    return term;
}

int main() {
    newTerm();
    pwLoop();
    return 0;
}
#undef main
int main() { extern WinMain(); WinMain(); }