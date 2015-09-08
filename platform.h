void *_pgMapFile(void **hostp, const wchar_t *filename);
void _pgFreeFileMap(void *host);
const wchar_t *_pgGetHomeDir();
const wchar_t *_pgGetConfigDir();
PgFontFamily *_pgScanFonts();

typedef struct Pw Pw;
Pw *_pwNew(int width, int height, const wchar_t *title, void (*onSetup)(Pw *win, void *etc), void *etc);
void _pwLoop();

int16_t native16(int16_t x);
uint16_t nativeu16(uint16_t x);
int32_t native32(int32_t x);
uint32_t nativeu32(uint32_t x);