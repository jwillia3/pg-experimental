#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <pg.h>
#include <pw.h>

#define B * 12
#define b * 12 + 1

PgFont      *UIFont;
PgFont      *TargetFont;
int         Codepoint = 0x0041;
char        GlyphData[65536] = "M500,0 C0,1000 1000,1000 1000,0 Z";
int         Cursor;

uint32_t    Grey = 0xffa0a0a0;
uint32_t    Black = 0xff333333;
uint32_t    White = 0xffffffff;
uint32_t    Alice = 0xfff0f0ff;

static void fillString(Pg *g, PgFont *font, float x, float y, float width, uint32_t colour, char *format, ...) {
    va_list ap;
    va_start(ap, format);
    char buf[65536];
    int length = vsprintf(buf, format, ap);
    va_end(ap);
    
    float nextX;
    float vx = x;
    float vy = y;
    for (char *p = buf; *p; vx = nextX) {
        nextX = vx + pgGetCharWidth(font, *p);
        if (nextX >= width) {
            vy += 2 B;
            nextX = x;
        } else
            pgFillChar(g, font, vx, vy, *p++, colour);
    }
}

static void sidebar(Pg *g) {
    pgClearCanvas(g, Grey);
    pgScaleFont(UIFont, 2 b, 2 b);
    fillString(g, UIFont, 1 b, 1 b, 24 b, Black, "U-%04X (%lc)", Codepoint, Codepoint);
    pgScaleFont(UIFont, 1 b, 1 b);
    fillString(g, UIFont, 1 b, 3 b, 24 b, Black, "%s", GlyphData);
    pgFreeCanvas(g);
}
static void designView(Pg *g) {
    pgScale(g, 0.75f, 0.75f);
    pgClearCanvas(g, White);
    
    g->flatness = 1.00f;
    ((PgSimpleFont*)TargetFont)->glyphData[Codepoint] = GlyphData;
    pgFillChar(g, TargetFont, 0.0f, 0.0f, Codepoint, Black);
    g->flatness = 1.01f;
    
    PgPath *path = pgGetGlyphPath(TargetFont, NULL, Codepoint);
    puts(pgPathAsSvgPath(NULL, path)->text);
    pgFreePath(path);
    
    pgFreeCanvas(g);
}

void insert(int c) {
    GlyphData[Cursor++] = c;
    GlyphData[Cursor] = 0;
}
void backspace() {
    if (Cursor)
        GlyphData[--Cursor] = 0;
}
static void event(Pw *pw, int msg, PwEventArgs *e) {
    switch (msg) {
    case PWE_DRAW:
        Pg *g = e->draw.g;
        sidebar(pgSubsectionCanvas(g, pgRect(pgPt(0 B, 0 B), pgPt(25 B, 75 B))));
        designView(pgSubsectionCanvas(g, pgRect(pgPt(25 B, 0 B), pgPt(100 B, 75 B))));
        break;
    case PWE_MOUSE_DOWN:
        char temp[32];
        sprintf(temp, "%0.0f,%0.0f,", e->mouse.at.x - 25 B, 75 B - e->mouse.at.y);
        for (char *p = temp; *p; p++) insert(*p);
        pwRefresh(pw);
        break;
    
    case PWE_KEY_DOWN:
        switch (e->key.key) {
        case 'W':
            if (e->key.ctl) pwClose(pw);
            break;
        case 27:
            pwClose(pw);
            break;
        case 'M': case 'm':
        case 'Z': case 'z':
        case 'L': case 'l':
        case 'H': case 'h':
        case 'V': case 'v':
        case 'C': case 'c':
        case 'S': case 's':
        case 'Q': case 'q':
        case 'T': case 't':
        case ' ': case ',':
        case '0': case '1':
        case '2': case '3':
        case '4': case '5':
        case '6': case '7':
        case '8': case '9':
        case '-': case '+':
            insert(e->key.key);
            pwRefresh(pw);
            break;
        case 8:
            backspace();
            pwRefresh(pw);
            break;
        }
        break;
    }
}

int main() {

    FILE *file = fopen("test.pgsf", "r");
    char data[65536];
    fread(data, 1, sizeof data, file);
    TargetFont = pgLoadFont(data, 0);


    Cursor = strlen(GlyphData);

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