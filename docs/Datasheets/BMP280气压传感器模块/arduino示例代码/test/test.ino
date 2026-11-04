#include <SPI.h>

// 定义SPI引脚 - 可根据需要修改
#define HW611_SCK_PIN  18
#define HW611_MISO_PIN 19
#define HW611_MOSI_PIN 23
#define HW611_CS_PIN   5

// HW-611寄存器地址
#define HW611_ID_REG    0xD0
#define HW611_RESET     0xE0
#define HW611_CTRL_MEAS 0xF4
#define HW611_CONFIG    0xF5
#define HW611_PRESS_MSB 0xF7
#define HW611_TEMP_MSB  0xFA
#define HW611_CALIB00   0x88

// 校准数据结构
struct {
  uint16_t dig_T1;
  int16_t dig_T2;
  int16_t dig_T3;
  uint16_t dig_P1;
  int16_t dig_P2;
  int16_t dig_P3;
  int16_t dig_P4;
  int16_t dig_P5;
  int16_t dig_P6;
  int16_t dig_P7;
  int16_t dig_P8;
  int16_t dig_P9;
} hw611_cal;

int32_t t_fine; // 温度补偿中间变量

void setup() {
  Serial.begin(115200);
  
  // 初始化SPI
  SPI.begin(HW611_SCK_PIN, HW611_MISO_PIN, HW611_MOSI_PIN, HW611_CS_PIN);
  pinMode(HW611_CS_PIN, OUTPUT);
  digitalWrite(HW611_CS_PIN, HIGH);
  
  // 初始化HW-611
  if (!initHW611()) {
    Serial.println("未检测到HW-611传感器!");
    while(1);
  }
  
  // 读取校准数据
  readCalibrationData();
  
  Serial.println("HW-611传感器就绪");
  Serial.println("-----------------");
}

void loop() {
  // 读取温度和气压
  float temperature = readTemperature();
  float pressure = readPressure();
  
  // 打印结果
  Serial.print("温度: ");
  Serial.print(temperature, 1);
  Serial.print(" ℃\t");
  
  Serial.print("压强: ");
  Serial.print(pressure / 100.0, 2); // 转换为hPa
  Serial.println(" hPa");
  
  delay(1000);
}

bool initHW611() {
  // 检查设备ID (BMP280/BME280应为0x58/0x60)
  uint8_t id = readRegister(HW611_ID_REG);
  if (id != 0x58 && id != 0x60) {
    Serial.print("未知设备ID: 0x");
    Serial.println(id, HEX);
    return false;
  }
  
  // 软复位
  writeRegister(HW611_RESET, 0xB6);
  delay(100);
  
  // 配置传感器
  // 温度x1，气压x1，正常模式
  writeRegister(HW611_CTRL_MEAS, 0x3F);
  
  // 待机时间500ms，滤波器关闭
  writeRegister(HW611_CONFIG, 0x04);
  
  return true;
}

void readCalibrationData() {
  // 读取温度校准系数
  hw611_cal.dig_T1 = read16(HW611_CALIB00);
  hw611_cal.dig_T2 = readS16(HW611_CALIB00 + 2);
  hw611_cal.dig_T3 = readS16(HW611_CALIB00 + 4);
  
  // 读取气压校准系数
  hw611_cal.dig_P1 = read16(HW611_CALIB00 + 6);
  hw611_cal.dig_P2 = readS16(HW611_CALIB00 + 8);
  hw611_cal.dig_P3 = readS16(HW611_CALIB00 + 10);
  hw611_cal.dig_P4 = readS16(HW611_CALIB00 + 12);
  hw611_cal.dig_P5 = readS16(HW611_CALIB00 + 14);
  hw611_cal.dig_P6 = readS16(HW611_CALIB00 + 16);
  hw611_cal.dig_P7 = readS16(HW611_CALIB00 + 18);
  hw611_cal.dig_P8 = readS16(HW611_CALIB00 + 20);
  hw611_cal.dig_P9 = readS16(HW611_CALIB00 + 22);
}

float readTemperature() {
  int32_t var1, var2;
  
  int32_t adc_T = read24(HW611_TEMP_MSB);
  adc_T >>= 4;
  
  var1 = ((((adc_T >> 3) - ((int32_t)hw611_cal.dig_T1 << 1))) * 
         ((int32_t)hw611_cal.dig_T2)) >> 11;
         
  var2 = (((((adc_T >> 4) - ((int32_t)hw611_cal.dig_T1)) * 
          ((adc_T >> 4) - ((int32_t)hw611_cal.dig_T1))) >> 12) * 
          ((int32_t)hw611_cal.dig_T3)) >> 14;
  
  t_fine = var1 + var2;
  
  float T = (t_fine * 5 + 128) >> 8;
  return T / 100.0;
}

float readPressure() {
  int64_t var1, var2, p;
  
  int32_t adc_P = read24(HW611_PRESS_MSB);
  adc_P >>= 4;
  
  var1 = ((int64_t)t_fine) - 128000;
  var2 = var1 * var1 * (int64_t)hw611_cal.dig_P6;
  var2 = var2 + ((var1 * (int64_t)hw611_cal.dig_P5) << 17);
  var2 = var2 + (((int64_t)hw611_cal.dig_P4) << 35);
  var1 = ((var1 * var1 * (int64_t)hw611_cal.dig_P3) >> 8) + 
         ((var1 * (int64_t)hw611_cal.dig_P2) << 12);
  var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)hw611_cal.dig_P1) >> 33;
  
  if (var1 == 0) {
    return 0; // 避免除以零
  }
  
  p = 1048576 - adc_P;
  p = (((p << 31) - var2) * 3125) / var1;
  var1 = (((int64_t)hw611_cal.dig_P9) * (p >> 13) * (p >> 13)) >> 25;
  var2 = (((int64_t)hw611_cal.dig_P8) * p) >> 19;
  
  p = ((p + var1 + var2) >> 8) + (((int64_t)hw611_cal.dig_P7) << 4);
  
  return (float)p / 256.0;
}

// SPI辅助函数
uint8_t readRegister(uint8_t reg) {
  uint8_t value;
  digitalWrite(HW611_CS_PIN, LOW);
  SPI.transfer(reg | 0x80); // 设置读位
  value = SPI.transfer(0);
  digitalWrite(HW611_CS_PIN, HIGH);
  return value;
}

uint16_t read16(uint8_t reg) {
  uint16_t value;
  digitalWrite(HW611_CS_PIN, LOW);
  SPI.transfer(reg | 0x80);
  value = (SPI.transfer(0) << 8) | SPI.transfer(0);
  digitalWrite(HW611_CS_PIN, HIGH);
  return value;
}

int16_t readS16(uint8_t reg) {
  return (int16_t)read16(reg);
}

int32_t read24(uint8_t reg) {
  uint32_t value;
  digitalWrite(HW611_CS_PIN, LOW);
  SPI.transfer(reg | 0x80);
  value = (uint32_t)SPI.transfer(0) << 16;
  value |= (uint32_t)SPI.transfer(0) << 8;
  value |= SPI.transfer(0);
  digitalWrite(HW611_CS_PIN, HIGH);
  return value;
}

void writeRegister(uint8_t reg, uint8_t value) {
  digitalWrite(HW611_CS_PIN, LOW);
  SPI.transfer(reg & 0x7F); // 清除写位
  SPI.transfer(value);
  digitalWrite(HW611_CS_PIN, HIGH);
}