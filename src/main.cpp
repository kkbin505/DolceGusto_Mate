#include <Arduino.h>
#include <Wire.h>               // I2C 通信库
#include <SSD1306.h>            // OLED 驱动库

#include <WIFI.h>
#include <HTTPClient.h>
// time
#include <time.h>                       // time() ctime()

#include <JsonListener.h>
#include <ArduinoOTA.h> ///

#include "OLEDDisplayUi.h"
//#include "OpenWeatherMapCurrent.h"
//#include "OpenWeatherMapForecast.h"
//#include "OpenWeatherMapOneCall.h" // uses OneCall API of OpenWeatherMap
#include "OpenMeteoOneCall.h" // uses OneCall API of Open-Meteo
#include "WeatherStationFonts.h"
#include "WeatherStationImages.h"
// Display Settings
const int I2C_DISPLAY_ADDRESS = 0x3c;

String OPEN_WEATHER_MAP_APP_ID = "xxx"; //https://home.openweathermap.org/api_keys


// OLED 配置（I2C 地址 0x3C 常用）
SSD1306 display(I2C_DISPLAY_ADDRESS, 4, 5);    // SDA=GPIO4, SCL=GPIO5
OLEDDisplayUi ui( &display );

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

// 加入待机时间显示功能
enum UiMode {
  UI_ACTIVE,    // 显示完整界面（工作中）
  UI_STANDBY    // 显示大时间（待机）
};
UiMode currentUiMode = UI_ACTIVE;

bool justWokeUp = false;  // 标志：刚从待机唤醒

unsigned long lastActivityTime = 0;
const unsigned long standbyTimeout = 600000;  // 60秒无操作进入待机

/***************************
 * Begin Settings
 **************************/

// WIFI
const char* ESP_HOST_NAME = "ESP-32 C3"; //AMA
const char* WIFI_SSID = "xxx"; // 改为你的实际WiFi名称和密码
const char* WIFI_PWD = "xxx";

// 中国上海时区设置
// const char* ntpServer = "pool.ntp.org";       // 国际公共
// const char* ntpServer = "ntp.aliyun.com";  // 阿里云
const char* ntpServer = "cn.ntp.org.cn";   // 中国 NTP
const long gmtOffset_sec = 8 * 3600;  // UTC+8
const int daylightOffset_sec = 0;     // 无夏令时

/*
Go to https://www.latlong.net/ and search for a location. Go through the
result set and select the entry closest to the actual location you want to display
data for. Use Latitude and Longitude values here.
 */
//Maulburg, DE
float OPEN_WEATHER_MAP_LOCATTION_LAT = 31.2304;  // China,Shanghai
float OPEN_WEATHER_MAP_LOCATTION_LON = 121.4737; // China,Shanghai

// Pick a language code from this list:
// Arabic - ar, Bulgarian - bg, Catalan - ca, Czech - cz, German - de, Greek - el,
// English - en, Persian (Farsi) - fa, Finnish - fi, French - fr, Galician - gl,
// Croatian - hr, Hungarian - hu, Italian - it, Japanese - ja, Korean - kr,
// Latvian - la, Lithuanian - lt, Macedonian - mk, Dutch - nl, Polish - pl,
// Portuguese - pt, Romanian - ro, Russian - ru, Swedish - se, Slovak - sk,
// Slovenian - sl, Spanish - es, Turkish - tr, Ukrainian - ua, Vietnamese - vi,
// Chinese Simplified - zh_cn, Chinese Traditional - zh_tw.
String OPEN_WEATHER_MAP_LANGUAGE = "zh_cn"; //"ru"; //"de"; //AMA for the name under the weather icon 
const uint8_t MAX_FORECASTS = 8; //"One Call API" gives 8 days

const boolean IS_METRIC = true;

// Adjust according to your language
const String WDAY_NAMES[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
//const String WDAY_NAMES[] = {"ВС", "ПН", "ВТ", "СР", "ЧТ", "ПТ", "СБ"}; //AMA Did not work, nothing is displayed
//const String MONTH_NAMES[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
const String WIND_NAMES[] = {"N","NNE","NE","ENE","E","ESE","SE","SSE","S","SSW","SW","WSW","W","WNW","NW","NNW","N"};

OpenWeatherMapOneCallData openWeatherMapOneCallData; //AMA use OneCall API
OpenWeatherMapOneCall oneCallClient; //AMA 

/*#define TZ_MN           ((TZ)*60)
#define TZ_SEC          ((TZ)*3600)
#define DST_SEC         ((DST_MN)*60)*/
//time_t now;

const int UPDATE_INTERVAL_SECS = 15 * 60; //15 min // open-meteo updates data every 15 minutes, at: :00, :15, :30, :45
bool readyForWeatherUpdate = false; // flag changed in the ticker function every UPDATE_INTERVAL_SECS
long timeSinceLastWUpdate = 0;
//String lastUpdate = "--";

//declaring prototypes
void drawProgress(OLEDDisplay *display, int percentage, String label);
void updateData(OLEDDisplay *display);
void drawDateTime(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawCurrentWeather(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawForecast(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawForecast2(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y); //AMA
void drawHourly(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawHourly2(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y); //AMA
// void drawHourly3(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y); //AMA
void drawCurrentDetails(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y); //AMA
void drawForecastDetails(OLEDDisplay *display, int x, int y, int dayIndex);
void drawHeaderOverlay1(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawHeaderOverlay2(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void setReadyForWeatherUpdate();
void drawHourlyDetails(OLEDDisplay *display, int x, int y, int hourIndex);

FrameCallback frames[] = { drawCurrentWeather, drawCurrentDetails, drawHourly, drawHourly2, drawCurrentWeather, drawForecast, drawForecast2 }; //AMA
int numberOfFrames = 7;

void printLocalTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("⛔️ 无法获取时间");
    return;
  }
  char timeStr[9]; // HH:MM:SS 共8位，预留1位结束符
  strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
  Serial.print("🕒 当前时间：");
  Serial.println(timeStr);
}

String getWiFiStatusString(wl_status_t status) {
  switch (status) {
    case WL_IDLE_STATUS: return "Idle";
    case WL_NO_SSID_AVAIL: return "No SSID";
    case WL_SCAN_COMPLETED: return "Scan Done";
    case WL_CONNECTED: return "Connected";
    case WL_CONNECT_FAILED: return "Failed";
    case WL_CONNECTION_LOST: return "Lost";
    case WL_DISCONNECTED: return "Disconnected";
    default: return "Unknown";
  }
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
void handleSwitch(bool autoOff = false) {
  unsigned long now = millis();

  // 如果当前是待机状态，先唤醒 UI，不启用计时
  if (currentUiMode == UI_STANDBY) {
    currentUiMode = UI_ACTIVE;
    lastActivityTime = now;
    justWokeUp = true;
    Serial.println("🌞 Woke up from standby.");
    return;
  }

  // 如果刚唤醒，忽略这次按键（防止误触）
  if (justWokeUp) {
    justWokeUp = false;  // 清除标志
    Serial.println("⏰ Wake-up confirmed, waiting for next press...");
    return;
  }

  // ✅ 只在真正处理逻辑时，才更新时间
  lastActivityTime = now;

  // 启停定时器
  timerEnable = !timerEnable;

  if (!timerEnable) {
    timerCount = 0;
  }

  outputState = timerEnable;
  digitalWrite(SWITCH_MOS, outputState);

  if (autoOff) {
    Serial.println("⚠️ Output turned OFF after timeout.");
  } else {
    Serial.println(timerEnable ? "▶️ Timer started" : "⏹ Timer stopped");
  }
}


void updateDisplay() {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);

  if (currentUiMode == UI_ACTIVE) {
    display.setFont(ArialMT_Plain_10);  // 小字体

    display.drawString(64, 4,  "DolceGusto Mate");
    display.drawString(64, 20, "Count down: " + String(OUTPUT_DURATION/1000-timerCount) + "s");

    char stateText[20];
    sprintf(stateText, "State: %s", timerEnable ? "RUN" : "STOP");
    display.drawString(64, 36, stateText);

    struct tm timeinfo;
    if (WiFi.status() == WL_CONNECTED) {
      if (getLocalTime(&timeinfo)) {
        char timeStr[16];
        strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
        display.drawString(64, 52, timeStr);
      } else {
        display.drawString(64, 52, "Time Error");
      }
    }else{
      display.drawString(64, 52, "Off Line");
    }
  } else if (currentUiMode == UI_STANDBY) {
    display.setFont(ArialMT_Plain_24);  // 大字体

    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      char timeStr[16];
      strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
      display.drawString(64, 20, timeStr);  // 居中显示
    } else {
      display.drawString(64, 20, "Time Err");
    }
  }

  display.display();
}


void setup() {
  // 初始化串口（调试用）
  Serial.begin(115200);
  delay(1000); // 给 USB CDC 初始化一点时间
  Serial.println("Hello from ESP32-C3!");

  // 初始化 OLED
  display.init();
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(64, 10, "DolceGusto Mate");
  display.drawString(64, 30, "Connecting to WiFi");
  display.display();

  // 初始化按键（开启内部上拉）
  pinMode(BUTTON_ADD, INPUT_PULLUP);
  pinMode(BUTTON_SUB, INPUT_PULLUP);
  pinMode(BUTTON_SW, INPUT_PULLUP);
  pinMode(SWITCH_MOS, OUTPUT);
  digitalWrite(SWITCH_MOS, LOW);

  // 启动 WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PWD);
  WiFi.setSleep(false);
  delay(1000);
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);

  int counter = 0;
  while (WiFi.status() != WL_CONNECTED && counter <30) {
    delay(500);
    //Serial.print(".");
    display.clear();
    display.drawString(64, 10, "Connecting to WiFi"); //commented out to avoid oled burn-in if wifi is not available
    display.drawXbm(46, 40, 8, 8, counter % 3 == 0 ? activeSymbole : inactiveSymbole);
    display.drawXbm(60, 40, 8, 8, counter % 3 == 1 ? activeSymbole : inactiveSymbole);
    display.drawXbm(74, 40, 8, 8, counter % 3 == 2 ? activeSymbole : inactiveSymbole);
    display.display();
    counter++;
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("✅ Connected to WiFi!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Signal strength (RSSI): ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
  } else {
    Serial.println("❌ Failed to connect to WiFi.");
    Serial.print("Status: ");
    Serial.println(getWiFiStatusString(WiFi.status()));
  }
  // Get time from network time service
  // configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  configTime(gmtOffset_sec, daylightOffset_sec,  "pool.ntp.org","0.cn.pool.ntp.org","1.cn.pool.ntp.org");

  Serial.print("⏳ 等待时间同步");
  struct tm timeinfo;
  int retry = 0;
  const int retry_count = 20;
  setenv("TZ", "CST-8", 1);          // Zeitzone MEZ setzen //https://www.mikrocontroller.net/topic/479624
  while (!getLocalTime(&timeinfo) && retry < retry_count && !time(nullptr)) {
    Serial.print(".");
    display.clear();
    display.drawString(64, 10, "Connecting to WiFi"); //commented out to avoid oled burn-in if wifi is not available
    display.drawXbm(46, 40, 8, 8, counter % 3 == 0 ? activeSymbole : inactiveSymbole);
    display.drawXbm(60, 40, 8, 8, counter % 3 == 1 ? activeSymbole : inactiveSymbole);
    display.drawXbm(74, 40, 8, 8, counter % 3 == 2 ? activeSymbole : inactiveSymbole);
    display.display();
    delay(500);
    retry++;
  }
  Serial.println();

  if (retry < retry_count) {
    Serial.println("✅ 时间同步成功");
    Serial.println(&timeinfo, "🕒 当前时间：%Y-%m-%d %H:%M:%S");
  } else {
    Serial.println("❌ 时间同步失败");
    }

 
  // while (!time(nullptr)) // vorsichtshalber auf die Initialisierund der Lib warten
  // {
  //   //Serial.print(".");
  //   //display.clear();
  //   //display.drawString(64, 10, "Getting time");
  //   //display.display();
  //   delay(500);
  // }
  //Serial.println("OK");
  delay(500); // Es kann einen Moment dauern, bis man die erste NTP-Zeit hat, solange bekommt man noch eine ungültige Zeit
  
  ///ui.setTargetFPS(30);
  ui.setTargetFPS(1); // updates display every second to show seconds counting

  ui.setActiveSymbol(activeSymbole);
  ui.setInactiveSymbol(inactiveSymbole);

  // You can change this to
  // TOP, LEFT, BOTTOM, RIGHT
  ui.setIndicatorPosition(BOTTOM);

  // Defines where the first frame is located in the bar.
  ui.setIndicatorDirection(LEFT_RIGHT);

  //https://github.com/ThingPulse/esp8266-oled-ssd1306/blob/master/README.md
  ui.disableAllIndicators(); //disableIndicator(); //AMA

  // You can change the transition that is used
  // SLIDE_LEFT, SLIDE_RIGHT, SLIDE_UP, SLIDE_DOWN
  ui.setFrameAnimation(SLIDE_LEFT); ///SLIDE_LEFT

  ui.setFrames(frames, numberOfFrames);
  ui.setTimePerFrame(2000); //6000 //5000 //AMA (time in ms) Set the approx. time a frame is displayed (incl. transition)
  ui.setTimePerTransition(0); //100 //0 //AMA (time in ms) Set the approx. time a transition will take
  //ui.disableAutoTransition(); //AMA do not slide my only frame
  
  //ui.setOverlays(overlays, numberOfOverlays);

  // Inital UI takes care of initalising the display too.
  ui.init();

  display.setContrast(60); /// 70 80 90 120 60
  //display.setBrightness(100); /// 
  ///display.flipScreenVertically(); //AMA // Turn the display upside down

  //Serial.println("");

  updateData(&display);
    
  // ArduinoOTA.begin();     // enable to receive update/uploade firmware via Wifi OTA
}

void loop() {

  unsigned long now = millis();
  // 判断是否应该进入待机
  if (currentUiMode == UI_ACTIVE && (now - lastActivityTime > standbyTimeout)) {
    currentUiMode = UI_STANDBY;
  }
  // currentUiMode = UI_STANDBY;


  if (digitalRead(BUTTON_SW) == LOW) {
    delay(50);
    while (digitalRead(BUTTON_SW) == LOW);
    handleSwitch(); // 手动按键启停
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
    handleSwitch(true); // 超时自动关闭，带日志

  }
 


  // display.display();
  updateDisplay();

  delay(100); // 主循环延时，降低功耗

  if (WiFi.status() == WL_CONNECTED) {
  Serial.println("WiFi status: CONNECTED");
  Serial.println("Local IP: " + WiFi.localIP().toString());
  Serial.println("DNS: " + WiFi.dnsIP().toString());
  printLocalTime();  // ✅ 仅在 WiFi 连接成功时调用
} else {
  Serial.println("WiFi status: NOT CONNECTED");
}


}

void updateData(OLEDDisplay *display) {
  //drawProgress(display, 10, "Updating time...");
  ///drawProgress(display, 30, "Updating weather...");
/*  currentWeatherClient.setMetric(IS_METRIC);
  currentWeatherClient.setLanguage(OPEN_WEATHER_MAP_LANGUAGE);
  currentWeatherClient.updateCurrentById(&currentWeather, OPEN_WEATHER_MAP_APP_ID, OPEN_WEATHER_MAP_LOCATION_ID);
  drawProgress(display, 50, "Updating forecasts...");
  forecastClient.setMetric(IS_METRIC);
  forecastClient.setLanguage(OPEN_WEATHER_MAP_LANGUAGE);
  uint8_t allowedHours[] = {12};
  forecastClient.setAllowedHours(allowedHours, sizeof(allowedHours));
  forecastClient.updateForecastsById(forecasts, OPEN_WEATHER_MAP_APP_ID, OPEN_WEATHER_MAP_LOCATION_ID, MAX_FORECASTS);
*/
//oneCall
  oneCallClient.setMetric(IS_METRIC);
  oneCallClient.setLanguage(OPEN_WEATHER_MAP_LANGUAGE);
  oneCallClient.update(&openWeatherMapOneCallData, OPEN_WEATHER_MAP_APP_ID, OPEN_WEATHER_MAP_LOCATTION_LAT, OPEN_WEATHER_MAP_LOCATTION_LON);
  /*OpenWeatherMapOneCall *oneCallClient = new OpenWeatherMapOneCall();
  oneCallClient->setMetric(IS_METRIC);
  oneCallClient->setLanguage(OPEN_WEATHER_MAP_LANGUAGE);
  oneCallClient->update(&openWeatherMapOneCallData, OPEN_WEATHER_MAP_APP_ID, OPEN_WEATHER_MAP_LOCATTION_LAT, OPEN_WEATHER_MAP_LOCATTION_LON);
  delete oneCallClient;
  oneCallClient = nullptr;*/
  
  readyForWeatherUpdate = false;
  ///drawProgress(display, 100, "Done");
  ///delay(500);
  delay(0); //share time
}

void drawCurrentWeather(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  time_t now = time(nullptr);
  struct tm* timeInfo;
  timeInfo = localtime(&now);
  char buff[16];

  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_16); //24
  //sprintf_P(buff, PSTR("%02d:%02d"), timeInfo->tm_hour, timeInfo->tm_min);
  sprintf_P(buff, PSTR("%02d:%02d:%02d"), timeInfo->tm_hour, timeInfo->tm_min, timeInfo->tm_sec);
  display->drawString(x, 36 + y, String(buff));

  display->setFont(Meteocons_Plain_36); //21
  display->drawString(x, y, openWeatherMapOneCallData.current.weatherIconMeteoCon);
//  display->setFont(ArialMT_Plain_16);
//  String temp = String(openWeatherMapOneCallData.current.temp, 1) + "°"; //(IS_METRIC ? "°C" : "°F"); //,1
//  display->drawString(26 + x, 34 + y, temp); //26+x, 24+
  display->setFont(ArialMT_Plain_16); //24
  String temp = String(openWeatherMapOneCallData.current.temp, 1) + "°"; //(IS_METRIC ? "°C" : "°F"); //,1
  display->drawString(42 + x, 0 + y, temp); 
  temp = String(openWeatherMapOneCallData.current.humidity) + " %";
  display->drawString(42 + x, 18 + y, temp); 
    
  display->setFont(ArialMT_Plain_16);
  display->setTextAlignment(TEXT_ALIGN_RIGHT);
//  temp = String(bme.readPressure()/100.0F +41.0f, 0) + "hPa"; //+41 +42 //attention: corrections of BME280 readings here
  // // temp = String(bme.readPressure()/100.0F +41.0f, 0); //+ "hPa"; //+41 +42 //attention: corrections of BME280 readings here
  // display->drawString(128 - 22 + x, 36 + y, temp); //128+x
  // display->setFont(ArialMT_Plain_10);
  // display->drawString(128 + x, 41 + y, "hPa"); //in small font

  //drawHeaderOverlay2(display, state, x, y); //footer string
  drawHeaderOverlay1(display, state, x, y); //footer string
}

void drawForecast(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  drawForecastDetails(display, x, y, 0);
  //display->drawHorizontalLine(0, 12, 32); //underline the current day
  drawForecastDetails(display, x + 32, y, 1); //AMA show 4 days instead of 3
  drawForecastDetails(display, x + 64, y, 2); 
  drawForecastDetails(display, x + 96, y, 3); 
  //no footer string
}

//AMA
void drawForecast2(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  drawForecastDetails(display, x,      y, 4);
  drawForecastDetails(display, x + 32, y, 5);
  drawForecastDetails(display, x + 64, y, 6); 
  drawForecastDetails(display, x + 96, y, 7); 
  //no footer string
}

void drawHourly(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  drawHourlyDetails(display, x, y, 0);
  drawHourlyDetails(display, x + 32, y, 1); 
  drawHourlyDetails(display, x + 64, y, 2); 
  drawHourlyDetails(display, x + 96, y, 3); 
  drawHeaderOverlay1(display, state, x, y); //footer string
}

void drawHourly2(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  drawHourlyDetails(display, x,      y, 4);
  drawHourlyDetails(display, x + 32, y, 5); //5  7
  drawHourlyDetails(display, x + 64, y, 6); //6  10
  drawHourlyDetails(display, x + 96, y, 7); //7  13
  drawHeaderOverlay1(display, state, x, y); //footer string
}


/*******************************************/
// Daily Forecast Details
/*******************************************/
void drawForecastDetails(OLEDDisplay *display, int x, int y, int dayIndex) {
  time_t observationTimestamp = openWeatherMapOneCallData.daily[dayIndex].dt; //forecasts[dayIndex].observationTime;
  struct tm* timeInfo;
  observationTimestamp += 3600; // 1 hour in s to avoid the wrong days by localtime() when the time is changed from summer time to winter time
  timeInfo = localtime(&observationTimestamp);
  //Serial.print("dayIndex = ");Serial.println(dayIndex);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  /*if (dayIndex == 0) {
    display->fillRect( x+2,  y+1,  29,  11); // fill white (x,  y,  width,  height)
    display->setColor(BLACK);
  }*/
  display->drawString(x + 16, y, WDAY_NAMES[timeInfo->tm_wday]);
  //Serial.print("WDAY_NAMES = ");Serial.println(WDAY_NAMES[timeInfo->tm_wday]);
  display->setColor(WHITE);
  if (dayIndex == 0) display->drawHorizontalLine(x+2, y+12, 29); //line under the current day

  // white rectangular around SAT and SUN
  if ((timeInfo->tm_wday == 0) || (timeInfo->tm_wday == 6)) {
    display->drawRect( x+2,  y+1,  29,  11); // fill white (x,  y,  width,  height)
  }
  
  display->setFont(Meteocons_Plain_21);
  display->drawString(x + 16, y + 15, openWeatherMapOneCallData.daily[dayIndex].weatherIconMeteoCon); //y+14
  String tempMin = String(openWeatherMapOneCallData.daily[dayIndex].tempMin, 0)+"°";// + (IS_METRIC ? "°C" : "°F");
  String tempMax = String(openWeatherMapOneCallData.daily[dayIndex].tempMax, 0)+"°";// + (IS_METRIC ? "°C" : "°F");
  String rain_prob = String(openWeatherMapOneCallData.daily[dayIndex].rain_prob)+"%"; // rain_probability_percentage
  display->setFont(ArialMT_Plain_10);
  //String temps = tempMax+"/"+tempMin+"°";
  //display->drawString(x + 16, y + 38, temps); //y+36
  display->drawString(x + 16, y + 34, rain_prob);
  display->drawString(x + 16, y + 44, tempMin);
  display->drawString(x + 16, y + 54, tempMax);

  display->setTextAlignment(TEXT_ALIGN_LEFT);
}

/*******************************************/
// Hourly Forecast Details
/*******************************************/
void drawHourlyDetails(OLEDDisplay *display, int x, int y, int hourIndex) {
  time_t observationTimestamp = openWeatherMapOneCallData.hourly[hourIndex].dt; //forecasts[dayIndex].observationTime;
  struct tm* timeInfo;
  timeInfo = localtime(&observationTimestamp);
  char buff[14];
  
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  sprintf_P(buff, PSTR("%02d:%02d"), timeInfo->tm_hour, timeInfo->tm_min);
  /*if (hourIndex == 0) {
    display->fillRect( x+2,  y+1,  29,  11); // fill white (x,  y,  width,  height)
    display->setColor(BLACK);
  }*/
  display->drawString(x + 16, y, String(buff));
  display->setColor(WHITE);
  //if (hourIndex == 0) display->drawHorizontalLine(x+4, y, x+24); //line above the current hour //x, 0, 32
  if (hourIndex == 0) display->drawHorizontalLine(x+2, y+12, 29); //line under the current hour

  display->setFont(Meteocons_Plain_21);
  display->drawString(x + 16, y + 16, openWeatherMapOneCallData.hourly[hourIndex].weatherIconMeteoCon); //y+14
  String temp = String(openWeatherMapOneCallData.hourly[hourIndex].temp, 0) + "°"; //+ (IS_METRIC ? "°C" : "°F");
  String rain_prob = String(openWeatherMapOneCallData.hourly[hourIndex].rain_prob)+"%"; // rain_probability_percentage
  display->setFont(ArialMT_Plain_10);
  display->drawString(x + 16, y + 34, rain_prob);
  display->drawString(x + 16, y + 45, temp); //y+36
  
  display->setTextAlignment(TEXT_ALIGN_LEFT);
}

/**********************************/
// Show daily[0] Details (current)
/**********************************/
void drawCurrentDetails(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  time_t timestamp;
  struct tm* timeInfo;
  char buff[25];
  char buff2[10];
  String temp;
  String temp2;
  String temp3;
    
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  
  //timestamp = openWeatherMapOneCallData.daily[0].dt;
  timestamp = openWeatherMapOneCallData.current.dt;
  timeInfo = localtime(&timestamp);
  sprintf_P(buff, PSTR("%02d:%02d %02d.%02d.%04d"), timeInfo->tm_hour, timeInfo->tm_min, timeInfo->tm_mday, timeInfo->tm_mon+1, timeInfo->tm_year + 1900);
  display->drawString( x, y + 0, "Update: " + String(buff));
  
  timestamp = openWeatherMapOneCallData.daily[0].sunrise;
  timeInfo = localtime(&timestamp);
  sprintf_P(buff, PSTR("%02d:%02d"), timeInfo->tm_hour, timeInfo->tm_min);
  temp3 = String(openWeatherMapOneCallData.daily[0].uvi, 1);
  //Serial.print(temp3);Serial.println(openWeatherMapOneCallData.daily[0].uvi);
  display->drawString( x, y + 14, "Sunrise: " + String(buff) + "   " + "UVI: " + temp3);
   
  timestamp = openWeatherMapOneCallData.daily[0].sunset;
  timeInfo = localtime(&timestamp);
  temp3 = String(openWeatherMapOneCallData.daily[0].rain, 0);
  sprintf_P(buff2, PSTR("%02d:%02d"), timeInfo->tm_hour, timeInfo->tm_min);
  display->drawString( x, y + 26, "Sunset: " + String(buff2) + "  " + "Rain: " + temp3);
  //display->drawString( x, y + 11, "Sun:       " + String(buff) + " - " + String(buff2));
    
  //temp = String(openWeatherMapOneCallData.current.humidity);
  //temp2 = String(openWeatherMapOneCallData.current.pressure);
  //display->drawString( x, y + 22, "RH, AP:  " + temp + " %  " + temp2 + " hPa");
 
  //temp = String(openWeatherMapOneCallData.current.windSpeed * 3.6, 1); // *3600/1000 m/s -> km/h
  temp = String(openWeatherMapOneCallData.daily[0].windSpeed,1); // *3600/1000 m/s -> km/h
  //temp2 = String(openWeatherMapOneCallData.current.windDeg, 0);
  temp2 = WIND_NAMES[(int)roundf((float)openWeatherMapOneCallData.daily[0].windDeg / 22.5)]; // Rounds the wind direction out into 17 sectors. Sectors 1 and 17 are both N.
  temp3 = String(openWeatherMapOneCallData.daily[0].windGusts,1); // *3600/1000 m/s -> km/h
  //display->drawString( x, y + 30, "Wind:     " + temp + "km/h  " + temp2 + "°  " + temp3);
  display->drawString( x, y + 38, "Wind: " + temp + " kn " + temp2 + " " + temp3 + " kn");
  //temp3 = String(openWeatherMapOneCallData.current.visibility * 0.001, 0); //[m]->km
  //display->drawString( x, y + 33, "W, S:  " + temp + " km/h " + temp2 + " " + temp3 + " km");

//  temp = String(openWeatherMapOneCallData.current.visibility * 0.001, 0); //[m]->km
//  display->drawString( x+1, y + 40, "Visibility: " + temp + " km");

  //temp = String(openWeatherMapOneCallData.current.feels_like , 0);
  temp = String(openWeatherMapOneCallData.daily[0].tempMin, 1);
  temp2 = String(openWeatherMapOneCallData.daily[0].tempMax , 1);
  //display->drawString( x, y + 44, "Feels:    " + temp + "°  Dew point: " + temp2 + "°");
  display->drawString( x, y + 50, "T.min: " + temp + "°  T.max: " + temp2 + "°");
    
  //drawHeaderOverlay1(display, state, x, y); //footer string
}



void drawHeaderOverlay1(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  time_t now = time(nullptr);
  struct tm* timeInfo;
  timeInfo = localtime(&now);
  char buff[25];

  //hotfix against 1s to early overlay drawing (display lib bug?) - not a bug, need to respect x and y coordinate offsets
  //display->setColor(BLACK);
  //display->fillRect( 0, 54,  128,  10); // fill white (x,  y,  width,  height)
    
  display->setColor(WHITE);
  display->setFont(ArialMT_Plain_10);

  display->setTextAlignment(TEXT_ALIGN_LEFT);
  sprintf_P(buff, PSTR("%02d:%02d:%02d"), timeInfo->tm_hour, timeInfo->tm_min, timeInfo->tm_sec);
  ///sprintf_P(buff, PSTR("%02d:%02d:%02d   %02d.%02d.%04d"), timeInfo->tm_hour, timeInfo->tm_min, timeInfo->tm_sec, timeInfo->tm_mday, timeInfo->tm_mon+1, timeInfo->tm_year + 1900);
  display->drawString(x + 0, y + 54, String(buff));

  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(x + 58, y + 54, WDAY_NAMES[timeInfo->tm_wday].c_str());
  
  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  sprintf_P(buff, PSTR("%02d.%02d.%04d"), timeInfo->tm_mday, timeInfo->tm_mon+1, timeInfo->tm_year + 1900);
  display->drawString(x + 128, y + 54, String(buff));

  //display->drawHorizontalLine(0, 52, 128); //(0, 52, 128)
}

void drawHeaderOverlay2(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {

  //hotfix against 1s to early overlay drawing (display lib bug?) - not a bug, need to respect x and y coordinate offsets
  //display->setColor(BLACK);
  //display->fillRect( 0, 54,  128,  10); // fill white (x,  y,  width,  height)
  
  display->setColor(WHITE);
  display->setFont(ArialMT_Plain_10);
  
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  //String temp = String(openWeatherMapOneCallData.current.temp, 0) + "°  " + openWeatherMapOneCallData.current.weatherDescription;
  //String temp = String(openWeatherMapOneCallData.current.temp, 1) + "°";
  //display->drawString(x + 0, y + 54, temp);
  display->drawString(x + 0, y + 52, openWeatherMapOneCallData.current.weatherDescription); //54 low case text needs two pixels below the line

  //display->setTextAlignment(TEXT_ALIGN_RIGHT);
  //display->drawString(x + 128, y + 52, openWeatherMapOneCallData.current.weatherDescription); //54 low case text needs two pixels below the line
}

void setReadyForWeatherUpdate() {
  //Serial.println("Setting readyForWeatherUpdate to true");
  readyForWeatherUpdate = true;
}
