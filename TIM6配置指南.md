# TIM6 定时器配置指南

## CubeMX 配置步骤

### 1. 启用 TIM6

- 在 CubeMX 的 Pinout & Configuration 页面
- 展开 Timers → TIM6
- 选择"Activated"

### 2. 配置 TIM6 参数

在 Configuration 页面的 TIM6 设置中：

**Basic Parameters：**

- Prescaler: 计算值，使得 TIM6 计数频率为合适值
- Counter Period: 计算值，使得总周期为 20ms

**计算公式：**

```
TIM6_CLK = 系统时钟频率 (例如: 400MHz)
定时器周期 = (Prescaler + 1) * (Counter Period + 1) / TIM6_CLK

对于20ms周期:
(Prescaler + 1) * (Counter Period + 1) = TIM6_CLK * 0.02
```

**示例配置（假设 TIM6_CLK = 200MHz）：**

- Prescaler: 1999 (分频器为 2000)
- Counter Period: 1999 (计数到 2000)
- 计算: 2000 \* 2000 / 200MHz = 20ms ✓

### 3. 启用中断

**NVIC Settings：**

- 勾选"TIM6 global interrupt"
- 设置合适的优先级（建议 Priority = 5）

### 4. 代码集成

在生成代码后，TIM6 会自动集成到项目中。我们的代码已经包含了中断处理：

```c
// 在main.c中会自动调用
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM6) {
        HAL_TIM_PeriodElapsedCallback_TIM6(htim); // 调用我们的处理函数
    }
}
```

### 5. 启动定时器

在模仿模式开始时，需要启动 TIM6：

```c
HAL_TIM_Base_Start_IT(&htim6); // 启动TIM6中断模式
```

在停止模式时：

```c
HAL_TIM_Base_Stop_IT(&htim6); // 停止TIM6
```

## 验证配置

1. 编译项目确认无错误
2. 运行程序，观察串口输出
3. 在模仿模式下，应该每 20ms 看到"20ms Timer triggered"消息

## 常见问题

1. **编译错误**：确认 CubeMX 已生成 TIM6 相关代码
2. **定时不准确**：检查 Prescaler 和 Counter Period 计算
3. **中断优先级冲突**：调整 TIM6 中断优先级
4. **系统卡死**：检查中断处理函数是否过长
