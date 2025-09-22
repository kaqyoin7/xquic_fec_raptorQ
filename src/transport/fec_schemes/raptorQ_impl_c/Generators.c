#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "Helper.h"
#include "Generators.h"
#include "Tables.h"

// 纯C实现的Generators核心算法

Generators *Generators_new(void)
{
    Generators *g = (Generators *)malloc(sizeof(Generators));
    if (!g)
        return NULL;

    g->status = 0;
    g->Tuples = NULL;
    g->C1 = NULL;
    g->C = NULL;
    g->sources = NULL;
    g->R = NULL;
    g->A = NULL;
    g->Abak = NULL;
    g->degree = NULL;
    g->isi = NULL;

    return g;
}

void Generators_free(Generators *g)
{
    if (!g)
        return;

    int i;
    if (g->C1)
    {
        for (i = 0; i < g->M; i++)
            if (g->C1[i])
                Symbol_free(g->C1[i]);
        free(g->C1);
    }

    if (g->C)
    {
        for (i = 0; i < g->L; i++)
            if (g->C[i])
                Symbol_free(g->C[i]);
        free(g->C);
    }

    if (g->A)
    {
        for (i = 0; i < g->M; i++)
            free(g->A[i]);
        free(g->A);
    }

    if (g->Abak)
    {
        for (i = 0; i < g->M; i++)
            free(g->Abak[i]);
        free(g->Abak);
    }

    if (g->Tuples)
        free(g->Tuples);

    if (g->isi)
        free(g->isi);

    if (g->degree)
        free(g->degree);

    // 注意：R数组由调用方释放
    free(g);
}

bool Generators_gen(Generators *g, int _K, int _N, int _T)
{
    int i, j;

    if (!Generators__0_init(g, _K, _N, _T))
        return false;

    Generators__1_Tuples(g);
    Generators__2_Matrix_GLDPC(g);
    Generators__3_Matrix_GHDPC(g);
    Generators__4_Matrix_GLT(g);

    /* save for reuse, A will be changed in calculating intermediates */
    for (i = 0; i < g->M; i++)
    {
        for (j = 0; j < g->L; j++)
            g->Abak[i][j] = g->A[i][j];
    }
    g->status = 1;

    Generators_ToString(g);
    return true;
}

bool Generators__0_init(Generators *g, int _K, int _N, int _T)
{
    int i, j;

    if (_K < 1 || _K > 56403)
    {
        printf("Invalid K, should in [1,56403]\n");
        return false;
    }

    if (_N < _K)
    {
        printf("N should not be smaller than K\n");
        return false;
    }

    g->K = _K;
    g->T = _T;
    g->N = _N;

    g->I = 0;
    while (g->K > lookup_table[g->I][0])
        g->I++;

    g->K1 = lookup_table[g->I][0];
    g->J_K = lookup_table[g->I][1];
    g->S = lookup_table[g->I][2];
    g->H = lookup_table[g->I][3];
    g->W = lookup_table[g->I][4];

    g->N1 = g->K1 - g->K + g->N;
    g->L = g->K1 + g->S + g->H;
    g->M = g->N1 + g->S + g->H;
    g->P = g->L - g->W;
    g->U = g->P - g->H;
    g->B = g->W - g->S;
    g->H1 = (int)ceil(g->H / 2.0);

    // P1 be the smallest prime that is greater than or equal to P
    int _P1_len = g->P;
    int flag = false;
    while (!flag)
    {
        for (i = 2; i <= sqrt((double)_P1_len); i++)
        {
            if (_P1_len % i == 0)
            {
                _P1_len++;
                flag = false;
                break;
            }
            else
                flag = true;
        }
    }
    g->P1 = _P1_len;

    // Allocate memory
    g->C1 = (Symbol **)malloc(g->M * sizeof(Symbol *));
    if (!g->C1)
        goto alloc_failed;

    for (i = 0; i < g->M; i++)
    {
        g->C1[i] = Symbol_new(g->T);
        if (!g->C1[i])
            goto alloc_failed;
    }

    g->C = (Symbol **)malloc(g->L * sizeof(Symbol *));
    if (!g->C)
        goto alloc_failed;

    for (i = 0; i < g->L; i++)
    {
        g->C[i] = Symbol_new(g->T);
        if (!g->C[i])
            goto alloc_failed;
        g->C[i]->esi = i;
    }

    g->A = (Elem **)malloc(g->M * sizeof(Elem *));
    if (!g->A)
        goto alloc_failed;

    for (i = 0; i < g->M; i++)
    {
        g->A[i] = (Elem *)malloc(g->L * sizeof(Elem));
        if (!g->A[i])
            goto alloc_failed;
    }

    g->Abak = (Elem **)malloc(g->M * sizeof(Elem *));
    if (!g->Abak)
        goto alloc_failed;

    for (i = 0; i < g->M; i++)
    {
        g->Abak[i] = (Elem *)malloc(g->L * sizeof(Elem));
        if (!g->Abak[i])
            goto alloc_failed;
    }

    for (i = 0; i < g->M; i++)
    {
        for (j = 0; j < g->L; j++)
            g->A[i][j].val = 0;
    }

    g->isi = (int *)malloc(g->N1 * sizeof(int));
    if (!g->isi)
        goto alloc_failed;

    for (i = 0; i < g->N1; i++)
        g->isi[i] = i;

    g->degree = (Degree *)malloc(g->M * sizeof(Degree));
    if (!g->degree)
        goto alloc_failed;

    return true;

alloc_failed:
    printf("Allocation failed!\n");
    Generators_free(g);
    return false;
}

#define MAX_OVERHEAD (40)
void Generators__1_Tuples(Generators *g)
{
    int i;

    g->Tuples = (TuplS *)malloc((g->M + MAX_OVERHEAD) * sizeof(TuplS));
    if (!g->Tuples)
    {
        printf("Allocate Tuples[] failed!\n");
        abort();
    }

    for (i = 0; i < g->M + MAX_OVERHEAD; i++)
    {
        g->Tuples[i] = Generators_Tupl(g, i);
    }

    g->tupl_len = g->M + MAX_OVERHEAD;
}

void Generators__2_Matrix_GLDPC(Generators *g)
{
    int i;
    int a, b;

    /* G_LDPC,1 */
    for (i = 0; i < g->B; i++)
    {
        a = 1 + i / g->S;
        b = i % g->S;
        g->A[b][i].val = 1;

        b = (b + a) % g->S;
        g->A[b][i].val = 1;

        b = (b + a) % g->S;
        g->A[b][i].val = 1;
    }

    /* identity part */
    for (i = 0; i < g->S; i++)
        g->A[i][g->B + i].val = 1;

    /* G_LDPC,2 */
    for (i = 0; i < g->S; i++)
    {
        a = i % g->P;
        b = (i + 1) % g->P;
        g->A[i][g->W + a].val = 1;
        g->A[i][g->W + b].val = 1;
    }
}

void Generators__3_Matrix_GHDPC(Generators *g)
{
    int i, j, k;

    for (j = 0; j < g->K1 + g->S - 1; j++)
    {
        i = Generators_RandYim(j + 1, 6, g->H);
        g->A[g->S + i][j].val = 1;
        i = (Generators_RandYim(j + 1, 6, g->H) + Generators_RandYim(j + 1, 7, g->H - 1) + 1) % g->H;
        g->A[g->S + i][j].val = 1;
    }

    for (i = g->S; i < g->S + g->H; i++)
    {
        g->A[i][g->K1 + g->S - 1].val = OCT_EXP[i - g->S];
    }

    for (i = g->S; i < g->S + g->H; i++)
    {
        for (j = 0; j < g->K1 + g->S; j++)
        {
            unsigned char tmp = 0;
            for (k = j; k < g->K1 + g->S; k++)
                if (g->A[i][k].val)
                    tmp ^= octmul(g->A[i][k].val, OCT_EXP[k - j]);
            g->A[i][j].val = tmp;
        }
    }

    /* identity part */
    for (i = g->S; i < (g->S + g->H); i++)
        g->A[i][g->K1 + i].val = 1;
}

void Generators__4_Matrix_GLT(Generators *g)
{
    int i, j;
    int a, b, d;
    TuplS tupl;

    i = 0;
    while (i < g->N1)
    {
        if (g->isi[i] < g->tupl_len)
            tupl = g->Tuples[g->isi[i]];
        else
            tupl = Generators_Tupl(g, g->isi[i]);

        a = tupl.a;
        b = tupl.b;
        d = tupl.d;

        g->A[g->S + g->H + i][b].val = 1;

        for (j = 1; j < d; j++)
        {
            b = (b + a) % g->W;
            g->A[g->S + g->H + i][b].val = 1;
        }

        a = tupl.a1;
        b = tupl.b1;
        d = tupl.d1;

        while (b >= g->P)
            b = (b + a) % g->P1;
        g->A[g->S + g->H + i][g->W + b].val = 1;

        for (j = 1; j < d; j++)
        {
            b = (b + a) % g->P1;
            while (b >= g->P)
                b = (b + a) % g->P1;
            g->A[g->S + g->H + i][g->W + b].val = 1;
        }

        i++;
    }
}

bool Generators_prepare(Generators *g, char **source, int _N, int *esi)
{
    int i, j;

    if (_N < g->K)
    {
        printf("Invalid N in prepare! %d\n", g->N);
        return false;
    }

    if (g->status != 1)
    {
        /* Not the first run, recover A */
        for (i = 0; i < g->M; i++)
        {
            for (j = 0; j < g->L; j++)
                g->A[i][j] = g->Abak[i][j];
        }
    }

    g->status = 2;

    if (esi)
    {
        int _N1 = _N + g->K1 - g->K;

        /* matrix LT parts changed, the data struct is not efficient now */
        for (i = 0; i < g->M; i++)
            free(g->A[i]);
        free(g->A);

        g->A = (Elem **)malloc((_N1 + g->S + g->H) * sizeof(Elem *));
        for (i = 0; i < (_N1 + g->S + g->H); i++)
        {
            g->A[i] = (Elem *)malloc(g->L * sizeof(Elem));
            if (!g->A[i])
                goto alloc_failed;
        }

        for (i = 0; i < (_N1 + g->S + g->H); i++)
        {
            for (j = 0; j < g->L; j++)
                if (i < g->S + g->H)
                    g->A[i][j] = g->Abak[i][j];
                else
                    g->A[i][j].val = 0;
        }

        for (i = 0; i < g->M; i++)
            free(g->Abak[i]);
        free(g->Abak);

        g->Abak = (Elem **)malloc((_N1 + g->S + g->H) * sizeof(Elem *));
        for (i = 0; i < (_N1 + g->S + g->H); i++)
        {
            g->Abak[i] = (Elem *)malloc(g->L * sizeof(Elem));
            if (!g->Abak[i])
                goto alloc_failed;
        }

        for (i = 0; i < (_N1 + g->S + g->H); i++)
        {
            for (j = 0; j < g->L; j++)
                if (i < g->S + g->H)
                    g->Abak[i][j] = g->A[i][j];
                else
                    g->Abak[i][j].val = 0;
        }

        free(g->degree);
        g->degree = (Degree *)malloc((_N1 + g->S + g->H) * sizeof(Degree));
        if (!g->degree)
            goto alloc_failed;

        for (i = 0; i < g->M; i++)
            Symbol_free(g->C1[i]);
        free(g->C1);

        g->C1 = (Symbol **)malloc((_N1 + g->S + g->H) * sizeof(Symbol *));
        for (i = 0; i < _N1 + g->S + g->H; i++)
        {
            g->C1[i] = Symbol_new(g->T);
            if (!g->C1[i])
                goto alloc_failed;
        }

        g->M = _N1 + g->S + g->H;
        g->N = _N;
        g->N1 = _N1;

        free(g->isi);
        g->isi = (int *)malloc(g->N1 * sizeof(int));
        if (!g->isi)
            goto alloc_failed;

        for (i = 0; i < g->N; i++)
        {
            if (esi[i] < g->K)
                g->isi[i] = esi[i];
            else
                g->isi[i] = esi[i] + g->K1 - g->K;
        }
        /* K1 - K (N1 - N) padding symbols */
        for (i = g->N; i < g->N1; i++)
            g->isi[i] = i - g->N + g->K;

        Generators__4_Matrix_GLT(g);

        for (i = g->S + g->H; i < g->M; i++)
            for (j = 0; j < g->L; j++)
                g->Abak[i][j] = g->A[i][j];
    }

    for (i = 0; i < g->L; i++)
    {
        Symbol_init(g->C[i], g->T);
        g->C[i]->esi = i;
    }

    for (i = g->S + g->H; i < g->M; i++)
        if (i < g->S + g->H + g->N)
            Symbol_fillData(g->C1[i], source[i - g->S - g->H], g->T);
        else // padding
            Symbol_init(g->C1[i], g->T);

    g->sources = source;
    return true;

alloc_failed:
    printf("Allocation in prepare() failed!\n");
    return false;
}

void Generators_swap_row(Generators *g, int i1, int i2)
{
    if (i1 == i2)
        return;

    Elem *e;
    e = g->A[i1];
    g->A[i1] = g->A[i2];
    g->A[i2] = e;

    Degree d = g->degree[i1];
    g->degree[i1] = g->degree[i2];
    g->degree[i2] = d;

    Symbol *s;
    s = g->C1[i1];
    g->C1[i1] = g->C1[i2];
    g->C1[i2] = s;
}

void Generators_swap_col(Generators *g, int j1, int j2)
{
    if (j1 == j2)
        return;

    for (int i = 0; i < g->M; i++)
    {
        int t = g->A[i][j1].val;
        g->A[i][j1].val = g->A[i][j2].val;
        g->A[i][j2].val = t;
    }

    Symbol *s = g->C[j1];
    g->C[j1] = g->C[j2];
    g->C[j2] = s;
}

void Generators_xxor(Generators *g, int i1, int i2, int U)
{
    if (i1 == i2)
        return;

    int d = 0;
    for (int j = i2; j < g->L; j++)
    {
        g->A[i1][j].val ^= g->A[i2][j].val;
        if (j < U && g->A[i1][j].val == 1)
            d++;
    }
    g->degree[i1].curr = d;
    Symbol_xxor(g->C1[i1], g->C1[i2]);
}

int Generators_gaussian_elimination(Generators *g, int starti, int startj)
{
    int i, k, q, jj, kk;
    int firstone;

    int *HI = (int *)malloc(g->L * sizeof(int));
    int *LOW = (int *)malloc(g->M * sizeof(int));
    if (!HI || !LOW)
    {
        if (HI)
            free(HI);
        if (LOW)
            free(LOW);
        return 0;
    }

    for (jj = startj; jj < g->L; jj++)
    {
        k = 0;
        for (i = starti; i <= jj - 1; i++)
        {
            if (g->A[i][jj].val)
            {
                HI[k] = i;
                k++;
            }
        }

        kk = 0;
        firstone = -1;
        for (i = jj; i < g->M; i++)
        {
            if (g->A[i][jj].val)
            {
                LOW[kk] = i;
                if (g->A[i][jj].val == 1 && firstone == -1)
                    firstone = kk;
                kk++;
            }
        }

        if (kk == 0)
        {
            printf(" Encoder: due to unclear reasons the process can not continue\n");
            free(HI);
            free(LOW);
            return 0;
        }

        if (firstone > 0)
        {
            int t = LOW[0];
            LOW[0] = LOW[firstone];
            LOW[firstone] = t;
        }

        if (g->A[LOW[0]][jj].val != 1)
        {
            unsigned char v = g->A[LOW[0]][jj].val;
            Symbol_div(g->C1[LOW[0]], v);
            for (q = jj; q < g->L; q++)
                g->A[LOW[0]][q].val = octdiv(g->A[LOW[0]][q].val, v);
        }

        for (i = 1; i < kk; i++)
        {
            unsigned char v = g->A[LOW[i]][jj].val;
            Symbol_muladd(g->C1[LOW[i]], g->C1[LOW[0]], v);
            for (q = jj; q < g->L; q++)
                g->A[LOW[i]][q].val ^= octmul(g->A[LOW[0]][q].val, v);
        }

        for (i = 0; i < k; i++)
        {
            unsigned char v = g->A[HI[i]][jj].val;
            Symbol_muladd(g->C1[HI[i]], g->C1[LOW[0]], v);
            for (q = jj; q < g->L; q++)
                g->A[HI[i]][q].val ^= octmul(g->A[LOW[0]][q].val, v);
        }

        if (LOW[0] != jj)
        {
            Elem *temp;
            Symbol *tempo;

            temp = g->A[jj];
            g->A[jj] = g->A[LOW[0]];
            g->A[LOW[0]] = temp;

            tempo = g->C1[jj];
            g->C1[jj] = g->C1[LOW[0]];
            g->C1[LOW[0]] = tempo;
        }
    }

    free(HI);
    free(LOW);
    return 1;
}

Symbol **Generators_generate_intermediates(Generators *g)
{
    if (g->status != 2)
    {
        printf("Wrong call sequence! Filling the source block before generate intermediates\n");
        return NULL;
    }

    int *cols1 = (int *)malloc(g->L * sizeof(int));
    int *cols2 = (int *)malloc(g->L * sizeof(int));
    if (!cols1 || !cols2)
    {
        if (cols1)
            free(cols1);
        if (cols2)
            free(cols2);
        return NULL;
    }

    int k1, k2;
    int i, j, d, gtone;

    /* init degree list */
    for (i = 0; i < g->M; i++)
    {
        d = 0;
        gtone = 0;
        for (j = 0; j < g->L - g->P; j++)
            if (g->A[i][j].val)
            {
                d++;
                if (g->A[i][j].val > 1)
                    gtone++;
            }
        g->degree[i].curr = g->degree[i].ori = d;
        g->degree[i].gtone = gtone;
    }

    Generators_PrintMatrix(g);

    /* step 1 */
    int _I, _U, r;
    int gtone_start = 0;

    _I = 0;
    _U = g->P;

    while (_I + _U < g->L)
    {
        int index, o;

        // select minimal current degree with minimal original degree
        // gtone == 0 first
    retry:
        index = g->M;
        o = g->L;
        r = g->L;
        for (i = _I; i < g->M; i++)
        {
            if ((gtone_start || (gtone_start == 0 && g->degree[i].gtone == 0)) && g->degree[i].curr > 0 && g->degree[i].curr <= r)
            {
                index = i;
                if (g->degree[i].curr < r || (g->degree[i].curr == r && g->degree[i].ori < o))
                {
                    o = g->degree[i].ori;
                    r = g->degree[i].curr;
                }
            }
        }

        if (index == g->M)
        {
            if (gtone_start)
                goto retry;
            printf("Cannot find enough rows to decode\n");
            Generators_PrintMatrix(g);
            free(cols1);
            free(cols2);
            return NULL;
        }

        Generators_swap_row(g, _I, index);

        k1 = k2 = 0;
        for (j = _I; j < g->L - _U; j++)
        {
            if (j < g->L - _U - r + 1)
            {
                if (g->A[_I][j].val != 0)
                {
                    cols1[k1++] = j;
                }
            }
            else
            {
                if (g->A[_I][j].val == 0)
                    cols2[k2++] = j;
            }
        }

        if (k1 != k2 + 1)
        {
            printf("Assert fail: %d!= %d + 1, _I=%d\n", k1, k2, _I);
            free(cols1);
            free(cols2);
            return NULL;
        }

        /* put one nonezero to [_I][_I], r-1 1 to [L-_U-r+1,L-_U) */
        Generators_swap_col(g, _I, cols1[0]);
        for (j = 0; j < k2; j++)
        {
            Generators_swap_col(g, cols2[j], cols1[j + 1]);
        }

        if (g->A[_I][_I].val > 1)
        {
            unsigned char v = g->A[_I][_I].val;
            Symbol_div(g->C1[_I], v);
            for (j = _I; j < g->L; j++)
                g->A[_I][j].val = octdiv(g->A[_I][j].val, v);
        }

        for (i = _I + 1; i < g->M; i++)
        {
            unsigned char v = g->A[i][_I].val;
            if (v)
            {
                g->A[i][_I].val = 0;
                g->degree[i].curr--;
                if (v > 1)
                    g->degree[i].gtone--;
                for (j = g->L - _U - (r - 1); j < g->L; j++)
                {
                    int oldv = g->A[i][j].val;
                    g->A[i][j].val ^= octmul(v, g->A[_I][j].val);
                    if (j < g->L - _U)
                    {
                        if (g->A[i][j].val > 0)
                        {
                            g->degree[i].curr++;
                            if (g->A[i][j].val > 1)
                                g->degree[i].gtone++;
                        }
                        if (oldv > 0)
                        {
                            g->degree[i].curr--;
                            if (oldv > 1)
                                g->degree[i].gtone--;
                        }
                    }
                }
                Symbol_muladd(g->C1[i], g->C1[_I], v);
            }

            /* we calculate only the intersect part of A&V
               Note: lines with A[i][_I] == 0 also needs this
             */
            for (j = g->L - _U - (r - 1); j < g->L - _U; j++)
                if (g->A[i][j].val)
                {
                    g->degree[i].curr--;
                    if (g->A[i][j].val > 1)
                        g->degree[i].gtone--;
                }
        }

        _I++;
        _U += r - 1;
    }

    free(cols1);
    free(cols2);

    printf("_I=%d _U=%d\n", _I, _U);

    /* step 2 */
    // gaussian elimination on the (M - _I) x _U matrix
    if (!Generators_gaussian_elimination(g, _I, _I))
        return NULL;

    /* step 3 */
    for (int jj = _I; jj < g->L; jj++)
        for (int i = 0; i < _I; i++)
        {
            unsigned char v = g->A[i][jj].val;
            if (v)
            {
                g->A[i][jj].val = 0;
                Symbol_muladd(g->C1[i], g->C1[jj], v);
            }
        }

    /* result now in C1, copy to C; C1 is useless from now on */
    for (i = 0; i < g->L; i++)
    {
        // borrow C1->esi to remember the real pos of C[i]
        g->C1[i]->esi = g->C[i]->esi;
    }

    for (i = 0; i < g->L; i++)
    {
        // copy C1[i] to its real pos in C
        // to avoid data copy, just swap the pointers
        Symbol *s;
        s = g->C[g->C1[i]->esi];
        g->C[g->C1[i]->esi] = g->C1[i];
        g->C1[i] = s;
    }

    g->status = 3;
    return g->C;
}

Symbol **Generators_generate_repairs(Generators *g, int count)
{
    int i, isi;

    if (g->status != 3)
    {
        printf("Wrong call sequence! Generate intermediates before generate repairs\n");
        return NULL;
    }

    /* caller needs to free R */
    g->R = (Symbol **)malloc(count * sizeof(Symbol *));
    if (!g->R)
        return NULL;

    for (i = g->K; i < g->K + count; i++)
    {
        TuplS tupl;
        isi = i + g->K1 - g->K;
        if (isi < g->tupl_len)
            tupl = g->Tuples[isi];
        else
            tupl = Generators_Tupl(g, isi);
        g->R[i - g->K] = Generators_LTEnc(g, g->C, tupl);
    }

    g->status = 4;
    return g->R;
}

Symbol *Generators_recover_symbol(Generators *g, int x)
{
    if (x >= g->K)
    {
        printf("try to recover non-source symbols!\n");
        return NULL;
    }

    TuplS tupl;
    tupl = g->Tuples[x];
    return Generators_LTEnc(g, g->C, tupl);
}

int Generators_getL(Generators *g)
{
    return g->L;
}

int Generators_getK(Generators *g)
{
    return g->K;
}

TuplS Generators_Tupl(Generators *g, int X)
{
    TuplS tupl;

    unsigned int A = (53591 + g->J_K * 997);
    if (A % 2 == 0)
        A = A + 1;
    unsigned int B = 10267 * (g->J_K + 1);
    unsigned int y = (B + X * A);

    int v = Generators_RandYim(y, 0, 1048576);
    tupl.d = Generators_Deg(g, v);
    tupl.a = 1 + Generators_RandYim(y, 1, g->W - 1);
    tupl.b = Generators_RandYim(y, 2, g->W);

    if (tupl.d < 4)
        tupl.d1 = 2 + Generators_RandYim(X, 3, 2);
    else
        tupl.d1 = 2;
    tupl.a1 = 1 + Generators_RandYim(X, 4, g->P1 - 1);
    tupl.b1 = Generators_RandYim(X, 5, g->P1);

    return tupl;
}

unsigned int Generators_RandYim(unsigned int y, unsigned char i, unsigned int m)
{
    return (V0[((y & 0xff) + i) & 0xff] ^ V1[(((y >> 8) & 0xff) + i) & 0xff] ^ V2[(((y >> 16) & 0xff) + i) & 0xff] ^ V3[(((y >> 24) & 0xff) + i) & 0xff]) % m;
}

unsigned int Generators_Deg(Generators *g, unsigned int v)
{
    int j = 0;
    while (v > f[j])
        j++;
    return min(j, g->W - 2);
}

Symbol *Generators_LTEnc(Generators *g, Symbol **C_L, TuplS tupl)
{
    int a = tupl.a;
    int b = tupl.b;
    int d = tupl.d;
    Symbol *s;

    s = Symbol_new(g->T);
    if (!s)
        return NULL;

    Symbol_copy(s, C_L[b]);
    for (int j = 1; j < d; j++)
    {
        b = (b + a) % g->W;
        Symbol_xxor(s, C_L[b]);
    }

    a = tupl.a1;
    b = tupl.b1;
    d = tupl.d1;

    while (b >= g->P)
        b = (b + a) % g->P1;
    Symbol_xxor(s, C_L[g->W + b]);

    for (int j = 1; j < d; j++)
    {
        b = (b + a) % g->P1;
        while (b >= g->P)
            b = (b + a) % g->P1;
        Symbol_xxor(s, C_L[g->W + b]);
    }

    return s;
}

void Generators_verify(Generators *g)
{
    int i, j;
    Symbol *s, *s1;
    char *p;

    s = Symbol_new(g->T);
    s1 = Symbol_new(g->T);
    if (!s || !s1)
    {
        if (s)
            Symbol_free(s);
        if (s1)
            Symbol_free(s1);
        return;
    }

    for (i = 0; i < g->M; i++)
    {
        Symbol_init(s, g->T);
        for (j = 0; j < g->L; j++)
        {
            if (g->Abak[i][j].val)
                Symbol_muladd(s, g->C[j], g->Abak[i][j].val);
        }

        if (i < g->S + g->H || i >= g->S + g->H + g->N)
            p = (char *)s1->data;
        else
            p = (char *)g->sources[i - g->S - g->H];

        if (memcmp(s->data, p, g->T) != 0)
        {
            printf("Check fail for line %d,%x vs %x\n", i, *(int *)s->data, *(int *)p);
        }
    }

    Symbol_free(s);
    Symbol_free(s1);
}

void Generators_PrintMatrix(Generators *g)
{
#ifdef DEBUG
    int i;
    printf("Press a number to print the generation matrix:\n");
    scanf("%d", &i);
    for (i = 0; i < g->M; i++)
    {
        for (int j = 0; j < g->L; j++)
            printf("%2x ", g->A[i][j].val);
        printf("\n");
    }
#endif
}

void Generators_ToString(Generators *g)
{
//    printf("K=%d\n", g->K);
//    printf("T=%d\n", g->T);
//    printf("H=%d\n", g->H);
//    printf("S=%d\n", g->S);
//    printf("L=%d\n", g->L);
//    printf("N=%d\n", g->N);
//    printf("M=%d\n", g->M);
//    printf("K1=%d\n", g->K1);
//    printf("W=%d\n", g->W);
//    printf("P=%d\n", g->P);
//    printf("B=%d\n", g->B);
//    printf("U=%d\n", g->U);
//    printf("P1=%d\n", g->P1);
//
//    printf("Tuples:\n");
//    for (int i = 0; i < g->L; i++)
//    {
//        printf("Tuple %d d,a,b=%d,%d,%d,", i, g->Tuples[i].d, g->Tuples[i].a, g->Tuples[i].b);
//        printf(" d1,a1,b1=%d,%d,%d\n", g->Tuples[i].d1, g->Tuples[i].a1, g->Tuples[i].b1);
//    }

//    printf("The generation matrix:\n");
//    for (int i = 0; i < g->M; i++)
//    {
//        for (int j = 0; j < g->L; j++)
//            printf("%2x ", g->A[i][j].val);
//        printf("\n");
//    }
}

/* section 5.7.2 */
unsigned char octmul(unsigned char u, unsigned char v)
{
    if (u == 0 || v == 0)
        return 0;
    if (v == 1)
        return u;
    if (u == 1)
        return v;
    return OCT_EXP[OCT_LOG[u] + OCT_LOG[v]];
}

unsigned char octdiv(unsigned char u, unsigned char v)
{
    if (u == 0)
        return 0;
    if (v == 1)
        return u;

    return OCT_EXP[OCT_LOG[u] - OCT_LOG[v] + 255];
}