#include <stdlib.h>
#include "Helper.h"
#include "Symbol.h"
#include "Decoder.h"

// 纯C版 Decoder

Decoder *Decoder_new(void)
{
    Decoder *d = (Decoder *)malloc(sizeof(Decoder));
    if (!d)
        return NULL;
    d->gen = NULL;
    return d;
}

void Decoder_free(Decoder *d)
{
    if (!d)
        return;
    if (d->gen)
        Generators_free(d->gen);
    free(d);
}

bool Decoder_init(Decoder *d, int K, int T)
{
    d->gen = Generators_new();
    if (!d->gen)
        return false;
    return Generators_gen(d->gen, K, K, T);
}

Symbol **Decoder_decode(Decoder *d, char **source, int _N, int *esi)
{
    Symbol **s;
    Generators_prepare(d->gen, source, _N, esi);
    s = Generators_generate_intermediates(d->gen);
    if (!s)
        return NULL;
    return s;
}

Symbol *Decoder_recover(Decoder *d, int x)
{
    return Generators_recover_symbol(d->gen, x);
}