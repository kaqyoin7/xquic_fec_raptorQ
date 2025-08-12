#pragma once

#include <stddef.h>

typedef struct Symbol
{
	int *data;
	int nbytes;
	int sbn; /* source block number */
	int esi; /* encoding symbol id */
} Symbol;

/* construction / destruction */
Symbol *Symbol_new(unsigned int size);
void Symbol_free(Symbol *s);

/* in-place init/reset */
void Symbol_init(Symbol *s, int size);
void Symbol_fillData(Symbol *s, char *src, int size);
void Symbol_print(Symbol *s);

/* operations */
void Symbol_copy(Symbol *dst, const Symbol *src);
void Symbol_xxor(Symbol *dst, Symbol *src);
void Symbol_mul(Symbol *s, unsigned char u);
void Symbol_div(Symbol *s, unsigned char u);
void Symbol_muladd(Symbol *dst, Symbol *src, unsigned char u);
