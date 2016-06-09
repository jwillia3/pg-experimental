#define main pwMain
typedef struct Pw Pw;
typedef struct PwWindow *PwWindow;

struct {
    PgFont *font;
    uint32_t titleBg;
    uint32_t titleFg;
    uint32_t border;
    uint32_t bg;
    uint32_t fg;
    uint32_t accent;
} PwConf;

enum {
    PWE_DRAW,
    PWE_SIZE,
    PWE_CLOSE,
    PWE_KEY_DOWN,
    PWE_KEY_UP,
    PWE_MOUSE_DOWN,
    PWE_MOUSE_UP,
};
typedef struct {
    union {
        struct { Pg *g; } draw;
        struct { int width, height; } size;
        struct {
            bool ctl;
            bool alt;
            bool shift;
            unsigned key;
        } key;
        struct {
            bool ctl;
            bool alt;
            bool shift;
            int button;
            PgPt at;
        } mouse;
    };
} PwEventArgs;

typedef void PwEvent(Pw *pw, int msg, PwEventArgs *e);

struct Pw {
    Pw      *parent;
    void    *sys;
    PgRect  rect;
    PwEvent *event;
    PwEvent *sysEvent;
    PwEvent *exec;
};

Pw *pwNew(Pw *pw);
Pw *pwNewWindow(int width, int height, void (*event)(Pw *pw, int msg, PwEventArgs *e));
bool pwWaitMessage();
void pwClose(Pw *pw);
void pwSize(Pw *pw, int width, int height);
void pwRefresh(Pw *pw);