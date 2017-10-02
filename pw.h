#ifdef _WIN32
    #define main() pwMain()
    extern int WinMain();
    static void *__ = WinMain;
#endif

typedef struct Pw Pw;
struct Pw {
    int     fixed;                          // fixed in major direction
    bool    horizontal;                     // children are laid out vertically
    bool    wrap;                           // enable wrapping
    int     min_size;                       // minimum size of items
    int     win_x, win_y;                   // position in window
    int     x, y;                           // position of this panel in parent
    int     width, height;                  // width and height of this area
    int     nsubs;                          // number of sub-panels
    Pw      **sub;                          // sub-panels
    Pw      *parent;                        // parent panel
    bool    (*clicked_down)(Pw *panel, int x, int y);
    bool    (*clicked)(Pw *panel, int x, int y);
    bool    (*mouse_moved)(Pw *panel, int x, int y);
    bool    (*wheel_rolled)(Pw *panel, int delta);
    bool    (*key_pressed)(Pw *panel, int key);
    bool    (*char_pressed)(Pw *panel, int c);
    bool    (*update)(Pw *panel);
    bool    (*draw)(Pw *panel, Pg *gs);
    bool    (*focus)(Pw *panel);
    void    (*free)(Pw *panel);
    
    uint32_t    bg, fg, border;
};

typedef struct PwLabel {
    union {Pw; Pw panel;};
    PgFont      *font;
    float       font_size;
    int         align;      // 1 = left; 2 = centre; 3 = right
    const wchar_t *text;
} PwLabel;

float       PwDpi;
Pw          *PwActive;     // active panel
Pw          *PwApp;
PgFont      *PwDefaultFont, *PwDefaultBoldFont;

Pw *pwNewAppPanel(int width, int height);
bool pwYield();

#define PW_NEW_PANEL(...) pwNewPanel((Pw){__VA_ARGS__})
#define PW_NEW_LABEL_PANEL(...) pwNewLabelPanel((PwLabel){__VA_ARGS__})
Pw *pwNewPanel(Pw proto);
PwLabel *pwNewLabelPanel(PwLabel proto);

void pwLayout(Pw *panel, int width, int height);
Pw *pwTargetPanel(Pw *panel, int x, int y);
PgRect pwPanelArea(Pw *panel);
Pg *pwPanelCanvas(Pg *parent_gs, Pw *panel);
void pwDrawPanel(Pw *panel, Pg *gs);
void pwUpdatePanel(Pw *panel);
Pw *pwSetActivePanel(Pw *panel);
Pw *pwRemovePanel(Pw *child);
void pwInsertPanel(Pw *parent, Pw *child, int index);
void pwShiftPanel(Pw *parent, Pw *child);
void pwPushPanel(Pw *parent, Pw *child);
void pwReplacePanel(Pw *old, Pw *new);
void pwFreePanel(Pw *panel);
