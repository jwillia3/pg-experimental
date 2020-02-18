#include <stdbool.h>
#include <stdint.h>
#ifdef _MSC_VER
#pragma comment(lib, "pg")
#endif

typedef struct { float x, y; } PgPt;
typedef union {
    struct { float x1, y1, x2, y2; };
    struct { PgPt a, b; };
} PgRect;
typedef struct { float a, b, c, d, e, f; } PgMatrix;
typedef enum { PG_MOVE, PG_LINE, PG_QUAD, PG_CUBIC } PgPathStepType;
typedef struct {
    int npoints, pointCap;
    int nsubs, subCap;
    int ntypes, typeCap;
    int *subs;
    PgPathStepType *types;
    PgPt *data;
} PgPath;
typedef struct {
    PgPathStepType  type;
    PgPt            *points;
} PgPathStep;
typedef struct {
    PgPath      *path;
    int         point;
    int         type;
    int         sub;
    PgPathStep  step;
} PgPathStepData;
typedef struct Pg Pg;
struct Pg {
    int width;
    int height;
    PgMatrix ctm;
    PgRect clip;
    float subsamples;
    float flatness;
    void (*resize)(Pg *pg, int width, int height);
    void (*clear)(Pg *pg, uint32_t color);
    void (*clearSection)(Pg *pg, PgPt a, PgPt b, uint32_t color);
    void (*free)(Pg *pg);
    void (*fillPath)(Pg *g, uint32_t color, PgPath *path);
    void (*strokePath)(Pg *g, uint32_t color, PgPath *path, float width);
    void (*setGamma)(Pg *g, float gamma);
    Pg *(*subsection)(Pg *pg, PgRect rect);
    uint32_t *bmp;
    int stride;
    bool borrowed;
    float gamma;
    uint16_t toLinear[256];
    uint8_t toGamma[32768 + 1];
};
typedef struct PgFont PgFont;
struct PgFont {
    const void *file;
    void *host;
    PgMatrix ctm;
    float em, xHeight, capHeight, ascender, descender, lineGap;
    uint32_t panose[10];
    int weight;
    bool isItalic;
    bool isFixedPitched;
    const wchar_t *familyName, *name, *styleName;
    int nfonts;
    struct { uint16_t in, out; } *subs;
    int nsubs;

    void (*free)(PgFont *font);
    void (*freeHost)(void *host);
    PgPath *(*getGlyphPath)(PgFont *font, PgPath *path, int glyph);
    int (*getGlyph)(PgFont *font, int c);
    float (*getGlyphWidth)(PgFont *font, int glyph);
    uint32_t *(*getFeatures)(PgFont *font);
    void (*setFeatures)(PgFont *font, const uint32_t *tags);
};
typedef struct {
    PgFont _;
    const int16_t *hmtx;
    const void *glyf;
    const void *loca;
    const void *gsub;
    uint16_t *cmap;
    bool longLoca;
    int nhmtx;
    int nglyphs;
    uint32_t lang;
    uint32_t script;
} PgOpenTypeFont;
typedef struct {
    const wchar_t *name;
    bool hasFixedPitched;

    // There is a slot for each of the font weights 100 - 900
    // Slots 0 and 10 are never used; they simplify the weight
    // matching algorithm. See note in pgOpenFont().
    struct pgFontDesc {
        wchar_t *path;
        int index;
        bool isFixedPitched;
    } roman[11], italic[11];
} PgFontFamily;

typedef struct PgStringBuffer {
    unsigned    length;
    char        *text;
} PgStringBuffer;

PgFontFamily    *PgFontFamilies;
int             PgNFontFamilies;


inline static PgPt pgPt(float x, float y) { return (PgPt){x,y}; }
inline static PgPt pgAddPt(PgPt a, PgPt b) { return (PgPt){a.x + b.x, a.y + b.y}; }
inline static PgPt pgSubtractPt(PgPt a, PgPt b) { return (PgPt){a.x - b.x, a.y - b.y}; }
inline static PgRect pgRect(PgPt a, PgPt b) { return (PgRect){ .a = a, .b = b }; }
uint32_t pgBlendWithGamma(uint32_t fg, uint32_t bg, uint8_t a255, uint16_t *toLinear, uint8_t *toGamma);
uint32_t pgBlend(uint32_t fg, uint32_t bg, uint8_t a);

unsigned pgStepUtf8(const char **input);
char *pgOutputUtf8(char **output, unsigned c);
char *pgToUtf8(const wchar_t *string);
wchar_t *pgFromUtf8(const char *string);
PgStringBuffer *pgNewStringBuffer(void);
PgStringBuffer *pgBufferCharacter(PgStringBuffer *buffer, unsigned c);
PgStringBuffer *pgBufferString(PgStringBuffer *buffer, const char *text, int length);
void pgFreeStringBuffer(PgStringBuffer *buffer);


// CANVAS MANAGEMENT
    Pg *pgNewBitmapCanvas(int width, int height);
    Pg *pgSubsectionCanvas(Pg *g, PgRect rect);
    void pgClearCanvas(Pg *g, uint32_t color);
    void pgClearSection(Pg *g, PgPt a, PgPt b, uint32_t color);
    void pgFreeCanvas(Pg *g);
    void pgResizeCanvas(Pg *g, int width, int height);

    // COLOUR MANAGEMENT
    void pgSetGamma(Pg *g, float gamma);

    // CANVAS MATRIX MANAGEMENT
    void pgIdentity(Pg *g);
    void pgTranslate(Pg *g, float x, float y);
    void pgScale(Pg *g, float x, float y);
    void pgShear(Pg *g, float x, float y);
    void pgRotate(Pg *g, float rad);
    void pgMultiply(Pg *g, const PgMatrix * __restrict b);

// MATRIX MANAGEMENT
    void pgIdentityMatrix(PgMatrix *mat);
    void pgTranslateMatrix(PgMatrix *mat, float x, float y);
    void pgScaleMatrix(PgMatrix *mat, float x, float y);
    void pgShearMatrix(PgMatrix *mat, float x, float y);
    void pgRotateMatrix(PgMatrix *mat, float rad);
    void pgMultiplyMatrix(PgMatrix * __restrict a, const PgMatrix * __restrict b);
    PgPt pgTransformPoint(PgMatrix ctm, PgPt p);
    PgPt *pgTransformPoints(PgMatrix ctm, PgPt *v, int n);
// Path management
    PgPath *pgNewPath(void);
    void pgFreePath(PgPath *path);
    void pgClearPath(PgPath *path);
    void pgMove(PgPath *path, PgPt a);
    void pgLine(PgPath *path, PgPt b);
    void pgQuad(PgPath *path, PgPt b, PgPt c);
    void pgCubic(PgPath *path, PgPt b, PgPt c, PgPt d);
    void pgClosePath(PgPath *path);
    void pgFillPath(Pg *g, uint32_t color, PgPath *path);
    void pgStrokePath(Pg *g, uint32_t color, PgPath *path, float width);
    PgRect pgGetPathBindingBox(PgPath *path, PgMatrix ctm);
    PgPathStepData pgNewPathStepData(PgPath *path);
    PgPathStep *pgNextPathStep(PgPathStepData *data);
    PgPath *pgTransformPath(PgPath *path, PgMatrix ctm);
    PgStringBuffer *pgPathAsSvgPath(PgStringBuffer *buffer, PgPath *path);
// Fonts
    PgFontFamily *pgScanFonts(void);
    PgFont *pgLoadFontHeader(const void *file, int fontIndex);
    PgFont *pgLoadFontFromMemory(const void *file, int fontIndex);
    PgFont *pgLoadFontFromFile(const wchar_t *filename, int index);
    PgFont *pgOpenFont(const wchar_t *family, int weight, bool italic);
    PgOpenTypeFont *pgLoadOpenTypeFontHeader(const void *file, int fontIndex);
    PgOpenTypeFont *pgLoadOpenTypeFont(const void *file, int fontIndex);

    float pgGetFontEm(PgFont *font);
    float pgGetFontHeight(PgFont *font);
    float pgGetFontBaseline(PgFont *font);
    float pgGetFontXHeight(PgFont *font);
    float pgGetFontCapHeight(PgFont *font);
    float pgGetFontAscender(PgFont *font);
    float pgGetFontDescender(PgFont *font);
    float pgGetFontLineGap(PgFont *font);
    int pgGetFontWeight(PgFont *font);
    bool pgIsFontItalic(PgFont *font);
    bool pgIsFontFixedPitched(PgFont *font);
    const wchar_t *pgGetFontName(PgFont *font);
    const wchar_t *pgGetFontFamilyName(PgFont *font);
    const wchar_t *pgGetFontStyleName(PgFont *font);


    uint32_t *pgGetFontFeatures(PgFont *font);
    void pgSetFontFeatures(PgFont *font, const uint32_t *tags);
    int pgGetGlyph(PgFont *font, int c);
    int pgGetGlyphNoSubstitute(PgFont *font, int c);
    void pgSubstituteGlyph(PgFont *font, uint16_t in, uint16_t out);
    void pgFreeFont(PgFont *font);
    void pgScaleFont(PgFont *font, float x, float y);
    PgPath *pgGetGlyphPath(PgFont *font, PgPath *path, int glyph);
    PgPath *pgGetCharPath(PgFont *font, PgPath *path, int c);
    float pgFillGlyph(Pg *g, PgFont *font, uint32_t color, float x, float y, int glyph);
    float pgFillChar(Pg *g, PgFont *font, uint32_t color, float x, float y, int c);
    float pgFillString(Pg *g, PgFont *font, uint32_t color, float x, float y, const wchar_t *text, int len);
    float pgFillUtf8(Pg *g, PgFont *font, uint32_t color, float x, float y, const char *text, int len);
    float pgPrintf(Pg *g, PgFont *font, uint32_t color, float x, float y, const char *format, ...);
    float pgGetCharWidth(PgFont *font, int c);
    float pgGetGlyphWidth(PgFont *font, int c);
    float pgGetStringWidth(PgFont *font, const wchar_t *text, int len);
    void pgFillRect(Pg *g, uint32_t color, PgPt a, PgPt b);
    void pgStrokeRect(Pg *g, uint32_t color, PgPt a, PgPt b, float width);
    void pgStrokeLine(Pg *g, uint32_t color, PgPt a, PgPt b, float width);
    void pgStrokeHLine(Pg *g, uint32_t color, PgPt a, float x2, float width);
    void pgStrokeVLine(Pg *g, uint32_t color, PgPt a, float y2, float width);
    PgPath *pgInterpretSvgPath(PgPath *path, const char *svg);
