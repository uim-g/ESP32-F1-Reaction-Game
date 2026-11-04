
#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <string.h>

// ==================== 1. 用户设置 ====================
// 最终时间 = 原始时间 × 倍速系数 - 固定延迟
const float TIME_MULTIPLIER = 1.0f;       // 倍速补偿，1.0为不补偿
const float FIXED_COMPENSATION_MS = -80.0f; // 固定延迟补偿，单位ms
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
volatile bool pressPending = false, pressArmed = true;
volatile uint32_t pressTimeUs = 0;
uint32_t releasedAtMs = 0;
bool releaseTiming = false;

// 中断只记录按钮按下的时间，不在中断中绘制屏幕
void IRAM_ATTR onButtonPress() {
  if (!pressArmed) return;
  pressArmed = false;
  pressTimeUs = micros();
  pressPending = true;
}

// 从中断变量中安全读取一次按键事件
bool takeButtonPress(uint32_t &timestamp) {
  bool found = false;
  noInterrupts();
  if (pressPending) {
    timestamp = pressTimeUs;
    pressPending = false;
    found = true;
  }
  interrupts();
  return found;
}

// 按钮稳定松开30ms后，才能再次触发
void updateButtonRelease() {
  if (digitalRead(BUTTON_PIN) == HIGH) {
    if (!releaseTiming) {
      releaseTiming = true;
      releasedAtMs = millis();
    }
    if (millis() - releasedAtMs >= BUTTON_RELEASE_MS) {
      noInterrupts();
      if (!pressPending) pressArmed = true;
      interrupts();
    }
  } else {
    releaseTiming = false;
  }
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
  gameState = ST_RESULT;
  turnOffPhysicalLEDs();
  drawBottomOff();
  clearFooter();
  drawText("FALSE START!", 189, 2, ILI9341_RED);
  drawText("PRESS TO RESET", 218, 1);
  Serial.println("FALSE START!");
}

// 实体LED熄灭后立即记录起跑时刻，之后才更新TFT
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

// 熄灯后按键：计算反应时间
void finishReaction(uint32_t pressedUs) {
  int32_t elapsed = (int32_t)(pressedUs - lightsOutUs);
  if (elapsed < 0) {
    falseStart();
    return;
  }
  if ((uint32_t)elapsed >= REACTION_TIMEOUT_US) {
    gameState = ST_RESULT;
    showMessage("TIMEOUT!", ILI9341_RED);
    drawText("PRESS TO RESET", 219, 1);
    return;
  }
  finalRawUs = (uint32_t)elapsed;
  gameState = ST_RESULT; // 先停止计时状态，再显示固定成绩
  showResult(finalRawUs);
}

// 超过10秒没有按键
void reactionTimeout() {
  gameState = ST_RESULT;
  showMessage("TIMEOUT!", ILI9341_RED);
  drawText("PRESS TO RESET", 219, 1);
}

// 第三次按键：复位，等待下一次手动开始
void resetGame() {
  gameState = ST_IDLE;
  turnOffPhysicalLEDs();
  nextLight = 0;
  finalRawUs = 0;
  lightsOutUs = 0;
  selectedDebug = readMode();
  lastRawMode = selectedDebug;
  modeChangedMs = millis();
  drawUI();
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

  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), onButtonPress, FALLING);
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
        if (takeButtonPress(lastPress)) finishReaction(lastPress);
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
