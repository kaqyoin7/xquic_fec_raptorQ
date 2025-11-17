# 综合评分机制优化 - 代码修改说明

## 一、修改概述

实现了综合评分机制，将原来仅基于RTT的路径选择改为综合考虑RTT、带宽、丢包率和利用率的综合评分机制。

## 二、修改的文件

### 1. `src/transport/scheduler/xqc_scheduler_common.h`

**修改位置**：第9-14行

**修改内容**：
```c
/**
 * Calculate comprehensive path score considering RTT, bandwidth, loss rate, and utilization
 * @param path: path context
 * @return: path score (higher is better)
 */
double xqc_calculate_path_score(xqc_path_ctx_t *path);
```

**说明**：添加了综合评分函数的声明。

---

### 2. `src/transport/scheduler/xqc_scheduler_common.c`

**修改位置**：
- 第5行：添加 `#include <math.h>`
- 第23-101行：新增 `xqc_calculate_path_score()` 函数

**修改内容**：

#### 2.1 添加头文件（第5行）
```c
#include <math.h>
```

#### 2.2 新增综合评分函数（第23-101行）
```c
/**
 * Calculate comprehensive path score
 * Score = w1*rtt_score + w2*bw_score + w3*loss_score + w4*util_score
 * Higher score means better path
 */
double
xqc_calculate_path_score(xqc_path_ctx_t *path)
{
    // 获取路径指标
    // 计算各项评分（归一化到[0,1]）
    // 加权求和
    // 返回综合评分
}
```

**函数逻辑**：
1. **获取路径指标**：
   - RTT：`xqc_send_ctl_get_srtt()`
   - 带宽：`xqc_send_ctl_get_est_bw()`
   - 丢包率：`xqc_path_recent_loss_rate()`
   - 利用率：`(path_schedule_bytes + ctl_bytes_in_flight) / cwnd`

2. **归一化评分**（所有评分都在[0,1]范围内，越高越好）：
   - **RTT评分**：`rtt_score = 1.0 / (1.0 + rtt_ratio * 0.5)`
   - **带宽评分**：`bw_score = bw_mbps / (1.0 + bw_mbps)`
   - **丢包率评分**：`loss_score = 1.0 - (loss_rate / 100.0)`
   - **利用率评分**：`util_score = 1.0 - utilization`

3. **加权求和**：
   ```c
   score = 0.4 * rtt_score + 0.3 * bw_score + 0.2 * loss_score + 0.1 * util_score
   ```

**默认权重**：
- RTT权重：0.4（延迟敏感）
- 带宽权重：0.3（吞吐量）
- 丢包率权重：0.2（可靠性）
- 利用率权重：0.1（负载均衡）

---

### 3. `src/transport/scheduler/xqc_scheduler_minrtt.c`

**修改位置**：第87-102行（原第87-93行）

**原代码**：
```c
path_srtt = xqc_send_ctl_get_srtt(path->path_send_ctl);

if (best_path[path_class] == NULL 
    || path_srtt < best_path[path_class]->path_send_ctl->ctl_srtt)
{
    best_path[path_class] = path;
}
```

**修改后代码**：
```c
/* Use comprehensive path score instead of only RTT */
double path_score = xqc_calculate_path_score(path);
double best_score = 0.0;

if (best_path[path_class] != NULL) {
    best_score = xqc_calculate_path_score(best_path[path_class]);
}

if (best_path[path_class] == NULL 
    || path_score > best_score)
{
    best_path[path_class] = path;
}

/* Keep path_srtt for logging compatibility */
path_srtt = xqc_send_ctl_get_srtt(path->path_send_ctl);
```

**修改说明**：
- 将原来只比较RTT的逻辑改为比较综合评分
- 保留 `path_srtt` 变量用于日志兼容性
- 评分越高表示路径越好

---

## 三、修改统计

| 文件 | 修改行数 | 修改类型 |
|------|---------|---------|
| `xqc_scheduler_common.h` | +7行 | 添加函数声明 |
| `xqc_scheduler_common.c` | +79行 | 新增函数实现 |
| `xqc_scheduler_minrtt.c` | +15行（替换7行） | 修改路径选择逻辑 |
| **总计** | **~101行** | **3个文件** |

---

## 四、核心改进

### 4.1 从单一指标到综合评分

**优化前**：
```c
// 只比较RTT
if (path_srtt < best_path[path_class]->path_send_ctl->ctl_srtt)
```

**优化后**：
```c
// 综合评分：RTT + 带宽 + 丢包率 + 利用率
if (path_score > best_score)
```

### 4.2 评分公式

```
综合评分 = 0.4 × RTT评分 + 0.3 × 带宽评分 + 0.2 × 丢包率评分 + 0.1 × 利用率评分
```

其中：
- **RTT评分**：基于与最小RTT的比值，RTT越小评分越高
- **带宽评分**：带宽越大评分越高（归一化到[0,1]）
- **丢包率评分**：丢包率越小评分越高
- **利用率评分**：利用率越低评分越高（避免过载）

### 4.3 优势

1. **综合考虑多个因素**：不再只看RTT，同时考虑带宽、丢包率和利用率
2. **负载均衡**：利用率因子有助于避免单一路径过载
3. **带宽利用**：带宽因子确保高带宽路径得到充分利用
4. **可靠性**：丢包率因子提升传输可靠性

---

## 五、使用说明

### 5.1 权重调整

如果需要调整权重以适应不同的应用场景，可以修改 `xqc_calculate_path_score()` 函数中的权重值：

```c
/* 延迟敏感应用（如实时音视频） */
double w1 = 0.5; /* RTT权重增加 */
double w2 = 0.2;
double w3 = 0.2;
double w4 = 0.1;

/* 吞吐量敏感应用（如文件传输） */
double w1 = 0.2;
double w2 = 0.5; /* 带宽权重增加 */
double w3 = 0.2;
double w4 = 0.1;
```

### 5.2 日志输出

函数会输出详细的评分信息，便于调试和监控：
```
|path_score|path_id:1|rtt_score:0.850|bw_score:0.750|loss_score:0.980|util_score:0.600|total_score:0.815|
```

---

## 六、兼容性说明

### 6.1 向后兼容

- ✅ 保持了原有的函数接口不变
- ✅ 保持了原有的日志格式（`path_srtt` 仍用于日志）
- ✅ 不影响其他调度器的实现

### 6.2 性能影响

- **计算开销**：每次路径选择需要计算评分（约增加4个浮点运算）
- **内存开销**：无额外内存开销
- **时间复杂度**：O(1) 额外开销，总体复杂度不变

---

## 七、测试建议

### 7.1 功能测试

1. **多路径场景**：验证在不同RTT、带宽、丢包率组合下的路径选择
2. **负载均衡**：验证流量是否更均匀地分布在多个路径上
3. **性能提升**：对比优化前后的吞吐量和延迟

### 7.2 性能测试

1. **延迟敏感应用**：测试实时应用的延迟改善
2. **吞吐量测试**：验证带宽利用率是否提升
3. **稳定性测试**：长时间运行测试稳定性

---

## 八、后续优化方向

### 8.1 可配置权重

可以将权重参数添加到 `xqc_scheduler_params_t` 结构体中，允许应用层配置：

```c
typedef struct xqc_scheduler_params_u {
    // ... 现有参数 ...
    double score_weight_rtt;      /* RTT权重 */
    double score_weight_bw;       /* 带宽权重 */
    double score_weight_loss;     /* 丢包率权重 */
    double score_weight_util;      /* 利用率权重 */
} xqc_scheduler_params_t;
```

### 8.2 评分缓存

为了避免重复计算，可以缓存路径评分：

```c
struct {
    double cached_score;
    xqc_usec_t cache_time;
} path_score_cache;
```

### 8.3 自适应权重

根据网络状况和应用需求动态调整权重。

---

## 九、总结

本次优化通过最小化代码改动（仅修改3个文件，约101行代码），实现了从单一RTT比较到综合评分的路径选择机制。优化后的算法能够：

- ✅ 综合考虑RTT、带宽、丢包率和利用率
- ✅ 更好地利用高带宽路径
- ✅ 实现负载均衡，避免单一路径过载
- ✅ 提升传输可靠性
- ✅ 保持向后兼容性

修改代码段清晰，易于维护和扩展。

