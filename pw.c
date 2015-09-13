#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "pg.h"
#include "pw.h"
#include "platform.h"


void _pwInit() {
    PwConf.font = pgOpenFont(L"Arial", 600, false);
    PwConf.titleBg = 0xffeeeeee;
    PwConf.titleFg = 0xff666666;
    PwConf.border = 0xff333333;
    PwConf.bg = 0xffeeeeee;
    PwConf.fg = 0xff666666;
    PwConf.accent = 0xffffaacc;
    pgScaleFont(PwConf.font, 16.0f, 0);
}

void pwExec(Pw *pw, int msg, PwEventArgs e) {
    pw->exec(pw, msg, &e);
}
void pwSize(Pw *pw, int width, int height) {
    pwExec(pw, PWE_SIZE, (PwEventArgs) {
        .size = {
            .width = width,
            .height = height,
        }});
}
void pwClose(Pw *pw) {
    pwExec(pw, PWE_CLOSE, (PwEventArgs) { 0 });
}