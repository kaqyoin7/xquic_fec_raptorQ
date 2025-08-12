#pragma once

#include <stdbool.h>
#include <stdio.h>

#define max(a, b) (((a) > (b)) ? (a) : (b))
#define min(a, b) (((a) < (b)) ? (a) : (b))

typedef struct PartitionS
{
	int IL;
	int IS;
	int JL;
	int JS;
} PartitionS;

typedef struct Helper
{
	int Kmax;
	int P;
	int F;
	int W;
	int Al;
	int Kmin;
	int Gmax;
	int G;
	int T;
	int Kt;
	int Z;
	int N;
	PartitionS KtZ;
	PartitionS TAlN;
} Helper;

Helper *Helper_new(void);
void Helper_free(Helper *h);

bool Helper_init(Helper *h, int Al, int Kmin, int Gmax, int P, int F, int W);

PartitionS Helper_partition(int I, int J);

void Helper_toString(Helper *h);

long Helper_Combin(int m, int n);