#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "pg.h"
#include "platform.h"
#include "util.h"

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

static int getGlyph(PgFont *font, int c) {
    PgOpenTypeFont *otf = (PgOpenTypeFont*)font;
    return otf->cmap[c & 0xffff];
}
static PgPath *_getGlyphPath(PgFont *font, PgPath *path, int glyph) {
    PgOpenTypeFont *otf = (PgOpenTypeFont*)font;
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
    if (!data) return NULL;
    
    if (!path) path = pgNewPath();
    
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
            } else *f++;
            xsize += dx;
        }
        const uint8_t *xs = f;
        const uint8_t *ys = xs + xsize;
        PgPt a = {0, 0}, b;
        bool inCurve = false;
        int vx = 0, vy = 0;
        PgPt start;
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
                if (inCurve)
                    pgQuad(path, b, start);
                else
                    pgClosePath(path);
                if (i != npoints - 1)
                    pgMove(path, p);
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
        if (inCurve)
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
            if (flags & 3 == 3) { // x & y words
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
            _getGlyphPath(font, path, index);
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
    return path;
}
static PgPath *getGlyphPath(PgFont *font, PgPath *path, int glyph) {
    path = _getGlyphPath(font, path, glyph);
    if (!path) return NULL;
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

redoHeader:
    if (native32(sfnt->ver) == 0x10000);
    else if (native32(sfnt->ver) == 'ttcf') { // TrueType Collection
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
    
    struct { uint32_t tag, checksum, offset, size; } *thead = (void*)(sfnt + 1);
    for (int i = 0; i < nativeu16(sfnt->ntables); i++, thead++) {
        void *address = (uint8_t*)file + nativeu32(thead->offset);
        switch(nativeu32(thead->tag)) {
        case 'head': head = address; break;
        case 'hhea': hhea = address; break;
        case 'maxp': maxp = address; break;
        case 'cmap': cmap = address; break;
        case 'hmtx': hmtx = address; break;
        case 'glyf': glyf = address; break;
        case 'loca': loca = address; break;
        case 'OS/2': os2  = address; break;
        case 'name': name = address; break;
        case 'GSUB': gsub = address; break;
        }
    }
    
    PgOpenTypeFont *otf = calloc(1, sizeof *otf);
    PgFont *font = &otf->_;
    font->file = file;
    otf->hmtx = hmtx;
    otf->glyf = glyf;
    otf->loca = loca;
    otf->cmap = (void*)cmap;
    otf->gsub = gsub;
    otf->lang = 'eng ';
    otf->script = 'latn';
    font->ctm = (PgMatrix){ 1, 0, 0, 1, 0, 0};
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
                    font->familyName = output;
                else if (id == 2)
                    font->styleName = output;
                else if (id == 4)
                    font->name = output;
                else if (id == 16) { // Preferred font family
                    free((void*) font->familyName);
                    font->familyName = output;
                }
                else if (id == 17) { // Preferred font style
                    free((void*) font->styleName);
                    font->styleName = output;
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
                    font->familyName = output;
                else if (id == 2)
                    font->styleName = output;
                else if (id == 4)
                    font->name = output;
                else if (id == 16) { // Preferred font family
                    free((void*) font->familyName);
                    font->familyName = output;
                }
                else if (id == 17) { // Preferred font style
                    free((void*) font->styleName);
                    font->styleName = output;
                }
            }
    }
    if (!font->familyName)
        font->familyName = wcsdup(L"");
    if (!font->styleName)
        font->styleName = wcsdup(L"");
    if (!font->name)
        font->name = wcsdup(L"");
    
    
    return otf;
}
static void freeFont(PgFont *font) {
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

#include <stdio.h>
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
    
    bool outputTags = tags != NULL;
    if (!tags)
        tags = (uint32_t[1]) { 0 };
    
    // Script List -> Script Table
    const ScriptTable *scriptTable = NULL;
    for (int i = 0; i < nativeu16(scriptList->nscripts); i++)
        if (nativeu32(scriptList->tag[i].tag) == 'DFLT' ||
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
                if (nativeu32(featureList->tag[index].tag) == tags[j])
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