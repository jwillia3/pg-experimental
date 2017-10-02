#include <iso646.h>
#include <stdlib.h>
#include <string.h>
#include <pg.h>
#include <pw.h>

void pwInit() {
    PwDefaultFont = pgOpenFont(L"Arial", 400, false);
	PwDefaultBoldFont = pgOpenFont(L"Arial", 600, false);
}

void pwLayout(Pw *panel, int width, int height) {
    if (not panel) return;
    panel->width = width;
    panel->height = height;
    int fixed = 0;
    int even_panels = 0;
    for (int i = 0; i < panel->nsubs; i++) {
        fixed += panel->sub[i]->fixed;
        even_panels += panel->sub[i]->fixed ? 0 : 1;
    }
    
    if (panel->horizontal) {
        int even_size = max(panel->min_size, even_panels ? max(width - fixed, 0) / even_panels : 0);
        int odd_scrap = even_size * even_panels != max(width - fixed, 0) ? 1 : 0;
        int full_width = fixed + even_size * even_panels + odd_scrap;
        int x = 0, y = 0, row_height = height;
        if (full_width > width && panel->wrap) {
            int ncolumns = max(1, width ? ceilf((float)full_width / width) : 1);
            row_height = max(256, width / ncolumns);
        }
        
        for (int i = 0; i < panel->nsubs; i++) {
            Pw *p = panel->sub[i];
            p->height = row_height;
            p->width = p->fixed ? p->fixed : even_size + odd_scrap;
            if (p->fixed == 0) odd_scrap = 0;
            if (x + p->width > width) {
                y += row_height;
                x = 0;
            }
            p->x = x;
            p->y = y;
            p->win_x = x + panel->win_x;
            p->win_y = y + panel->win_y;
            x += p->width;
        }
    } else {
        int even_size = max(panel->min_size, even_panels ? max(height - fixed, 0) / even_panels : 0);
        int odd_scrap = even_size * even_panels != max(height - fixed, 0) ? 1 : 0;
        int full_height = fixed + even_size * even_panels + odd_scrap;
        int x = 0, y = 0, column_width = width;
        if (full_height > height && panel->wrap) {
            int ncolumns = max(2, height ? ceilf((float)full_height / height) : 1);
            column_width = max(256, width / ncolumns);
        }
        
        for (int i = 0; i < panel->nsubs; i++) {
            Pw *p = panel->sub[i];
            p->width = column_width;
            p->height = p->fixed ? p->fixed : even_size + odd_scrap;
            if (p->fixed == 0) odd_scrap = 0;
            if (y + p->height > height) {
                x += column_width;
                y = 0;
            }
            p->x = x;
            p->y = y;
            p->win_x = x + panel->win_x;
            p->win_y = y + panel->win_y;
            y += p->height;
        }
    }
    for (int i = 0; i < panel->nsubs; i++)
        pwLayout(panel->sub[i], panel->sub[i]->width, panel->sub[i]->height);
}
Pw *pwTargetPanel(Pw *panel, int x, int y) {
    if (not panel) return NULL;
    for (Pw *p = panel; ; ) {
        Pw **xs;
        next:
        if (p->nsubs == 0) return p;
        xs = p->sub;
        for (int i = 0; i < p->nsubs; i++)
            if (x >= xs[i]->win_x and x < xs[i]->win_x + xs[i]->width and
                y >= xs[i]->win_y and y < xs[i]->win_y + xs[i]->height)
                    { p = xs[i]; goto next; }
        return p;
    }
}
PgRect pwPanelArea(Pw *panel) {
    if (not panel) return (PgRect){0.0f,};
    return pgRect(pgPt(panel->x, panel->y), pgPt(panel->x + panel->width, panel->y + panel->height));
}
Pg *pwPanelCanvas(Pg *parent_gs, Pw *panel) {
    if (not panel) return NULL;
    return pgSubsectionCanvas(parent_gs, pwPanelArea(panel));
}
void pwDrawPanel(Pw *panel, Pg *gs) {
    if (not panel) return;
    if (panel->bg & 0xff000000)
        pgClearCanvas(gs, panel->bg);
    if (panel->draw == NULL || panel->draw(panel, gs))
        for (int i = 0; i < panel->nsubs; i++) {
            Pg *tmp = pwPanelCanvas(gs, panel->sub[i]);
            pwDrawPanel(panel->sub[i], tmp);
            pgFreeCanvas(tmp);
        }
    if (panel->border & 0xff000000)
        pgStrokeRect(gs, pgPt(0.5f, 0.5f), pgPt(gs->width - 1.0f, gs->height - 1.0f), 1.0f, panel->border);
}
void pwUpdatePanel(Pw *panel) {
    if (not panel) return;
    pwLayout(panel, panel->width, panel->height);
    for (Pw *p = panel; p; p = p->parent)
        if (p->update && p->update(panel)) break;
}
Pw *pwSetActivePanel(Pw *panel) {
    return PwActive = panel;
}
Pw *pwRemovePanel(Pw *child) {
    if (not child or not child->parent) return NULL;
    Pw *found = NULL;
    Pw *parent = child->parent;
    for (int i = 0, j = 0; i < parent->nsubs; i++)
        if (parent->sub[i] == child) {
            parent->sub[i]->parent = NULL;
            found = parent->sub[i];
        } else parent->sub[j++] = parent->sub[i];
    if (found) {
        parent->sub = realloc(parent->sub, --parent->nsubs * sizeof *parent->sub);
        pwUpdatePanel(parent);
        if (PwActive == found)
            pwSetActivePanel(parent);
    }
    return found;
}
void pwInsertPanel(Pw *parent, Pw *child, int index) {
    if (not parent or not child) return;
    if (child->parent) return;
    if (index < 0) index += parent->nsubs + 1;
    if (index >= 0 && index <= parent->nsubs) {
        parent->sub = realloc(parent->sub, ++parent->nsubs * sizeof *parent->sub);
        for (int i = parent->nsubs - 1; i > index; i--)
            parent->sub[i] = parent->sub[i - 1];
        parent->sub[index] = child;
        child->parent = parent;
        pwUpdatePanel(parent);
    }
}
void pwShiftPanel(Pw *parent, Pw *child) {
    pwInsertPanel(parent, child, 0);
}
void pwPushPanel(Pw *parent, Pw *child) {
    pwInsertPanel(parent, child, parent->nsubs);
}
void pwReplacePanel(Pw *old, Pw *new) {
    if (not old or not new) return;
    Pw *parent = old->parent;
    if (!parent) return;
    for (int i = 0, j = 0; i < parent->nsubs; i++)
        if (parent->sub[i] == old) {
            bool was_focused = PwActive == old;
            pwRemovePanel(old);
            pwInsertPanel(parent, new, i);
            if (was_focused)
                pwSetActivePanel(new);
        }
}
void pwFreePanel(Pw *panel) {
    if (not panel) return;
    for (int i = 0; i < panel->nsubs; i++)
        pwFreePanel(panel->sub[i]);
    if (panel->free) panel->free(panel);
    else free(panel);
}

Pw *pwNewPanel(Pw proto) {
    return memmove(malloc(sizeof proto), &proto, sizeof proto);
}

static bool draw_label_panel(Pw *_panel, Pg *gs) {
    PwLabel *panel = (void*)_panel;
    pgScaleFont(panel->font, 0.0f, panel->font_size);
    float m = (1.0f / 16.0f) * 72.0f * PwDpi;
    float w = pgGetStringWidth(panel->font, panel->text, -1);
    float x = panel->align == 1 ? m :
            panel->align == 2 ? (gs->width - w) * 0.5f :
            (gs->width - w - m);
	float y = (gs->height - panel->font_size) * 0.5f;
    pgFillString(gs, panel->font, x, y, panel->text, -1, panel->fg);
    return true;
}
static void free_label_panel(Pw *_panel) {
    PwLabel *panel = (void*)_panel;
    free((void*)panel->text);
    free(_panel);
}
PwLabel *pwNewLabelPanel(PwLabel proto) {
    if (not proto.font) proto.font = PwDefaultFont;
    if (not proto.font_size) proto.font_size = 14;
    if (not proto.draw) proto.draw = draw_label_panel;
    if (not proto.free) proto.free = free_label_panel;
    if (not proto.align) proto.align = 1;
    proto.text = wcsdup(proto.text);
    return memmove(malloc(sizeof proto), &proto, sizeof proto);
}
