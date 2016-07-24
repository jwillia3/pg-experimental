#include <math.h>
#undef min
#undef max
static float min(float a, float b) { return a < b? a: b; }
static float max(float a, float b) { return a > b? a: b; }
static float clamp(float a, float b, float c) { return min(max(a, b), c); }
static float fraction(float a) { return a - floorf(a); }
static float distance(PgPt p) { return sqrtf(p.x * p.x + p.y * p.y); }
static PgPt midpoint(PgPt a, PgPt b) { return pgPt((a.x + b.x) / 2.0f, (a.y + b.y) / 2.0f); }