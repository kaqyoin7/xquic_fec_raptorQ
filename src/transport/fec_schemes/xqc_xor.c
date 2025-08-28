
/**
 * @copyright Copyright (c) 2022, Alibaba Group Holding Limited
 */

#include "src/transport/fec_schemes/xqc_xor.h"
#include "src/transport/xqc_conn.h"

/**
 * @brief 初始化 XOR FEC 算法相关参数
 * @param conn 连接对象，内含与当前连接相关的状态和参数，包括FEC控制：fec_ctl。
 * 主要设置发送冗余块数量为 1
 */
void xqc_xor_init(xqc_connection_t *conn)
{
    if (conn->fec_ctl == NULL)
    {
        xqc_log(conn->log, XQC_LOG_ERROR, "|quic_fec|fail to malloc space for fec_ctl");
        return;
    }
    conn->fec_ctl->fec_send_required_repair_num[XQC_DEFAULT_SIZE_REQ] = 1;
    return;
}

/**
 * @brief 初始化单个 XOR FEC 编码块（此处为空实现，预留接口）
 * @param conn 连接对象
 * @param bm_idx 编码块索引
 */
void xqc_xor_init_one(xqc_connection_t *conn, uint8_t bm_idx)
{
    return;
}

#ifdef XQC_ON_x86
/**
 * @brief 使用 x86 SIMD 指令加速的异或操作
 * @param input 输入数据
 * @param outputs 输出数据（结果写回 outputs）
 * @param item_size 数据长度
 */
void xqc_xor_string_simd_x86(unsigned char *input, unsigned char *outputs,
                             xqc_int_t item_size)
{
    size_t i = 0;
    size_t aligned_len = item_size & ~(size_t)15;

    for (i = 0; i < aligned_len; i += 16)
    {
        __m128i output_unit = _mm_loadu_si128((__m128i *)(outputs + i));
        __m128i input_unit = _mm_loadu_si128((__m128i *)(input + i));
        __m128i result = _mm_xor_si128(output_unit, input_unit);
        _mm_storeu_si128((__m128i *)(outputs + i), result);
    }

    for (; i < item_size; ++i)
    {
        outputs[i] ^= input[i];
    }
}
#endif

#ifdef XQC_ON_ARM
/**
 * @brief 使用 ARM SIMD 指令加速的异或操作
 * @param input 输入数据
 * @param outputs 输出数据（结果写回 outputs）
 * @param item_size 数据长度
 */
void xqc_xor_string_simd_arm(unsigned char *input, unsigned char *outputs,
                             xqc_int_t item_size)
{
    size_t i = 0;
    size_t aligned_len = item_size & ~(size_t)15;

    for (i = 0; i < aligned_len; i += 16)
    {
        uint8x16_t out_data = vld1q_u8(outputs + i);
        uint8x16_t in_data = vld1q_u8(input + i);
        uint8x16_t result = veorq_u8(out_data, in_data);
        vst1q_u8(outputs + i, result);
    }

    for (; i < item_size; ++i)
    {
        outputs[i] ^= input[i];
    }
}
#endif

/**
 * @brief 通用的字节异或操作（无平台加速）
 * @param input 输入数据
 * @param outputs 输出数据（结果写回 outputs）
 * @param item_size 数据长度
 */
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

/**
 * @brief 对单个符号进行异或编码（可选用 SIMD 加速）
 * @param input 输入数据
 * @param outputs 输出数据（结果写回 outputs）
 * @param item_size 数据长度
 * @return XQC_OK 成功，负值失败
 */
xqc_int_t
xqc_xor_code_one_symbol(unsigned char *input, unsigned char *outputs,
                        xqc_int_t item_size)
{
    if (outputs == NULL)
    {
        return -XQC_EMALLOC;
    }

#if defined(XQC_ON_x86)
    xqc_xor_string_simd_x86(input, outputs, item_size);
#else
    xqc_xor_string(input, outputs, item_size);
#endif

    return XQC_OK;
}

/**
 * @brief XOR FEC 解码函数
 * @param conn 连接对象
 * @param outputs 输出数据指针数组（只用 outputs[0]）
 * @param output_size 输出数据长度
 * @param block_idx 需要恢复的块编号
 * @return XQC_OK 成功，负值失败
 *
 * 该函数遍历冗余块和源块链表，依次对 outputs[0] 进行异或操作，恢复丢失数据。
 * 只支持单符号丢失的场景。
 */
xqc_int_t
xqc_xor_decode(xqc_connection_t *conn, unsigned char **outputs, size_t *output_size, xqc_int_t block_idx)
{
    printf("xqc_xor_decode() triggered!");

    xqc_int_t i, j, ret, recv_repair_symbols_num, output_len, block_mod;
    xqc_usec_t now, diff_time;
    xqc_stream_t *stream;
    xqc_list_head_t *pos, *next, *fec_recv_src_syb_list, *fec_recv_rpr_syb_list;
    xqc_fec_rpr_syb_t *symbol = NULL;

    *output_size = 0;
    ret = -XQC_EFEC_SYMBOL_ERROR;
    fec_recv_src_syb_list = &conn->fec_ctl->fec_recv_src_syb_list;
    fec_recv_rpr_syb_list = &conn->fec_ctl->fec_recv_rpr_syb_list;
    block_mod = conn->conn_settings.fec_params.fec_blk_log_mod;

    // 先遍历冗余块链表，找到对应 block 的冗余块并异或到 outputs[0]
    xqc_list_for_each_safe(pos, next, fec_recv_rpr_syb_list)
    {
        xqc_fec_rpr_syb_t *rpr_syb = xqc_list_entry(pos, xqc_fec_rpr_syb_t, fec_list);
        if (rpr_syb->block_id < block_idx)
        {
            continue;
        }
        if (rpr_syb->block_id == block_idx)
        {
            if (rpr_syb->payload_size > XQC_MAX_SYMBOL_SIZE)
            {
                xqc_log(conn->log, XQC_LOG_ERROR, "|quic_fec|xor decoder can't process rpr symbol with size bigger than XQC_MAX_SYMBOL_SIZE.");
                return -XQC_EFEC_SCHEME_ERROR;
            }
            ret = xqc_xor_code_one_symbol(rpr_syb->payload, outputs[0], rpr_syb->payload_size);
            if (ret != XQC_OK)
            {
                return ret;
            }
            *output_size = rpr_syb->payload_size;
            symbol = rpr_syb;
            break;
        }
        if (rpr_syb->block_id > block_idx)
        {
            return -XQC_EFEC_SCHEME_ERROR;
        }
    }
    // 再遍历源块链表，找到对应 block 的所有源块并依次异或到 outputs[0]
    xqc_list_for_each_safe(pos, next, fec_recv_src_syb_list)
    {
        xqc_fec_src_syb_t *src_syb = xqc_list_entry(pos, xqc_fec_src_syb_t, fec_list);
        if (src_syb->block_id < block_idx)
        {
            continue;
        }
        if (src_syb->block_id == block_idx)
        {
            if (src_syb->payload_size > XQC_MAX_SYMBOL_SIZE)
            {
                xqc_log(conn->log, XQC_LOG_ERROR, "|quic_fec|xor decoder can't process src symbol with size bigger than XQC_MAX_SYMBOL_SIZE.");
                return -XQC_EFEC_SCHEME_ERROR;
            }
            ret = xqc_xor_code_one_symbol(src_syb->payload, outputs[0], src_syb->payload_size);
            if (ret != XQC_OK)
            {
                return ret;
            }
        }
        if (src_syb->block_id > block_idx)
        {
            break;
        }
    }

    if (ret != XQC_OK)
    {
        xqc_log(conn->log, XQC_LOG_ERROR, "|quic_fec|xqc_xor_decode|xor decode symbols failed");
        return ret;
    }

    // 日志记录恢复延迟
    if (conn->conn_settings.fec_params.fec_log_on && symbol && symbol->block_id % block_mod == 0)
    {
        now = xqc_monotonic_timestamp();
        diff_time = xqc_calc_delay(now, symbol->recv_time);
        xqc_log(conn->log, XQC_LOG_REPORT, "|fec_stats|XOR|current block: %d|recovered %ui ms after rpr received", symbol->block_id, diff_time / 1000);
    }

    return XQC_OK;
}

/**
 * @brief XOR FEC 编码函数
 * @param conn 连接对象
 * @param stream 输入数据
 * @param st_size 输入数据长度
 * @param outputs 输出数据指针数组（只用 outputs[0]）
 * @param fec_bm_mode FEC 编码模式
 * @return XQC_OK 成功，负值失败
 *
 * 该函数对输入数据进行异或编码，生成冗余块，并设置相关元数据。
 */
xqc_int_t
xqc_xor_encode(xqc_connection_t *conn, unsigned char *stream, size_t st_size, unsigned char **outputs,
               uint8_t fec_bm_mode)
{
    printf("xqc_xor_encode() triggered!");
    size_t tmp_size;
    xqc_int_t ret;

    if (st_size > XQC_MAX_SYMBOL_SIZE)
    {
        xqc_log(conn->log, XQC_LOG_ERROR, "|quic_fec|xqc_xor_encode|invalid input size:%d|", st_size);
        return -XQC_EFEC_SYMBOL_ERROR;
    }

    ret = xqc_xor_code_one_symbol(stream, outputs[0], st_size);
    // 设置冗余块对象的有效性和 payload size
    tmp_size = xqc_max(st_size, conn->fec_ctl->fec_send_repair_symbols_buff[fec_bm_mode][0].payload_size);
    if (tmp_size > XQC_MAX_SYMBOL_SIZE)
    {
        xqc_log(conn->log, XQC_LOG_ERROR, "|quic_fec|repair symbol payload exceeds the buffer size");
        ret = -XQC_EFEC_SCHEME_ERROR;
    }
    // 必须设置 fec_send_repair_symbols_buff，否则编码后 payload 不会被置 0
    xqc_set_object_value(&conn->fec_ctl->fec_send_repair_symbols_buff[fec_bm_mode][0], 1, outputs[0],
                         tmp_size);

    if (ret != XQC_OK)
    {
        xqc_log(conn->log, XQC_LOG_ERROR, "|quic_fec|xqc_xor_encode|code one symbol failed");
        return -XQC_EFEC_SCHEME_ERROR;
    }

    return XQC_OK;
}

/**
 * @brief XOR FEC 算法的回调结构体，注册到 FEC 框架中
 */
const xqc_fec_code_callback_t xqc_xor_code_cb = {
    .xqc_fec_init = xqc_xor_init,
    .xqc_fec_init_one = xqc_xor_init_one,
    .xqc_fec_decode = xqc_xor_decode,
    .xqc_fec_encode = xqc_xor_encode,
    // .destroy = xqc_rs_destroy,
};