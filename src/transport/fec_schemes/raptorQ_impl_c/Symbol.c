#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "Symbol.h"
#include "Generators.h"

// 纯C实现的Symbol工具

Symbol *Symbol_new(unsigned int size)
{
    Symbol *s = (Symbol *)malloc(sizeof(Symbol));
    if (!s)
        return NULL;
    s->data = NULL;
    s->nbytes = 0;
    s->sbn = -1;
    s->esi = -1;
    Symbol_init(s, (int)size);
    return s;
}

void Symbol_free(Symbol *s)
{
    if (!s)
        return;
    if (s->data)
        free(s->data);
    free(s);
}

void Symbol_init(Symbol *s, int size)
{
    if (s->nbytes != size)
    {
        if (s->data)
            free(s->data);
        if (size % (int)sizeof(int) != 0)
        {
            printf("size must be a multiple of %d bytes\n", (int)sizeof(int));
        }
        s->nbytes = size;
        s->data = (int *)malloc((size_t)size);
    }
    if (s->data)
        memset(s->data, 0, (size_t)size);
}

void Symbol_fillData(Symbol *s, char *src, int size)
{
    if (s->nbytes != size)
    {
        if (s->data)
            free(s->data);
        s->nbytes = size;
        s->data = (int *)malloc((size_t)size);
    }
    memcpy(s->data, src, (size_t)size);
}

void Symbol_print(Symbol *s)
{
    for (int i = 0; i < s->nbytes / (int)sizeof(int); i++)
    {
        printf("%d ", s->data[i]);
    }
    printf("\n");
}

void Symbol_copy(Symbol *dst, const Symbol *src)
{
    if (dst->nbytes != src->nbytes)
    {
        if (dst->data)
            free(dst->data);
        dst->nbytes = src->nbytes;
        dst->data = (int *)malloc((size_t)dst->nbytes);
    }
    memcpy(dst->data, src->data, (size_t)src->nbytes);
}

void Symbol_xxor(Symbol *dst, Symbol *src)
{
    if (dst->nbytes != src->nbytes)
        printf("Error! try to xor symbols with unmatched size\n");
    for (int i = 0; i < dst->nbytes / 4; i++)
        dst->data[i] ^= src->data[i];
}

void Symbol_mul(Symbol *s, unsigned char u)
{
    unsigned char *p = (unsigned char *)s->data;
    for (int i = 0; i < s->nbytes; i++)
        p[i] = octmul(p[i], u);
}

void Symbol_div(Symbol *s, unsigned char u)
{
    unsigned char *p = (unsigned char *)s->data;
    for (int i = 0; i < s->nbytes; i++)
        p[i] = octdiv(p[i], u);
}

void Symbol_muladd(Symbol *dst, Symbol *src, unsigned char u)
{
    if (dst->nbytes != src->nbytes)
        printf("Error! try to muladd symbols with unmatched size\n");
    unsigned char *p = (unsigned char *)dst->data;
    unsigned char *p1 = (unsigned char *)src->data;
    for (int i = 0; i < dst->nbytes; i++)
    {
        if (u == 1)
            p[i] ^= p1[i];
        else
            p[i] ^= octmul(p1[i], u);
    }
}
