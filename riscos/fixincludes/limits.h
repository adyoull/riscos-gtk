/* riscos-gtk: fix for UnixLib 5.0.3.1's <limits.h>, which defines
 *   #define LLONG_MIN 0x8000000000000000LL
 * A hex literal that doesn't fit long long is unsigned long long, so
 * LLONG_MIN is positive and unsigned: C++ rejects it as a narrowing
 * conversion (harfbuzz) and C comparisons against it go wrong. Reported to
 * riscos-unixlib; remove this when a fixed UnixLib is in riscos-crossdev.
 * Added to the include path first (-isystem) by build/env.sh. MIT. */
#include_next <limits.h>
#ifdef LLONG_MIN
#undef LLONG_MIN
#define LLONG_MIN (-LLONG_MAX - 1LL)
#endif
