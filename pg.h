typedef struct { float x, y; } PgPt;
typedef union {
    struct { float x1, y1, x2, y2; };
    struct { PgPt a, b; };
} PgRect;
typedef struct { float a, b, c, d, e, f; } PgMatrix;
typedef struct {
    int npoints, pointCap;
    int nsubs, subCap;
    int ntypes, typeCap;
    int *subs;
    enum { PG_MOVE, PG_LINE, PG_QUAD, PG_CUBIC } *types;
    PgPt *data;
} PgPath;
typedef struct Pg Pg;
struct Pg {
    int width;
    int height;
    PgMatrix ctm;
    PgRect clip;
    uint32_t *bmp;
    void (*resize)(Pg *pg, int width, int height);
    void (*clear)(Pg *pg, uint32_t color);
    void (*free)(Pg *pg);
    void (*fillPath)(Pg *g, PgPath *path, uint32_t color);
    void (*strokePath)(Pg *g, PgPath *path, float width, uint32_t color);
};
typedef struct PgFont PgFont;
struct PgFont {
    const void *file;
    void *host;
    PgMatrix ctm;
    float em, xHeight, capHeight, ascender, descender, lineGap;
    int weight;
    bool isItalic;
    const wchar_t *family, name;
    
    void (*free)(PgFont *font);
    PgPath *(*getGlyphPath)(PgFont *font, PgPath *path, int glyph);
    int (*getGlyph)(PgFont *font, int glyph);
    float (*getGlyphWidth)(PgFont *font, int glyph);
};
typedef struct {
    PgFont _;
    const int16_t *hmtx;
    const void *glyf;
    const void *loca;
    uint16_t *cmap;
    bool longLoca;
    int nhmtx;
    int nglyphs;
    int nfonts;
} PgOpenTypeFont;

static PgPt pgPt(float x, float y) { return (PgPt){x,y}; }
static PgRect pgRect(PgPt a, PgPt b) { return (PgRect){ .a = a, .b = b }; }

// CANVAS MANAGEMENT
    Pg *pgNewBitmapCanvas(int width, int height);
    void pgClearCanvas(Pg *g, uint32_t color);
    void pgFreeCanvas(Pg *g);
    void pgResizeCanvas(Pg *g, int width, int height);
    // CANVAS MATRIX MANAGEMENT
    void pgIdentity(Pg *g);
    void pgTranslate(Pg *g, float x, float y);
    void pgScale(Pg *g, float x, float y);
    void pgShear(Pg *g, float x, float y);
    void pgRotate(Pg *g, float rad);
    void pgMultiply(Pg *g, const PgMatrix * __restrict b);
    
// MATRIX MANAGEMENT
    static PgMatrix PgIdentity = { 1, 0, 0, 1, 0, 0 };
    void pgIdentityMatrix(PgMatrix *mat);
    void pgTranslateMatrix(PgMatrix *mat, float x, float y);
    void pgScaleMatrix(PgMatrix *mat, float x, float y);
    void pgShearMatrix(PgMatrix *mat, float x, float y);
    void pgRotateMatrix(PgMatrix *mat, float rad);
    void pgMultiplyMatrix(PgMatrix * __restrict a, const PgMatrix * __restrict b);
    PgPt pgTransformPoint(PgMatrix ctm, PgPt p);
    PgPt *pgTransformPoints(PgMatrix ctm, PgPt *v, int n);
// Path management
    PgPath *pgNewPath();
    void pgFreePath(PgPath *path);
    void pgClearPath(PgPath *path);
    void pgMove(PgPath *path, PgPt a);
    void pgLine(PgPath *path, PgPt b);
    void pgQuad(PgPath *path, PgPt b, PgPt c);
    void pgCubic(PgPath *path, PgPt b, PgPt c, PgPt d);
    void pgClosePath(PgPath *path);
    void pgFillPath(Pg *g, PgPath *path, uint32_t color);
    void pgStrokePath(Pg *g, PgPath *path, float width, uint32_t color);
// Fonts
    PgOpenTypeFont *pgLoadOpenTypeFontHeader(const void *file, int fontIndex);
    PgOpenTypeFont *pgLoadOpenTypeFont(const void *file, int fontIndex);
    
    PgFont *pgLoadFontFromFile(wchar_t *filename, int index);
    int pgGetGlyph(PgFont *font, int c);
    void pgFreeFont(PgFont *font);
    void pgScaleFont(PgFont *font, float x, float y);
    PgPath *pgGetGlyphPath(PgFont *font, PgPath *path, int glyph);
    PgPath *pgGetCharPath(PgFont *font, PgPath *path, int c);
    float pgFillChar(Pg *g, PgFont *font, float x, float y, int c, uint32_t color);
    float pgFillString(Pg *g, PgFont *font, float x, float y, const wchar_t *text, int len, uint32_t color);
    float pgGetCharWidth(PgFont *font, int c);
    float pgGetGlyphWidth(PgFont *font, int c);
    float pgGetStringWidth(PgFont *font, wchar_t *text, int len);