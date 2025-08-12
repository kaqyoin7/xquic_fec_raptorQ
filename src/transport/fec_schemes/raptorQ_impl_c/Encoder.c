#include <stdlib.h>
#include "Helper.h"
#include "Symbol.h"
#include "Encoder.h"

// 纯C版 Encoder

Encoder *Encoder_new(void)
{
    Encoder *e = (Encoder *)malloc(sizeof(Encoder));
    if (!e)
        return NULL;
    e->gen = NULL;
    return e;
}

void Encoder_free(Encoder *e)
{
    if (!e)
        return;
    if (e->gen)
        Generators_free(e->gen);
    free(e);
}

bool Encoder_init(Encoder *e, int K, int T)
{
    e->gen = Generators_new();
    if (!e->gen)
        return false;
    return Generators_gen(e->gen, K, K, T);
}

Symbol **Encoder_encode(Encoder *e, char **source, int overhead)
{
    Symbol **s;
    Generators_prepare(e->gen, source, Generators_getK(e->gen), NULL);
    s = Generators_generate_intermediates(e->gen);
    if (!s)
        return NULL;
    return Generators_generate_repairs(e->gen, overhead);
}