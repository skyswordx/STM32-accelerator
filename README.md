# STM32-accelerator

[FPGA_MCU_SPI_COM: 基于SPI的FPGA与MCU简易通讯，以EP4CE15与STM32F407为例 (gitee.com)](https://gitee.com/themql/FPGA_MCU_SPI_COM)

[STM32H7 ADC交替采样实现8M采样率(14bit) 与高速USB（VCP）传输 - STM32/STM8单片机论坛 - ST MCU意法半导体官方技术支持论坛 (21ic.com)](https://bbs.21ic.com/icview-3409296-1-1.html)

```
ADC12_CDR 的 RDATA_SLV 是 0
但是 ADC12_CCR 寄存器各个位是正常的双 ADC 交替采样
ADC12_CSR 寄存器 EOC_MST 和 EOC_SLV 标志位都有不断刷新（但是频率不一样，不知道是否有影响）

ADRDY_MST 和 ADRDY_SLV 都是 1

发现 8bit 模式解包

8bit模式下的 双ADC 数据都在 ADC12_RDATA_MST里面
```


```
/**
  \brief   D-Cache Invalidate by address
  \details Invalidates D-Cache for the given address.
           D-Cache is invalidated starting from a 32 byte aligned address in 32 byte granularity.
           D-Cache memory blocks which are part of given address + given size are invalidated.
  \param[in]   addr    address
  \param[in]   dsize   size of memory block (in number of bytes)
*/
__STATIC_FORCEINLINE void SCB_InvalidateDCache_by_Addr (void *addr, int32_t dsize)
{
  #if defined (__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
    if ( dsize > 0 ) { 
       int32_t op_size = dsize + (((uint32_t)addr) & (__SCB_DCACHE_LINE_SIZE - 1U));
      uint32_t op_addr = (uint32_t)addr /* & ~(__SCB_DCACHE_LINE_SIZE - 1U) */;
    
      __DSB();

      do {
        SCB->DCIMVAC = op_addr;             /* register accepts only 32byte aligned values, only bits 31..5 are valid */
        op_addr += __SCB_DCACHE_LINE_SIZE;
        op_size -= __SCB_DCACHE_LINE_SIZE;
      } while ( op_size > 0 );

      __DSB();
      __ISB();
    }
的
SCB->DCIMVAC = op_addr;             /* register accepts only 32byte aligned values, only bits 31..5 are valid */
```