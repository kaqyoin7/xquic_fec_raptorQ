#pragma once

#include <stdbool.h>
#include "Generators.h"
#include "Symbol.h"

typedef struct Encoder
{
	Generators *gen;
} Encoder;

Encoder *Encoder_new(void);
void Encoder_free(Encoder *e);

bool Encoder_init(Encoder *e, int K, int T);
Symbol **Encoder_encode(Encoder *e, char **source, int overhead);
