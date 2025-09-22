/**
 * @copyright Copyright (c) 2022, Alibaba Group Holding Limited
 */

#include "src/transport/fec_schemes/xqc_xor.h"
#include "src/transport/xqc_conn.h"
#include "src/transport/fec_schemes/xqc_fountain.h"
#include "src/transport/xqc_fec.h"
#include "src/transport/xqc_fec_scheme.h"
#include "src/transport/xqc_packet_out.h"

#include "src/transport/fec_schemes/raptorQ_impl_c/Encoder.h"
#include "src/transport/fec_schemes/raptorQ_impl_c/Decoder.h"
#include "src/transport/fec_schemes/raptorQ_impl_c/Symbol.h"
#include "src/transport/fec_schemes/raptorQ_impl_c/Generators.h"

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

//每一 connection 的全局block context映射
typedef struct xqc_fountain_conn_ctx_s
{
    xqc_connection_t *conn;
    xqc_fountain_block_ctx_t **block_ctxs;
    uint32_t max_block_id;
    uint32_t capacity;
} xqc_fountain_conn_ctx_t;

//connection context映射
static xqc_fountain_conn_ctx_t **g_conn_ctxs = NULL;
static uint32_t g_conn_count = 0;
static uint32_t g_conn_capacity = 0;

//更具FEC参数计算repair symbols数量
static uint32_t xqc_fountain_calc_repair_num(xqc_connection_t *conn, uint8_t bm_idx)
{
    uint32_t max_k = xqc_get_fec_blk_size(conn, bm_idx);
    if (max_k == 0)
    {
        return 1;
    }

    if (conn->conn_settings.fec_params.fec_code_rate == 0)
    {
        //编码速率未设置时的默认方案
        return 1;
    }

    uint32_t r = (uint32_t)(max_k * conn->conn_settings.fec_params.fec_code_rate);
    if (r == 0)
        r = 1;
    if (r > XQC_REPAIR_LEN)
        r = XQC_REPAIR_LEN;
    return r;
}

// 确保size是4字节对齐的辅助函数
static uint32_t align_to_4_bytes(uint32_t size)
{
    return (size + 3) & ~3;
}

// 获取\创建 context 连接
static xqc_fountain_conn_ctx_t *get_conn_ctx(xqc_connection_t *conn)
{
    for (uint32_t i = 0; i < g_conn_count; i++)
    {
        if (g_conn_ctxs[i] && g_conn_ctxs[i]->conn == conn)
        {
            return g_conn_ctxs[i];
        }
    }
    return NULL;
}

//创建新 context 连接
static xqc_fountain_conn_ctx_t *create_conn_ctx(xqc_connection_t *conn)
{
    if (g_conn_count >= g_conn_capacity)
    {
        uint32_t new_capacity = g_conn_capacity == 0 ? 4 : g_conn_capacity * 2;
        xqc_fountain_conn_ctx_t **new_ctxs = realloc(g_conn_ctxs,
                                                     new_capacity * sizeof(xqc_fountain_conn_ctx_t *));
        if (!new_ctxs)
            return NULL;
        g_conn_ctxs = new_ctxs;
        g_conn_capacity = new_capacity;
    }

    xqc_fountain_conn_ctx_t *conn_ctx = malloc(sizeof(xqc_fountain_conn_ctx_t));
    if (!conn_ctx)
        return NULL;

    conn_ctx->conn = conn;
    conn_ctx->block_ctxs = NULL;
    conn_ctx->max_block_id = 0;
    conn_ctx->capacity = 0;

    g_conn_ctxs[g_conn_count++] = conn_ctx;
    return conn_ctx;
}

//扩展block context数组
static void expand_block_ctx_array(xqc_fountain_conn_ctx_t *conn_ctx, uint32_t block_id)
{
    if (block_id >= conn_ctx->capacity)
    {
        uint32_t new_capacity = conn_ctx->capacity == 0 ? block_id + 1 : conn_ctx->capacity * 2;
        if (new_capacity <= block_id)
            new_capacity = block_id + 1;

        xqc_fountain_block_ctx_t **new_ctxs = realloc(conn_ctx->block_ctxs,
                                                      new_capacity * sizeof(xqc_fountain_block_ctx_t *));
        if (!new_ctxs)
            return;

        for (uint32_t i = conn_ctx->capacity; i < new_capacity; i++)
        {
            new_ctxs[i] = NULL;
        }

        conn_ctx->block_ctxs = new_ctxs;
        conn_ctx->capacity = new_capacity;
    }
}

void xqc_xor_init(xqc_connection_t *conn)
{
    printf("xqc_fountain_init() triggered!");
    if (conn->fec_ctl == NULL)
    {
        xqc_log(conn->log, XQC_LOG_ERROR, "|quic_fec|fail to malloc space for fec_ctl");
        return;
    }

    uint32_t r = xqc_fountain_calc_repair_num(conn, XQC_DEFAULT_SIZE_REQ);
    conn->fec_ctl->fec_send_required_repair_num[XQC_DEFAULT_SIZE_REQ] = r;
    return;
}

void xqc_xor_init_one(xqc_connection_t *conn, uint8_t bm_idx)
{
   if (bm_idx >= XQC_BLOCK_MODE_LEN)
    {
        return;
    }

    uint32_t r = xqc_fountain_calc_repair_num(conn, bm_idx);
    conn->fec_ctl->fec_send_required_repair_num[bm_idx] = r;
}

void xqc_xor_string(unsigned char *input, unsigned char *outputs,
                    xqc_int_t item_size)
{
    size_t i = 0;
    unsigned char *output_p;

    output_p = outputs;
    for (i = 0; i < item_size; i++)
    {
        *(output_p + i) ^= *(input + i);
    }
}

xqc_int_t
xqc_xor_code_one_symbol(unsigned char *input, unsigned char *outputs,
                        xqc_int_t item_size)
{
    if (outputs == NULL)
    {
        return -XQC_EMALLOC;
    }

    xqc_xor_string(input, outputs, item_size);
    return XQC_OK;
}

/* -------------------- 修复的 decode -------------------- */
xqc_int_t
xqc_xor_decode(xqc_connection_t *conn, unsigned char **outputs, size_t *output_size, xqc_int_t block_idx)
{
    printf("xqc_fountain_decode() triggered!");
    xqc_fountain_block_ctx_t *ctx = xqc_fountain_get_or_create_block_ctx(conn, block_idx, 0);
    if (!ctx) return -XQC_EMALLOC;

    uint32_t recv_src = xqc_cnt_src_symbols_num(conn->fec_ctl, block_idx);
    uint32_t recv_rpr = xqc_cnt_rpr_symbols_num(conn->fec_ctl, block_idx);
    uint32_t total_received = recv_src + recv_rpr;

    if (total_received < ctx->K) {
        return -XQC_EFEC_SCHEME_ERROR;
    }
    if (recv_src == ctx->K) {
        *output_size = 0;
        return XQC_OK;
    }

    char **received_symbols = malloc(total_received * sizeof(char *));
    int *esi_list = malloc(total_received * sizeof(int));
    if (!received_symbols || !esi_list) {
        free(received_symbols); free(esi_list);
        return -XQC_EMALLOC;
    }

    uint32_t idx = 0;
    xqc_list_head_t *pos, *next;
    xqc_list_for_each_safe(pos, next, &conn->fec_ctl->fec_recv_src_syb_list) {
        xqc_fec_src_syb_t *src = xqc_list_entry(pos, xqc_fec_src_syb_t, fec_list);
        if (src->block_id == block_idx) {
            received_symbols[idx] = malloc(ctx->T);
            memcpy(received_symbols[idx], src->payload, src->payload_size);
            if (src->payload_size < ctx->T)
                memset(received_symbols[idx]+src->payload_size,0,ctx->T-src->payload_size);
            esi_list[idx] = src->symbol_idx;
            idx++;
        }
    }

    xqc_list_for_each_safe(pos, next, &conn->fec_ctl->fec_recv_rpr_syb_list) {
        xqc_fec_rpr_syb_t *rpr = xqc_list_entry(pos, xqc_fec_rpr_syb_t, fec_list);
        if (rpr->block_id == block_idx) {
            received_symbols[idx] = malloc(ctx->T);
            memcpy(received_symbols[idx], rpr->payload, rpr->payload_size);
            if (rpr->payload_size < ctx->T)
                memset(received_symbols[idx]+rpr->payload_size,0,ctx->T-rpr->payload_size);
            esi_list[idx] = ctx->K + rpr->symbol_idx;
            idx++;
        }
    }

    if (!ctx->decoder_ready) {
        if (!ctx->decoder) ctx->decoder = Decoder_new();
        if (!ctx->decoder || !Decoder_init(ctx->decoder, ctx->K, ctx->T))
            goto cleanup;
        ctx->decoder_ready = true;
    }

    /* ⚠️修复：使用 idx 而不是 total_received */
    if (!Decoder_decode(ctx->decoder, received_symbols, idx, esi_list))
        goto cleanup;

    uint32_t recovered_count = 0;
    for (uint32_t i = 0; i < ctx->K && recovered_count < XQC_MAX_SYMBOL_SIZE; i++) {
        if (!ctx->src_received[i] && outputs[recovered_count]) {
            Symbol *recovered = Decoder_recover(ctx->decoder, i);
            if (recovered) {
                memcpy(outputs[recovered_count], (char*)recovered->data, ctx->T);
                recovered_count++;
                Symbol_free(recovered);
            }
        }
    }

    *output_size = ctx->T;

cleanup:
    for (uint32_t i = 0; i < idx; i++)
        free(received_symbols[i]);
    free(received_symbols);
    free(esi_list);
    return (recovered_count>0)?XQC_OK:-XQC_EFEC_SCHEME_ERROR;
}

/* -------------------- 修复的 encode -------------------- */
xqc_int_t
xqc_xor_encode(xqc_connection_t *conn, unsigned char *stream, size_t st_size, unsigned char **outputs,
               uint8_t fec_bm_mode)
{
    printf("xqc_fountain_encode() triggered!");
    if (st_size > XQC_MAX_SYMBOL_SIZE) return -XQC_EFEC_SYMBOL_ERROR;

    uint32_t block_id = conn->fec_ctl->fec_send_block_num[fec_bm_mode];
    xqc_fountain_block_ctx_t *ctx = xqc_fountain_get_or_create_block_ctx(conn, block_id, fec_bm_mode);
    if (!ctx) return -XQC_EMALLOC;

    uint32_t newT = st_size > ctx->T ? align_to_4_bytes(st_size) : ctx->T;
    if (ctx->T == 0) {
        ctx->T = newT;
    } else if (newT > ctx->T) {
        /* ⚠️修复：扩展所有已有 src_buffers */
        for (uint32_t i=0;i<ctx->curr_count;i++){
            if (ctx->src_buffers && ctx->src_buffers[i]){
                char *p = realloc(ctx->src_buffers[i], newT);
                if (!p) return -XQC_EMALLOC;
                memset(p+ctx->T,0,newT-ctx->T);
                ctx->src_buffers[i]=p;
            }
        }
        ctx->T = newT;
    }

    if (ctx->curr_count >= ctx->K) return -XQC_EFEC_SCHEME_ERROR;

    if (!ctx->src_buffers) {
        ctx->src_buffers = malloc(ctx->K*sizeof(char*));
        ctx->src_received= malloc(ctx->K*sizeof(bool));
        if (!ctx->src_buffers||!ctx->src_received) return -XQC_EMALLOC;
        for(uint32_t i=0;i<ctx->K;i++){ctx->src_buffers[i]=NULL;ctx->src_received[i]=false;}
    }

    if (!ctx->src_buffers[ctx->curr_count]) {
        ctx->src_buffers[ctx->curr_count]=malloc(ctx->T);
        memset(ctx->src_buffers[ctx->curr_count],0,ctx->T);
    }
    memcpy(ctx->src_buffers[ctx->curr_count], stream, st_size);
    if (st_size<ctx->T) memset(ctx->src_buffers[ctx->curr_count]+st_size,0,ctx->T-st_size);
    ctx->src_received[ctx->curr_count]=true;
    ctx->curr_count++;

    if (ctx->curr_count==ctx->K) {
        if (!ctx->encoder_ready) {
            if (!ctx->encoder) ctx->encoder=Encoder_new();
            if (!ctx->encoder||!Encoder_init(ctx->encoder,ctx->K,ctx->T))
                return -XQC_EFEC_SCHEME_ERROR;
            ctx->encoder_ready=true;
        }

        char **source_data=malloc(ctx->K*sizeof(char*));
        for(uint32_t i=0;i<ctx->K;i++) source_data[i]=ctx->src_buffers[i];

        uint32_t repair_num=conn->fec_ctl->fec_send_required_repair_num[fec_bm_mode];
        Symbol **repairs=Encoder_encode(ctx->encoder,source_data,repair_num);

        for(uint32_t i=0;i<repair_num;i++){
            if (outputs[i]&&repairs[i]){
                memcpy(outputs[i],(char*)repairs[i]->data,ctx->T);
                xqc_set_object_value(&conn->fec_ctl->fec_send_repair_symbols_buff[fec_bm_mode][i],
                                     1, outputs[i], ctx->T);
                Symbol_free(repairs[i]);
            }
        }

        free(repairs);
        free(source_data);
    }

    return XQC_OK;
}

const xqc_fec_code_callback_t xqc_xor_code_cb = {
    .xqc_fec_init = xqc_xor_init,
    .xqc_fec_init_one = xqc_xor_init_one,
    .xqc_fec_decode = xqc_xor_decode,
    .xqc_fec_encode = xqc_xor_encode,
};
