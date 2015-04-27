void *_pgMapFile(void **hostp, const wchar_t *filename);
void _pgFreeFileMap(void *host);

typedef struct Pw Pw;
Pw *_pwNew(int width, int height, const wchar_t *text, void (*onRepaint)(Pw *win));
void _pwLoop();

int16_t native16(int16_t x);
uint16_t nativeu16(uint16_t x);
int32_t native32(int32_t x);
uint32_t nativeu32(uint32_t x);