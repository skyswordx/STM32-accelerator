
# 以下命令用于设置系统参数

- `SET:DDS_FREQ:value` - 设置 DDS 频率
- `SET:ADC_RATE:value` - 设置 ADC 采样率
- `SET:DDS_TYPE:value` - 设置 DDS 类型 (AD9833 或 AD9954)
- `SET:DDS_PHASE:value` - 设置 DDS 相位
- `SET:DDS_AMP:value` - 设置 DDS 幅度
- `SET:DAC_WAVE:value` - 设置 DAC 波形 (SINE, SQUARE, TRIANGLE)
- `SET:DAC_FREQ:value` - 设置 DAC 频率
- `SET:DAC_AMP:value` - 设置 DAC 幅度
- `SET:RELAY:value` - 设置继电器
- `SET:FUNC:value` - 设置功能状态 (LCR, SPECTRUM, TIME, DIY)

# 4.2.2 查询命令 (GET)

用于查询系统状态：

- `GET:ALL` - 获取所有参数状态
- `GET:DDS_FREQ` - 获取 DDS 频率
- `GET:ADC_RATE` - 获取 ADC 采样率
- 等等...
