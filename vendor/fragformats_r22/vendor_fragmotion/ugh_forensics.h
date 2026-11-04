#ifndef UGH_FORENSICS_H
#define UGH_FORENSICS_H
#include <stdio.h>
/* Diagnostics only. This is NOT a mesh/skeleton/clip decoder. */
int ugh_inspect(FILE *fp, FILE *report);
int ugh_compare(FILE *left, FILE *right, FILE *report);
#endif
