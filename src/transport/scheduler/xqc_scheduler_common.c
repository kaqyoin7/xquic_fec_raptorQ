#include "src/transport/scheduler/xqc_scheduler_common.h"
#include "src/transport/xqc_multipath.h"
#include "src/transport/xqc_packet_out.h"
#include "src/transport/xqc_send_ctl.h"
#include <math.h>

xqc_bool_t
xqc_scheduler_check_path_can_send(xqc_path_ctx_t *path, xqc_packet_out_t *packet_out, int check_cwnd)
{
    xqc_send_ctl_t *send_ctl = path->path_send_ctl;
    uint32_t schedule_bytes = path->path_schedule_bytes;

    /* normal packets in send list will be blocked by cc */
    if (check_cwnd && (!xqc_send_packet_cwnd_allows(send_ctl, packet_out, schedule_bytes, 0)))
    {
        xqc_log(send_ctl->ctl_conn->log, XQC_LOG_DEBUG, "|path:%ui|blocked by cwnd|", path->path_id);
        return XQC_FALSE;
    }

    return XQC_TRUE;
}

/**
 * Calculate comprehensive path score
 * Score = w1*rtt_score + w2*bw_score + w3*loss_score + w4*util_score
 * Higher score means better path
 */
double
xqc_calculate_path_score(xqc_path_ctx_t *path)
{
    xqc_send_ctl_t *send_ctl = path->path_send_ctl;
    xqc_connection_t *conn = path->parent_conn;
    
    /* Get path metrics */
    xqc_usec_t path_srtt = xqc_send_ctl_get_srtt(send_ctl);
    uint64_t path_bw = xqc_send_ctl_get_est_bw(send_ctl);
    double loss_rate = xqc_path_recent_loss_rate(path);
    
    /* Get congestion window */
    uint64_t cwnd = send_ctl->ctl_cong_callback->xqc_cong_ctl_get_cwnd(send_ctl->ctl_cong);
    if (cwnd == 0) {
        cwnd = 1; /* avoid division by zero */
    }
    
    /* Calculate utilization */
    uint64_t bytes_on_path = path->path_schedule_bytes + send_ctl->ctl_bytes_in_flight;
    double utilization = (double)bytes_on_path / (double)cwnd;
    
    /* Normalize metrics to [0, 1] range (higher is better) */
    
    /* 1. RTT score: lower RTT is better, convert to higher score */
    /* Use min_srtt as reference, normalize to [0, 1] */
    xqc_usec_t min_srtt = xqc_conn_get_min_srtt(conn, 0);
    if (min_srtt == 0) {
        min_srtt = 1; /* avoid division by zero */
    }
    double rtt_ratio = (double)path_srtt / (double)min_srtt;
    double rtt_score = 1.0 / (1.0 + rtt_ratio * 0.5); /* normalize to [0, 1] */
    
    /* 2. Bandwidth score: higher bandwidth is better */
    /* Normalize bandwidth (assuming max reasonable bandwidth ~1Gbps = 125MB/s) */
    double bw_mbps = (double)path_bw / 125000.0; /* convert to MB/s */
    double bw_score = bw_mbps / (1.0 + bw_mbps); /* normalize to [0, 1] */
    if (bw_score > 1.0) {
        bw_score = 1.0;
    }
    
    /* 3. Loss rate score: lower loss rate is better */
    double loss_score = 1.0 - (loss_rate / 100.0); /* loss_rate is percentage */
    if (loss_score < 0.0) {
        loss_score = 0.0;
    }
    if (loss_score > 1.0) {
        loss_score = 1.0;
    }
    
    /* 4. Utilization score: lower utilization is better (avoid congestion) */
    double util_score = 1.0 - utilization;
    if (util_score < 0.0) {
        util_score = 0.0;
    }
    if (util_score > 1.0) {
        util_score = 1.0;
    }
    
    /* Weighted combination */
    /* Default weights: RTT=0.4, Bandwidth=0.3, Loss=0.2, Utilization=0.1 */
    /* These can be adjusted based on application requirements */
    double w1 = 0.4; /* RTT weight */
    double w2 = 0.3; /* Bandwidth weight */
    double w3 = 0.2; /* Loss rate weight */
    double w4 = 0.1; /* Utilization weight */
    
    double score = w1 * rtt_score + w2 * bw_score + w3 * loss_score + w4 * util_score;
    
    xqc_log(conn->log, XQC_LOG_DEBUG, 
            "|path_score|path_id:%ui|rtt_score:%.3f|bw_score:%.3f|loss_score:%.3f|util_score:%.3f|total_score:%.3f|",
            path->path_id, rtt_score, bw_score, loss_score, util_score, score);
    
    return score;
}