/**
 * Fountain (RaptorQ-like) FEC implementation in C for xquic
 * Based on raptorQ_impl_c implementation
 */

#ifndef _XQC_FEC_FOUNTAIN_H_
#define _XQC_FEC_FOUNTAIN_H_

#include <xquic/xquic.h>
#include <xquic/xqc_errno.h>
#include <xquic/xquic_typedef.h>
#include <stdbool.h>

#include "src/transport/xqc_conn.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /* Forward declarations for RaptorQ implementation */
    typedef struct Encoder Encoder;
    typedef struct Decoder Decoder;
    typedef struct Symbol Symbol;

    /* Block context for Fountain Code */
    typedef struct xqc_fountain_block_ctx_s
    {
        uint32_t block_id;
        uint32_t K;          // Source symbol count
        uint32_t T;          // Symbol size in bytes
        uint32_t curr_count; // Current received source symbols count
        char **src_buffers;  // K source symbol buffers, each T bytes
        bool *src_received;  // Mark which source symbols are received
        Encoder *encoder;    // RaptorQ encoder
        Decoder *decoder;    // RaptorQ decoder
        bool encoder_ready;  // Encoder ready flag
        bool decoder_ready;  // Decoder ready flag
    } xqc_fountain_block_ctx_t;

    /* Main interface functions */
    void xqc_fountain_init(xqc_connection_t *conn);
    void xqc_fountain_init_one(xqc_connection_t *conn, uint8_t bm_idx);

    xqc_int_t xqc_fountain_encode(xqc_connection_t *conn,
                                  unsigned char *stream,
                                  size_t st_size,
                                  unsigned char **outputs,
                                  uint8_t fec_bm_mode);

    xqc_int_t xqc_fountain_decode(xqc_connection_t *conn,
                                  unsigned char **outputs,
                                  size_t *output_size,
                                  xqc_int_t block_idx);

    /* Internal management functions */
    xqc_fountain_block_ctx_t *xqc_fountain_get_or_create_block_ctx(xqc_connection_t *conn,
                                                                   uint32_t block_id,
                                                                   uint8_t bm_idx);
    void xqc_fountain_destroy_block_ctx(xqc_fountain_block_ctx_t *ctx);
    void xqc_fountain_cleanup_connection(xqc_connection_t *conn);

    extern const xqc_fec_code_callback_t xqc_fountain_code_cb;

#ifdef __cplusplus
}
#endif

#endif /* _XQC_FEC_FOUNTAIN_H_ */
