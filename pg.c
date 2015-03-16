#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pg.h"

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
Pg *pgNewBitmapCanvas(int width, int height) {
    Pg *g = calloc(1, sizeof *g);
    g->resize = bmp_resize;
    g->clear = bmp_clear;
    g->free = bmp_free;
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
