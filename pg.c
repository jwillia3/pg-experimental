#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pg.h"

#undef min
#undef max
static float min(float a, float b) { return a < b? a: b; }
static float max(float a, float b) { return a > b? a: b; }
static float clamp(float a, float b, float c) { return min(max(a, b), c); }
static float fraction(float a) { return a - floor(a); }


static uint32_t blend(uint32_t fg, uint32_t bg, uint32_t a) {
    if (a == 0xff)
        return fg;
    else if (a == 0)
        return bg;
    unsigned na = 255 - a;
    unsigned rb = ((( fg & 0x00ff00ff) * a) +
                    ((bg & 0x00ff00ff) * na)) &
                    0xff00ff00;
    unsigned g = (((  fg & 0x0000ff00) * a) +
                    ((bg & 0x0000ff00) * na)) &
                    0x00ff0000;
    return (rb | g) >> 8;
}

static void bmp_clear(Pg *g, uint32_t color) {
    for (int i = 0; i < g->width * g->height; i++)
        g->bmp[i] = color;
}
static void bmp_free(Pg *g) {
    free(g->bmp);
    free(g);
}
static void bmp_resize(Pg *g, int width, int height) {
    free(g->bmp);
    g->width = width;
    g->height = height;
    g->bmp = malloc(width * height * 4);
}
static void bmp_hline(uint32_t *__restrict bmp, float x1, float x2, uint32_t color) {
    int a = x1;
    int b = x2;
    bmp[a] = blend(bmp[a], color, fraction(x1) * 255);
    for (int x = a + 1; x <= b; x++) bmp[x] = color;
    bmp[b+1] = blend(bmp[b+1], color, (1 - fraction(x2)) * 255);
}
static void bmp_triangle(Pg *g, PgPt a, PgPt b, PgPt c, uint32_t color) {
    if (a.y > b.y) { PgPt t = b; b = a; a = t; }
    if (b.y > c.y) {
        PgPt t = b;
        if (a.y > c.y) { b = a; a = c; c = t; }
        else { b = c; c = t; }
    }
    float ab = b.y - a.y > 0? (b.x - a.x) / (b.y - a.y): 0;
    float ac = c.y - a.y > 0? (c.x - a.x) / (c.y - a.y): 0;
    float bc = c.y - b.y > 0? (c.x - b.x) / (c.y - b.y): 0;
    float x1 = a.x, x2 = a.x;
    uint32_t *__restrict bmp = g->bmp + (int)a.y * g->width;
    if (ab > ac) {
        for (float y = a.y; y < b.y; y++, bmp += g->width, x1 += ac, x2 += ab)
            bmp_hline(bmp, x1, x2, color);
        x2 = b.x;
        for (float y = b.y; y < c.y; y++, bmp += g->width, x1 += ac, x2 += bc)
            bmp_hline(bmp, x1, x2, color);
    } else {
        for (float y = a.y; y < b.y; y++, bmp += g->width, x1 += ab, x2 += ac)
            bmp_hline(bmp, x1, x2, color);
        x1 = b.x;
        for (float y = b.y; y < c.y; y++, bmp += g->width, x1 += bc, x2 += ac)
            bmp_hline(bmp, x1, x2, color);
    }
}
static void bmp_triangleStrip(Pg *g, PgPt *v, int n, uint32_t color) {
    for (int i = 2; i < n; i++)
        g->triangle(g, v[i - 2], v[i - 1], v[i], color);
}
Pg *pgNewBitmapCanvas(int width, int height) {
    Pg *g = calloc(1, sizeof *g);
    g->resize = bmp_resize;
    g->clear = bmp_clear;
    g->free = bmp_free;
    g->triangle = bmp_triangle;
    g->triangleStrip = bmp_triangleStrip;
    pgIdentityMatrix(&g->ctm);
    g->resize(g, width, height);
    return g;
}
void pgClearCanvas(Pg *g, uint32_t color) {
    g->clear(g, color);
}
void pgFreeCanvas(Pg *g) {
    g->free(g);
}
void pgResizeCanvas(Pg *g, int width, int height) {
    g->resize(g, width, height);
}
void pgIdentityMatrix(PgMatrix *mat) {
    mat->a = 1;
    mat->b = 0;
    mat->c = 0;
    mat->d = 1;
    mat->e = 0;
    mat->f = 0;
}
void pgTranslateMatrix(PgMatrix *mat, float x, float y) {
    mat->e += x;
    mat->f += y;
}
void pgScaleMatrix(PgMatrix *mat, float x, float y) {
    mat->a *= x;
    mat->c *= x;
    mat->e *= x;
    mat->b *= y;
    mat->d *= y;
    mat->f *= y;
}
void pgShearMatrix(PgMatrix *mat, float x, float y) {
    mat->a = mat->a + mat->b * y;
    mat->c = mat->c + mat->d * y;
    mat->e = mat->e + mat->f * y;
    mat->b = mat->a * x + mat->b;
    mat->d = mat->c * x + mat->d;
    mat->f = mat->e * x + mat->f;
}
void pgRotateMatrix(PgMatrix *mat, float rad) {
    PgMatrix old = *mat;
    float m = cosf(rad);
    float n = sinf(rad);
    mat->a = old.a * m - old.b * n;
    mat->b = old.a * n + old.b * m;
    mat->c = old.c * m - old.d * n;
    mat->d = old.c * n + old.d * m;
    mat->e = old.e * m - old.f * n;
    mat->f = old.e * n + old.f * m;
}
void pgMultiplyMatrix(PgMatrix * __restrict a, const PgMatrix * __restrict b) {
    PgMatrix old = *a;
    
    a->a = old.a * b->a + old.b * b->c;
    a->c = old.c * b->a + old.d * b->c;
    a->e = old.e * b->a + old.f * b->c + b->e;
    
    a->b = old.a * b->b + old.b * b->d;
    a->d = old.c * b->b + old.d * b->d;
    a->f = old.e * b->b + old.f * b->d + b->f;
}
PgPt pgTransformPoint(const PgMatrix *ctm, PgPt p) {
    return (PgPt) {
        ctm->a * p.x + ctm->c * p.y + ctm->e,
        ctm->b * p.x + ctm->d * p.y + ctm->f
    };
}
PgPt *pgTransformPoints(const PgMatrix *ctm, PgPt *v, int n) {
    for (int i = 0; i < n; i++)
        v[i] = pgTransformPoint(ctm, v[i]);
    return v;
}
