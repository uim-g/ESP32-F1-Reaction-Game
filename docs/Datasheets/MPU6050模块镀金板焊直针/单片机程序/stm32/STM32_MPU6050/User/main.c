/*
STM32    MPU6050		OLED
PB10			SCL
PB11			SDA	
3V3				VCC				VCC
GND				GND				GND
PB8									SCL
PB9									SDA
*/
#include "stm32f10x.h"                  // Device header
#include "Delay.h"
#include "OLED.h"
#include "MPU6050.h"

uint8_t ID;								//定义用于存放ID号的变量
int16_t AX, AY, AZ, GX, GY, GZ;			//定义用于存放各个数据的变量

int main(void)
{
	/*模块初始化*/
	OLED_Init();		//OLED初始化
	MPU6050_Init();		//MPU6050初始化
	OLED_Refresh();
	/*显示ID号*/
	OLED_ShowString(1, 1, "ID:",16,1);		//显示静态字符串
	ID = MPU6050_GetID();				//获取MPU6050的ID号
	OLED_ShowHexNum(25, 1, ID,2,16,1);		//OLED显示ID号
	OLED_ShowString(1, 16, "AX",16,1);
	OLED_ShowString(1, 32, "AY",16,1);
	OLED_ShowString(1, 48, "AZ",16,1);
	
	OLED_ShowString(66, 16, "GX",16,1);
	OLED_ShowString(66, 32, "GY",16,1);
	OLED_ShowString(66, 48, "GZ",16,1);
	while(1)
	{
		MPU6050_GetData(&AX, &AY, &AZ, &GX, &GY, &GZ);		//获取MPU6050的数据
		OLED_ShowNum(20, 16, AX, 5,16,1);					//OLED显示数据
		OLED_ShowNum(20, 32, AY, 5,16,1);
		OLED_ShowNum(20, 48, AZ, 5,16,1);
		OLED_ShowNum(82, 16, GX, 5,16,1);
		OLED_ShowNum(82, 32, GY, 5,16,1);
		OLED_ShowNum(82, 48, GZ, 5,16,1);
		OLED_Refresh();
	}
}

