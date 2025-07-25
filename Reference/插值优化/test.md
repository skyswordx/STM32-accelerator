这是一份为 AI Agent 准备的高级代码编辑指导文件。

这份文件将引导您修改现有的 my\_freq\_config.c 和 my\_freq\_config.h 文件，以增加一个**运行时可选**的频谱插值功能。该功能旨在加汉宁窗之后，通过插值算法（如二次抛物线插值或汉宁窗专用公式）更精确地估算基波的频率和幅度。

**核心要求：** 您需要自行实现插值算法的具体逻辑，本文档仅提供框架、接口设计和集成思路。

### **AI Agent 指导文件：为 FFT 结果增加可选的频谱插值功能**

#### **一、 总体目标**

在 my\_freq\_config.c 中实现频谱插值算法，以提高基波频率和幅度测量的精度。此功能应是**运行时可配置的**，通过传递给主处理函数的参数，在“禁用”、“二次抛物线插值”和“汉宁窗专用插值”等模式间切换，而非使用编译时宏定义。

#### **二、 目标文件**

* my\_freq\_config.h  
* my\_freq\_config.c

#### **三、 详细执行步骤**

**步骤 1: 在 my\_freq\_config.h 中定义插值模式**

为了在运行时控制插值方法，请在 my\_freq\_config.h 文件中定义一个枚举类型来表示不同的插值模式。同时，保留用于存储插值结果的结构体。

指导：  
在 my\_freq\_config.h 文件顶部区域添加以下定义。  
// \--- 频谱插值模式定义 \---  
typedef enum {  
    INTERPOLATION\_DISABLED \= 0,      // 禁用插值  
    INTERPOLATION\_PARABOLIC \= 1,     // 启用二次抛物线插值 (通用方法)  
    INTERPOLATION\_HANNING\_SPECIAL \= 2  // 启用汉宁窗专用插值 (理论上更精确)  
} spectral\_interpolation\_mode\_t;

// 用于存储插值计算结果的内部结构体  
typedef struct {  
    float32\_t corrected\_frequency;  
    float32\_t corrected\_magnitude;  
} interpolated\_peak\_t;

**步骤 2: 修改主处理函数的接口**

为了实现运行时配置，您需要向 my\_armcfft32\_apply 和 my\_armcfft32\_apply 函数的声明和定义中添加一个新的参数，用于接收插值模式。

指导：  
例如，将 my\_armcfft32\_apply 函数的签名修改为：  
// 旧签名:  
// void my\_armcfft32\_apply(uint16\_t fftLen, bool enable\_window, my\_freq\_result\_t\* result);

// 新签名 (在 .h 和 .c 文件中同步修改):  
void my\_armcfft32\_apply(uint16\_t fftLen, bool enable\_window, spectral\_interpolation\_mode\_t interpolation\_mode, my\_freq\_result\_t\* result);

对 my\_armcfft32\_apply 函数也进行类似的修改。

**步骤 3: 在 my\_freq\_config.c 中规划插值计算辅助函数**

为了代码的模块化和复用，您需要自行设计并实现一个辅助函数来执行插值计算。

指导：  
在 my\_freq\_config.c 文件顶部，规划一个静态辅助函数。以下是该函数的建议原型和功能说明，您需要自行填充具体的实现。  
/\*\*  
 \* @brief (需要您实现) 执行频谱插值以查找更精确的峰值  
 \* @param magnitude\_spectrum 幅度谱数组 (线性幅度)  
 \* @param peak\_index 检测到的峰值点的索引  
 \* @param mode 要使用的插值算法模式  
 \* @param result\_out 指向插值结果的结构体指针  
 \* @retval arm\_status ARM\_MATH\_SUCCESS 如果成功, ARM\_MATH\_ARGUMENT\_ERROR 如果无法插值  
 \*/  
static arm\_status perform\_spectral\_interpolation(  
    float32\_t\* magnitude\_spectrum,   
    uint16\_t peak\_index,   
    spectral\_interpolation\_mode\_t mode,   
    interpolated\_peak\_t\* result\_out  
) {  
    // 1\. 实现边界检查：如果峰值在频谱的边缘，则无法插值，应返回错误。  
      
    // 2\. 使用 switch 或 if-else 结构，根据传入的 mode 参数选择相应的算法。

    // 3\. 在 case INTERPOLATION\_PARABOLIC 中:  
    //    \- 实现二次抛物线插值算法。  
    //    \- 提示：此方法通常在对数幅度谱（dB谱）上执行，以获得更好的效果。  
    //    \- 计算并填充 result\_out-\>corrected\_frequency 和 result\_out-\>corrected\_magnitude。

    // 4\. 在 case INTERPOLATION\_HANNING\_SPECIAL 中:  
    //    \- 实现基于汉宁窗特性的插值公式。  
    //    \- 提示：此方法直接在线性幅度谱上操作，并能同时校正频率和幅度。  
    //    \- 计算并填充 result\_out 的两个字段。

    // 5\. 如果 mode 为 INTERPOLATION\_DISABLED 或其他无效值，直接返回。

    // ... AI Agent 在此实现具体算法 ...

    return ARM\_MATH\_SUCCESS; // 成功时返回  
}

**步骤 4: 在 my\_armcfft32\_apply 函数中集成插值逻辑**

在找到基波峰值后，根据传入的 interpolation\_mode 参数决定是否调用插值函数。

指导：  
在 my\_armcfft32\_apply 函数中，定位到寻找基波分量的 for 循环之后。您需要在这里插入新的逻辑。  
// ...  
// for (...) { /\* 寻找基波峰值的循环 \*/ }  
// ...

// \[在此处插入新逻辑\]

// 1\. 初始化最终频率和幅度  
float32\_t final\_magnitude \= fundamental\_magnitude;  
float32\_t final\_frequency \= (float32\_t)fundamental\_index \* g\_ADC\_SAMPLE\_RATE\_Hz / FFT\_LENGTH;

// 2\. 根据运行时参数决定是否执行插值  
if (enable\_window && interpolation\_mode \!= INTERPOLATION\_DISABLED) {  
    interpolated\_peak\_t interpolated\_result;  
      
    // 调用您在步骤3中实现的函数  
    if (perform\_spectral\_interpolation(g\_single\_magnitude\_spectrum, fundamental\_index, interpolation\_mode, \&interpolated\_result) \== ARM\_MATH\_SUCCESS)  
    {  
        // 如果插值成功，则使用插值结果  
        final\_magnitude \= interpolated\_result.corrected\_magnitude;  
        final\_frequency \= interpolated\_result.corrected\_frequency;  
    }  
}

// 3\. \*\*保持关键的相位计算和幅度归一化逻辑不变\*\*  
//    相位计算必须使用未插值的索引 \`fundamental\_index\`。  
float32\_t real\_part \= g\_fft\_output\_buffer\[2 \* fundamental\_index\];  
float32\_t imag\_part \= g\_fft\_output\_buffer\[2 \* fundamental\_index \+ 1\];  
result-\>fundamental\_phase\_angle \= atan2f(imag\_part, real\_part) \* (180.0f / PI);

//    幅度归一化逻辑至关重要，它负责将FFT输出转换为物理电压值。  
float32\_t normalization\_factor \= enable\_window ? (4.0f / FFT\_LENGTH) : (2.0f / FFT\_LENGTH);  
result-\>fundamental\_vpp \= final\_magnitude \* normalization\_factor;  
result-\>fundamental\_vrms \= result-\>fundamental\_vpp / sqrtf(2.0f);  
result-\>fundamental\_frequency \= (uint16\_t)final\_frequency;

// ...


**核心提示**：新的幅度归一化逻辑 (normalization\_factor) 与插值算法协同工作。插值算法补偿了主瓣的扇贝损失，而归一化因子校正了 FFT 点数和窗函数相干增益的影响。两者结合才能得到最准确的物理幅度。