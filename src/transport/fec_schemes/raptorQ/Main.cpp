#include <iostream>
#include <fstream>
#include <time.h>
#include <stdlib.h>
#include <sys/time.h>
#ifdef WINDOWS
#include "StdAfx.h"
#include <windows.h>
#include <WinBase.h>
#endif

#include <ctime>

#include "Helper.h"
#include "Symbol.h"

using namespace std;

// 项目主程序，包含编码、丢包仿真、解码、正确性校验等流程
// 主要流程：生成源数据 → 编码 → 模拟丢包 → 解码 → 校验恢复正确性
#define TEST

// 获取当前时间(ms)
time_t GetTickCount()
{
	struct timeval t;

	gettimeofday(&t, NULL);

	return t.tv_sec * 1000 + t.tv_usec / 1000;
}

int main(int argc, char *argv[])
{

#ifdef TEST
	/*
	Helper helper;

	int al = 4;
	int kmin = 128;
	int gmax = 16;
	int packagesize = 1024;
	int w = 4096000;
	int overhead = 20;
	double loss = 0.1;
	int size = 256 * 1024;

	bool ret = helper.init(al,kmin,gmax,packagesize,size,w);

	if (ret)
		helper.toString();
	else
		cout<<"error"<<endl;

	PartitionS p = helper.getKtZ();

	int i;
	int t = helper.getT();
	int k = p.IS;
	int z = p.JS;
	*/

#define OVERHEAD 4 // K + OVERHEAD received symbols can have high possibility to recover
	// TODO：参数定义与初始化
	// K：源符号数量（源数据块数）
	// T：每个符号的字节数（符号长度）
	// lossrate：信道丢包率（模拟丢包环境）
	// overhead：冗余/修复符号数量（用于修复丢包）
	int i, j, K, T;
	double lossrate;
	int overhead;

	K = 8;
	T = 4;
	lossrate = 0.4; // 40%丢包率，模拟弱网
	overhead = (int)((K * lossrate + 10) / (1 - lossrate));

	cout << "K=" << K << " T=" << T << " Overhead=" << overhead << " lossrate=" << lossrate << endl;

	// TODO:源数据生成
	// 生成 K 个源符号
	char **source = new char *[K];

// #define RANDOM_DATA
#ifdef RANDOM_DATA
	for (i = 0; i < K; i++)
	{
		// 每个符号 T 字节
		source[i] = new char[T];
		for (j = 0; j < T; j++)
		{
			// T字节的符号内容为 0~T-1
			*(char *)(source[i] + j) = j;
		}
	}
#endif

	srand((unsigned)time(NULL)); // 确保随机性
	for (i = 0; i < K; i++)
	{
		source[i] = new char[T];
		for (j = 0; j < T; j++)
		{
			*(char *)(source[i] + j) = rand() % 256; // 随机赋值0~255
		}
	}

	// 输出源数据 source
	cout << "Source data:" << endl;
	for (i = 0; i < K; i++)
	{
		cout << "source[" << i << "]: ";
		for (j = 0; j < T; j++)
		{
			printf("%02x ", (unsigned char)source[i][j]);
		}
		cout << endl;
	}

	Encoder *encoder;

#if 0
	//test whether we can succeed for all K
	int success=0, fail=0;

	for (K=4;K<30;K++) {

		encoder = new Encoder();
		encoder->init(K,T);
		cout << "initialized with K=" << K << " and T=" << T << endl;

		if (encoder->encode(source, 2)) 
			success ++;
		else
			fail++;
			
		delete encoder;

	}
	cout << "success " << success << " fail " << fail << endl;
	while (true);
#endif

	Symbol **repairs;

	int start, end;

	/* encode */
	// TODO：模拟编码过程
	start = GetTickCount();

	encoder = new Encoder();
	encoder->init(K, T);
	// 对源数据编码，生成 overhead 个修复符号（repair符号），repairs 为修复符号数组
	repairs = encoder->encode(source, overhead);

	end = GetTickCount();

	// 编码带宽计算
	cout << "encode bandwidth=" << K * T / ((end - start) * 1000.0) << "MB/s" << endl;

	/* send in packet erasure channel */
	srand((unsigned)time(NULL));

	// TODO：丢包信道仿真
	// received：丢包场景下接收端实际收到的符号（源符号+修复符号）
	char **received = new char *[K + overhead];
	for (i = 0; i < K + overhead; i++)
		received[i] = new char[T];
	int n = 0; // received count
	// esi：每个接收符号的编码符号ID（ESI）
	int *esi = new int[K + overhead];

	// l：source丢包数
	int l = 0; // lost source packet count
	// lost：记录丢失的源符号下标
	int *lost = new int[K];

	/* send source */
	// 发送源符号：按丢包率随机决定每个源符号是否丢失，丢失的记录到 lost
	for (i = 0; i < K; i++)
	{
		if (rand() / (RAND_MAX + 1.0) > lossrate && i != 2)
		{
			memcpy(received[n], source[i], T);
			esi[n] = i;
			n++;
		}
		else
		{
			lost[l++] = i;
		}
	}

	// 输出 received 数据（源+修复）
	cout << "\nReceived source data (after loss simulation):" << endl;
	for (int idx = 0; idx < n; idx++)
	{
		cout << "received[" << idx << "] (source_index=" << esi[idx] << "): ";
		for (j = 0; j < T; j++)
		{
			printf("%02x ", (unsigned char)received[idx][j]);
		}
		cout << endl;
	}

	/* send repairs */
	// 发送修复符号：修复符号同样按丢包率随机丢弃，未丢失的加入 received
	for (i = 0; i < overhead; i++)
	{
		if (rand() / (RAND_MAX + 1.0) > lossrate)
		{
			memcpy(received[n], repairs[i]->data, T);
			esi[n] = K + i;
			n++;
		}
		delete repairs[i];
	}
	delete[] repairs;

	cout << "Received " << n << " packets out of total " << K + overhead << endl;

	/* decode */
	// 解码
	// 初始化参数,调用 decode，输入接收到的符号、数量、ESI 列表，尝试恢复中间符号。
	// 对每个丢失的源符号，调用 recover 恢复，并与原始数据对比校验。
	int no_decode = 0, success_decode = 0;
	int failed_decode1 = 0, failed_decode2 = 0, failed_decode3 = 0;
	Decoder *decoder;

	start = GetTickCount();

	if (l == 0)
	{
		cout << "All source arrived!" << endl;
		no_decode++;
	}
	else
	{
		if (n < K)
		{
			cout << "Too few packets got " << n << endl;
			failed_decode1++;
		}
		else
		{
			cout << "l:" << l << endl;
			Symbol *s;
			decoder = new Decoder();
			decoder->init(K, T);

			if (decoder->decode(received, n, esi))
			{
				for (i = 0; i < l; i++)
				{
					s = decoder->recover(lost[i]);
#if 1
					// 校验恢复的符号是否正确
					if (memcmp(s->data, source[lost[i]], T) != 0)
					{
						cout << "Recoverd symbol is not correct! x=" << lost[i] << endl;
						failed_decode3++;
						// break;
					}
					else
					{
						cout << "Recoverd symbol is ok! x=" << lost[i] << endl;
					}
#endif
					// 输出恢复后的数据
					cout << "Recovered data for lost[" << i << "] (source_index=" << lost[i] << "): ";
					for (j = 0; j < T; j++)
					{
						printf("%02x ", ((unsigned char *)s->data)[j]);
					}
					cout << endl;
					delete s;
				}
				cout << "All lost symbol recovered!" << endl;
				success_decode++;
			}
			else
			{
				cout << "Decode failed" << endl;
				failed_decode2++;
			}

			delete decoder;
		}
	}

	end = GetTickCount();

	// 解码带宽计算
	cout << "decode bandwidth=" << K * T / ((end - start) * 1000.0) << "MB/s" << endl;

	for (i = 0; i < K; i++)
		delete[] source[i];
	delete[] source;

	for (i = 0; i < K + overhead; i++)
		delete[] received[i];
	delete[] received;

	/* handled in generator. the primciple is that consumer handle the free of memories */
	// delete[] esi;
	delete[] lost;

	while (true)
		;

#endif

	return 0;
}
