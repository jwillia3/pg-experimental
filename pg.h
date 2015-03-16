typedef struct { float x, y; } PgPt;
typedef struct { float a, b, c, d, e, f; } PgMatrix;
typedef struct Pg Pg;
struct Pg {
    int width;
    int height;
    PgMatrix ctm;
    uint32_t *bmp;
    void (*resize)(Pg *pg, int width, int height);
    void (*clear)(Pg *pg, uint32_t color);
    void (*free)(Pg *pg);
    void (*triangle)(Pg *g, PgPt a, PgPt b, PgPt c, uint32_t color);
    void (*triangleStrip)(Pg *g, PgPt *v, int n, uint32_t color);
};

static PgPt pgPt(float x, float y) { return (PgPt){x,y}; }

// CANVAS MANAGEMENT
    Pg *pgNewBitmapCanvas(int width, int height);
    void pgClearCanvas(Pg *g, uint32_t color);
    void pgFreeCanvas(Pg *g);
    void pgResizeCanvas(Pg *g, int width, int height);
// MATRIX MANAGEMENT
    static PgMatrix PgIdentity = { 1, 0, 0, 1, 0, 0 };
    void pgIdentityMatrix(PgMatrix *mat);
    void pgTranslateMatrix(PgMatrix *mat, float x, float y);
    void pgScaleMatrix(PgMatrix *mat, float x, float y);
    void pgShearMatrix(PgMatrix *mat, float x, float y);
    void pgRotateMatrix(PgMatrix *mat, float rad);
    void pgMultiplyMatrix(PgMatrix * __restrict a, const PgMatrix * __restrict b);
    PgPt pgTransformPoint(const PgMatrix *ctm, PgPt p);
    PgPt *pgTransformPoints(const PgMatrix *ctm, PgPt *v, int n);