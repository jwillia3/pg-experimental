typedef struct { float x, y; } PgPt;
typedef struct Pg Pg;
struct Pg {
    int width;
    int height;
    uint32_t *bmp;
    void (*resize)(Pg *pg, int width, int height);
    void (*clear)(Pg *pg, uint32_t color);
    void (*free)(Pg *pg);
};

// CANVAS MANAGEMENT
    Pg *pgNewBitmapCanvas(int width, int height);
    void pgClearCanvas(Pg *g, uint32_t color);
    void pgFreeCanvas(Pg *g);
    void pgResizeCanvas(Pg *g, int width, int height);