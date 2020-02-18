#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "pg.h"
#include "platform.h"
#include "util.h"

#define CHR4(A,B,C,D) ((A << 24) + (B << 16) + (C << 8) + D)

struct CFFEntry {
    unsigned        key;
    const uint8_t   *value;
};

typedef struct {
    unsigned        n;
    struct CFFEntry *entries;
    const uint8_t   *base;
} CFFDict;

typedef struct {
    unsigned n;
    unsigned sz;
    const uint8_t *offsets;
    const uint8_t *data;
} CFFIndex;

struct PgOpenTypeFont {
    PgFont          _;

    // OpenType table pointers
    const int16_t   *hmtx;
    const void      *glyf;
    const void      *loca;
    const void      *gsub;
    uint16_t        *cmap;
    const uint8_t   *cff;

    // CFF/Postscript tables
    CFFIndex        gsubrIndex;
    CFFIndex        subrIndex;
    CFFIndex        charstringIndex;

    bool            longLoca;
    int             nhmtx;
    int             nglyphs;
    bool            isPostscript;
    uint32_t        lang;
    uint32_t        script;
};

#pragma pack(push, 1)
typedef struct {
    uint32_t ver;
    uint16_t ntables, range, selector, shift;
} SfntHeader;
typedef struct { uint32_t tag, ver, nfonts, offsets[]; } TtcHeader;
typedef struct {
        uint32_t ver, revision, checksum, magic;
        uint16_t flags, em;
        uint64_t created, modified;
        int16_t  minx, miny, maxx, maxy;
        uint16_t macStyle, lowestPPEM;
        int16_t  direction, locaFormat, glyfFormat;
} HeadTable;
typedef struct {
    uint32_t ver;
    int16_t ascender, descender, lineGap;
    uint16_t maxAdvWidth;
    int16_t minLsb, maxRsb, maxExtent, caretSlopeY, caretSlopeX, caretOffset;
    uint16_t resv[5], nhmtx;
} HheaTable;
typedef struct {
    uint16_t ver;
    int16_t avgWidth;
    uint16_t weight, width, fsType;
    int16_t subSizeX, subSizeY, subX, subY;
    int16_t supSizeX, supSizeY, supX, supY;
    int16_t strikeWidth, strikeY, family;
    uint8_t panose[10];
    uint32_t unicode[4];
    uint8_t vendor[4];
    uint16_t style, firstChar, lastChar;
    int16_t ascender, descender, lineGap, winAscent, winDescent;
    uint32_t codePages[2];
    int16_t xHeight, capHeight;
    uint16_t defaultChar, breakChar, maxContext, lowestPointSize, highestPointSize;
} Os2Table;
typedef struct {
    uint32_t ver;
    uint16_t nglyphs;
} MaxpTable;
typedef struct {
    uint16_t format, n, offset;
    struct NameTableRecord { uint16_t platform, encoding, language, name, len, offset; } rec[];
} NameTable;
typedef struct { uint16_t platform, encoding; uint32_t offset; } CmapSubTable;
typedef struct { uint16_t ver, n; CmapSubTable sub[]; } CmapTable;
typedef struct {
    uint16_t format, size, lang;
    uint8_t glyphs[];
} CmapFormat0;
typedef struct {
    uint16_t format, size, lang, n2, range, select, shift, ends[];
} CmapFormat4;
typedef struct {
    uint16_t ncontours, x1, y1, x2, y2, ends[];
} Glyf;
#pragma pack(pop)

static bool loadCFF(PgOpenTypeFont *otf);
static bool cffGetGlyphPath(PgOpenTypeFont *otf, PgPath *path, int glyph);

static int getGlyph(PgFont *font, int c) {
    PgOpenTypeFont *otf = (PgOpenTypeFont*)font;
    return otf->cmap[c & 0xffff];
}

static bool ttfGetGlyphPath(PgOpenTypeFont *otf, PgPath *path, int glyph) {
    glyph &= 0xffff;
    if (glyph >= otf->nglyphs)
        glyph = 0;
    const uint32_t *loca32 = otf->loca;
    const uint16_t *loca16 = otf->loca;
    const Glyf *data = otf->longLoca?
        loca32[glyph] == loca32[glyph + 1]? NULL:
            (void*)((uint8_t*)otf->glyf + nativeu32(loca32[glyph])):
        loca16[glyph] == loca16[glyph + 1]? NULL:
            (void*)((uint8_t*)otf->glyf + nativeu16(loca16[glyph]) * 2);
    if (!data) return false;

    int ncontours = native16(data->ncontours);
    if (ncontours > 0) {
        const uint16_t *ends = data->ends;
        int npoints = nativeu16(ends[ncontours - 1]) + 1;
        int ninstr = nativeu16(ends[ncontours]);
        const uint8_t *flags = (const uint8_t*)(ends + ncontours + 1) + ninstr;
        const uint8_t *f = flags;
        int xsize = 0;
        // Run through to find the start of the x coordinates
        for (int i = 0; i < npoints; i++) {
            int dx = *f & 2? 1:
                    *f & 16? 0:
                    2;
            if (*f & 8) {
                i += f[1];
                dx *= f[1] + 1;
                f += 2;
            } else f++;
            xsize += dx;
        }
        const uint8_t *xs = f;
        const uint8_t *ys = xs + xsize;
        PgPt a = {0, 0}, b;
        bool inCurve = false;
        int vx = 0, vy = 0;
        PgPt start;
        int leadingInCurveIndex = -1;
        for (int i = 0, rep = 0, end = 0; i < npoints; i++) {
            int dx = *flags & 2? *flags & 16? *xs++: -*xs++:
                    *flags & 16? 0:
                    native16( ((int16_t*)(xs += 2))[-1] );
            int dy = *flags & 4? *flags & 32? *ys++: -*ys++:
                    *flags & 32? 0:
                    native16( ((int16_t*)(ys += 2))[-1] );
            vx += dx;
            vy += dy;
            PgPt p = { vx, vy };
            if (i == end) {
                end = native16(*ends++) + 1;
                if (leadingInCurveIndex != -1) {
                    PgPt c = midpoint(path->data[leadingInCurveIndex], path->data[leadingInCurveIndex + 1]);
                    pgQuad(path, path->data[leadingInCurveIndex], c);
                    path->data[leadingInCurveIndex] = c;
                } else if (inCurve)
                    pgQuad(path, b, start);
                else
                    pgClosePath(path);
                if (i != npoints - 1)
                    pgMove(path, p);
                leadingInCurveIndex = ((~*flags & 1) && i != npoints - 1) ? path->npoints - 1 : -1;
                start = a = p;
                inCurve = false;
            } else if (~*flags & 1) // curve
                if (inCurve) {
                    PgPt q = midpoint(b, p);
                    pgQuad(path, b, q);
                    a = q;
                    b = p;
                } else {
                    b = p;
                    inCurve = true;
                }
            else if (inCurve) { // curve to line
                pgQuad(path, b, p);
                a = p;
                inCurve = false;
            } else { // line to line
                pgLine(path, p);
                a = p;
            }

            if (rep) {
                if (--rep == 0)
                    flags += 2;
            } else if (*flags & 8) {
                rep = flags[1];
                if (!rep) flags += 2;
            } else
                flags++;
        }
        if (leadingInCurveIndex != -1) {
            PgPt c = midpoint(path->data[leadingInCurveIndex], path->data[leadingInCurveIndex + 1]);
            pgQuad(path, path->data[leadingInCurveIndex], c);
            path->data[leadingInCurveIndex] = c;
        } else if (inCurve)
            pgQuad(path, b, start);
        else
            pgClosePath(path);
    } else if (ncontours < 0) {
        uint16_t *base = (uint16_t*)(data + 1);
        uint16_t flags;
        do {
            flags = nativeu16(*base++);
            uint16_t index = nativeu16(*base++);

            float a = 1, b = 0, c = 0, d = 1, e = 0, f = 0;
            if ((flags & 3) == 3) { // x & y words
                e = native16(*base++);
                f = native16(*base++);
            } else if (flags & 2) { // x & y bytes
                e = ((int8_t*)base)[0];
                f = ((int8_t*)base)[1];
                base++;
            } else if (flags & 1) // "matcing points"???
                base +=2, e = f =0;
            else base++; // "matching points"???

            if (flags & 8) // single scale
                a = d = native16(*base++);
            else if (flags & 64) { // x & y scales
                a = native16(*base++);
                d = native16(*base++);
            } else if (flags & 128) { // 2x2
                a = native16(*base++);
                b = native16(*base++);
                c = native16(*base++);
                d = native16(*base++);
            }

            int oldN = path->npoints;
            ttfGetGlyphPath(otf, path, index);
            for (int i = oldN; i < path->npoints; i++) {
                float m = max(fabsf(a), fabsf(b));
                float n = max(fabsf(c), fabsf(d));
                if (fabsf(fabsf(a) - fabsf(c)) <= 33.0f / 65536.0f) m += m;
                if (fabsf(fabsf(c) - fabsf(d)) <= 33.0f / 65536.0f) n += n;
                path->data[i] = pgPt(
                    m * ((a / m) * path->data[i].x + (c / m) * path->data[i].y + e),
                    n * ((b / n) * path->data[i].x + (d / n) * path->data[i].y + f));
            }
        } while (flags & 32);
    }
    return true;
}
static PgPath *getGlyphPath(PgFont *font, PgPath *path, int glyph) {
    bool created = !path;
    if (created) path = pgNewPath();

    bool success = ((PgOpenTypeFont*) font)->isPostscript
        ? cffGetGlyphPath((PgOpenTypeFont *)font, path, glyph)
        : ttfGetGlyphPath((PgOpenTypeFont *)font, path, glyph);

    if (!success) {
        if (created)
            pgFreePath(path);
        return NULL;
    }

    PgMatrix ctm = font->ctm;
    ctm.d *= -1;
    ctm.f -= font->ascender * ctm.d;
    pgTransformPoints(ctm, path->data, path->npoints);
    return path;
}
static float getGlyphWidth(PgFont *font, int glyph) {
    PgOpenTypeFont *otf = (PgOpenTypeFont*)font;
    return glyph < otf->nhmtx? native16(otf->hmtx[glyph * 2]) * font->ctm.a:
            glyph < otf->nglyphs? native16(otf->hmtx[(otf->nhmtx - 1) * 2]) * font->ctm.a:
            0;
}

PgOpenTypeFont *pgLoadOpenTypeFontHeader(const void *file, int fontIndex) {
    if (!file) return NULL;
    SfntHeader *sfnt = (void*)file;
    int nfonts = 1;
    bool isPostscript = false;

redoHeader:
    if (native32(sfnt->ver) == 0x10000) {
    } else if (native32(sfnt->ver) == CHR4('O','T','T','O'))
        isPostscript = true;
    else if (native32(sfnt->ver) == CHR4('t','t','c','f')) { // TrueType Collection
        TtcHeader *ttc = (void*)file;
        nfonts = nativeu32(ttc->nfonts);
        if (fontIndex >= nfonts) return NULL;
        sfnt = (void*)((uint8_t*)file + nativeu32(ttc->offsets[fontIndex]));
        goto redoHeader;
    } else return NULL;

    const HeadTable *head = NULL;
    const HheaTable *hhea = NULL;
    const Os2Table *os2  = NULL;
    const MaxpTable *maxp = NULL;
    const NameTable *name = NULL;
    const void *cmap = NULL;
    const void *hmtx = NULL;
    const void *glyf = NULL;
    const void *loca = NULL;
    const void *gsub = NULL;
    const void *cff = NULL;

    struct { uint32_t tag, checksum, offset, size; } *thead = (void*)(sfnt + 1);
    for (int i = 0; i < nativeu16(sfnt->ntables); i++, thead++) {
        void *address = (uint8_t*)file + nativeu32(thead->offset);
        switch(nativeu32(thead->tag)) {
        case CHR4('h','e','a','d'): head = address; break;
        case CHR4('h','h','e','a'): hhea = address; break;
        case CHR4('m','a','x','p'): maxp = address; break;
        case CHR4('c','m','a','p'): cmap = address; break;
        case CHR4('h','m','t','x'): hmtx = address; break;
        case CHR4('g','l','y','f'): glyf = address; break;
        case CHR4('l','o','c','a'): loca = address; break;
        case CHR4('O','S','/','2'): os2  = address; break;
        case CHR4('n','a','m','e'): name = address; break;
        case CHR4('G','S','U','B'): gsub = address; break;
        case CHR4('C','F','F',' '): cff = address; break;
        }
    }

    // These are required by all OpenType fonts
    if (!cmap || !head || !hhea || !hmtx || !maxp || !name || !os2)
        return NULL;

    if (!isPostscript && (!glyf || !loca))
        return NULL;
    if (isPostscript && (!cff))
        return NULL;

    PgOpenTypeFont *otf = calloc(1, sizeof *otf);
    PgFont *font = &otf->_;
    font->file = file;
    otf->isPostscript = isPostscript;
    otf->hmtx = hmtx;
    otf->glyf = glyf;
    otf->loca = loca;
    otf->cmap = (void*)cmap;
    otf->gsub = gsub;
    otf->cff = cff;
    otf->lang = CHR4('e','n','g',' ');
    otf->script = CHR4('l','a','t','n');
    font->ctm = (PgMatrix){ 1, 0, 0, 1, 0, 0 };
    font->nfonts = nfonts;

    font->em = nativeu16(head->em);
    otf->longLoca = native16(head->locaFormat);
    otf->nhmtx = nativeu16(hhea->nhmtx);
    font->weight = nativeu16(os2->weight);
    memmove(font->panose, os2->panose, 10);
    font->ascender = native16(os2->ascender);
    font->descender = native16(os2->descender);
    font->lineGap = native16(os2->lineGap);
    font->xHeight = native16(os2->xHeight);
    font->capHeight = native16(os2->capHeight);
    font->isItalic = nativeu16(os2->style) & 0x101; // italic or oblique
    otf->nglyphs = nativeu16(maxp->nglyphs);
    font->isFixedPitched = font->panose[3] == 9;

    int n = nativeu16(name->n);
    const uint8_t *name_data = (uint8_t*)name + nativeu16(name->offset);
    for (int i = 0; i < n; i++) {
        int platform = nativeu16(name->rec[i].platform);
        int encoding = nativeu16(name->rec[i].encoding);
        int offset = nativeu16(name->rec[i].offset);
        int len = nativeu16(name->rec[i].len);
        int language = nativeu16(name->rec[i].language);
        int id = nativeu16(name->rec[i].name);
        if (platform == 0 || (platform == 3 && (encoding == 0 || encoding == 1) && language == 0x0409)) {
            if (id == 1 || id == 2 || id == 4 || id == 16 || id == 17) {
                const uint16_t *source = (uint16_t*)(name_data + offset);
                uint16_t *output = malloc((len / 2 + 1) * sizeof *output);
                len /= 2;
                for (int i = 0; i < len; i++)
                    output[i] = nativeu16(source[i]);
                output[len] = 0;
                if (id == 1)
                    font->familyName = (wchar_t*) output;
                else if (id == 2)
                    font->styleName = (wchar_t*) output;
                else if (id == 4)
                    font->name = (wchar_t*) output;
                else if (id == 16) { // Preferred font family
                    free((void*) font->familyName);
                    font->familyName = (wchar_t*) output;
                }
                else if (id == 17) { // Preferred font style
                    free((void*) font->styleName);
                    font->styleName = (wchar_t*) output;
                }
            }
        }
        else if (platform == 1 && encoding == 0)
            if (id == 1 || id == 2 || id == 4 || id == 16) {
                const uint8_t *source = name_data + offset;
                uint16_t *output = malloc((len + 1) * sizeof *output);
                for (int i = 0; i < len; i++)
                    output[i] = source[i];
                output[len] = 0;

                if (id == 1)
                    font->familyName = (wchar_t*) output;
                else if (id == 2)
                    font->styleName = (wchar_t*) output;
                else if (id == 4)
                    font->name = (wchar_t*) output;
                else if (id == 16) { // Preferred font family
                    free((void*) font->familyName);
                    font->familyName = (wchar_t*) output;
                }
                else if (id == 17) { // Preferred font style
                    free((void*) font->styleName);
                    font->styleName = (wchar_t*) output;
                }
            }
    }
    if (!font->familyName)
        font->familyName = wcsdup(L"");
    if (!font->styleName)
        font->styleName = wcsdup(L"");
    if (!font->name)
        font->name = wcsdup(L"");

    if (isPostscript)
        if (!loadCFF(otf))
            return NULL;

    return otf;
}
static void freeFont(PgFont *font) {
    (void)font; // unused parameter
}

#pragma pack(push, 1)
typedef struct {
    uint32_t ver;
    uint16_t scriptList;
    uint16_t featureList;
    uint16_t lookupList;
} GsubTable;
typedef struct {
    uint32_t tag;
    uint16_t offset;
} Tag;
typedef struct {
    uint16_t nscripts;
    Tag tag[];
} ScriptList;
typedef struct {
    uint16_t defaultLanguage, nlangs;
    Tag tag[];
} ScriptTable;
typedef struct {
    uint16_t lookupOrder, required, nfeatures, features[];
} LangSysTable;
typedef struct {
    uint16_t nfeatures;
    Tag tag[];
} FeatureList;
typedef struct {
    uint16_t params, nlookups, lookups[];
} FeatureTable;
typedef struct {
    uint16_t nlookups, lookups[];
} LookupList;
typedef struct {
    uint16_t type, flag, nsubtables, subtables[];
} LookupTable;
typedef union {
    struct { uint16_t format, coverage; };
    struct {
        uint16_t format, coverage, delta;
    } f1;
    struct {
        uint16_t format, coverage, nglyphs, glyphs[1];
    } f2;
    struct {
        uint16_t format, lookupType;
        uint32_t offset;
    } f7;
} GsubFormat;
typedef union {
    uint16_t format;
    struct {
        uint16_t format, nglyphs, glyphs[1];
    } f1;
    struct {
        uint16_t format, nranges;
        struct {
            uint16_t start, end, coverageIndex;
        } ranges[1];
    } f2;
} Coverage;

#pragma pack(pop)

static const void *offset(const void *base, unsigned offset) {
    return (const uint8_t*)base + offset;
}

static void gsubSubtable(PgFont *font, int type, const GsubFormat *subtable) {
    const Coverage *coverage = offset(subtable, nativeu16(subtable->coverage));

    if (type == 1) { // Single Substitution
        if (nativeu16(subtable->format) == 1) {
            if (nativeu16(coverage->format) == 1)
                for (int i = 0; i < nativeu16(coverage->f1.nglyphs); i++) {
                    uint16_t g = nativeu16(coverage->f1.glyphs[i]);
                    pgSubstituteGlyph(font, g, g + nativeu16(subtable->f1.delta));
                }
            else if (nativeu16(coverage->format) == 2)
                for (int i = 0; i < nativeu16(coverage->f2.nranges); i++) {
                    int start = nativeu16(coverage->f2.ranges[i].start);
                    int end = nativeu16(coverage->f2.ranges[i].end);
                    for (int g = start; g <= end; g++)
                        pgSubstituteGlyph(font, g, g + nativeu16(subtable->f1.delta));
                }
        } else if (nativeu16(subtable->format) == 2) {
            int o = 0;
            if (nativeu16(coverage->format) == 1)
                for (int i = 0; i < nativeu16(coverage->f1.nglyphs); i++) {
                    uint16_t input = nativeu16(coverage->f1.glyphs[i]);
                    uint16_t output = nativeu16(subtable->f2.glyphs[o++]);
                    pgSubstituteGlyph(font, input, output);
                }
            else if (nativeu16(coverage->format) == 2)
                for (int i = 0; i < nativeu16(coverage->f2.nranges); i++) {
                    int start = nativeu16(coverage->f2.ranges[i].start);
                    int end = nativeu16(coverage->f2.ranges[i].end);
                    for (int input = start; input <= end; input++) {
                        uint16_t output = nativeu16(subtable->f2.glyphs[o++]);
                        pgSubstituteGlyph(font, input, output);
                    }
                }
        }
    } else if (type == 7) // Extension
        gsubSubtable(font,
            nativeu16(subtable->f7.lookupType),
            offset(subtable, nativeu32(subtable->f7.offset)));
}
static uint32_t *lookupFeatures(PgFont *font, const uint32_t *tags) {
    const GsubTable *gsub = ((PgOpenTypeFont*) font)->gsub;
    if (!gsub)
        return calloc(1, sizeof (uint32_t));
    const ScriptList *scriptList = offset(gsub, nativeu16(gsub->scriptList));

    if (!tags)
        tags = (uint32_t[1]) { 0 };

    // Script List -> Script Table
    const ScriptTable *scriptTable = NULL;
    for (int i = 0; i < nativeu16(scriptList->nscripts); i++)
        if (nativeu32(scriptList->tag[i].tag) == CHR4('D','F','L','T') ||
            nativeu32(scriptList->tag[i].tag) == ((PgOpenTypeFont*) font)->script)
        {
            scriptTable = offset(scriptList, nativeu16(scriptList->tag[i].offset));
        }

    // Script Table -> Language System Table
    const LangSysTable *langSysTable = NULL;
    if (scriptTable) {
        if (nativeu32(scriptTable->defaultLanguage))
            langSysTable = offset(scriptTable, nativeu16(scriptTable->defaultLanguage));
        for (int i = 0; i < nativeu16(scriptTable->nlangs); i++)
            if (nativeu32(scriptTable->tag[i].tag) == ((PgOpenTypeFont*) font)->lang)
                langSysTable = offset(scriptTable, nativeu16(scriptTable->tag[i].offset));
    }

    uint32_t *returnFeatures = NULL;

    // Language System Table -> Feature Table
    if (langSysTable) {
        const FeatureList *featureList = offset(gsub, nativeu16(gsub->featureList));

        returnFeatures = calloc(1 + nativeu16(langSysTable->nfeatures), sizeof *returnFeatures);

        for (int i = 0; i < nativeu16(langSysTable->nfeatures); i++) {
            const FeatureTable *featureTable = NULL;
            int index = nativeu16(langSysTable->features[i]);
            for (int j = 0; tags[j]; j++)
                if (featureList->tag[index].tag == tags[j])
                    featureTable = offset(featureList, nativeu16(featureList->tag[index].offset));

            if (returnFeatures)
                returnFeatures[i] = nativeu32(featureList->tag[index].tag);

            // Tag was not selected by user
            if (!featureTable)
                continue;

            const LookupList *lookupList = offset(gsub, nativeu16(gsub->lookupList));
            for (int i = 0; i < nativeu16(featureTable->nlookups); i++) {
                int index = nativeu16(featureTable->lookups[i]);
                const LookupTable *lookupTable = offset(lookupList, nativeu16(lookupList->lookups[index]));

                for (int i = 0; i < nativeu16(lookupTable->nsubtables); i++)
                    gsubSubtable(font,
                        nativeu16(lookupTable->type),
                        offset(lookupTable, nativeu16(lookupTable->subtables[i])));
            }
        }
    }
    return returnFeatures;
}
static uint32_t *getFeatures(PgFont *font) {
    return lookupFeatures(font, NULL);
}
static void setFeatures(PgFont *font, const uint32_t *tags) {
    if (tags)
        lookupFeatures(font, tags);
}

PgOpenTypeFont *pgLoadOpenTypeFont(const void *file, int fontIndex) {
    PgOpenTypeFont *otf = (PgOpenTypeFont*)pgLoadOpenTypeFontHeader(file, fontIndex);
    if (!otf) return NULL;
    PgFont *font = &otf->_;

    const CmapTable *cmap = (void*)otf->cmap;
    const void *table = NULL;
    for (int i = 0; i < nativeu16(cmap->n); i++)
        if ((nativeu16(cmap->sub[i].platform) == 3 && nativeu16(cmap->sub[i].encoding) == 1) ||
            (nativeu16(cmap->sub[i].platform) == 0 && nativeu16(cmap->sub[i].encoding) == 1) ||
            (!table && nativeu16(cmap->sub[i].platform) == 1 && nativeu16(cmap->sub[i].encoding) == 0))
        {
            table = (uint8_t*)cmap + nativeu32(cmap->sub[i].offset);
        }

    if (!table) {
        otf->cmap = NULL;
        pgFreeFont(font);
        return NULL;
    }

    otf->cmap = calloc(65536, sizeof(uint16_t));
    switch (nativeu16(*(uint16_t*)table)) {
    case 0: {
            const CmapFormat0 *tab = table;
            for (int i = 0; i < 255; i++)
                otf->cmap[i] = tab->glyphs[i];
            break;
        }
    case 4: {
            const CmapFormat4 *tab = table;
            int n = nativeu16(tab->n2) / 2;
            const uint16_t *endp = tab->ends;
            const uint16_t *startp = tab->ends + n + 1;
            const uint16_t *deltap = startp + n;
            const uint16_t *offsetp = deltap + n;
            for (int i = 0; i < n; i++) {
                int end = nativeu16(endp[i]);
                int start = nativeu16(startp[i]);
                int delta = native16(deltap[i]);
                int offset = nativeu16(offsetp[i]);
                if (offset)
                    for (int c = start; c <= end; c++) {
                        int index = (offset / 2 + (c - start) + i) & 0xffff;
                        int g = native16(offsetp[index]);
                        otf->cmap[c] = g? g + delta: 0;
                    }
                else for (int c = start; c <= end; c++)
                    otf->cmap[c] = c + delta;
            }
            break;
        }
    }
    font->free = freeFont;
    font->getGlyph = getGlyph;
    font->getGlyphPath = getGlyphPath;
    font->getGlyphWidth = getGlyphWidth;
    font->setFeatures = setFeatures;
    font->getFeatures = getFeatures;
    return otf;
}

static int cffRead16(const uint8_t ** restrict cff) {
    uint8_t b1 = *(*cff)++;
    uint8_t b2 = *(*cff)++;
    return (b1 << 8) + b2;
}

static int cffRead24(const uint8_t ** restrict cff) {
    uint8_t b1 = *(*cff)++;
    uint8_t b2 = *(*cff)++;
    uint8_t b3 = *(*cff)++;
    return (b1 << 16) + (b2 << 8) + b3;
}

static int cffRead32(const uint8_t ** restrict cff) {
    uint8_t b1 = *(*cff)++;
    uint8_t b2 = *(*cff)++;
    uint8_t b3 = *(*cff)++;
    uint8_t b4 = *(*cff)++;
    return (b1 << 24) + (b2 << 16) + (b3 << 8) + b4;
}

static CFFDict *cffAddEntry(CFFDict *dict, unsigned key, const uint8_t *value) {
    if ((dict->n & 7) == 0)
        dict->entries = realloc(dict->entries, (dict->n + 7 + 1) * sizeof *dict->entries);
    dict->entries[dict->n++] = (struct CFFEntry) { key, value };
    return dict;
}

static unsigned cffOffset(const CFFIndex *index, unsigned i) {
    if (i > index->n || index->sz > 4)
        return 0;
    const uint8_t *ptr = index->offsets + i * index->sz;
    return  index->sz == 1? *ptr:
            index->sz == 2? cffRead16(&ptr):
            index->sz == 3? cffRead24(&ptr):
            index->sz == 4? cffRead32(&ptr):
            0;
}

static const uint8_t *cffPointer(const CFFIndex *index, unsigned i, unsigned *size) {
    *size = 0;
    unsigned offset = cffOffset(index, i);
    unsigned after = cffOffset(index, i + 1);
    if (!offset || !after)
        return NULL;
    if (size)
        *size = after - offset;
    return index->data + offset;
}

static float cffParseNumber(const uint8_t ** restrict cff) {
    if (**cff >= 32 && **cff <= 246)
        return *(*cff)++ - 139;
    else if (**cff >= 247 && **cff <= 250) {
        uint8_t b0 = *(*cff)++;
        uint8_t b1 = *(*cff)++;
        return (b0 - 247) * 256 + b1 + 108;
    } else if (**cff >= 251 && **cff <= 254) {
        uint8_t b0 = *(*cff)++;
        uint8_t b1 = *(*cff)++;
        return -(b0 - 251) * 256 - b1 - 108;
    } else if (**cff == 28) {
        (*cff)++;
        return (int16_t)cffRead16(cff); // sign-extend 16-bit value
    } else if (**cff == 29) {
        (*cff)++;
        return cffRead32(cff);
    } else if (**cff == 30) {
        char buffer[64];
        char *p = buffer;
        (*cff)++;
        for (uint8_t b; (b = *(*cff)++) != 0xff; )
            for (int n = 0, nibble = b >> 4; n < 2; n++, nibble = b & 0xf)
                if (nibble < 10)
                    *p++ = nibble + '0';
                else if (nibble == 10)
                    *p++ = '.';
                else if (nibble == 11)
                    *p++ = 'E';
                else if (nibble == 12)
                    *p++ = 'E',
                    *p++ = '-';
                else if (nibble == 14)
                    *p++ = '-';
                else
                    goto done;
        done:
        *p = 0;
        return strtof(buffer, NULL);
    } else
        return 0;
}

// static const char *cffParseSID(const CFFIndex *stringIndex, const uint8_t ** restrict cff, unsigned *size) {
//     int sid = cffParseNumber(cff) - 391;
//     return (const char*) cffPointer(stringIndex, sid, size);
// }


static CFFIndex cffParseIndex(const uint8_t ** restrict cff) {
    CFFIndex index;
    int n = cffRead16(cff);
    if (n == 0) {
        // Short-cut zero-length index
        index = (CFFIndex){ 0, 0, 0, 0 };
        return index;
    } else {
        int sz = *(*cff)++;
        const uint8_t *offsets = *cff;
        const uint8_t *data = offsets + (n + 1) * sz;
        index = (CFFIndex){ n, sz, offsets, data - 1 };

        // Jump after the index
        *cff = index.data + cffOffset(&index, n);
        return index;
    }
}

static CFFDict cffParseDict(const uint8_t ** restrict cff, unsigned size) {
    CFFDict dict = {0, 0, *cff};
    const uint8_t *end = *cff + size;
    const uint8_t *value = *cff;
    while (*cff < end) {
        unsigned b = *(*cff)++;
        if (b <= 21) {
            if (b == 12)
                b = (b << 8) + *(*cff)++;
            cffAddEntry(&dict, b, value);
            value = *cff;
        } else if (b >= 28 && b <= 30 || b >= 32 && b <= 254) {
            (*cff)--;
            cffParseNumber(cff);
        }
    }
    return dict;
}

// static const uint8_t *cffGetSubr(const CFFIndex *index, int i, unsigned *size) {
//     unsigned bias =
//         index->n < 1240? 107:
//         index->n < 33900? 1131:
//         32768;
//     return cffPointer(index, i + bias, size);
// }

static bool loadCFF(PgOpenTypeFont *otf) {
    const uint8_t   *cff = otf->cff;
    unsigned        size;

    int major = *cff++;
    int minor = *cff++;
    int headerSize = *cff++;
    int absoluteOffsetSize = *cff++;
    cff += headerSize - 4;

    if (major != 1)
        return false;

    CFFIndex nameIndex = cffParseIndex(&cff);
    CFFIndex topDictIndex = cffParseIndex(&cff);
    CFFIndex stringIndex = cffParseIndex(&cff);
    otf->gsubrIndex = cffParseIndex(&cff);

    (void)minor;
    (void)absoluteOffsetSize;
    (void)nameIndex;
    (void)stringIndex;

    // OpenType specifies that there should be only one font
    if (topDictIndex.n != 1)
        return 0;

    // Extract the settings we need out of Top DICT
    CFFDict privateDict = {0};
    unsigned charstringType = 2;

    cff = cffPointer(&topDictIndex, 0, &size);
    CFFDict topDict = cffParseDict(&cff, size);
    for (unsigned i = 0; i < topDict.n; i++) {
        unsigned size, offset;
        const uint8_t *value = topDict.entries[i].value;

        switch (topDict.entries[i].key) {

        // CharString
        case 17:
            offset = cffParseNumber(&value);
            value = otf->cff + offset;
            otf->charstringIndex = cffParseIndex(&value);
            break;

        // CharstringType
        case 0x0c06:
            charstringType = cffParseNumber(&value);
            break;

        // PrivateDICT
        case 18:
            size = cffParseNumber(&value);
            offset = cffParseNumber(&value);
            value = otf->cff + offset;
            privateDict = cffParseDict(&value, size);
            break;
        }
    }

    // Only handle Type2 charstrings
    if (!otf->charstringIndex.n || charstringType != 2)
        return false;

    // Get local subroutines
    if (privateDict.n == 0)
        return false;
    for (unsigned i = 0; i < privateDict.n; i++) {
        const uint8_t *value = topDict.entries[i].value;
        switch (privateDict.entries[i].key) {

        // Subrs
        case 19:
            value = privateDict.base + (int)cffParseNumber(&value);
            otf->subrIndex = cffParseIndex(&value);
            break;
        }
    }
    return true;
}

static void qqq(int operator, float *stack, int nstack) {
    for (int i = 0; i < nstack; i++)
        printf("%g ", stack[i]);
    switch (operator) {
    case  1: puts("hstem"); break;
    case  3: puts("vstem"); break;
    case  4: puts("vmoveto"); break;
    case  5: puts("rlineto"); break;
    case  6: puts("hlineto"); break;
    case  7: puts("vlineto"); break;
    case  8: puts("rrcurveto"); break;
    case 10: puts("callsubr"); break;
    case 11: puts("return"); break;
    case 14: puts("endchar"); break;
    case 18: puts("hstemhm"); break;
    case 19: puts("hintmask"); break;
    case 20: puts("cntrmask"); break;
    case 21: puts("rmoveto"); break;
    case 22: puts("hmoveto"); break;
    case 23: puts("vstemhm"); break;
    case 24: puts("rcuveline"); break;
    case 25: puts("rlinecurve"); break;
    case 26: puts("vvcurveto"); break;
    case 27: puts("hhcurveto"); break;
    case 29: puts("callgsubr"); break;
    case 30: puts("vhcurveto"); break;
    case 31: puts("hvcurveto"); break;
    case 0xc03: puts("and"); break;
    case 0xc04: puts("or"); break;
    case 0xc05: puts("not"); break;
    case 0xc09: puts("abs"); break;
    case 0xc0a: puts("add"); break;
    case 0xc0b: puts("sub"); break;
    case 0xc0c: puts("div"); break;
    case 0xc0e: puts("neg"); break;
    case 0xc0f: puts("eq"); break;
    case 0xc12: puts("drop"); break;
    case 0xc14: puts("put"); break;
    case 0xc15: puts("get"); break;
    case 0xc16: puts("ifelse"); break;
    case 0xc17: puts("random"); break;
    case 0xc18: puts("mul"); break;
    case 0xc1a: puts("sqrt"); break;
    case 0xc1b: puts("dup"); break;
    case 0xc1c: puts("exch"); break;
    case 0xc1d: puts("index"); break;
    case 0xc1e: puts("roll"); break;
    case 0xc22: puts("hflex"); break;
    case 0xc23: puts("flex"); break;
    case 0xc24: puts("hflex1"); break;
    case 0xc25: puts("flex1"); break;
    default:
        printf("resv %d\n", operator);
    }
}

static bool cffInterpretCharstring(PgPath *path, const uint8_t *cur, const uint8_t *end) {
    float   stack[48];
    int     nstack = 0;

    PgPt a = {0.0f, 0.0f};
    while (cur < end) {

        // Operand
        if (*cur == 28 || *cur >= 32) {
            if (nstack >= 48) {
                puts("QQQ: Stack Overflow");
                fflush(stdout);
                return false;
            }
            stack[nstack++] = *cur == 255
                ? (cur++, cffRead32(&cur)) / 65536.0f
                : cffParseNumber(&cur);
            continue;
        }

        unsigned operator = *cur == 12
            ? (cur++, *cur++ + 0xc00)
            : *cur++;
            qqq(operator, stack, nstack);

        switch (operator) {
        // case  1: puts("hstem"); break;
        // case  3: puts("vstem"); break;
        case 4: // vmoveto
            a = pgAddPt(a, pgPt(0.0f, stack[nstack - 1]));
            pgMove(path, a);
            break;
        case  5: // rlineto
            for (int i = 0; i < nstack; i += 2)
                pgLine(path, a = pgAddPt(a, pgPt(stack[i], stack[i + 1])));
            break;

        case  6: // hlineto
            if (nstack == 1)
                pgLine(path, a = pgAddPt(a, pgPt(stack[0], 0.0f)));
            else {
                for (int i = 0; i + 1 < nstack; i += 2) {
                    pgLine(path, a = pgAddPt(a, pgPt(stack[i], 0.0f)));
                    pgLine(path, a = pgAddPt(a, pgPt(0.0f, stack[i + 1])));
                }
                if (nstack & 1)
                    pgLine(path, a = pgAddPt(a, pgPt(stack[nstack - 1], 0.0f)));
            }
            break;

        case  7: // vlineto
            if (nstack == 1)
                pgLine(path, a = pgAddPt(a, pgPt(0.0f, stack[0])));
            else {
                for (int i = 0; i + 1 < nstack; i += 2) {
                    pgLine(path, a = pgAddPt(a, pgPt(0.0f, stack[i])));
                    pgLine(path, a = pgAddPt(a, pgPt(stack[i + 1], 0.0f)));
                }
                if (nstack & 1)
                    pgLine(path, a = pgAddPt(a, pgPt(0.0f, stack[nstack - 1])));
            }
            break;

        case  8: // rrcurveto
            for (int i = 0; i + 5 < nstack; i += 6) {
                PgPt b = pgAddPt(a, pgPt(stack[i], stack[i + 1]));
                PgPt c = pgAddPt(a, pgPt(stack[i + 2], stack[i + 3]));
                PgPt d = pgAddPt(a, pgPt(stack[i + 4], stack[i + 5]));
                pgCubic(path, b, c, a = d);
            }
            break;

        // case 10: puts("callsubr"); break;
        // case 11: puts("return"); break;

        case 14: // endchar
            pgClosePath(path);
            return true;
        // case 18: puts("hstemhm"); break;
        // case 19: puts("hintmask"); break;
        // case 20: puts("cntrmask"); break;
        case 21: // rmoveto
            pgMove(path, a = pgAddPt(a, pgPt(stack[nstack - 2], stack[nstack - 1])));
            break;
        case 22: // hmoveto
            pgMove(path, a = pgAddPt(a, pgPt(stack[nstack - 1], 0.0f)));
            break;
        // case 23: puts("vstemhm"); break;
        // case 24: puts("rcuveline"); break;
        // case 25: puts("rlinecurve"); break;
        // case 26: puts("vvcurveto"); break;
        // case 27: puts("hhcurveto"); break;
        // case 29: puts("callgsubr"); break;
        // case 30: puts("vhcurveto"); break;
        // case 31: puts("hvcurveto"); break;
        // case 0xc03: puts("and"); break;
        // case 0xc04: puts("or"); break;
        // case 0xc05: puts("not"); break;
        // case 0xc09: puts("abs"); break;
        // case 0xc0a: puts("add"); break;
        // case 0xc0b: puts("sub"); break;
        // case 0xc0c: puts("div"); break;
        // case 0xc0e: puts("neg"); break;
        // case 0xc0f: puts("eq"); break;
        // case 0xc12: puts("drop"); break;
        // case 0xc14: puts("put"); break;
        // case 0xc15: puts("get"); break;
        // case 0xc16: puts("ifelse"); break;
        // case 0xc17: puts("random"); break;
        // case 0xc18: puts("mul"); break;
        // case 0xc1a: puts("sqrt"); break;
        // case 0xc1b: puts("dup"); break;
        // case 0xc1c: puts("exch"); break;
        // case 0xc1d: puts("index"); break;
        // case 0xc1e: puts("roll"); break;
        // case 0xc22: puts("hflex"); break;
        // case 0xc23: puts("flex"); break;
        // case 0xc24: puts("hflex1"); break;
        // case 0xc25: puts("flex1"); break;
        default:
            // qqq(operator, stack, nstack);
            break;
        }

        nstack = 0;
    }
    return true;
}

static bool cffGetGlyphPath(PgOpenTypeFont *otf, PgPath *path, int glyph) {
    unsigned size;
    const uint8_t *cur = cffPointer(&otf->charstringIndex, glyph, &size);
    return cffInterpretCharstring(path, cur, cur + size);
}
