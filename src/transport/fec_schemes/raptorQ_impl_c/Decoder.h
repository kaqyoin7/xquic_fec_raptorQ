#pragma once

#include <stdbool.h>
#include "Generators.h"

typedef struct Decoder
{
	Generators *gen;
} Decoder;

Decoder *Decoder_new(void);
void Decoder_free(Decoder *d);

bool Decoder_init(Decoder *d, int K, int T);
Symbol **Decoder_decode(Decoder *d, char **source, int _N, int *esi);
Symbol *Decoder_recover(Decoder *d, int x);