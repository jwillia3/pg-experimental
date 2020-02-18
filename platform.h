void *_pgMapFile(void **hostp, const wchar_t *filename);
void _pgFreeFileMap(void *host);
PgFontFamily *_pgScanFonts(void);

int16_t native16(int16_t x);
uint16_t nativeu16(uint16_t x);
int32_t native32(int32_t x);
uint32_t nativeu32(uint32_t x);

int wcsicmp(const wchar_t *s1, const wchar_t *s2);
