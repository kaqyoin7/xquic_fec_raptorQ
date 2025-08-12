#pragma once

#include <stdbool.h>
#include "Symbol.h"

typedef struct TuplS
{
	int d;
	int a;
	int b;
	int d1;
	int a1;
	int b1;
} TuplS;

typedef struct Element
{
	unsigned char val;
#ifdef SPARSE
	int rprev;
	int rnext;
	int cprev;
	int cnext;
#endif
} Elem;

typedef struct Degree
{
	int ori;
	int curr;
	int gtone;
} Degree;

typedef struct Generators
{
	/* refer to RFC5053 for the naming */
	int K;	 // symbol number of a source block
	int I;	 // index of K1 in lookup table
	int K1;	 // symbol number of an extended source block
	int J_K; // systematic index of K1
	int T;	 // symbol size
	int X;	 // not use
	int S;	 // LDPC row number
	int H;	 // Half row number
	int W;	 // LT symbol number
	int P;	 // Permanent inactivated symbol number
	int P1;	 // smallest prime >= P
	int U;	 // P - H
	int B;	 // W - S
	int H1;	 // ceil(H/2)

	int L;		   // K+S+H
	int N;		   // received symbol number; set to K1 for encoder
	int N1;		   // received symbol plus extended one
	int M;		   // N+S+H
	TuplS *Tuples; // Tuples list
	int tupl_len;
	Symbol **C1;	// size M: S+H zero + N symbols
	Symbol **C;		// L intermediate symbols
	char **sources; // pointer to original source array
	Symbol **R;		// repair symbols
	Elem **A;		// generator matrix
	Elem **Abak;	// backup of A
#ifdef SPARSE
	int *rowh; // row list head
	int *colh; // column list head
#endif
	Degree *degree; // number of 1 in row i
	int dgh;		// degree list head
	int *isi;		// Encoding Symbol ID list
	int status;		/* 1: para inited, 2: source filled 3: intermediate generated 4: repair generated */
} Generators;

/* lifecycle */
Generators *Generators_new(void);
void Generators_free(Generators *g);

/* API */
bool Generators_gen(Generators *g, int _K, int _N, int _T);
bool Generators__0_init(Generators *g, int _K, int _N, int _T);
void Generators__1_Tuples(Generators *g);
void Generators__2_Matrix_GLDPC(Generators *g);
void Generators__3_Matrix_GHDPC(Generators *g);
void Generators__4_Matrix_GLT(Generators *g);

bool Generators_prepare(Generators *g, char **source, int _N, int *esi);
void Generators_swap_row(Generators *g, int i1, int i2);
void Generators_swap_col(Generators *g, int j1, int j2);
void Generators_xxor(Generators *g, int i1, int i2, int u);
int Generators_gaussian_elimination(Generators *g, int starti, int startj);
Symbol **Generators_generate_intermediates(Generators *g);
Symbol **Generators_generate_repairs(Generators *g, int count);
Symbol *Generators_recover_symbol(Generators *g, int x);

int Generators_getL(Generators *g);
int Generators_getK(Generators *g);

TuplS Generators_Tupl(Generators *g, int X);
unsigned int Generators_RandYim(unsigned int y, unsigned char i, unsigned int m);
unsigned int Generators_Deg(Generators *g, unsigned int v);
Symbol *Generators_LTEnc(Generators *g, Symbol **C_L, TuplS tupl);
void Generators_verify(Generators *g);
void Generators_PrintMatrix(Generators *g);
void Generators_ToString(Generators *g);

unsigned char octmul(unsigned char u, unsigned char v);
unsigned char octdiv(unsigned char u, unsigned char v);
