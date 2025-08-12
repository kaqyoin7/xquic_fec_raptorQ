#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include "Helper.h"

Helper *Helper_new(void)
{
    Helper *h = (Helper *)malloc(sizeof(Helper));
    if (!h)
        return NULL;

    h->Kmax = 0;
    h->P = 0;
    h->F = 0;
    h->W = 0;
    h->Al = 0;
    h->Kmin = 0;
    h->Gmax = 0;
    h->G = 0;
    h->T = 0;
    h->Kt = 0;
    h->Z = 0;
    h->N = 0;

    return h;
}

void Helper_free(Helper *h)
{
    if (h)
        free(h);
}

bool Helper_init(Helper *h, int Al, int Kmin, int Gmax, int P, int F, int W)
{
    h->Al = Al;
    h->P = P;
    if (P % Al != 0)
    {
        return false;
    }
    h->Kmin = Kmin;
    h->Kmax = 8192;
    h->Gmax = Gmax;
    h->F = F;
    h->W = W;

    h->G = min((min((int)ceil((double)P * Kmin / F), (int)P / Al)), Gmax);
    h->T = (int)floor((double)P / (Al * h->G)) * Al;
    h->Kt = (int)ceil((double)F / h->T);
    h->Z = (int)ceil((double)h->Kt / h->Kmax);
    h->N = min((int)ceil(ceil((double)h->Kt / h->Z) * h->T / W), (int)h->T / Al);

    if (ceil(ceil((double)F / h->T) / h->Z) > h->Kmax)
    {
        return false;
    }

    h->KtZ = Helper_partition(h->Kt, h->Z);
    h->TAlN = Helper_partition(h->T / Al, h->N);

    return true;
}

PartitionS Helper_partition(int I, int J)
{
    PartitionS result;
    result.IL = (int)ceil((double)I / J);
    result.IS = (int)floor((double)I / J);
    result.JL = I - result.IS * J;
    result.JS = J - result.JL;

    return result;
}

void Helper_toString(Helper *h)
{
    printf("Kmax=%d\nP=%d\nF=%d\nW=%d\nAl=%d\nKmin=%d\nGmax=%d\nG=%d\nT=%d\nKt=%d\nZ=%d\nN=%d\n",
           h->Kmax, h->P, h->F, h->W, h->Al, h->Kmin, h->Gmax, h->G, h->T, h->Kt, h->Z, h->N);

    printf("KL,KS,ZL,ZS\n%d,%d,%d,%d\n", h->KtZ.IL, h->KtZ.IS, h->KtZ.JL, h->KtZ.JS);
    printf("TL,TS,NL,NS\n%d,%d,%d,%d\n", h->TAlN.IL, h->TAlN.IS, h->TAlN.JL, h->TAlN.JS);
}

long Helper_Combin(int m, int n)
{
    if (n == 1 || n == 0)
        return m;
    if (n > m - n)
    {
        return Helper_Combin(m, m - n);
    }
    return (long)((double)Helper_Combin(m - 1, n - 1) * m / n);
}