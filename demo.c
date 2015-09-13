#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "pg.h"
#include "pw.h"
#include "demo.h"
#include "platform.h"

uint32_t bg = 0xffeeeeee;
uint32_t fg = 0xff666666;

//uint32_t bg = 0x404040;
//uint32_t fg = 0xf09050;
//uint32_t bg = 0xd0d0c0;
//uint32_t fg = 0x904040;
PgFont *Font;

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
            pgClosePath(path);
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
void pgFillSvgPath(Pg *g, const char *svg, uint32_t color) {
    PgPath *path = pgGetSvgPath(svg);
    pgFillPath(g, path, color);
    pgFreePath(path);
}
void pgStrokeSvgPath(Pg *g, const char *svg, float width, uint32_t color) {
    PgPath *path = pgGetSvgPath(svg);
    pgStrokePath(g, path, width, color);
    pgFreePath(path);
}

PgPath *SvgPath[sizeof TestSVG / sizeof *TestSVG];
wchar_t Buf[1024*1024] = L"0123456789";
int BufLen = 10;
void setup() {
    if (SvgPath[0]) return;
    for (int i = 0; TestSVG[i]; i++)
        SvgPath[i] = pgGetSvgPath(TestSVG[i]);
    
    if (!Font) {
        void *host;
//        Font = pgOpenFont(L"Source Sans Pro", 950, false);
//        Font = pgOpenFont(L"Fira Sans", 650, false);
//        Font = pgOpenFont(L"Consolas", 400, false);
//        Font = pgOpenFont(L"Cambria", 400, false);
        Font = pgOpenFont(L"Arial", 400, false);
        if (!Font)
            Font = pgOpenFont(L"Arial", 700, false);
        uint32_t *features = pgGetFontFeatures(Font);
        for (int i = 0; features[i]; i++) {
            Buf[BufLen++] = '\n';
            Buf[BufLen++] = ((char*) &features[i])[3];
            Buf[BufLen++] = ((char*) &features[i])[2];
            Buf[BufLen++] = ((char*) &features[i])[1];
            Buf[BufLen++] = ((char*) &features[i])[0];
        }
        pgSetFontFeatures(Font, (uint32_t[]) { 'onum', 'zero', 0 });
//        pgSetFontFeatures(Font, pgGetFontFeatures(Font));
    }
}
static bool onChar(Pw *win, uint32_t state, int c) {
    if (c == 8) {
        if (BufLen) BufLen--;
    }
    else if (c == '\n' || c == '\r')
        Buf[BufLen++] = '\n';
    else if (c == 23) // ^W
        win->close(win);
    else if (c < 32);
    else
        Buf[BufLen++] = c;
    pwUpdate(win);
    return 0;
}
static void onRepaint(Pw *win) {
    Pg *g = win->g;
    
    pgClearCanvas(g, bg);
    pgIdentity(g);
    setup();
    
    pgScaleFont(Font, 16.0f, 0);
    {
        float x = 0;
        float y = 0;
        for (int i = 0; i < BufLen; i++) {
            if (Buf[i] == '\n') {
                x = 0, y += pgGetFontEm(Font);
                continue;
            }
            if (x + pgGetCharWidth(Font, Buf[i]) >= g->width) {
                x = 0;
                y += pgGetFontEm(Font);
            }
            x = pgFillChar(g, Font, x, y, Buf[i], fg);
        }
        
        for (int i = 0; i < Font->nsubs; i++) {
            char buf[64];
            y += pgGetFontEm(Font) + 2;
            sprintf(buf, "%04x  ", Font->subs[i].in);
            x = pgFillUtf8(g, Font, 0, y, buf, -1, fg);
            x = pgFillGlyph(g, Font, x, y, Font->subs[i].in, fg);
            x = pgFillUtf8(g, Font, x, y, "→", -1, fg);
            x = pgFillGlyph(g, Font, x, y, Font->subs[i].out, fg);
            sprintf(buf, "  %04x", Font->subs[i].out);
            x = pgFillUtf8(g, Font, x, y, buf, -1, fg);
        }
    }
    return ;

    float x = 0;
    float y = 0;
    float width = 0;
    float fontSize = 13.0f;
    for (int i = 0; i < PgNFontFamilies; i++) {
        for (int style = 0; style < 2; style++)
            for (int w = 0; w < 10; w++) {
                if ((!style && !PgFontFamilies[i].roman[w])
                  || (style && !PgFontFamilies[i].italic[w]))
                    continue;
                PgFont *font = pgLoadFontFromFile(
                    style? PgFontFamilies[i].italic[w]: PgFontFamilies[i].roman[w],
                    style? PgFontFamilies[i].italicIndex[w]: PgFontFamilies[i].romanIndex[w]);
                if (!font)
                    font = Font;
                wchar_t text[128];
                swprintf(text, 128, L"%ls %ls %d", PgFontFamilies[i].name, font->styleName, font->weight);
                pgScaleFont(font, fontSize, 0);
                float tmp = pgGetStringWidth(font, text, -1);
                if (width < tmp)
                    width = tmp;
                pgFillString(g, font, x, y, text, -1, font == Font? 0xffffaacc: fg);
                y += pgGetFontEm(Font) + 2;
                if (y >= g->height - 10) {
                    y = 0;
                    x += width + 5;
                }
                if (font != Font)
                    pgFreeFont(font);
            }
    }
}
static void onSetup(Pw *win, void *etc) {
    win->onRepaint = onRepaint;
}


int pwMain() {
    Pw *win = pwNew(1024, 800, L"Test Window", onSetup, NULL);
    win->onChar = onChar;
    pwLoop();
    return 0;
}
//#undef main
//int main() {
//    void WinMain();
//    WinMain();
//}