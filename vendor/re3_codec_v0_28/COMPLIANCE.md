# C89 / no-heap compliance

The recovered v0.13 source is compiled with:

`-std=c89 -pedantic -Wall -Wextra -Werror -O2`

Current reconstruction verification on 2026-09-19:

- GCC full build: PASS
- Clang full build: PASS
- synthetic fixed-point / PARTS_SET / PSX color checks: PASS
- synthetic PSX TIM parse/sample: PASS
- forbidden-token scan (`malloc/calloc/realloc/free`, float/double, 64-bit types): PASS

The three remaining `long` uses are restricted to the C89 stdio boundary where
`fseek` accepts `long` and `ftell` returns `long`; EMD/TIM data structures and
runtime arithmetic use explicit 8/16/32-bit typedefs and Q12 fixed point.
