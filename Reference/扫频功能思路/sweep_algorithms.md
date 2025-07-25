# STM32 扫频策略算法实现

## 1. 扫描策略实现

### 1.1 快速概览策略
适用于快速了解元件在某个频段的趋势，通常只需要10到20个点。

特点：
- 测量速度快
- 精度相对较低
- 适用于初步判断元件特性

实现要点：
- 点数少，减少测量时间
- 可以使用较粗略的频率分布
- 适合宽频带快速扫描

### 1.2 标准特性表征策略
适用于产品数据手册中的标准特性曲线，通常使用50到200个点。

特点：
- 平衡测量速度和曲线平滑度
- 提供足够的信息量
- 适用于常规元件评估

实现要点：
- 点数适中，兼顾速度和精度
- 在关键频段增加点数密度
- 适合标准特性分析

### 1.3 精细分析策略
适用于精确分析自谐振频率、滤波器通带/阻带特性等，可能需要数百到数千个点。

特点：
- 高精度测量
- 适用于关键参数分析
- 测量时间较长

实现要点：
- 点数多，提高测量精度
- 在关键频段大幅增加点数密度
- 适合谐振点精确测量

## 2. 扫描方式实现

### 2.1 线性扫描算法
频率点按等差数列分布，适用于窄带高分辨率分析。

算法公式：
```
f(n) = f_start + n * (f_stop - f_start) / (points - 1)
```

其中：
- f(n)：第n个频率点
- f_start：起始频率
- f_stop：终止频率
- points：总点数
- n：点索引(0到points-1)

实现代码：
```c
void generate_linear_frequency_points(uint32_t start_freq, uint32_t stop_freq, uint32_t points, uint32_t* freq_array) {
    if (points <= 1) {
        freq_array[0] = start_freq;
        return;
    }
    
    for (uint32_t i = 0; i < points; i++) {
        freq_array[i] = start_freq + i * (stop_freq - start_freq) / (points - 1);
    }
}
```

### 2.2 对数扫描算法
频率点在对数坐标上等间距分布，适用于宽频带特性分析。

算法公式：
```
f(n) = f_start * (f_stop / f_start) ^ (n / (points - 1))
```

为了提高计算效率，可以使用以下等价公式：
```
f(n) = f_start * exp(ln(f_stop / f_start) * n / (points - 1))
```

实现代码：
```c
void generate_logarithmic_frequency_points(uint32_t start_freq, uint32_t stop_freq, uint32_t points, uint32_t* freq_array) {
    if (points <= 1) {
        freq_array[0] = start_freq;
        return;
    }
    
    if (start_freq == 0) start_freq = 1; // 避免对数计算错误
    
    float log_start = logf((float)start_freq);
    float log_stop = logf((float)stop_freq);
    float log_step = (log_stop - log_start) / (points - 1);
    
    for (uint32_t i = 0; i < points; i++) {
        freq_array[i] = (uint32_t)(expf(log_start + i * log_step) + 0.5f); // 四舍五入
    }
}
```

## 3. 特殊扫描策略

### 3.1 谐振点精细扫描
对于需要精确测量谐振频率的应用，可以采用分两步的扫描策略：

1. 宽带粗扫：先在较宽的范围内进行低密度扫描，快速定位谐振点的大致位置
2. 窄带精扫：以找到的谐振点为中心，进行高密度的窄带扫描

实现思路：
```c
// 第一步：宽带粗扫
sweep_config_t coarse_config = {
    .type = SWEEP_LOGARITHMIC,
    .start_freq = 100000,  // 100kHz
    .stop_freq = 5000000,  // 5MHz
    .points = 50,
    .strategy = STRATEGY_QUICK_VIEW
};

// 执行粗扫并找到谐振点
uint32_t resonant_freq = find_resonant_frequency(&coarse_config);

// 第二步：窄带精扫
sweep_config_t fine_config = {
    .type = SWEEP_LINEAR,
    .start_freq = resonant_freq - 100000,  // 谐振点前后100kHz
    .stop_freq = resonant_freq + 100000,
    .points = 201,
    .strategy = STRATEGY_FINE_ANALYSIS
};

// 执行精扫
perform_sweep(&fine_config);
```

### 3.2 多十倍频扫描
对于需要覆盖多个数量级频率范围的应用，可以按十倍频程分段扫描：

实现思路：
```c
void perform_decade_sweep(uint32_t start_freq, uint32_t stop_freq, uint32_t points_per_decade) {
    // 计算起始和终止频率的对数
    uint32_t start_decade = (uint32_t)floorf(log10f((float)start_freq));
    uint32_t stop_decade = (uint32_t)ceilf(log10f((float)stop_freq));
    
    // 对每个十倍频程进行扫描
    for (uint32_t decade = start_decade; decade <= stop_decade; decade++) {
        uint32_t decade_start = (uint32_t)powf(10.0f, (float)decade);
        uint32_t decade_stop = decade_start * 10;
        
        // 调整边界
        if (decade_start < start_freq) decade_start = start_freq;
        if (decade_stop > stop_freq) decade_stop = stop_freq;
        
        // 在当前十倍频程内进行对数扫描
        sweep_config_t config = {
            .type = SWEEP_LOGARITHMIC,
            .start_freq = decade_start,
            .stop_freq = decade_stop,
            .points = points_per_decade,
            .strategy = STRATEGY_STANDARD
        };
        
        perform_sweep(&config);
    }
}
```

## 4. 点数选择指南

### 4.1 根据测量目的选择点数

| 测量目的 | 推荐点数 | 说明 |
|---------|---------|------|
| 快速概览 | 10-20点 | 仅需了解大致趋势 |
| 标准特性表征 | 50-200点 | 平衡速度和精度 |
| 精细分析 | 200-1000点 | 高精度测量需求 |

### 4.2 根据元件特性选择点数

| 元件类型 | 推荐点数 | 特殊考虑 |
|---------|---------|---------|
| 普通R/L/C | 50-100点 | 特性变化平缓 |
| 谐振电路 | 200-500点 | 需在谐振点附近加密 |
| 滤波器 | 100-300点 | 通带和阻带边缘需要关注 |
| 磁性材料 | 300-800点 | 需完整描绘阻抗曲线 |

## 5. 实现注意事项

### 5.1 频率点生成精度
- 使用浮点运算时注意精度损失
- 对于整数频率点，需要适当的四舍五入处理
- 避免生成重复的频率点

### 5.2 边界处理
- 确保起始和终止频率点正确生成
- 处理特殊频率值（如0Hz）
- 验证频率点在DDS芯片支持范围内

### 5.3 性能优化
- 对数计算可以预先计算常数因子
- 可以使用查表法提高计算速度
- 对于固定模式的扫描，可以预先计算频率点数组