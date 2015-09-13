void *_pgMapFile(void **hostp, const wchar_t *filename);
void _pgFreeFileMap(void *host);
const wchar_t *_pgGetHomeDir();
const wchar_t *_pgGetConfigDir();
PgFontFamily *_pgScanFonts();

int16_t native16(int16_t x);
uint16_t nativeu16(uint16_t x);
int32_t native32(int32_t x);
uint32_t nativeu32(uint32_t x);

void _pwInit();