/* xp_config.h - compile-time limits (arena/static storage)
   MIT License
*/
#ifndef XP_CONFIG_H
#define XP_CONFIG_H

/* Reader */
#define XP_RING_SIZE        8192u

/* Tokenization */
#define XP_MAX_TOKEN_TEXT     64u
#define XP_MAX_STRING        260u  /* file paths */

/* Parser safety */
#define XP_MAX_DEPTH          64u  /* nesting of { } */
#define XP_MAX_FACE_DEGREE    32u  /* max indices in a face list entry */

/* Animation safety */
#define XP_MAX_KEY_VALUES     16u

/* MSZIP / DEFLATE fixed limits.  MSZIP resets every 32 KiB block while
   retaining up to 32 KiB of prior history as the preset dictionary. */
#define XP_MSZIP_BLOCK_MAX     32768u
#define XP_MSZIP_COMP_SLACK       64u
#define XP_MSZIP_HISTORY       32768u

/* Feature toggles (phases) */
#ifndef XP_ENABLE_TEXT
#define XP_ENABLE_TEXT        1
#endif
#ifndef XP_ENABLE_BINARY
#define XP_ENABLE_BINARY      1
#endif
#ifndef XP_ENABLE_MSZIP
#define XP_ENABLE_MSZIP       1
#endif

#endif
