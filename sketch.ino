#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <string.h>
// ==================== 1. 用户设置 ====================
// 最终时间 = 原始时间 × 倍速系数 - 固定延迟
const float TIME_MULTIPLIER = 1.0f;       // 倍速补偿，1.0为不补偿
const float FIXED_COMPENSATION_MS = -80.0f; // 保留现有实验参数：减去负数实际增加80ms；未校准时可设为0。
const uint32_t LIGHT_INTERVAL_MS = 800;  // 每颗灯点亮的间隔
const uint32_t WAIT_MIN_MS = 1000;       // 随机等待最短时间
const uint32_t WAIT_MAX_MS = 3000;       // 随机等待最长时间
const uint32_t REACTION_TIMEOUT_US = 10000000UL; // 10秒超时
const uint32_t DEBUG_REFRESH_MS = 100;   // Debug每100ms刷新一次
const uint32_t BUTTON_RELEASE_MS = 30;   // 按钮松开消抖
const uint32_t MODE_DEBOUNCE_MS = 40;    // 模式开关消抖
// ==================== 2. 引脚定义 ====================
#define TFT_CS 10
#define TFT_DC 9
#define TFT_RST 8
#define TFT_MOSI 11
#define TFT_SCK 12
#define BUTTON_PIN 16  // 按钮另一端接GND
#define MODE_PIN 17    // 开关公共端接GPIO17，一侧接GND
const int LED_PINS[5] = {4, 5, 6, 7, 15};
Adafruit_ILI9341 tft(TFT_CS, TFT_DC, TFT_RST);
// ==================== 3. UI参数 ====================
const int BOX_X[5] = {21, 79, 137, 195, 253};
const int LAMP_X[5] = {44, 102, 160, 218, 276};
const int BOX_Y = 72, BOX_W = 46, BOX_H = 82, BOX_R = 10;
const int TOP_Y = 96, BOTTOM_Y = 130, LAMP_R = 14;
const uint16_t COLOR_BG = ILI9341_BLACK;
const uint16_t COLOR_BOX = 0x2104;
const uint16_t COLOR_BORDER = 0x39E7;
const uint16_t COLOR_OFF = 0x4A49;
const uint16_t COLOR_ON = ILI9341_RED;
// ==================== 4. 游戏状态 ====================
enum GameState { ST_IDLE, ST_LIGHTING, ST_WAIT, ST_REACTION, ST_RESULT };
GameState gameState = ST_IDLE;
bool selectedDebug = false; // 开关选中的模式
bool roundDebug = false;    // 当前轮次锁定的模式
bool lastRawMode = false;
uint32_t modeChangedMs = 0;
uint32_t phaseStartMs = 0, randomWaitMs = 0;
uint32_t lightsOutUs = 0, finalRawUs = 0, lastDebugMs = 0;
int nextLight = 0;
// ==================== 5. 按钮中断与消抖 ====================
// 主循环和GPIO中断共用同一把锁，保证时间戳、待处理标志和重新布防作为整体更新。
portMUX_TYPE buttonMux = portMUX_INITIALIZER_UNLOCKED;
volatile bool pressPending = false, pressArmed = false;
volatile bool buttonReleased = false;
volatile uint32_t pressTimeUs = 0, lastButtonEdgeUs = 0;
// 同时监听按下和松开：即使主循环正在绘制TFT，也能记录释放抖动、重启稳定计时。
// ISR只读取引脚、保存时间与标志；不绘图、不打印、不等待，也不在ISR里执行游戏流程。
void IRAM_ATTR onButtonChange() {
  portENTER_CRITICAL_ISR(&buttonMux);
  bool released = digitalRead(BUTTON_PIN) == HIGH;
  uint32_t edgeUs = micros();
  buttonReleased = released;
  lastButtonEdgeUs = edgeUs; // 每次边沿都使旧稳定时间失效，包括未布防时的抖动。
  if (!released && pressArmed) {
    pressArmed = false; // 一个完整的按下/释放周期只接受第一次按下。
    pressTimeUs = edgeUs;
    pressPending = true;
  }
  portEXIT_CRITICAL_ISR(&buttonMux);
}
// 取出第一笔按下事件，消费后仍保持未布防，必须重新稳定松开才能再次触发。
// closeIfEmpty仅用于超时边界：若没有待处理事件，在同一临界区关门，防止迟到按键变成复位。
bool takeButtonPress(uint32_t &timestamp, bool closeIfEmpty = false) {
  bool found = false;
  portENTER_CRITICAL(&buttonMux);
  if (pressPending) {
    timestamp = pressTimeUs;
    pressPending = false;
    found = true;
  } else if (closeIfEmpty) {
    pressArmed = false;
  }
  portEXIT_CRITICAL(&buttonMux);
  return found;
}
// 进入结果页、复位或初始化时清除旧事件，并从当前时刻重新确认稳定释放。
// 结果绘制前后各调用一次：绘制期间的按键不作为复位；页面显示后须先松开30ms再按。
void restartButtonRelease() {
  portENTER_CRITICAL(&buttonMux);
  pressArmed = false;
  pressPending = false;
  buttonReleased = digitalRead(BUTTON_PIN) == HIGH;
  lastButtonEdgeUs = micros();
  portEXIT_CRITICAL(&buttonMux);
}
// 只有引脚为HIGH、最近边沿已过去30ms、且没有未消费事件时，才允许下次按下。
// 边沿由CHANGE中断记录，TFT绘制阻塞不会沿用上一轮的松开时间；无符号减法兼容计时回绕。
void updateButtonRelease() {
  portENTER_CRITICAL(&buttonMux);
  bool released = digitalRead(BUTTON_PIN) == HIGH;
  uint32_t nowUs = micros();
  if (released != buttonReleased) {
    buttonReleased = released;
    lastButtonEdgeUs = nowUs; // 中断尚未反映引脚变化时，保守地从本次采样重新计时。
  }
  if (released && !pressPending && (uint32_t)(nowUs - lastButtonEdgeUs) >= BUTTON_RELEASE_MS * 1000UL)
    pressArmed = true;
  portEXIT_CRITICAL(&buttonMux);
}
// ==================== 6. 时间计算 ====================
float rawMs(uint32_t us) {
  return us / 1000.0f;
}
// 倍速补偿与固定延迟补偿
float correctedMs(uint32_t us) {
  float result = rawMs(us) * TIME_MULTIPLIER - FIXED_COMPENSATION_MS;
  return result < 0 ? 0 : result;
}
// ==================== 7. 屏幕文字与计时 ====================
void drawText(const char* text, int y, int size, uint16_t color = ILI9341_WHITE) {
  tft.setTextSize(size);
  tft.setTextColor(color);
  int x = (320 - strlen(text) * 6 * size) / 2;
  tft.setCursor(x, y);
  tft.print(text);
}
void clearFooter() {
  tft.fillRect(0, 175, 320, 65, COLOR_BG);
}
void showMessage(const char* text, uint16_t color = ILI9341_WHITE) {
  clearFooter();
  drawText(text, 194, 2, color);
}
// Debug：显示原始时间RAW和补偿时间CAL
void drawDebugTime(uint32_t us) {
  char a[32], b[32];
  snprintf(a, sizeof(a), "RAW: %.1f ms", rawMs(us));
  snprintf(b, sizeof(b), "CAL: %.1f ms", correctedMs(us));
  tft.fillRect(15, 191, 290, 47, COLOR_BG);
  drawText(a, 194, 2);
  drawText(b, 216, 2);
}
void showLightsOut() {
  clearFooter();
  if (roundDebug) {
    drawText("LIGHTS OUT!", 179, 1);
    drawDebugTime(0);
  } else {
    drawText("GO!", 194, 2);
  }
}
// 显示最终成绩，此函数只执行一次
void showResult(uint32_t us) {
  char text[32];
  clearFooter();
  drawText("PRESS TO RESET", 179, 1);
  if (roundDebug) {
    snprintf(text, sizeof(text), "RAW: %.1f ms", rawMs(us));
    drawText(text, 194, 2);
    snprintf(text, sizeof(text), "CAL: %.1f ms", correctedMs(us));
    drawText(text, 216, 2);
  } else {
    snprintf(text, sizeof(text), "%.1f ms", correctedMs(us));
    drawText(text, 202, 2);
  }
  Serial.printf("RAW: %.3f ms | Multiplier: %.2f | Offset: %.2f ms | Final: %.3f ms\n",
                rawMs(us), TIME_MULTIPLIER, FIXED_COMPENSATION_MS, correctedMs(us));
}
// ==================== 8. 模式开关 ====================
// GPIO17为LOW时是Debug，HIGH时是Normal
bool readMode() {
  return digitalRead(MODE_PIN) == LOW;
}
void drawModeLabel(bool debug) {
  tft.fillRect(255, 3, 65, 14, COLOR_BG);
  tft.setTextSize(1);
  tft.setTextColor(debug ? ILI9341_YELLOW : ILI9341_CYAN);
  tft.setCursor(debug ? 280 : 274, 5);
  tft.print(debug ? "DEBUG" : "NORMAL");
}
// 游戏中途切换模式，不影响当前轮次
void updateMode() {
  bool raw = readMode();
  if (raw != lastRawMode) {
    lastRawMode = raw;
    modeChangedMs = millis();
  }
  if (millis() - modeChangedMs >= MODE_DEBOUNCE_MS && raw != selectedDebug) {
    selectedDebug = raw;
    if (gameState == ST_IDLE) drawModeLabel(selectedDebug);
  }
}
// ==================== 9. LED与UI绘制 ====================
void turnOnLamp(int i) {
  digitalWrite(LED_PINS[i], HIGH);
  tft.fillCircle(LAMP_X[i], BOTTOM_Y, LAMP_R, COLOR_ON);
}
void turnOffPhysicalLEDs() {
  for (int i = 0; i < 5; i++) digitalWrite(LED_PINS[i], LOW);
}
void drawBottomOff() {
  for (int i = 0; i < 5; i++)
    tft.fillCircle(LAMP_X[i], BOTTOM_Y, LAMP_R, COLOR_OFF);
}
// 绘制五个独立灯箱，上排永远是灰色
void drawUI() {
  tft.fillScreen(COLOR_BG);
  drawText("F1 REACTION", 28, 3);
  drawText("WATCH PHYSICAL LEDS", 56, 1, ILI9341_YELLOW);
  drawModeLabel(selectedDebug);
  for (int i = 0; i < 5; i++) {
    tft.fillRoundRect(BOX_X[i], BOX_Y, BOX_W, BOX_H, BOX_R, COLOR_BOX);
    tft.drawRoundRect(BOX_X[i], BOX_Y, BOX_W, BOX_H, BOX_R, COLOR_BORDER);
    tft.fillCircle(LAMP_X[i], TOP_Y, LAMP_R, COLOR_OFF);
    tft.fillCircle(LAMP_X[i], BOTTOM_Y, LAMP_R, COLOR_OFF);
  }
  showMessage("PRESS TO START");
}
// ==================== 10. 游戏流程 ====================
// 第一次按键：开始游戏并锁定模式
void beginRound() {
  roundDebug = selectedDebug;
  nextLight = 0;
  finalRawUs = 0;
  lightsOutUs = 0;
  turnOffPhysicalLEDs();
  drawBottomOff();
  gameState = ST_LIGHTING;
  phaseStartMs = millis();
  drawModeLabel(roundDebug);
  showMessage("WAIT...");
}
// 熄灯前按键：抢跑
void falseStart() {
  restartButtonRelease();
  gameState = ST_RESULT;
  turnOffPhysicalLEDs();
  drawBottomOff();
  clearFooter();
  drawText("FALSE START!", 189, 2, ILI9341_RED);
  drawText("PRESS TO RESET", 218, 1);
  Serial.println("FALSE START!");
  restartButtonRelease();
}
// 正式起跑信号是实体LED熄灭；TFT灯和GO只是随后更新的提示，不是独立计时基准。
// 不用屏幕绘图耗时修正起点，也不假设固定80ms能代表真实硬件显示延迟。
void lightsOut() {
  uint32_t pendingUs;
  if (takeButtonPress(pendingUs) || digitalRead(BUTTON_PIN) == LOW) {
    falseStart();
    return;
  }
  turnOffPhysicalLEDs();
  lightsOutUs = micros();
  gameState = ST_REACTION;
  drawBottomOff();
  showLightsOut();
  lastDebugMs = millis();
}
// 超过10秒没有有效反应，统一显示超时；按下恰好等于10秒也算超时。
void reactionTimeout() {
  restartButtonRelease();
  gameState = ST_RESULT;
  showMessage("TIMEOUT!", ILI9341_RED);
  drawText("PRESS TO RESET", 219, 1);
  Serial.println("TIMEOUT!");
  restartButtonRelease();
}
// 熄灯后按键：按中断保存的时间计算，而不是按主循环处理/绘图结束时间计算
void finishReaction(uint32_t pressedUs) {
  int32_t elapsed = (int32_t)(pressedUs - lightsOutUs);
  if (elapsed < 0) {
    falseStart();
    return;
  }
  if ((uint32_t)elapsed >= REACTION_TIMEOUT_US) {
    reactionTimeout();
    return;
  }
  finalRawUs = (uint32_t)elapsed;
  restartButtonRelease();
  gameState = ST_RESULT; // 先停止计时并关闭输入，再显示固定成绩。
  showResult(finalRawUs);
  restartButtonRelease();
}
// 第三次按键：复位，等待下一次手动开始
void resetGame() {
  restartButtonRelease();
  gameState = ST_IDLE;
  turnOffPhysicalLEDs();
  nextLight = 0;
  finalRawUs = 0;
  lightsOutUs = 0;
  selectedDebug = readMode();
  lastRawMode = selectedDebug;
  modeChangedMs = millis();
  // 标题、灯箱和上排灰灯不变，只刷新模式、下排灯和提示，避免每次复位全屏闪烁。
  drawModeLabel(selectedDebug);
  drawBottomOff();
  showMessage("PRESS TO START");
  restartButtonRelease();
}
// 根据游戏状态决定按钮的功能
void handleButton(uint32_t pressedUs) {
  switch (gameState) {
    case ST_IDLE: beginRound(); break;
    case ST_LIGHTING:
    case ST_WAIT: falseStart(); break;
    case ST_REACTION: finishReaction(pressedUs); break;
    case ST_RESULT: resetGame(); break;
  }
}
// ==================== 11. 初始化 ====================
void setup() {
  Serial.begin(115200);
  for (int i = 0; i < 5; i++) {
    pinMode(LED_PINS[i], OUTPUT);
    digitalWrite(LED_PINS[i], LOW);
  }
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(MODE_PIN, INPUT_PULLUP);
  selectedDebug = readMode();
  roundDebug = selectedDebug;
  lastRawMode = selectedDebug;
  modeChangedMs = millis();
  SPI.begin(TFT_SCK, -1, TFT_MOSI, TFT_CS);
  tft.begin();
  tft.setRotation(1);
  tft.setTextWrap(false);
  drawUI();
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), onButtonChange, CHANGE);
  restartButtonRelease(); // 上电按住按钮不会立即开始，须先稳定松开再按。
  randomSeed(micros());
}
// ==================== 12. 主循环 ====================
void loop() {
  updateButtonRelease();
  updateMode();
  // 优先处理按键
  uint32_t pressedUs;
  if (takeButtonPress(pressedUs)) handleButton(pressedUs);
  uint32_t now = millis();
  switch (gameState) {
    case ST_IDLE:
      // 等待按键，不自动开始
      break;
    case ST_LIGHTING:
      if (now - phaseStartMs >= LIGHT_INTERVAL_MS) {
        turnOnLamp(nextLight++);
        phaseStartMs = millis();
        if (nextLight >= 5) {
          gameState = ST_WAIT;
          randomWaitMs = random(WAIT_MIN_MS, WAIT_MAX_MS + 1);
          phaseStartMs = millis();
        }
      }
      break;
    case ST_WAIT:
      if (now - phaseStartMs >= randomWaitMs) lightsOut();
      break;
    case ST_REACTION: {
      uint32_t elapsed = (uint32_t)(micros() - lightsOutUs);
      if (elapsed >= REACTION_TIMEOUT_US) {
        uint32_t lastPress;
        // 先消费已记录的反应：即使处理时已超时，截止前按下仍按原时间计算。
        // 没有事件时原子地关闭输入，随后到达的迟到按键不能越过边界变成复位。
        if (takeButtonPress(lastPress, true)) finishReaction(lastPress);
        else reactionTimeout();
        break;
      }
      // Debug只在熄灯后刷新，按键完成后立即停止
      if (roundDebug && now - lastDebugMs >= DEBUG_REFRESH_MS) {
        lastDebugMs = millis();
        drawDebugTime(elapsed);
      }
      break;
    }
    case ST_RESULT:
      // 不刷新计时、不自动复位
      break;
  }
  delay(1);
}
