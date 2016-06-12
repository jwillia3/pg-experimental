#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <pg.h>
#include <pw.h>

#define B * 12
#define b * 12 + 1

//const wchar_t *TemplateFontName = L"Courier New";
const wchar_t *TemplateFontName = L"Fira Mono";
PgFont      *UIFont;
PgFont      *TemplateFont;
PgFont      *TargetFont;
//int         Codepoint = 0x0041;
int         Codepoint = 'o';


PgPath      *currentGlyph;
uint32_t    Grey = 0xffa0a0a0;
uint32_t    Black = 0xff333333;
uint32_t    White = 0xffffffff;
uint32_t    Alice = 0xfff0f0ff;

static void fillString(Pg *g, PgFont *font, float x, float y, uint32_t colour, char *format, ...) {
    va_list ap;
    va_start(ap, format);
    char buf[65536];
    int length = vsprintf(buf, format, ap);
    va_end(ap);
    pgFillUtf8(g, font, x, y, buf, length, colour);
}

static void sidebar(Pg *g) {
    pgClearCanvas(g, Grey);
    pgScaleFont(UIFont, 2 b, 2 b);
    fillString(g, UIFont, 1 b, 1 b, Black, "U-%04X (%lc)", Codepoint, Codepoint);
    pgScaleFont(UIFont, 1 b, 1 b);
    fillString(g, UIFont, 1 b, 4 b, Black, "Template: %ls", TemplateFontName);
    pgFreeCanvas(g);
}
X;
static void designView(Pg *g) {
    pgScale(g, 0.75f, 0.75f);
    pgClearCanvas(g, White);
    
    extern X;
    X=1;
    PgPath *path = pgGetCharPath(TemplateFont, NULL, Codepoint);
    X=0;
    PgMatrix m = PgIdentity;
    pgScaleMatrix(&m, 1000.0f / pgGetFontEm(TemplateFont), 1000.0f / pgGetFontEm(TemplateFont));
    pgTransformPath(path, m);
    pgFillPath(g, path, Black);
    pgFreePath(path);
    
    g->flatness = 1.00f;
//    pgFillPath(g, currentGlyph, Black);
    g->flatness = 1.001f;
    
    pgFreeCanvas(g);
}

static void event(Pw *pw, int msg, PwEventArgs *e) {
    switch (msg) {
    case PWE_DRAW:
        Pg *g = e->draw.g;
        sidebar(pgSubsectionCanvas(g, pgRect(pgPt(0 B, 0 B), pgPt(25 B, 75 B))));
        designView(pgSubsectionCanvas(g, pgRect(pgPt(25 B, 0 B), pgPt(100 B, 75 B))));
        break;
    case PWE_KEY_DOWN:
        if (e->key.key == 27) pwClose(pw);
        break;
    }
}

int main() {

    FILE *file = fopen("test.pgsf", "r");
    char data[65536];
    fread(data, 1, sizeof data, file);
    TargetFont = pgLoadFont(data, 0);
    
    currentGlyph = pgNewPath();
    
//    pgInterpretSvgPath(currentGlyph, "M100,100 L200,100 L200,200 L100,200 Z");
    pgInterpretSvgPath(currentGlyph, "M100,100 h500 v500 h-500 z m125,125 h250 v250 h-250 z");

    TemplateFont = pgOpenFont(TemplateFontName, 300, false);
    UIFont = pgOpenFont(L"Arial", 300, false);
    Pw *pw = pwNewWindow(100 B, 75 B, event);
    while (pwWaitMessage());
    return 0;
}
#undef main
int main() {
    extern int WinMain();
    return WinMain();
}