#include <math.h>
#undef min
#undef max
static inline float min(float a, float b) { return a < b? a: b; }
static inline float max(float a, float b) { return a > b? a: b; }
static inline float clamp(float a, float b, float c) { return min(max(a, b), c); }
static inline float fraction(float a) { return a - floorf(a); }
static inline float distance(PgPt p) { return sqrtf(p.x * p.x + p.y * p.y); }
static inline PgPt midpoint(PgPt a, PgPt b) { return pgPt((a.x + b.x) / 2.0f, (a.y + b.y) / 2.0f); }
