#include <Wire.h>               // I2C 通信库
#include <SSD1306.h>            // OLED 驱动库

// OLED 配置（I2C 地址 0x3C 常用）
SSD1306 display(0x3C, 4, 5);    // SDA=GPIO4, SCL=GPIO5

// 按键引脚定义
const int BUTTON_ADD = 6;       // 加键
const int BUTTON_SUB = 7;       // 减键
const int BUTTON_SW = 9;        // 开关键
const int SWITCH_MOS = 8;       // MOS开关，控制咖啡机

// 定时器与功能变量
unsigned long timerInterval = 1000;  // 定时器周期（1秒，可修改）
unsigned long lastTimer = 0;         // 上一次定时器触发时间
int timerCount = 0;                  // 计时变量（秒数）
bool timerEnable = false;            // 开关状态（true=启动，false=停止）

// 输出控制变量
bool outputState = false;     // 输出状态
unsigned long outputStartTime = 0;  // 输出开始时间
unsigned long OUTPUT_DURATION = 30000;  // 输出持续时间(30秒)

void setup() {
  // 初始化串口（调试用）
  Serial.begin(9600);
  Serial.println("Hello from ESP32-C3!");

  // 初始化 OLED
  display.init();
  display.clear();
  display.setFont(ArialMT_Plain_16);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(64, 32, "Timer Demo");
  display.display();

  // 初始化按键（开启内部上拉）
  pinMode(BUTTON_ADD, INPUT_PULLUP);
  pinMode(BUTTON_SUB, INPUT_PULLUP);
  pinMode(BUTTON_SW, INPUT_PULLUP);
  pinMode(SWITCH_MOS, OUTPUT);
  digitalWrite(SWITCH_MOS, LOW);
}

// 加键处理（长按可连续加，需消抖）
void handleAdd() {
  if (timerEnable) {
    OUTPUT_DURATION -= 1000;       // 减 1 秒（最小 1 秒）
    if (OUTPUT_DURATION < 1000) {  // 防止负数
      OUTPUT_DURATION = 1000;
    }
  }
}

// 减键处理（长按可连续减，需消抖）
void handleSub() {
  if (timerEnable) {
    OUTPUT_DURATION += 1000;       // 加 1 秒
  }
}

// 开关键处理（启停定时器）
void handleSw() {
  timerEnable = !timerEnable;
  if (!timerEnable) {            // 停止时清零计时
    timerCount = 0;
  }
  outputState = !outputState;
  digitalWrite(SWITCH_MOS, outputState);
}

// 控制输出的函数
void handleOutput() {
  // outputState = false;
  handleSw();
  digitalWrite(SWITCH_MOS, outputState);
  Serial.println("Output turned OFF after 30 seconds");
  // timerEnable = !timerEnable;
}

void loop() {
  // 轮询按键（消抖处理）
  if (digitalRead(BUTTON_ADD) == LOW) {
    delay(50);                    // 消抖
    while (digitalRead(BUTTON_ADD) == LOW); // 等待松开
    handleAdd();
  }
  if (digitalRead(BUTTON_SUB) == LOW) {
    delay(50);
    while (digitalRead(BUTTON_SUB) == LOW);
    handleSub();
  }
  if (digitalRead(BUTTON_SW) == LOW) {
    delay(50);
    while (digitalRead(BUTTON_SW) == LOW);
    handleSw();
  }

  // 软件定时器逻辑（秒级精度）
  if (millis() - lastTimer > timerInterval) {
    lastTimer = millis();         // 更新触发时间
    if (timerEnable) {
      timerCount++;               // 计时+1秒
      Serial.print("Timer: ");
      Serial.println(timerCount);
    }
  }

    // 检查输出是否需要关闭（30秒后）
  Serial.println(timerCount);
  if (outputState && (timerCount * 1000 >= OUTPUT_DURATION)) {
    handleOutput();
  }

  // OLED 显示更新
  display.clear();
  display.drawString(64, 0, "Interval: " + String(timerInterval/1000) + "s");
  display.drawString(64, 20, "Count: " + String(timerCount) + "s");
  char stateText[20];
  sprintf(stateText, "State: %s", timerEnable ? "RUN" : "STOP");
  display.drawString(64, 40, stateText); 
  display.display();

  delay(100); // 主循环延时，降低功耗
}