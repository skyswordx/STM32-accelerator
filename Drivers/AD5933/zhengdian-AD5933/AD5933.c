#include "AD5933.h"
#include "delay.h"
#include "IIC.h"
#include "math.h"
#include "stdlib.h"

/* AD5933 Instruct*********  指令****************************************************************/
u8 D_ADDR=0x1A;			//地址寄存器
u8 SET_POINT=0xB0;   	//0xB0命令表示写入地址指令

u8 data_clk=0x00;		//外:0x08 内0x00  时钟选择
u8 data_gain=0x01;		//增益	 1倍:0x01  5倍：0x00
u8 data_Vpp=0x00;		//激励峰峰值：0.2Vpp:0x02 0.4Vpp:0x04 1Vpp:0x06	 2Vpp:0x00

u8 data_AddH=0x00;		//频率增量，100Hz=0x000c80,10Hz=0x000140,1Hz=0x20
u8 data_AddM=0x0c;
u8 data_AddL=0x80;

u8 data_StartH=0x0F; 	//起始频率,10kHz=0x04E214   30kHz=0x 0f 00 00 ;
u8 data_StartM=0x00;		//1kHz=0x007d02,50kHz=ox186A64,35kHz=0x111764,10kHz=0x04E214
u8 data_StartL=0x00;

u8 data_CountFH=0x00;	//扫描点数
u8 data_CountFL=10;

u8 data_CountTH=0x00;	//延时周期数
u8 data_CountTL=0x3f;

float FreFlag;

u8 ReIm[6];	  			//数据寄存器 REH REL IMH IML   符号 结束
u8 ReIm10[10];


int Re_Result;
int Im_Result;
float Phase_Result;
float Impedance_Result;


/* 开始一次测试返回*/
void AD5933_StartOnceTest(  ImpeType *AA, u8 Add_OK )
{
    AD5933_StartTest( Add_OK );
    AD5933_ReadImpedance( AA );
}


void AD5933_StartTest( u8 Add_OK )
{
    if( Add_OK>0 )  //启动新的一次扫描并自增频率
    {
        AD5933_WriteByte( 0x80, 0x30|data_Vpp|data_gain );     /*启动频率扫描测量*/
    }
    else			//重复扫描不自增
    {
        AD5933_WriteByte( 0x80, 0x40|data_Vpp|data_gain );     /*启动再次测量*/
    }
}

float AD5933_ReadImpedance( ImpeType *AA )
{
    u8 j=0;
    while( (j&0x02)==0 )
    {
        AD5933_WriteByte( SET_POINT, 0x8f ); 		//数据指针指向状态寄存器
        delay_ms(5);
        j =  AD5933_ReadByte();
    }
    AD5933_WriteByte( SET_POINT, 0x94 );		//指向数据寄存器

    AA->Re = AD5933_ReadByte();
    AA->Re <<= 8;
    AA->Re |= AD5933_ReadByte();

    AA->Im = AD5933_ReadByte();
    AA->Im <<= 8;
    AA->Im |= AD5933_ReadByte();

    AA->Impedance = sqrt( ( (float)(abs(AA->Re) ) ) * ( (float)(abs(AA->Re) ) ) \
                          +   ( (float)( abs(AA->Im ) ) ) * ( (float)(abs(AA->Im) ) )  );

    if( (AA->Re>0)&&(AA->Im>0))	 		//第一项限
    {
        AA->Phase = atan( ((float)(AA->Im)) / ((float)(AA->Re))) ;
    }
    else
    {
        if( (AA->Re<0)&&(AA->Im>0))		//第二项限
        {
            AA->Phase = atan( ((float)(AA->Im)) / ((float)(AA->Re))) + PI;
        }
        else
        {
            if( (AA->Re<0)&&(AA->Im<0))	 //第三项限
            {
                AA->Phase = atan( ((float)(AA->Im)) / ((float)(AA->Re))) + PI;
            }
            else
            {
                if( (AA->Re>0)&&(AA->Im<0))	//第四项限
                {
                    AA->Phase = atan( ((float)(AA->Im)) / ((float)(AA->Re)))+ 2*PI;
                }
            }
        }
    }
    return 0;
}

void AD5933_FreInit( float Fre,float AddFre)	  			//启动一个频率点的扫描
{
    u32 StartHz;

    AD5933_WriteByte( 0x80, 0xA0|data_Vpp|data_gain );

    if( Fre != 0 )
    {
        StartHz = (u32)(Fre*4.0*134217728.0/AD5933Fre);
        data_StartH = (StartHz>>16)&0xFF;
        data_StartM = (StartHz>>8)&0xFF;
        data_StartL = (StartHz)&0xFF;
    }
    AD5933_WriteByte( 0x83, data_StartM );  	//起始频率,Fmclk=16.776MHz
    AD5933_WriteByte( 0x82, data_StartH );    //start frequency，
    AD5933_WriteByte( 0x84, data_StartL );  	//100kHz=0x30D4C8

    if( AddFre != 0 )
    {
        StartHz = (u32)(AddFre*4.0*134217728.0/AD5933Fre);
        data_AddH = (StartHz>>16)&0xFF;
        data_AddM = (StartHz>>8)&0xFF;
        data_AddL = (StartHz)&0xFF;
    }
    else
    {
        data_AddH = 0;
        data_AddM = 0;
        data_AddL = 0;
    }
    AD5933_WriteByte( 0x85, data_AddH );   	//频率增量
    AD5933_WriteByte( 0x86, data_AddM );
    AD5933_WriteByte( 0x87, data_AddL );

    AD5933_WriteByte( 0x88, data_CountFH );   //测量点数或频率个数
    AD5933_WriteByte( 0x89, data_CountFL );

    AD5933_WriteByte( 0x8A, data_CountTH );   //等待建立周期数
    AD5933_WriteByte( 0x8B, data_CountTL );

    AD5933_WriteByte( 0x80, 0xB0|data_Vpp|data_gain );
    AD5933_WriteByte( 0x81, 0x10|data_clk );		//复位
    delay_ms(150);			//复位延时
    AD5933_WriteByte( 0x81, 0x00|data_clk );

    AD5933_WriteByte( 0x80, 0x10|data_Vpp|data_gain );	/*以起始频率扫描*/
    delay_ms(30);
    AD5933_WriteByte( 0x80, 0x20|data_Vpp|data_gain );     /*启动频率扫描*/

}

void AD5933_WriteByte( u8 RegOrIns, u8 DataOrReg )	   //写一个字节到寄存器 或者指令
{
    IIC_Start();
    IIC_Send_Byte( D_ADDR );
    IIC_Send_Byte( RegOrIns );		//寄存器指针设置指令
    IIC_Send_Byte( DataOrReg );
    IIC_Stop();
}

u8 AD5933_ReadByte( void )  			//读一个字节
{
    u8 Temp;
    IIC_Start();
    IIC_Send_Byte( D_ADDR|0x01 );		//发送读地址
    Temp = 	IIC_Read_Byte(0);
    IIC_Stop();
    return( Temp );
}

float Temperature_Test( void )
{
    u8 i;
    int  k=0;
    AD5933_WriteByte( 0x80, 0x90|data_Vpp|data_gain );     //启动频率扫描 |data_Vpp|data_gain

    do
    {
        AD5933_WriteByte( SET_POINT, 0x8f ); 		//数据指针指向状态寄存器
        i =  AD5933_ReadByte();
    }
    while( (i&0x01)==0 );	 						//检测完成标志是否置位

    AD5933_WriteByte( SET_POINT, 0x92 );	//指向温度数据寄存器


    k = AD5933_ReadByte(); 		//读取数据寄存器
    k <<= 8;
    k += AD5933_ReadByte();
    if( k<8192 )
    {
        return( (float)((float)(k))/32.0);
    }
    else
    {
        return( ((float)(k-16384))/32.0);
    }

}
