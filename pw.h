#define main pwMain
typedef struct Pw Pw;
struct Pw {
    Pg *g;
    wchar_t *title;
    void *etc;
    
    void (*onClose)(Pw *win);
    void (*onMove)(Pw *win, int x, int y);
    void (*onResize)(Pw *win, int width, int height);
    void (*onRepaint)(Pw *win);
    bool (*onKeyDown)(Pw *win, uint32_t state, int key);
    bool (*onKeyUp)(Pw *win, uint32_t state, int key);
    bool (*onChar)(Pw *win, uint32_t state, int c);
    bool (*onClick)(Pw *win, uint32_t state);
            
    void (*close)(Pw *win);
    void (*resize)(Pw *win, int width, int height);
    void (*update)(Pw *win);
    void (*setTitle)(Pw *win, const wchar_t *title);
};
Pw *pwNew(int width, int height, const wchar_t *title, void (*onSetup)(Pw *win, void *etc), void *etc);
void pwClose(Pw *win);
void pwResize(Pw *win, int width, int height);
void pwUpdate(Pw *win);
void pwSetTitle(Pw *win, const wchar_t *title);
void pwLoop();