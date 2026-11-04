#ifndef MD2_CODEC_H
#define MD2_CODEC_H

#include <stdio.h>
#include "md2_model.h"

int md2_decode_fp(FILE *fp, md2_model_t *out_model);
int md2_decode_file(const char *path, md2_model_t *out_model);

int md2_encode_fp(FILE *fp, const md2_model_t *model);
int md2_encode_file(const char *path, const md2_model_t *model);

#endif /* MD2_CODEC_H */
