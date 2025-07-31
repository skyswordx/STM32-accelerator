#__author__ = 'sn01309'
#coding=utf-8
# 本程序实现对Rigol信号发生器的控制与示波器的通信和控制以实现stm32h7的数据校准
# 在command.md中有详细的命令说明

if __name__ == '__main__' :

    import pyvisa;
    import serial;
    import pandas as pd;
    rm = pyvisa.ResourceManager();
    reslist = rm.list_resources();
    print(reslist);
    # digital_generator = rm.open_resource("TCPIP::192.168.1.2::INSTR");
    digital_osc = rm.open_resource("TCPIP::192.168.1.2::INSTR");
    # digital_generator.write("*IDN?");
    digital_osc.write("*IDN?");
    # print(digital_generator.read());
    print(digital_osc.read());

    # 设置示波器参数
    digital_osc.write(":MEASUrement:IMMed:TYPe PK2pk");#设置测量类型为峰峰值
    digital_osc.write(":MEASUrement:IMMed:SOURCE CH1");#设置测量源为通道1
    digital_osc.write(":MEASUrement:IMMed:REFerence 0");#设置参考值为0
    digital_osc.write(":MEASUrement:IMMed:VALue?");#查询测量值
    print(digital_osc.read());

    # # stm32 串口初始化
    # ser = serial.Serial('COM14', 115200, timeout=1)  # 打开串口COM14，波特率115200，超时1秒
    # ser.write(b'Hello, Rigol!')  # 发送数据到串口
    # ser.write(b'SET:DDS_TYPE:9954')  # 设置DDS类型

    # ser.write(b'SET:DDS_FREQ:1000000')  # 设置DDS频率为1MHz
    # ser.write(b'SET:DDS_AMPL:0.5')  # 设置DDS幅度为0.5Vpp

    # digital_generator.write(":SOURce:FREQuency 1MHz");#设置频率
    # digital_generator.write(":SOURce:VOLTage 0.5");#设置幅度为0.5Vpp
    rm.close();
    