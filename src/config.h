//#######################################################
//   时钟页面配置
//#######################################################
#define TMS 1000               //一千毫秒
#define SD_FONT_YELLOW 0xD404  // 黄色字体颜色
#define SD_FONT_WHITE 0xFFFF   // 白色字体颜色
#define WEATHER_UPDATE_TIME 5 //默认天气更新时间
#define timeY 82               // 定义高度

//#######################################################
//    纪念日页面配置
//#######################################################
#define DEFAULT_WORDS "{\"word\":\"因有着你，跟你一起，亲爱的你\"}" //只能修改汉字内容
#define WORD_UPDATE_TIME 3                                          //情话切换时间单位 - 分钟
#define ANIMATE_CHANGE_FREQUENTCE 20                                //动画切换频次，20次切换一张

#define BUTTOM_WORD "Love since 2021.05.20"                         //纪念日页面底部文字

#define YEAR 2021  //纪念日 年
#define MONTH  5  //纪念日  月
#define DAY 20   // 纪念日  日

//#######################################################
//    想你页面配置
//#######################################################

// mqtt 配置
#define MQTT_IP "test.ranye-iot.net" // mqtt服务器地址  test.ranye-iot.net

//1是我的小电视，0是女朋友的小电视
#if 0
    #define OUT_TOPIC "baoge"  //本机要推送消息的主题
    #define IN_TOPIC "baoge_girl"    //本机订阅消息的主题
#else
    #define OUT_TOPIC "baoge_girl"  //本机要推送消息的主题
    #define IN_TOPIC "baoge"    //本机订阅消息的主题
#endif

#define IN_MSG "imu"          //收到消息
#define IN_MSG_CHECK "check"  //收到消息确认
#define OUT_MSG "imu"         //推送消息
#define OUT_MSG_CHECK "check" //推送消息确认
