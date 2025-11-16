#ifndef _XQC_SCHEDULER_COMMON_H_INCLUDED_
#define _XQC_SCHEDULER_COMMON_H_INCLUDED_

#include <xquic/xquic.h>
#include <xquic/xquic_typedef.h>

xqc_bool_t xqc_scheduler_check_path_can_send(xqc_path_ctx_t *path, xqc_packet_out_t *packet_out, int check_cwnd);

/**
 * Calculate comprehensive path score considering RTT, bandwidth, loss rate, and utilization
 * @param path: path context
 * @return: path score (higher is better)
 */
double xqc_calculate_path_score(xqc_path_ctx_t *path);

#endif