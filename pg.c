#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdarg.h>
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
static float distance(PgPt p) { return sqrtf(p.x * p.x + p.y * p.y); }
static PgPt midpoint(PgPt a, PgPt b) { return pgPt((a.x + b.x) / 2.0f, (a.y + b.y) / 2.0f); }

typedef struct { float x, m, y2; } edge_t;
typedef struct { PgPt a, b; float m; } seg_t;
typedef struct { seg_t *data; int n, cap; } segs_t;
static void addSeg(segs_t *segs, PgPt a, PgPt b) {
    if (segs->n >= segs->cap) {
        segs->cap = segs->cap? segs->cap * 2: 8;
        segs->data = realloc(segs->data, segs->cap * sizeof(seg_t));
    }
    segs->data[segs->n++] = (seg_t) { a, b };
}
static float Flatness = 1.01f;
static void segmentQuad(segs_t *segs, PgPt a, PgPt b, PgPt c) {
    float ab = distance(pgPt(a.x - b.x, a.y - b.y));
    float bc = distance(pgPt(b.x - c.x, b.y - c.y));
    float ac = distance(pgPt(a.x - c.x, a.y - c.y));
    if (ab + bc + ac >= Flatness * ac) {
        PgPt ab = midpoint(a, b);
        PgPt bc = midpoint(b, c);
        PgPt abc = midpoint(ab, bc);
        segmentQuad(segs, a, ab, abc);
        segmentQuad(segs, abc, bc, c);
    } else addSeg(segs, a, c);
}
static void segmentCubic(segs_t *segs, PgPt a, PgPt b, PgPt c, PgPt d) {
    float ab = distance(pgPt(a.x - b.x, a.y - b.y));
    float bc = distance(pgPt(b.x - c.x, b.y - c.y));
    float cd = distance(pgPt(c.x - d.x, c.y - d.y));
    float ad = distance(pgPt(a.x - d.x, a.y - d.y));
    if (ab + bc + cd >= Flatness * ad) {
        PgPt ab = midpoint(a, b);
        PgPt bc = midpoint(b, c);
        PgPt cd = midpoint(c, d);
        PgPt abc = midpoint(ab, bc);
        PgPt bcd = midpoint(bc, cd);
        PgPt abcd = midpoint(abc, bcd);
        segmentCubic(segs, a, ab, abc, abcd);
        segmentCubic(segs, abcd, bcd, cd, d);
    } else addSeg(segs, a, d);
}
static int sortSegsDescending(const void *x, const void *y) {
    const seg_t *a = x;
    const seg_t *b = y;
    return
        a->a.y < b->a.y? -1:
        a->a.y > b->a.y? 1:
        a->a.x < b->a.x? -1:
        a->a.x > b->a.x? 1: 0;
}

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
static segs_t bmp_segmentPath(PgPath *path, PgMatrix ctm, bool close) {
    segs_t segs = { .data = NULL, .n = 0 };
    PgPt *p = path->data;
    int *t = (int*)path->types;
    // Decompose curves into line segments
    for (int *sub = path->subs; sub < path->subs + path->nsubs; sub++) {
        PgPt a = *p++, first = a;
        for (PgPt *end = p + *sub - 1; p < end; a = (p += *t++)[-1])
            if (*t == PG_LINE) addSeg(&segs, a, p[0]);
            else if (*t == PG_QUAD) segmentQuad(&segs, a, p[0], p[1]);
            else if (*t == PG_CUBIC) segmentCubic(&segs, a, p[0], p[1], p[2]);
        if (close)
            addSeg(&segs, a, first);
    }
    // Transform points and make sure point A is the above B
    // Then sort them so that A's that Y's are first; for ties, low X's first
    for (int i = 0; i < segs.n; i++) {
        PgPt a = pgTransformPoint(&ctm, segs.data[i].a);
        PgPt b = pgTransformPoint(&ctm, segs.data[i].b);
        float m = a.y < b.y?
            (b.x - a.x) / (b.y - a.y):
            a.y == b.y? 0: (a.x - b.x) / (a.y - b.y);
        segs.data[i].a = a.y < b.y? a: b;
        segs.data[i].b = a.y < b.y? b: a;
        segs.data[i].m = m;
    }
    return segs;
}
static void bmp_fillPath(Pg *g, PgPath *path, uint32_t color) {
    segs_t segs = bmp_segmentPath(path, g->ctm, true);
    qsort(segs.data, segs.n, sizeof(seg_t), sortSegsDescending);
    float maxY = segs.data[0].b.y;
    for (seg_t *seg = segs.data + 1; seg < segs.data + segs.n; seg++)
        maxY = max(maxY, seg->b.y);
    
    // Scan through lines filling between edge
    typedef struct { float x, m, y2; } edge_t;
    edge_t *edges = malloc(segs.n * sizeof (edge_t));
    int nedges = 0;
    seg_t *seg = segs.data;
    seg_t *endSeg = seg + segs.n;
    for (int scanY = max(0, segs.data[0].a.y); scanY < min(maxY, g->height); scanY++) {
        float y = scanY + 0.5f;
        edge_t *endEdge = edges + nedges;
        nedges = 0;
        for (edge_t *e = edges; e < endEdge; e++)
            if (y < e->y2) {
                e->x += e->m;
                edges[nedges++] = *e;
            }
        for ( ; seg < endSeg && seg->a.y < y; seg++)
            if (seg->b.y > y)
                edges[nedges++] = (edge_t) {
                    .x = seg->a.x + seg->m * (y - seg->a.y),
                    .m = seg->m,
                    .y2 = seg->b.y,
                };
        for (int i = 1; i < nedges; i++)
            for (int j = i; j > 0 && edges[j - 1].x > edges[j].x; j--) {
                edge_t tmp = edges[j];
                edges[j] = edges[j - 1];
                edges[j - 1] = tmp;
            }
        uint32_t *__restrict bmp = g->bmp + scanY * g->width;
        for (edge_t *e = edges + 1; e < edges + nedges; e += 2) {
            float x1 = e[-1].x;
            float x2 = e[0].x;
            if (x2 < 0 || x1 >= g->width) continue;
            int a = max(0, x1);
            int b = min(x2, g->width - 1);
            if (x1 >= 0) {
                bmp[a] = blend(bmp[a], color, fraction(x1) * 255);
                a++;
            }
            for (int x = a; x < b; x++) bmp[x] = color;
            if (x2 < g->width)
                bmp[b] = blend(bmp[b+1], color, (1 - fraction(x2)) * 255);
        }
    }
    free(edges);
    free(segs.data);
}
static void bmp_strokePath(Pg *g, PgPath *path, float width, uint32_t color) {
    PgMatrix otm = g->ctm;
    segs_t segs = bmp_segmentPath(path, g->ctm, false);
    for (seg_t *seg = segs.data; seg < segs.data + segs.n; seg++) {
        PgPath *sub = pgNewPath();
        float dx = seg->b.x - seg->a.x;
        float dy = seg->b.y - seg->a.y;
        float len = sqrt(dx*dx + dy*dy);
        float rad = atan2f(dy, dx);
        pgIdentity(g);
        pgRotate(g, rad);
        pgTranslate(g, seg->a.x, seg->a.y);
        PgPt vert[4] = {
            { -width / 2.0f, -width / 2.0f },
            { len + width / 2.0f, -width / 2.0f },
            { len + width / 2.0f, +width / 2.0f },
            { -width / 2.0f, + width / 2.0f },
        };
        pgMove(sub, vert[0]);
        pgLine(sub, vert[1]);
        pgLine(sub, vert[2]);
        pgLine(sub, vert[3]);
        pgFillPath(g, sub, color);
        pgFreePath(sub);
    }
    free(segs.data);
    g->ctm = otm;
}
Pg *pgNewBitmapCanvas(int width, int height) {
    Pg *g = calloc(1, sizeof *g);
    g->resize = bmp_resize;
    g->clear = bmp_clear;
    g->free = bmp_free;
    g->fillPath = bmp_fillPath;
    g->strokePath = bmp_strokePath;
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
void pgIdentity(Pg *g) { pgIdentityMatrix(&g->ctm); }
void pgTranslate(Pg *g, float x, float y) { pgTranslateMatrix(&g->ctm, x, y); }
void pgScale(Pg *g, float x, float y) { pgScaleMatrix(&g->ctm, x, y); }
void pgShear(Pg *g, float x, float y) { pgShearMatrix(&g->ctm, x, y); }
void pgRotate(Pg *g, float rad) { pgRotateMatrix(&g->ctm, rad); }
void pgMultiply(Pg *g, const PgMatrix * __restrict b) { pgMultiplyMatrix(&g->ctm, b); }

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
static int pathCapacity(int n) {
    return n;
}
static void addPathPart(PgPath *path, int type, ...) {
    if (type == PG_MOVE) {
        if (path->nsubs >= pathCapacity(path->nsubs))
            path->subs = realloc(path->subs, pathCapacity(path->nsubs + 1) * sizeof(int));
        path->subs[path->nsubs++] = 0;
    } else {
        if (path->ntypes >= pathCapacity(path->ntypes))
            path->types = realloc(path->types, pathCapacity(path->ntypes + 1) * sizeof(int));
        path->types[path->ntypes++] = type;
    }
    
    if (path->n >= pathCapacity(path->n))
        path->data = realloc(path->data, pathCapacity(path->n + (type? type: 1)) * sizeof(PgPt));
    
    va_list ap;
    va_start(ap, type);
    for (int i = 0; i < (type? type: 1); i++) {
        path->data[path->n++] = va_arg(ap, PgPt);
        path->subs[path->nsubs - 1]++;
    }
    va_end(ap);
}
PgPath *pgNewPath() {
    PgPath *path = calloc(1, sizeof *path);
    return path;
}
void pgFreePath(PgPath *path) {
    free(path->subs);
    free(path->types);
    free(path->data);
    free(path);
}
void pgMove(PgPath *path, PgPt a) {
    addPathPart(path, PG_MOVE, a);
}
void pgLine(PgPath *path, PgPt b) {
    addPathPart(path, PG_LINE, b);
}
void pgQuad(PgPath *path, PgPt b, PgPt c) {
    addPathPart(path, PG_QUAD, b, c);
}
void pgCubic(PgPath *path, PgPt b, PgPt c, PgPt d) {
    addPathPart(path, PG_CUBIC, b, c, d);
}
void pgFillPath(Pg *g, PgPath *path, uint32_t color) {
    g->fillPath(g, path, color);
}
void pgStrokePath(Pg *g, PgPath *path, float width, uint32_t color) {
    g->strokePath(g, path, width, color);
}