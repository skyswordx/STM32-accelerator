# 卡尔曼滤波器模块设计文档

## 1. 模块基本原理

卡尔曼滤波器是一种高效的递归滤波器，能够从一系列存在噪声的测量数据中估计动态系统的状态。本模块实现了一维卡尔曼滤波算法，用于对传感器测量值进行滤波处理，减少噪声干扰，提高测量精度。

算法基本步骤：
1. 预测阶段：基于系统模型预测当前状态和误差协方差
2. 更新阶段：结合实际测量值和预测值，计算最优状态估计

## 2. 数据结构

### 2.1 kalman_filter_t
```c
typedef struct {
    float q;              // 过程噪声协方差
    float r;              // 测量噪声协方差
    float x;              // 状态估计值
    float p;              // 估计误差协方差
    float k;              // 卡尔曼增益
    bool initialized;     // 初始化标志
} kalman_filter_t;
```

## 3. 函数接口

### 3.1 kalman_filter_init
```c
void kalman_filter_init(kalman_filter_t *kf, float process_noise, float measurement_noise);
```
功能：初始化卡尔曼滤波器
参数：
- kf: 指向卡尔曼滤波器结构体的指针
- process_noise: 过程噪声协方差
- measurement_noise: 测量噪声协方差

### 3.2 kalman_filter_update
```c
float kalman_filter_update(kalman_filter_t *kf, float measurement);
```
功能：更新卡尔曼滤波器状态，返回滤波后的值
参数：
- kf: 指向卡尔曼滤波器结构体的指针
- measurement: 当前测量值
返回值：滤波后的估计值

### 3.3 kalman_filter_reset
```c
void kalman_filter_reset(kalman_filter_t *kf);
```
功能：重置卡尔曼滤波器状态
参数：
- kf: 指向卡尔曼滤波器结构体的指针

### 3.4 kalman_filter_set_process_noise
```c
void kalman_filter_set_process_noise(kalman_filter_t *kf, float process_noise);
```
功能：设置过程噪声协方差
参数：
- kf: 指向卡尔曼滤波器结构体的指针
- process_noise: 过程噪声协方差值

### 3.5 kalman_filter_set_measurement_noise
```c
void kalman_filter_set_measurement_noise(kalman_filter_t *kf, float measurement_noise);
```
功能：设置测量噪声协方差
参数：
- kf: 指向卡尔曼滤波器结构体的指针
- measurement_noise: 测量噪声协方差值

## 4. 数据获取与处理流程

1. 系统通过函数指针从传感器获取原始测量数据
2. 如果启用了卡尔曼滤波器，则将原始测量数据传递给kalman_filter_update函数
3. 卡尔曼滤波器处理后返回滤波值，用于后续PID控制计算
4. 如果未启用卡尔曼滤波器，则直接使用原始测量数据

## 5. 与系统的接口

本模块作为一个独立的库提供给PID控制器使用：
- PID控制器创建kalman_filter_t实例
- 通过kalman_filter_init初始化滤波器参数
- 在控制循环中通过kalman_filter_update处理测量数据
- 提供enable_kalman_filter标志位控制是否启用滤波功能