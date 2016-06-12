#include <iso646.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "pg.h"
#include "platform.h"
#include "util.h"

const char      *SimpleSignature = "# PG SIMPLE FONT";

static void freeFont(PgFont *font) {
    free((void*)((PgSimpleFont*)font)->glyphData);
}
static PgPath *getGlyphPath(PgFont *font, PgPath *path, int glyph) {
    PgSimpleFont *sf = (PgSimpleFont*)font;
    if (glyph < 0 or glyph >= 65536 or not sf->glyphData[glyph])
        return NULL;
    
    const uint8_t *start = sf->glyphData[glyph];
    int length = strcspn(start, "\r\n#");
    uint8_t *svg = malloc(length + 1);
    memmove(svg, start, length);
    svg[length] = 0;
    path = pgInterpretSvgPath(path, svg);
    
    PgMatrix ctm = font->ctm;
    ctm.d *= -1;
    ctm.f -= font->ascender * ctm.d;
    pgTransformPoints(ctm, path->data, path->npoints);
    
    free(svg);
    return path;
}
static int getGlyph(PgFont *font, int c) {
    return c;
}
static float getGlyphWidth(PgFont *font, int glyph) {
    PgSimpleFont *sf = (PgSimpleFont*)font;
    return (glyph < 0 or glyph >= 65536) ? 0.0f :
        sf->glyphData[glyph] ? sf->avgWidth * font->ctm.a :
        0.0f;
}
static uint32_t *getFeatures(PgFont *font) {
    return calloc(1, sizeof(uint32_t));
}
static void setFeatures(PgFont *font, const uint32_t *tags) {
}
bool pgIsSimpleFont(const void *file) {
    return not memcmp(file, SimpleSignature, strlen(SimpleSignature));
}
static bool stringField(const char *cmd, const char *src, int length, const char *name, const wchar_t **output) {
    if (strcmp(cmd, name)) return false;
    *output = calloc(length + 1, sizeof **output);
    wchar_t *out = (wchar_t*) *output;
    const char *end = src + length;
    for (const char *in = src; in < end; )
        *out++ = pgStepUtf8(&in);
    *out = 0;
    return true;
}
static bool intField(const char *cmd, const char *src, int length, const char *name, int *output) {
    if (strcmp(cmd, name)) return false;
    *output = strtoull(src, NULL, 0);
    return true;
}
static bool floatField(const char *cmd, const char *src, int length, const char *name, float *output) {
    if (strcmp(cmd, name)) return false;
    *output = strtod(src, NULL);
    return true;
}
static bool boolField(const char *cmd, const char *src, int length, const char *name, bool *output) {
    if (strcmp(cmd, name)) return false;
    *output =   not strcmp(src, "yes") or
                not strcmp(src, "true") or
                not strcmp(src, "1");
    return true;
}
PgSimpleFont *pgLoadSimpleFontHeader(const void *file, int fontIndex) {
    PgSimpleFont    *sf = calloc(1, sizeof *sf);
    PgFont          *font = (PgFont*)sf;
    char            cmd[32];
    const char      *p = file;
    
    pgIdentityMatrix(&font->ctm);
    font->file = file;
    font->nfonts = 1;
    sf->glyphData = calloc(65536, sizeof *sf->glyphData);
    while (*p) {
        int cmdLength = strcspn(p, " :\t\r\n#");
        if (not cmdLength or cmdLength >= sizeof cmd - 1) {
            for (p++; *p and p[-1] != '\n'; p++);
            continue;
        }
        
        for (int i = 0; i < cmdLength; i++)
            cmd[i] = tolower(*p++);
        cmd[cmdLength] = 0;
        
        p += strspn(p, " :\t");
        const char *data = p;
        int dataLength = strcspn(data, "\r\n#");
        p += dataLength;
        
        if (stringField(cmd, data, dataLength, "family-name", &font->familyName));
        else if (stringField(cmd, data, dataLength, "full-name", &font->name));
        else if (stringField(cmd, data, dataLength, "style-name", &font->styleName));
        else if (floatField(cmd, data, dataLength, "em", &font->em));
        else if (floatField(cmd, data, dataLength, "x-height", &font->xHeight));
        else if (floatField(cmd, data, dataLength, "cap-height", &font->capHeight));
        else if (floatField(cmd, data, dataLength, "ascender", &font->ascender));
        else if (floatField(cmd, data, dataLength, "descender", &font->descender));
        else if (floatField(cmd, data, dataLength, "line-gap", &font->lineGap));
        else if (floatField(cmd, data, dataLength, "avgerage-width", &sf->avgWidth));
        else if (intField(cmd, data, dataLength, "weight", &font->weight));
        else if (boolField(cmd, data, dataLength, "italic?", &font->isItalic));
        else if (not strcmp(cmd, "end")) break;
        else if (cmd[0] == 'u' and cmd[1] == '-' and strlen(cmd) == 6) {
            unsigned codepoint = strtoul(cmd + 2, 0, 16);
            sf->glyphData[codepoint] = data;
        }
    }
    return sf;
}
PgSimpleFont *pgLoadSimpleFont(const void *file, int fontIndex) {
    PgSimpleFont *sf = pgLoadSimpleFontHeader(file, fontIndex);
    PgFont *font = (PgFont*)sf;
    font->free = freeFont;
    font->getGlyphPath = getGlyphPath;
    font->getGlyph = getGlyph;
    font->getGlyphWidth = getGlyphWidth;
    font->getFeatures = getFeatures;
    font->setFeatures = setFeatures;
    return sf;
}