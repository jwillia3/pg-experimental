#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include <pg.h>

float dpi = 72.0f;

void draw(SDL_Window *window) {
    SDL_Surface *screen = SDL_GetWindowSurface(window);
    SDL_LockSurface(screen);
    Pg *g = pgNewBitmapCanvas(screen->w, screen->h);
    free(g->bmp);
    g->borrowed = true;
    g->bmp = screen->pixels;

    pgClearCanvas(g, 0xeeeeee);

    // float lineHeight = 10.0f * dpi / 72.0f;
    // float x = 0.0f;
    // float y = 0.0f;
    // float maxWidth = 0.0f;

    // pgScanFonts();
    // for (int i = 0; i < PgNFontFamilies; i++) {
    //     PgFont *font = pgOpenFont(PgFontFamilies[i].name, 0, false);
    //     pgScaleFont(font, 0, lineHeight * 0.75f);

    //     float width = pgPrintf(g, font, 0x555555, x, y, "%ls", PgFontFamilies[i].name) - x;

    //     pgFreeFont(font);

    //     maxWidth = width > maxWidth? width: maxWidth;
    //     y += lineHeight;
    //     if (y + lineHeight >= g->height) {
    //         y = 0.0f;
    //         x += maxWidth + 5.0f * dpi / 72.0f;
    //         maxWidth = 0.0f;
    //     }
    // }

// pgTranslate(g, 1000, 500);

    PgFont *font = 0;
    // font = pgOpenFont(L"URW Bookman", 0, false);
    // font = pgOpenFont(L"courier 10 pitch", 0, false);
    // font = pgOpenFont(L"courier", 0, false);
    // font = pgOpenFont(L"arial", 0, false);
    // font = pgLoadFontFromFile(L"/usr/share/fonts/gsfonts/URWBookman-Light.otf", 0);
    font = pgLoadFontFromFile(L"/home/jlw/silly.otf", 0);
    if (!font) {
        puts("FONT NOT LOADED");
        exit(1);
    }
    // pgScaleFont(font, 0, g->height);
    pgScaleFont(font, 0, 100);
    pgPrintf(g, font, 0x555555, 0, 0, "b");
    pgFreeFont(font);

    pgFreeCanvas(g);
    SDL_UnlockSurface(screen);
    SDL_UpdateWindowSurface(window);
}

int main(void) {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window *window =
        SDL_CreateWindow(
            "Demo",
            SDL_WINDOWPOS_UNDEFINED,
            SDL_WINDOWPOS_UNDEFINED,
            2000,
            2000,
            SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

    SDL_GetDisplayDPI(0, NULL, &dpi, NULL);

    draw(window);

    for (SDL_Event e; SDL_WaitEvent(&e); )
        switch (e.type) {
        case SDL_QUIT:
            exit(0);
            break;

        case SDL_WINDOWEVENT:
            switch (e.window.event) {
            case SDL_WINDOWEVENT_EXPOSED:
            case SDL_WINDOWEVENT_RESIZED:
                // draw(window);
                break;
            }
            break;
        }
}
