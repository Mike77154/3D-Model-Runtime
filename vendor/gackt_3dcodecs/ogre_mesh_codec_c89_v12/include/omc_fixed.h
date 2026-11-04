#ifndef OMC_FIXED_H
#define OMC_FIXED_H

#ifdef __cplusplus
extern "C" {
#endif

/* Q16.16 signed fixed point. No float or double types are used anywhere. */
typedef long OMC_Fixed;

#define OMC_FIXED_SHIFT 16
#define OMC_FIXED_ONE ((OMC_Fixed)65536L)
#define OMC_FIXED_HALF ((OMC_Fixed)32768L)

OMC_Fixed omc_fixed_from_int(long v);
OMC_Fixed omc_fixed_from_decimal(const char *text);
OMC_Fixed omc_fixed_from_ieee754(unsigned long bits32);
void omc_fixed_to_string(OMC_Fixed v, char *out, unsigned long out_cap, int decimals);
OMC_Fixed omc_fixed_abs(OMC_Fixed v);
OMC_Fixed omc_fixed_min(OMC_Fixed a, OMC_Fixed b);
OMC_Fixed omc_fixed_max(OMC_Fixed a, OMC_Fixed b);

#ifdef __cplusplus
}
#endif

#endif
