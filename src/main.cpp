/* *****************************************************************
 *  库文件、头文件
 * *****************************************************************/
#include <ArduinoJson.h>
#include <Button2.h> //按钮库
#include <EEPROM.h>  //内存
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <StaticThreadController.h> //协程控制
#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>
//使用Tjpg_Decoder库创建C数组图片的网址: http://tomeko.net/online_tools/file_to_hex.php?lang=en 
#include <Thread.h> //协程
#include <TimeLib.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <SetWiFi.h>

#include "Animate/Animate.h" //动画模块
#include "config.h"          //配置文件
#include "loveimg/Anilove.h"
#include "weatherNum/weatherNum.h" //天气图库
#include "misspage/missu.h"

#define Version "LOVE TV UI 2.0"
#define CLOCK_PAGE 0
#define LOVE_PAGE 1
#define MISSU_PAGE 2
/* *****************************************************************
 *  配置使能位
 * *****************************************************************/

// 按钮定义
Button2 Button_right = Button2(15); // 右按钮
Button2 Button_left = Button2(13);  // 左按钮 控制想你页面按键
/* *****************************************************************
 *  字库、图片库
 * *****************************************************************/
#include "font/Chancery_L_20.h" //字体库
#include "font/KT_20.h"
#include "font/ZdyLwFont_20.h"  //字体库
#include "font/timeClockFont.h" //字体库
#include "img/humidity.h"       //湿度图标
#include "img/temperature.h"    //温度图标

#include "img/battery.h"
#include "img/love.h"
#include "img/love1.h"
#include "img/setWiFi_img.h"

/* *****************************************************************
 *  函数声明
 * *****************************************************************/
void sendNTPpacket(IPAddress &address); // 向NTP服务器发送请求
time_t getNtpTime();                    // 从NTP获取时间
void digitalClockDisplay(int reflash_en);
void printDigits(int digits);
String num2str(int digits);
void LCD_reflash();
void clear_screen();
void savewificonfig();   // wifi ssid，psw保存到eeprom
void readwificonfig();   // 从eeprom读取WiFi信息ssid，psw
void deletewificonfig(); // 删除原有eeprom中的信息
void getCityWeather();          // 获取城市天气
void wifi_reset(Button2 &btn); // WIFI重设
void saveParamCallback();
void esp_reset(Button2 &btn);
void scrollBanner();
void weaterData(String *cityDZ, String *dataSK, String *dataFC); // 天气信息写到屏幕上
void refresh_AnimatedImage();    // 更新右下角
void goto_clock_page();
void Battery();
// 纪念日页面函数
void show_mg_love_day();
void show_love_day(int reflash_en);
unsigned int get_love_day(time_t start_love);
void show_mg_love_page();
void refresh_loveImage();
void get_honeyed_words();
void show_honey_words(String *words);
void goto_love_page();
// 想你页面
void mqtt_reconnect();
void mqtt_callback(char *topic, byte *payload, unsigned int length);
void rtos_reconnect_mqtt(void *);
void leftsingleclick(Button2 &btn);
void leftdoubleclick(Button2 &btn);
void rightsingleclick(Button2 &btn);
void rightdoubleclick(Button2 &btn);
void goto_missu_page(String word, uint16_t word_color);
void goto_page(byte page);
void change_gif();
//*********************时钟线程****************************
// 创建时间更新函数线程
Thread reflash_time = Thread();
// 创建副标题切换线程
Thread reflash_Banner = Thread();
// 创建恢复WIFI链接
Thread reflash_openWifi = Thread();
// 创建动画绘制线程
Thread reflash_Animate = Thread();
// 电池电压检测
Thread reflash_Battery = Thread();
// 联网后所有需要更新的数据
Thread WIFI_reflash = Thread();
// 创建协程池
StaticThreadController<5> controller(&reflash_time, &reflash_Banner, &reflash_openWifi, &reflash_Animate, &reflash_Battery);

//********************love线程******************************
// 创建四毛线程
Thread reflash_Love_Animate = Thread();
// 创建
Thread reflash_Love_openWifi = Thread();
StaticThreadController<2> controller_love(&reflash_Love_Animate, &reflash_Love_openWifi);

//********************imu线程******************************

Thread reflash_mqtt = Thread();

/* *****************************************************************
 *  参数设置
 * *****************************************************************/
struct config_type {
  char stassid[32]; // 定义配网得到的WIFI名长度(最大32字节)
  char stapsw[64];  // 定义配网得到的WIFI密码长度(最大64字节)
};
//---------------修改此处""内的信息--------------------
// 如开启WEB配网则可不用设置这里的参数，前一个为wifi ssid，后一个为密码
config_type wificonf = {{"WiFi名"}, {"密码"}};

// 天气更新时间  X 分钟（默认1分钟）
unsigned int updateweater_time = WEATHER_UPDATE_TIME;
// 天气信息
struct Weather_Msg {
  String cityDZ;
  String dataSK;
  String dataFC;
};

Weather_Msg weather_msg = {{""}, {""}, {""}};
//----------------------------------------------------

#define battery 34 // 电压检测

// LCD屏幕相关设置
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite clk = TFT_eSprite(&tft);
#define LCD_BL_PIN 22 // LCD背光引脚
uint16_t bgColor = 0x0000;
uint16_t pinkColor = tft.color565(255, 174, 201);
uint16_t zongseColor = tft.color565(128, 64, 64);
uint16_t whiteColor = tft.color565(245, 246, 247);
uint16_t fhColor = tft.color565(253, 99, 139);
uint16_t blueColor = tft.color565(175, 221, 224);
// 其余状态标志位
int LCD_Rotation = 0; // LCD屏幕方向
uint8_t brightness = 255;
uint8_t Wifi_en = 1;         // WIFI模块启动  1：打开    0：关闭
uint8_t UpdateWeater_en = 0; // 更新时间标志位
int prevTime = 0;            // 滚动显示更新标志位
time_t love_start_time = 0;
int Mode = 1;
// EEPROM参数存储地址位
int Ro_addr = 2;    // 被写入数据的EEPROM地址编号  2 旋转方向
int CC_addr = 10;   // 被写入数据的EEPROM地址编号  10城市
int wifi_addr = 30; // 被写入数据的EEPROM地址编号  20wifi-ssid-psw

time_t prevDisplay = 0;       // 显示时间显示记录
int Amimate_reflash_Time = 0; // 更新时间记录
int Amilove_reflash_Time = 0; // 更新时间记录
int connectTimes = 0;

/*** Component objects ***/
WeatherNum wrat;

uint32_t targetTime = 0;
int tempnum = 0;               // 温度百分比
int huminum = 0;               // 湿度百分比
int tempcol = 0xffff;          // 温度显示颜色
int humicol = 0xffff;          // 湿度显示颜色

// NTP服务器参数
static const char ntpServerName[] = "ntp6.aliyun.com";
const int timeZone = 8; // 东八区

// wifi连接UDP设置参数
WiFiUDP Udp;
WiFiClient wificlient;
unsigned int localPort = 8000;
float duty = 0;

// mqtt参数
PubSubClient mqttClient(wificlient);
const char *mqtt_server = MQTT_IP;

// 显示页面
byte page = CLOCK_PAGE; // 默认时钟页面
byte pre_page = CLOCK_PAGE; // 切换前一个页面,后面单击退出时能够回到之前的页面
byte role = RECEIVER;

String words = DEFAULT_WORDS;

// 星期
String week() {
  String wk[7] = {"日", "一", "二", "三", "四", "五", "六"};
  String s = "周" + wk[weekday() - 1];
  return s;
}

// 月日
String monthDay() {
  String s = String(month());
  s = s + "月" + day() + "日";
  return s;
}

/* *****************************************************************
 *  函数
 * *****************************************************************/

// TFT屏幕输出函数
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap) {
  if (y >= tft.height())
    return 0;
  tft.pushImage(x, y, w, h, bitmap);
  return 1;
}

// 进度条函数
byte loadNum = 6;
void loading(byte delayTime){
  clk.setColorDepth(8);

  clk.createSprite(200, 100); // 创建窗口
  clk.fillSprite(0x0000);     // 填充率

  clk.drawRoundRect(0, 0, 200, 16, 8, 0xFFFF);     // 空心圆角矩形
  clk.fillRoundRect(3, 3, loadNum, 10, 5, 0xFFFF); // 实心圆角矩形
  clk.setTextDatum(CC_DATUM);                      // 设置文本数据
  clk.setTextColor(TFT_GREEN, 0x0000);
  clk.drawString("Connecting to WiFi......", 100, 40, 2);
  clk.setTextColor(TFT_WHITE, 0x0000);
  clk.drawRightString(Version, 180, 60, 2);
  clk.pushSprite(20, 120); // 窗口位置

  clk.deleteSprite();
  loadNum += 1;
  delay(delayTime);
}

//强制门户Web配网
bool setWiFi_Flag = false;
void setWiFi() {  //设置WiFi
  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(true);
  TJpgDec.setCallback(tft_output);
  TJpgDec.drawJpg(0,0,setWiFi_img, sizeof(setWiFi_img));

  initBasic();
  initSoftAP();
  initWebServer();
  initDNS();
  while(setWiFi_Flag == false) {
    server.handleClient();
    dnsServer.processNextRequest();
    if(WiFi.status() == WL_CONNECTED) {
      server.stop();
      setWiFi_Flag = true;
    }
  }
}


// 湿度图标显示函数
void humidityWin() {
  clk.setColorDepth(8);

  huminum = huminum / 2;
  clk.createSprite(52, 6); // 创建窗口
  clk.fillSprite(0x0000);  // 填充率
  clk.drawRoundRect(
      0, 0, 52, 6, 3,
      0xFFFF); // 空心圆角矩形  起始位x,y,长度，宽度，圆弧半径，颜色
  clk.fillRoundRect(1, 1, huminum, 4, 2, humicol); // 实心圆角矩形
  clk.pushSprite(45, 222);                         // 窗口位置
  clk.deleteSprite();
}

// 温度图标显示函数
void tempWin() {
  clk.setColorDepth(8);

  clk.createSprite(52, 6); // 创建窗口
  clk.fillSprite(0x0000);  // 填充率
  clk.drawRoundRect(
      0, 0, 52, 6, 3,
      0xFFFF); // 空心圆角矩形  起始位x,y,长度，宽度，圆弧半径，颜色
  clk.fillRoundRect(1, 1, tempnum, 4, 2, tempcol); // 实心圆角矩形
  clk.pushSprite(45, 192);                         // 窗口位置
  clk.deleteSprite();
}

// WEB配网LCD显示函数
void Web_win() {
  clk.setColorDepth(8);

  clk.createSprite(200, 60); // 创建窗口
  clk.fillSprite(0x0000);    // 填充率

  clk.setTextDatum(CC_DATUM); // 设置文本数据
  clk.setTextColor(TFT_GREEN, 0x0000);
  clk.drawString("WiFi Connect Fail!", 100, 10, 2);
  clk.drawString("SSID:", 45, 40, 2);
  clk.setTextColor(TFT_WHITE, 0x0000);
  clk.drawString("zwz的小电视", 125, 40, 2);
  clk.pushSprite(20, 50); // 窗口位置

  //clk.deleteSprite();
}

// 删除原有eeprom中的信息
void deletewificonfig() {
  config_type deletewifi = {{""}, {""}};
  uint8_t *p = (uint8_t *)(&deletewifi);
  for (unsigned int i = 0; i < sizeof(deletewifi); i++) {
    EEPROM.write(i + wifi_addr, *(p + i)); // 在闪存内模拟写入
  }
  delay(10);
  EEPROM.commit(); // 执行写入ROM
  delay(10);
}

// 从eeprom读取WiFi信息ssid，psw
void readwificonfig() {
  uint8_t *p = (uint8_t *)(&wificonf);
  for (unsigned int i = 0; i < sizeof(wificonf); i++) {
    *(p + i) = EEPROM.read(i + wifi_addr);
  }
  // EEPROM.commit();
  // ssid = wificonf.stassid;
  // pass = wificonf.stapsw;
  Serial.printf("Read WiFi Config.....\r\n");
  Serial.printf("SSID:%s\r\n", wificonf.stassid);
  Serial.printf("PSW:%s\r\n", wificonf.stapsw);
  Serial.printf("Connecting.....\r\n");
}

// 获取城市天气
void getCityWeather() {
  // String URL = "http://d1.weather.com.cn/dingzhi/" + cityCode +
  // ".html?_="+String(now());//新
  String URL = "http://d1.weather.com.cn/weather_index/" + cityCode + ".html?_=" + String(now()); // 原来
  // 创建 HTTPClient 对象
  HTTPClient httpClient;

  // httpClient.begin(URL);
  httpClient.begin(wificlient, URL); // 使用新方法

  // 设置请求头中的User-Agent
  httpClient.setUserAgent("Mozilla/5.0 (iPhone; CPU iPhone OS 11_0 like Mac "
                          "OS X) AppleWebKit/604.1.38 (KHTML, like Gecko) "
                          "Version/11.0 Mobile/15A372 Safari/604.1");
  httpClient.addHeader("Referer", "http://www.weather.com.cn/");

  // 启动连接并发送HTTP请求
  int httpCode = httpClient.GET();
  Serial.println("正在获取天气数据");
  Serial.println(URL);

  // 如果服务器响应OK则从服务器获取响应体信息并通过串口输出
  if (httpCode == HTTP_CODE_OK) {

    String str = httpClient.getString();
    // Serial.println("httpdata=" + str);
    int indexStart = str.indexOf("weatherinfo\":");
    int indexEnd = str.indexOf("};var alarmDZ");

    weather_msg.cityDZ = str.substring(indexStart + 13, indexEnd);
    // Serial.println(jsonCityDZ);

    indexStart = str.indexOf("dataSK =");
    indexEnd = str.indexOf(";var dataZS");
    weather_msg.dataSK = str.substring(indexStart + 8, indexEnd);
    // Serial.println(jsonDataSK);

    indexStart = str.indexOf("\"f\":[");
    indexEnd = str.indexOf(",{\"fa");
    weather_msg.dataFC = str.substring(indexStart + 5, indexEnd);
    // Serial.println(jsonFC);

    // weaterData(&jsonCityDZ, &jsonDataSK, &jsonFC);
    weaterData(&(weather_msg.cityDZ), &(weather_msg.dataSK), &(weather_msg.dataFC));
    Serial.println("获取成功");
  } else {
    Serial.println("请求城市天气错误：");
    Serial.print(httpCode);
  }

  // 关闭ESP32与服务器连接
  httpClient.end();
}

String scrollText[7];
// int scrollTextWidth = 0;

// 天气信息写到屏幕上
void weaterData(String *cityDZ, String *dataSK, String *dataFC) {
  // 解析第一段JSON
  DynamicJsonDocument doc(1024);
  deserializeJson(doc, *dataSK);
  JsonObject sk = doc.as<JsonObject>();

  /***绘制相关文字***/
  clk.setColorDepth(8);
  clk.loadFont(ZdyLwFont_20);

  // 温度
  clk.createSprite(58, 24);
  clk.fillSprite(bgColor);
  clk.setTextDatum(CC_DATUM);
  clk.setTextColor(TFT_WHITE, bgColor);
  clk.drawString(sk["temp"].as<String>() + "℃", 28, 13);
  clk.pushSprite(100, 184);
  clk.deleteSprite();
  tempnum = sk["temp"].as<int>();
  tempnum = tempnum + 10;
  if (tempnum < 10)
    tempcol = 0x00FF;
  else if (tempnum < 28)
    tempcol = 0x0AFF;
  else if (tempnum < 34)
    tempcol = 0x0F0F;
  else if (tempnum < 41)
    tempcol = 0xFF0F;
  else if (tempnum < 49)
    tempcol = 0xF00F;
  else {
    tempcol = 0xF00F;
    tempnum = 50;
  }
  tempWin();

  // 湿度
  clk.createSprite(58, 24);
  clk.fillSprite(bgColor);
  clk.setTextDatum(CC_DATUM);
  clk.setTextColor(TFT_WHITE, bgColor);
  clk.drawString(sk["SD"].as<String>(), 28, 13);
  // clk.drawString("100%",28,13);
  clk.pushSprite(100, 214);
  clk.deleteSprite();
  // String A = sk["SD"].as<String>();
  huminum = atoi((sk["SD"].as<String>()).substring(0, 2).c_str());

  if (huminum > 90)
    humicol = 0x00FF;
  else if (huminum > 70)
    humicol = 0x0AFF;
  else if (huminum > 40)
    humicol = 0x0F0F;
  else if (huminum > 20)
    humicol = 0xFF0F;
  else
    humicol = 0xF00F;
  humidityWin();

  // 城市名称
  clk.createSprite(94, 30);
  clk.fillSprite(bgColor);
  clk.setTextDatum(CC_DATUM);
  clk.setTextColor(TFT_WHITE, bgColor);
  clk.drawString(sk["cityname"].as<String>(), 44, 16);
  clk.pushSprite(15, 15);
  clk.deleteSprite();

  // PM2.5空气指数
  uint16_t pm25BgColor = tft.color565(156, 202, 127); // 优
  String aqiTxt = "优";
  int pm25V = sk["aqi"];
  if (pm25V > 200) {
    pm25BgColor = tft.color565(136, 11, 32); // 重度
    aqiTxt = "重度";
  } else if (pm25V > 150) {
    pm25BgColor = tft.color565(186, 55, 121); // 中度
    aqiTxt = "中度";
  } else if (pm25V > 100) {
    pm25BgColor = tft.color565(242, 159, 57); // 轻
    aqiTxt = "轻度";
  } else if (pm25V > 50) {
    pm25BgColor = tft.color565(247, 219, 100); // 良
    aqiTxt = "良";
  }
  clk.createSprite(56, 24);
  clk.fillSprite(bgColor);
  clk.fillRoundRect(0, 0, 50, 24, 4, pm25BgColor);
  clk.setTextDatum(CC_DATUM);
  clk.setTextColor(0x0000);
  clk.drawString(aqiTxt, 25, 13);
  clk.pushSprite(104, 18);
  clk.deleteSprite();

  scrollText[0] = "实时天气 " + sk["weather"].as<String>();
  scrollText[1] = "空气质量 " + aqiTxt;
  scrollText[2] = "风向 " + sk["WD"].as<String>() + sk["WS"].as<String>();

  // scrollText[6] =
  // atoi((sk["weathercode"].as<String>()).substring(1,3).c_str()) ;

  // 天气图标
  wrat.printfweather(
      170, 15, atoi((sk["weathercode"].as<String>()).substring(1, 3).c_str()));

  // 左上角滚动字幕
  // 解析第二段JSON
  deserializeJson(doc, *cityDZ);
  JsonObject dz = doc.as<JsonObject>();
  // Serial.println(sk["ws"].as<String>());
  // 横向滚动方式
  // String aa = "今日天气:" + dz["weather"].as<String>() + "，温度:最低" +
  // dz["tempn"].as<String>() + "，最高" + dz["temp"].as<String>() + "
  // 空气质量:" + aqiTxt + "，风向:" + dz["wd"].as<String>() +
  // dz["ws"].as<String>(); scrollTextWidth = clk.textWidth(scrollText);
  // Serial.println(aa);
  scrollText[3] = "今日" + dz["weather"].as<String>();

  deserializeJson(doc, *dataFC);
  JsonObject fc = doc.as<JsonObject>();

  scrollText[4] = "最低温度" + fc["fd"].as<String>() + "℃";
  scrollText[5] = "最高温度" + fc["fc"].as<String>() + "℃";

  clk.unloadFont();
}

int currentIndex = 0;
TFT_eSprite clkb = TFT_eSprite(&tft);

void scrollBanner() {
  if (scrollText[currentIndex]) {
    clkb.setColorDepth(8);
    clkb.loadFont(ZdyLwFont_20);
    clkb.createSprite(150, 30);
    clkb.fillSprite(bgColor);
    clkb.setTextWrap(false);
    clkb.setTextDatum(CC_DATUM);
    clkb.setTextColor(TFT_WHITE, bgColor);
    clkb.drawString(scrollText[currentIndex], 74, 16);
    clkb.pushSprite(10, 45);

    clkb.deleteSprite();
    clkb.unloadFont();

    if (currentIndex >= 5)
      currentIndex = 0; // 回第一个
    else
      currentIndex += 1; // 准备切换到下一个
  }
  prevTime = 1;
}
// 用快速线方法绘制数字
void drawLineFont(uint32_t _x, uint32_t _y, uint32_t _num, uint32_t _size, uint32_t _color) {
  uint32_t fontSize;
  const LineAtom *fontOne;
  // 小号(9*14)
  if (_size == 1) {
    fontOne = smallLineFont[_num];
    fontSize = smallLineFont_size[_num];
    // 绘制前清理字体绘制区域
    tft.fillRect(_x, _y, 9, 14, TFT_BLACK);
  }
  // 中号(18*30)
  else if (_size == 2) {
    fontOne = middleLineFont[_num];
    fontSize = middleLineFont_size[_num];
    // 绘制前清理字体绘制区域
    tft.fillRect(_x, _y, 18, 30, TFT_BLACK);
  }
  // 大号(36*90)
  else if (_size == 3) {
    fontOne = largeLineFont[_num];
    fontSize = largeLineFont_size[_num];
    // 绘制前清理字体绘制区域
    tft.fillRect(_x, _y, 36, 90, TFT_BLACK);
  } else
    return;

  for (uint32_t i = 0; i < fontSize; i++) {
    tft.drawFastHLine(fontOne[i].xValue + _x, fontOne[i].yValue + _y, fontOne[i].lValue, _color);
  }
}

int Hour_sign = 60;
int Minute_sign = 60;
int Second_sign = 60;
// 日期刷新
void digitalClockDisplay(int reflash_en = 0) {
  // 时钟刷新,输入1强制刷新
  int now_hour = hour();     // 获取小时
  int now_minute = minute(); // 获取分钟
  int now_second = second(); // 获取秒针
  // 小时刷新
  if ((now_hour != Hour_sign) || (reflash_en == 1)) {
    drawLineFont(20, timeY, now_hour / 10, 3, SD_FONT_WHITE);
    drawLineFont(60, timeY, now_hour % 10, 3, SD_FONT_WHITE);
    Hour_sign = now_hour;
  }
  // 分钟刷新
  if ((now_minute != Minute_sign) || (reflash_en == 1)) {
    drawLineFont(101, timeY, now_minute / 10, 3, SD_FONT_YELLOW);
    drawLineFont(141, timeY, now_minute % 10, 3, SD_FONT_YELLOW);
    Minute_sign = now_minute;
  }
  // 秒针刷新
  if ((now_second != Second_sign) || (reflash_en == 1)) // 分钟刷新
  {
    drawLineFont(182, timeY + 30, now_second / 10, 2, SD_FONT_WHITE);
    drawLineFont(202, timeY + 30, now_second % 10, 2, SD_FONT_WHITE);
    Second_sign = now_second;
  }

  if (reflash_en == 1)
    reflash_en = 0;
  /***日期****/
  clk.setColorDepth(8);
  clk.loadFont(ZdyLwFont_20);

  // 星期
  clk.createSprite(58, 30);
  clk.fillSprite(bgColor);
  clk.setTextDatum(CC_DATUM);
  clk.setTextColor(TFT_WHITE, bgColor);
  clk.drawString(week(), 29, 16);
  clk.pushSprite(102, 150);
  clk.deleteSprite();

  // 月日
  clk.createSprite(95, 30);
  clk.fillSprite(bgColor);
  clk.setTextDatum(CC_DATUM);
  clk.setTextColor(TFT_WHITE, bgColor);
  clk.drawString(monthDay(), 49, 16);
  clk.pushSprite(5, 150);
  clk.deleteSprite();

  clk.unloadFont();
  /***日期****/
}

/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48; // NTP时间在消息的前48字节中
byte
    packetBuffer[NTP_PACKET_SIZE]; // buffer to hold incoming & outgoing packets

time_t getNtpTime() {
  while(1){
    IPAddress ntpServerIP; // NTP server's ip address

    while (Udp.parsePacket() > 0);
    WiFi.hostByName(ntpServerName, ntpServerIP);
    // Serial.print(ntpServerName);
    // Serial.print(": ");
    // Serial.println(ntpServerIP);
    sendNTPpacket(ntpServerIP);
    uint32_t beginWait = millis();
    while (millis() - beginWait < 1500) {
      int size = Udp.parsePacket();

      if (size >= NTP_PACKET_SIZE) {
        Serial.println("Receive NTP Response");
        Udp.read(packetBuffer, NTP_PACKET_SIZE); // read packet into the buffer
        unsigned long secsSince1900;
        // convert four bytes starting at location 40 to a long integer
        secsSince1900 = (unsigned long)packetBuffer[40] << 24;
        secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
        secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
        secsSince1900 |= (unsigned long)packetBuffer[43];
        // Serial.println(secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR);
        return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
      }
    }
  }
}

// 向NTP服务器发送请求
void sendNTPpacket(IPAddress &address) {
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011; // LI, Version, Mode
  packetBuffer[1] = 0;          // Stratum, or type of clock
  packetBuffer[2] = 6;          // Polling Interval
  packetBuffer[3] = 0xEC;       // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); // NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

void esp_reset(Button2 &btn) { 
  ESP.restart(); 
}

void wifi_reset(Button2 &btn) {
  deletewificonfig();
  delay(10);
  Serial.println("重置WiFi成功");
  setWiFi();
}

// 更新时间
void reflashTime() {
  prevDisplay = now();
  // timeClockDisplay(1);
  digitalClockDisplay();
  prevTime = 0;
}

// 切换天气 or 空气质量
void reflashBanner() { scrollBanner(); }

// 所有需要联网后更新的方法都放在这里
void WIFI_reflash_All() {
  if (Wifi_en == 1) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("WIFI connected");

      // Serial.println("getCityWeather start");
      getCityWeather();
      // Serial.println("getCityWeather end");

      getNtpTime();
      // 其他需要联网的方法写在后面

      // WiFi.forceSleepBegin(); // Wifi Off
      Serial.println("WIFI sleep......");
      Wifi_en = 0;
    } else {
      // Serial.println("WIFI unconnected");
      goto_missu_page("wifi unconnected", TFT_RED);
    }
  }
}

// 打开WIFI
void openWifi() {
  Serial.println("WIFI reset......");
  // WiFi.forceSleepWake(); // wifi on
  Wifi_en = 1;
}

// 强制屏幕刷新
void LCD_reflash() {
  reflashTime();
  reflashBanner();
  openWifi();
}

// 清空屏幕
void clear_screen() { tft.fillScreen(bgColor); }

// 守护线程池
void Supervisor_controller() {
  if (controller.shouldRun()) {
    // Serial.println("controller 启动");
    controller.run();
  }
}

void Supervisor_love_controller() {
  if (controller_love.shouldRun()) {
    controller_love.run();
  }
}

void Supervisor_imu_controller() {
  if (reflash_mqtt.shouldRun()) {
    reflash_mqtt.run();
  }
}



void setup() {
  Button_right.setDoubleClickTime(500);
  Button_right.setLongClickTime(1000);
  Button_right.setClickHandler(rightsingleclick);
  Button_right.setDoubleClickHandler(rightdoubleclick);
  Button_right.setLongClickHandler(wifi_reset);

  Button_left.setDoubleClickTime(500);                // 双击时间
  Button_left.setLongClickTime(1000);                 // 设置长按时间
  Button_left.setClickHandler(leftsingleclick);       // 单击事件
  Button_left.setDoubleClickHandler(leftdoubleclick); // 双击事件
  Button_left.setLongClickDetectedHandler(esp_reset); // 长按事件，重启

  Serial.begin(115200);
  EEPROM.begin(1024);

  // 纪念日
  setTime(0, 0, 0, DAY, MONTH, YEAR);
  love_start_time = now();

  pinMode(LCD_BL_PIN, OUTPUT);
  analogWrite(LCD_BL_PIN, 255);

  tft.init(); /* TFT init */
  tft.invertDisplay(1); //反转所有显示颜色：1反转，0正常
  // tft.setSwapBytes(true); //使图片颜色由RGB->BGR
  tft.setRotation(LCD_Rotation);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_BLACK, bgColor);

  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(true);
  TJpgDec.setCallback(tft_output);

  /*首次使用自动进入配网模式*/
  preferences.begin("wifi", false);
  PrefSSID =  preferences.getString("ssid", "none");
  PrefPassword =  preferences.getString("password", "none");
  cityCode = preferences.getString("citycode", "none");
  preferences.end();

  targetTime = millis() + 1000;  

  if( PrefSSID == "none" ){
    setWiFi();
  }
  Serial.println("正在连接"+ PrefSSID + " ...");
  WiFi.begin(PrefSSID.c_str(), PrefPassword.c_str());
  
  while (WiFi.status() != WL_CONNECTED) {
    for(byte n=0;n<10;n++){ 
      loading(30);
      connectTimes++;
      if(connectTimes >= 190) { //进度条即将结束时还未连接成功，则提示wifi连接失败，自动进入配网模式
        connectTimes = 0;
        setWiFi();
      }
    }
  }

  while(loadNum < 194 & connectTimes <= 189){ //让动画走完
    loading(1);
    connectTimes = 0;
  }

  Serial.print("本地IP： ");
  Serial.println(WiFi.localIP());
  Serial.println("启动UDP");
  Udp.begin(localPort);
  Serial.println("等待同步...");
  setSyncProvider(getNtpTime);
  setSyncInterval(300);

  // love线程
  reflash_Love_Animate.setInterval(TMS / 10);
  reflash_Love_Animate.onRun(refresh_loveImage);

  reflash_Love_openWifi.setInterval(WORD_UPDATE_TIME * 60 * TMS); // 5分钟刷新情话
  reflash_Love_openWifi.onRun(openWifi);
  controller_love.run();

  tft.fillScreen(TFT_BLACK);                                  // 清屏
  TJpgDec.drawJpg(15, 183, temperature, sizeof(temperature)); // 温度图标
  TJpgDec.drawJpg(15, 213, humidity, sizeof(humidity));       // 湿度图标

  Battery();
  getCityWeather();
  
  // mqtt
  mqttClient.setServer(mqtt_server, 1883);
  mqttClient.setCallback(mqtt_callback);

  // WiFi.forceSleepBegin(); // wifi off
  Serial.println("WIFI休眠......");
  Wifi_en = 0;

  // 时钟线程
  reflash_time.setInterval(300); // 设置所需间隔 300毫秒
  reflash_time.onRun(reflashTime);

  reflash_Battery.setInterval(60 * TMS);
  reflash_Battery.onRun(Battery);

  reflash_Banner.setInterval(2 * TMS); // 设置所需间隔 2秒
  reflash_Banner.onRun(reflashBanner);

  reflash_openWifi.setInterval(updateweater_time * 60 * TMS); // 设置所需间隔 1分钟
  reflash_openWifi.onRun(openWifi);

  reflash_Animate.setInterval(TMS / 10); // 设置帧率
  reflash_Animate.onRun(refresh_AnimatedImage);
  controller.run();

  // 想你页面 mqtt线程
  reflash_mqtt.setInterval(2 * TMS); // 设置1秒钟连一次
  reflash_mqtt.onRun(mqtt_reconnect);
}

const uint8_t *Animate_value; // 指向关键帧的指针
uint32_t Animate_size;        // 指向关键帧大小的指针
void refresh_AnimatedImage() {
  if (millis() - Amimate_reflash_Time > 50) // x ms切换一次
  {
    Amimate_reflash_Time = millis();
    imgAnim(&Animate_value, &Animate_size, Mode);
    TJpgDec.drawJpg(160, 160, Animate_value, Animate_size);
  }
}

long t = 0;

void loop() {
  Button_right.loop(); // 按钮轮询.
  Button_left.loop();

  // 守护mqtt进程
  Supervisor_imu_controller();
  mqttClient.loop();

  if (page == CLOCK_PAGE) {
    Supervisor_controller(); // 守护线程池
    WIFI_reflash_All();      // WIFI应用
  } else if (page == LOVE_PAGE) {
    show_mg_love_page();
  } else if (page == MISSU_PAGE) {
    show_missu_page("", TFT_WHITE);
  }
}
/******************************************************************************
 * 纪念日页面
 ********************************************************************************/

/**
 * 计算纪念日天数  start_time 参数是从在一起的时间-秒
 */
unsigned int get_love_day(time_t start_time) {
  // 获取最新时间
  time_t cur_time = now();
  time_t love_seconds = cur_time - start_time;
  unsigned int day = (love_seconds) / (24 * 3600) + 1;
  return day;
}

/**
 * 显示恋爱时间
 */
unsigned int DAY_SIGN = 0;
const uint8_t DAY_Y = 110; // 日期y坐标
const uint8_t DAY_FONT_SIZE = 2;
const byte OFFSET_X = 65; // x坐标偏移量
const uint16_t NUM_COLOR = fhColor;
TFT_eSprite sdm = TFT_eSprite(&tft);

// 用快速线方法绘制数字
void drawLineFontForDay(uint32_t _x, uint32_t _y, uint32_t _num, uint32_t _size,
                        uint32_t _color) {
  uint32_t fontSize;
  const LineAtom *fontOne;
  fontOne = middleLineFont[_num];
  fontSize = middleLineFont_size[_num];

  // 绘制前清理字体绘制区域  周围填充4像素
  tft.fillRoundRect(_x - 4, _y - 4, 26, 38, 4, whiteColor);

  for (uint32_t i = 0; i < fontSize; i++) {
    tft.drawFastHLine(fontOne[i].xValue + _x, fontOne[i].yValue + _y,
                      fontOne[i].lValue, _color);
  }
}

void show_love_day(int reflash_en = 0) {
  unsigned int day = get_love_day(love_start_time);

  // 显示相关文字
  sdm.setColorDepth(16);
  sdm.loadFont(KT_20);

  sdm.createSprite(58, 30);
  // sdm.createSprite(46, 22);
  sdm.fillSprite(pinkColor);
  sdm.setTextWrap(false);
  // sdm.fillRoundRect(0, 0, 46, 22, 4, pinkColor);
  sdm.setTextDatum(CC_DATUM);
  sdm.setTextColor(zongseColor, pinkColor);
  sdm.drawString("相恋", 29, 16);
  sdm.pushSprite(0, 110);
  sdm.deleteSprite();

  sdm.createSprite(30, 30);
  sdm.fillSprite(pinkColor);
  sdm.setTextWrap(false);
  sdm.setTextDatum(CC_DATUM);
  sdm.setTextColor(zongseColor, pinkColor);
  sdm.drawString("天", 15, 16);
  sdm.pushSprite(185, 110);
  sdm.deleteSprite();

  sdm.unloadFont();

  // 显示数字
  if (day != DAY_SIGN || reflash_en == 1) // relash_en == 1强制刷新
  {
    drawLineFontForDay(OFFSET_X, DAY_Y, day / 1000, DAY_FONT_SIZE, NUM_COLOR);
    drawLineFontForDay(30 + OFFSET_X, DAY_Y, day / 100 % 10, DAY_FONT_SIZE, NUM_COLOR);
    drawLineFontForDay(60 + OFFSET_X, DAY_Y, day / 10 % 10, DAY_FONT_SIZE,  NUM_COLOR);
    drawLineFontForDay(90 + OFFSET_X, DAY_Y, day % 10, DAY_FONT_SIZE, NUM_COLOR);
    DAY_SIGN = day;
  }
}

//////////////////////////////////////////////////////////////////
// 显示动画
///////////////////////////////////////////////////////////////
const uint8_t *Anilove_value; // 指向关键帧的指针
uint32_t Anilove_size;        // 指向关键帧大小的指针
void refresh_loveImage() {

  if (millis() - Amilove_reflash_Time > 10) // x ms切换一次
  {
    uint16_t w = 0, h = 0;
    bool swith_animate = false;

    Amilove_reflash_Time = millis();
    loveImg(&Anilove_value, &Anilove_size, &swith_animate);

    // Get the width and height in pixels of the jpeg if you wish

    TJpgDec.getJpgSize(&w, &h, Anilove_value, Anilove_size);
    // Serial.print("Width = ");
    // Serial.print(w);
    // Serial.print(", height = ");
    // Serial.println(h);
    uint8_t x = (240 - w) / 2;
    uint8_t y = (102 - h) / 2;

    // Draw the image, top left at 0,0
    TJpgDec.drawJpg(x, y, Anilove_value, Anilove_size);

    // 如果到了切换动画，把之前的残影消除，否则影响视觉效果
    if (swith_animate) {
      sdm.createSprite(w, h);
      sdm.fillSprite(pinkColor);
      sdm.pushSprite(x, y);
      sdm.deleteSprite();
    }
  }
}

/**
 * 显示底部文字
 *
 */
void show_buttom(String s) {
  clk.setColorDepth(16);
  // clk.loadFont(URW_Chancery_L_Medium_Italic_21Bitmaps);
  clk.setFreeFont(&URW_Chancery_L_Medium_Italic_20);
  clk.createSprite(240, 30);
  clk.fillSprite(pinkColor);
  clk.setTextDatum(CC_DATUM);
  clk.setTextColor(zongseColor, pinkColor);
  clk.drawString(s, 120, 15);
  clk.pushSprite(0, 210);
  clk.deleteSprite();

  clk.unloadFont();
}

/**
 * 显示情话
 *
 */

// 获取情话

void get_honeyed_words() {
  String URL = "http://api.1314.cool/words/api.php?return=json";
  // 创建 HTTPClient 对象
  HTTPClient httpClient;

  // 配置请求地址。此处也可以不使用端口号和PATH而单纯的
  httpClient.begin(wificlient, URL);

  // 设置请求头中的User-Agent
  httpClient.setUserAgent(
      "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, "
      "like Gecko) Chrome/101.0.4951.64 Safari/537.36 Edg/101.0.1210.47");
  httpClient.addHeader("Referer", "http://api.1314.cool/");

  // 启动连接并发送HTTP请求
  int httpCode = httpClient.GET();
  Serial.print("Send GET request to URL: ");
  Serial.println(URL);

  // 如果服务器响应OK则从服务器获取响应体信息并通过串口输出
  if (httpCode == HTTP_CODE_OK) {
    words = httpClient.getString();
    Serial.println(words);
    show_honey_words(&words);
  } else {
    Serial.print(httpCode);
    Serial.println("  获取情话失败...");
  }
  // 关闭ESP8266与服务器连接
  httpClient.end();
}

// 显示情话

void show_honey_words(String *words) {

  // 解析第一段JSON
  DynamicJsonDocument doc(300);
  deserializeJson(doc, *words);
  JsonObject data = doc.as<JsonObject>();
  String word = data["word"];
  Serial.println(word);

  // 字符串预处理
  word.replace("<br>", " | "); // 将数据里面的 <br> 标签替换掉
  word.replace(",", "，");
  word.replace("?", "？");
  word.replace(".", "...");
  word.replace("=", " = ");
  word.replace("(", "（");
  word.replace(")", "）");

  // 去掉顿号
  int index_str = word.indexOf(
      "、"); // 只显示第一个'、'号后面的内容-->"二十五、心中万点涟漪，都是为你而动。眼中万里柔情，皆是为你而起。"
  if (index_str == -1) {
    index_str = 0; // 如果没有找到、
  } else {
    index_str += 3; // 找到了把、号去掉
  }

  uint16_t num = 11; // 每行显示多少中文字符
  // 分割字符串，一行 11 个中文字符
  String first_str = word.substring(index_str, index_str + num * 3);
  String second_str =
      word.substring(index_str + num * 3, index_str + num * 3 * 2);
  String third_str =
      word.substring(index_str + num * 3 * 2, index_str + num * 3 * 3);
  String forth_str = "";
  forth_str = word.substring(index_str + num * 3 * 3, index_str + num * 3 * 4);

  Serial.println("第一段=" + first_str);
  Serial.println("第二段=" + second_str);
  Serial.println("第三段=" + third_str);
  Serial.println("第四段=" + forth_str);

  // 如果超过四行,这段情话不显示；显示自定义的
  if (forth_str != "") {
    first_str = "哈哈哈，想你辣！";
    second_str = "";
    third_str = "";
  }

  // 如果只有一行，那么居中显示，（显示在第二行）
  if (second_str == "") {
    second_str = first_str;
    first_str = "";
  }

  // 显示情话
  sdm.setColorDepth(16);
  sdm.loadFont(KT_20);
  // 第一行
  sdm.createSprite(240, 20);
  sdm.fillSprite(pinkColor);
  sdm.setTextWrap(false);
  sdm.setTextDatum(CC_DATUM);
  sdm.setTextColor(TFT_WHITE, pinkColor);
  sdm.drawString(first_str, 120, 11);
  sdm.pushSprite(0, 153);
  sdm.deleteSprite();

  // 第二行
  sdm.createSprite(240, 20);
  sdm.fillSprite(pinkColor);
  sdm.setTextWrap(false);
  sdm.setTextDatum(CC_DATUM);
  sdm.setTextColor(TFT_WHITE, pinkColor);
  sdm.drawString(second_str, 120, 11);
  sdm.pushSprite(0, 173);
  sdm.deleteSprite();

  // 第三行
  sdm.createSprite(240, 20);
  sdm.fillSprite(pinkColor);
  sdm.setTextWrap(false);
  sdm.setTextDatum(CC_DATUM);
  sdm.setTextColor(TFT_WHITE, pinkColor);
  sdm.drawString(third_str, 120, 11);
  sdm.pushSprite(0, 193);
  sdm.deleteSprite();

  sdm.unloadFont();
}

/*电量显示*/
void Battery() {
  uint16_t batteryLevel = analogRead(battery);
  if (batteryLevel >= 2400) {
    TJpgDec.drawJpg(190, 85, battery0, sizeof(battery0));
  } else if (batteryLevel >= 2300) {
    TJpgDec.drawJpg(190, 85, battery1, sizeof(battery1));
  } else if (batteryLevel >= 2200) {
    TJpgDec.drawJpg(190, 85, battery2, sizeof(battery2));
  } else {
    TJpgDec.drawJpg(190, 85, battery3, sizeof(battery3));
  }
}

void change_gif() {
  if(Mode == 1) {
    Mode = 2;
  } else {
    Mode = 1;
  }
  
}

/**
 * 主页面
 */

void show_mg_love_page() {
  // 显示底部文字
  show_buttom(BUTTOM_WORD);

  // 显示恋爱天数
  show_love_day(0);

  Supervisor_love_controller();

  if (Wifi_en == 1) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("WIFI connected");

      get_honeyed_words();

      // WiFi.forceSleepBegin(); // Wifi Off
      Serial.println("WIFI sleep......");
      Wifi_en = 0;
    } else {
      // Serial.println("WIFI 连接失败...");
    }
  }
}

unsigned int mqtt_reconn_count =
    0; // 记录Mqtt重连次数，如果重连失败超过5次，就放弃连接；
bool mqtt_conn_flag = true; // 默认是可以重连的；

void mqtt_reconnect() {
  // Loop until we're reconnected
  if (!mqttClient.connected() && mqtt_conn_flag) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (mqttClient.connect(clientId.c_str())) {
      Serial.println("connected...");
      // Once connected, publish an announcement...
      // mqttClient.publish("outTopic", "hello world");
      // ... and resubscribe
      mqttClient.subscribe(IN_TOPIC);

      // 回复重连次数
      mqtt_reconn_count = 0;
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");

      mqtt_reconn_count++;
      if (mqtt_reconn_count > 5)
        mqtt_conn_flag = false; // 放弃连接
                                // Wait 5 seconds before retrying
                                // delay(5000);
    }
  }
}

void mqtt_callback(char *topic, byte *payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] -->");
  String msg = "";
  for (int i = 0; i < length; i++) {
    // Serial.print((char)payload[i]);
    msg += (char)payload[i];
  }
  Serial.println(msg);

  // 收到想你消息
  if (msg == IN_MSG) {
    role = RECEIVER; // 角色切换到接受者
    pre_page = page; // 记录当前页面再切换
    goto_missu_page("收到：宝贝，我想你辣！", TFT_WHITE); // 切换到想你页面
  }

  if (msg == OUT_MSG_CHECK) {
    goto_missu_page("收到：对方已经收到想念信号", TFT_WHITE);
  }
}

/**
 * 单击按钮
 *
 **/
void leftsingleclick(Button2 &btn) {
  Serial.println("click\n");

  // 如果当前页面不是想你页面,则单击按钮，时钟页面和纪念日页面来回切换
  if (page != MISSU_PAGE) {
    if (page == CLOCK_PAGE) {
      goto_love_page();
    } else {
      goto_clock_page();
    }
    return;
  }

  // 如果是发送想你身份（发送者），单击按钮退出
  if (role == SENDER) {
    goto_page(pre_page); // 回到前一页
  }

  // 如果是接收者（收到想你消息的一方），单击按钮退出，并且发送收到消息确认消息
  if (role == RECEIVER) {

    if (mqttClient.connected()) {
      mqttClient.publish(OUT_TOPIC, IN_MSG_CHECK); // 发送收到消息确认
    }
    goto_page(pre_page);
  }
}

/**
 *
 * 双击按钮，发送想你消息，正常显示发送消息成功
 * 如果网络不连接，或者mqtt服务器断开连接，显示发送失败
 **/
void leftdoubleclick(Button2 &btn) {
  Serial.println("double click\n");

  if (page == MISSU_PAGE && role == SENDER) {
    // 如果当前在想你页面，而且是发送消息身份，不能再发送消息
    // 可以返回在主页面，双击再发送；
    return;
  }

  String word = "已发送：我想你！";
  uint16_t word_color = TFT_WHITE;
  if (mqttClient.connected()) {
    mqttClient.publish(OUT_TOPIC, OUT_MSG);
  } else {
    word = "send fail...";
    word_color = TFT_RED;
  }
  role = SENDER;
  pre_page = page; // 跳转之前记录跳转前的页面
  goto_missu_page(word, word_color);
}

void rightsingleclick(Button2 &btn) {
  analogWrite(LCD_BL_PIN, brightness = (brightness == 15) ? 255 : brightness / 2);
}

void rightdoubleclick(Button2 &btn) {
  change_gif();
}

/**
 * 切换到想你页面，前边做清屏等准备工作，
 **/
void goto_missu_page(String word, uint16_t word_color) {
  // 切换到 纪念日页面
  Serial.println("正在切换到想你页面....");
  clear_screen(); // 清屏
  page = MISSU_PAGE;
  tft.fillScreen(pinkColor);
  show_missu_page(word, word_color);
  Serial.println("已切换到想你页面");
}

/**
 * 切换到时钟页面
 **/
void goto_clock_page() {

  // WiFi.forceSleepWake(); // wifi on
  clear_screen();                                             // 清屏
  page = CLOCK_PAGE;                                          // 记录页面
  TJpgDec.drawJpg(15, 183, temperature, sizeof(temperature)); // 温度图标
  TJpgDec.drawJpg(15, 213, humidity, sizeof(humidity));       // 湿度图标
  Battery();
  digitalClockDisplay(1); // 强制刷新显示时间
  weaterData(&(weather_msg.cityDZ), &(weather_msg.dataSK), &(weather_msg.dataFC)); // 显示缓存温湿度缓存值
  Serial.println("已切换到时钟页面");
}

/**
 * //切换到 纪念日页面
 */
void goto_love_page() {

  clear_screen(); // 清屏
  page = LOVE_PAGE;
  tft.fillScreen(pinkColor); // 填充背景粉色 tft.color565(255,182,193)
                             // tft.color565(255,174,201)
  show_love_day(1);          // 强制刷新显示天数
  show_honey_words(&words);  // 显示情话
  Serial.println("已切换到纪念日页面");
}

void goto_page(byte page) {
  switch (page) {
  case CLOCK_PAGE:
    goto_clock_page();
    break;
  case LOVE_PAGE:
    goto_love_page();
    break;
  default:
    Serial.println("错误");
    break;
  }
}