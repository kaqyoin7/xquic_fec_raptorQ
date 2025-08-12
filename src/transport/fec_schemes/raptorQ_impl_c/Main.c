#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>

#include "Helper.h"
#include "Symbol.h"
#include "Encoder.h"
#include "Decoder.h"

// 纯C版本的主程序

// 获取当前时间(ms)
time_t GetTickCount()
{
    struct timeval t;
    gettimeofday(&t, NULL);
    return t.tv_sec * 1000 + t.tv_usec / 1000;
}

int main(int argc, char *argv[])
{
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

    printf("K=%d T=%d Overhead=%d lossrate=%f\n", K, T, overhead, lossrate);

    // TODO:源数据生成
    // 生成 K 个源符号
    char **source = (char **)malloc(K * sizeof(char *));
    if (!source)
    {
        printf("Failed to allocate source array\n");
        return 1;
    }

    srand((unsigned)time(NULL)); // 确保随机性
    for (i = 0; i < K; i++)
    {
        source[i] = (char *)malloc(T);
        if (!source[i])
        {
            printf("Failed to allocate source[%d]\n", i);
            // 清理已分配的内存
            for (j = 0; j < i; j++)
                free(source[j]);
            free(source);
            return 1;
        }
        for (j = 0; j < T; j++)
        {
            source[i][j] = rand() % 256; // 随机赋值0~255
        }
    }

    // 输出源数据 source
    printf("Source data:\n");
    for (i = 0; i < K; i++)
    {
        printf("source[%d]: ", i);
        for (j = 0; j < T; j++)
        {
            printf("%02x ", (unsigned char)source[i][j]);
        }
        printf("\n");
    }

    Encoder *encoder;
    Symbol **repairs;
    int start, end;

    /* encode */
    // TODO：模拟编码过程
    start = GetTickCount();

    encoder = Encoder_new();
    if (!encoder)
    {
        printf("Failed to create encoder\n");
        goto cleanup;
    }

    if (!Encoder_init(encoder, K, T))
    {
        printf("Failed to initialize encoder\n");
        Encoder_free(encoder);
        goto cleanup;
    }

    // 对源数据编码，生成 overhead 个修复符号（repair符号），repairs 为修复符号数组
    repairs = Encoder_encode(encoder, source, overhead);
    if (!repairs)
    {
        printf("Failed to encode\n");
        Encoder_free(encoder);
        goto cleanup;
    }

    end = GetTickCount();

    // 编码带宽计算
    printf("encode bandwidth=%f MB/s\n", K * T / ((end - start) * 1000.0));

    /* send in packet erasure channel */
    srand((unsigned)time(NULL));

    // TODO：丢包信道仿真
    // received：丢包场景下接收端实际收到的符号（源符号+修复符号）
    char **received = (char **)malloc((K + overhead) * sizeof(char *));
    if (!received)
    {
        printf("Failed to allocate received array\n");
        goto cleanup;
    }

    for (i = 0; i < K + overhead; i++)
    {
        received[i] = (char *)malloc(T);
        if (!received[i])
        {
            printf("Failed to allocate received[%d]\n", i);
            // 清理已分配的内存
            for (j = 0; j < i; j++)
                free(received[j]);
            free(received);
            goto cleanup;
        }
    }

    int n = 0; // received count
    // esi：每个接收符号的编码符号ID（ESI）
    int *esi = (int *)malloc((K + overhead) * sizeof(int));
    if (!esi)
    {
        printf("Failed to allocate esi array\n");
        goto cleanup;
    }

    // l：source丢包数
    int l = 0; // lost source packet count
    // lost：记录丢失的源符号下标
    int *lost = (int *)malloc(K * sizeof(int));
    if (!lost)
    {
        printf("Failed to allocate lost array\n");
        goto cleanup;
    }

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
    printf("\nReceived source data (after loss simulation):\n");
    for (int idx = 0; idx < n; idx++)
    {
        printf("received[%d] (source_index=%d): ", idx, esi[idx]);
        for (j = 0; j < T; j++)
        {
            printf("%02x ", (unsigned char)received[idx][j]);
        }
        printf("\n");
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
        Symbol_free(repairs[i]);
    }
    free(repairs);

    printf("Received %d packets out of total %d\n", n, K + overhead);

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
        printf("All source arrived!\n");
        no_decode++;
    }
    else
    {
        if (n < K)
        {
            printf("Too few packets got %d\n", n);
            failed_decode1++;
        }
        else
        {
            printf("l:%d\n", l);
            Symbol *s;
            decoder = Decoder_new();
            if (!decoder)
            {
                printf("Failed to create decoder\n");
                goto cleanup;
            }

            if (!Decoder_init(decoder, K, T))
            {
                printf("Failed to initialize decoder\n");
                Decoder_free(decoder);
                goto cleanup;
            }

            if (Decoder_decode(decoder, received, n, esi))
            {
                for (i = 0; i < l; i++)
                {
                    s = Decoder_recover(decoder, lost[i]);
                    if (!s)
                    {
                        printf("Failed to recover symbol %d\n", lost[i]);
                        continue;
                    }

                    // 校验恢复的符号是否正确
                    if (memcmp(s->data, source[lost[i]], T) != 0)
                    {
                        printf("Recovered symbol is not correct! x=%d\n", lost[i]);
                        failed_decode3++;
                    }
                    else
                    {
                        printf("Recovered symbol is ok! x=%d\n", lost[i]);
                    }

                    // 输出恢复后的数据
                    printf("Recovered data for lost[%d] (source_index=%d): ", i, lost[i]);
                    for (j = 0; j < T; j++)
                    {
                        printf("%02x ", ((unsigned char *)s->data)[j]);
                    }
                    printf("\n");
                    Symbol_free(s);
                }
                printf("All lost symbol recovered!\n");
                success_decode++;
            }
            else
            {
                printf("Decode failed\n");
                failed_decode2++;
            }

            Decoder_free(decoder);
        }
    }

    end = GetTickCount();

    // 解码带宽计算
    printf("decode bandwidth=%f MB/s\n", K * T / ((end - start) * 1000.0));

cleanup:
    // 清理内存
    for (i = 0; i < K; i++)
        free(source[i]);
    free(source);

    if (received)
    {
        for (i = 0; i < K + overhead; i++)
            free(received[i]);
        free(received);
    }

    free(esi);
    free(lost);

    printf("Press Enter to exit...\n");
    getchar();
    return 0;
}