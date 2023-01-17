
/*
  Заметки ESPшника - Урок 6 - Построение графиков по данным с датчиков в браузере.
  Mautoz Tech https://www.youtube.com/channel/UCWN_KT_CAjGZQG5grHldL8w
  Заметки ESPшника - https://www.youtube.com/channel/UCQAbEIaWFdARXKqcufV6y_g
*/
/*********
  Rui Santos
  Complete project details at https://RandomNerdTutorials.com
  
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files.
  
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
  Default DHT pin is D4 (GPIO2)
*********/


  #include <Arduino.h>
  #include <ESP8266WiFi.h>
  #include <ESPAsyncTCP.h>
  #include <ESPAsyncWebServer.h>
  #include "ESP8266mDNS.h"
  #include "WiFiUdp.h"
  #include "ArduinoOTA.h"
  #include <SoftwareSerial.h>
  #include "config.h"
  #include <AsyncElegantOTA.h>
  #include <TimerMs.h>
  #include <microDS18B20.h>

MicroDS18B20<13> sensor;

TimerMs tmr1(900, 1, 0);  //таймер для запроса температуры 
TimerMs tmr2(1000, 1, 0); //таймер для записи температуры
TimerMs tmr3(330000, 1, 0); //таймер для измерения pm - народный мониторинг
TimerMs tmr4(1000, 1, 0); //таймер для измерения pm - освещенность 
TimerMs tmr5(60000, 1, 0); //таймер для измерения pm - локальный сервер
const char* ssid = "youssid";  // тут название wifi 
const char* password = "youpasswd"; // тут пароль wifi
int prevVal = LOW;


//averaging stuff
float pm1=0;
float pm25=0;
float pm10=0;
float temp=0;
float PM01Mean=0;
float PM2_5Mean=0;
float PM10Mean=0;
float PM01Mean_loc=0;
float PM2_5Mean_loc=0;
float PM10Mean_loc=0;
int p_count=0;
float temp_summ=0;
int t_count=0;
byte conection_try=0;
byte narodmon_conect_count=0;
bool narm_isConnected = false;
float temperature;
const int analogInPin = A0;  // ESP8266 Analog Pin ADC0 = A0
int sensorValue = 0;  // value read from the pot
String Hostname; //имя железки - выглядит как ESPAABBCCDDEEFF т.е. ESP+mac адрес.


//PMS5003 comms stuff
typedef enum {
    FIRSTBYTE,
    SECONDBYTE,
    READ,   
}dataparcer;



int transmitPM01(char *thebuf)
{
  int PM01Val;
  PM01Val=((thebuf[2]<<8) + thebuf[3]); //count PM1.0 value of the air detector module
  return PM01Val;
}
 
int transmitPM2_5(char *thebuf)
{
  int PM2_5Val;
  PM2_5Val=((thebuf[4]<<8) + thebuf[5]);//count PM2.5 value of the air detector module
  return PM2_5Val;
  }
 
int transmitPM10(char *thebuf)
{
  int PM10Val;
  PM10Val=((thebuf[6]<<8) + thebuf[7]); //count PM10 value of the air detector module  
  return PM10Val;
}

//more pms5003 data stuff
char a;
char databuffer[32];
uint8_t i_data=0;
char mystring[10];
bool dataready = false;
dataparcer datastate = FIRSTBYTE;
int PM01Value=0;          //define PM1.0 value of the air detector module
int PM2_5Value=0;         //define PM2.5 value of the air detector module
int PM10Value=0;         //define PM10 value of the air detector module

//serial connection for pms5003 particulate sensor
SoftwareSerial swSer(14, 12);  //RX, TX --- GPIO pins for RX and TX, WeMos D1 Mini is D5, D6, NodeMCU is D5, D6


AsyncWebServer server(80);


void PM_read(){
  while (swSer.available() > 0) {  //wait for data at software serial    
      a = swSer.read();
      switch(datastate){
        case FIRSTBYTE:
          if (a == 0x42) {
            datastate = SECONDBYTE;     
          }
          break;
        case SECONDBYTE:
          if (a == 0x4d) {
            datastate = READ;          
          }
          break;
        case READ:
          databuffer[i_data] = a;
          i_data++;
          if(i_data>29){
            datastate = FIRSTBYTE;
            dataready = true;
            i_data = 0;
          }
          break;
        default:
          break;
      } 
    }
    if(dataready){

        PM01Value = transmitPM01(databuffer); //count PM1.0 value of the air detector module
        PM2_5Value = transmitPM2_5(databuffer); //count PM2.5 value of the air detector module
        PM10Value = transmitPM10(databuffer); //count PM10 value of the air detector module 
            
        dataready = false;

        if(PM01Value > 0 && PM01Value < 500 && PM2_5Value > 0 && PM2_5Value < 500 && PM10Value > 0 && PM10Value < 500){ 
          pm1=pm1+PM01Value;
          pm25=pm25+PM2_5Value;
          pm10=pm10+PM10Value;
          p_count++;
        }
    } 
    if (tmr3.tick()) {
       PM01Mean=pm1/p_count;
       if (isnan(PM01Mean)) PM01Mean = 0;
       PM2_5Mean=pm25/p_count;
       if (isnan(PM2_5Mean)) PM2_5Mean = 0;
       PM10Mean=pm10/p_count;
       if (isnan(PM10Mean)) PM10Mean = 0;
       p_count=0;
       pm1=0;
       pm25=0;
       pm10=0;
    };
    if (tmr5.tick()) {
       PM01Mean_loc=pm1/p_count;
       PM2_5Mean_loc=pm25/p_count;
       PM10Mean_loc=pm10/p_count;   
    };



};


String SendHTML(float temperature, float PM01Mean,float PM2_5Mean, float PM10Mean, int sensorValue, bool narm_isConnected){
String ptr = "<!DOCTYPE html> <html>\n";
ptr +="<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
ptr +="<link href=\"https://fonts.googleapis.com/css?family=Open+Sans:300,400,600\" rel=\"stylesheet\">\n";
ptr +="<title>ESP32 Weather Report</title>\n";
ptr +="<style>html { font-family: 'Open Sans', sans-serif; display: block; margin: 0px auto; text-align: center;color: #333333;}\n";
ptr +="body{margin-top: 50px;}\n";
ptr +="h1 {margin: 50px auto 30px;}\n";
ptr +=".side-by-side{display: inline-block;vertical-align: middle;position: relative;}\n";
ptr +=".humidity-icon{background-color: #3498db;width: 30px;height: 30px;border-radius: 50%;line-height: 36px;}\n";
ptr +=".humidity-text{font-weight: 600;padding-left: 15px;font-size: 19px;width: 160px;text-align: left;}\n";
ptr +=".humidity{font-weight: 300;font-size: 60px;color: #3498db;}\n";
ptr +=".temperature-icon{background-color: #f39c12;width: 30px;height: 30px;border-radius: 50%;line-height: 40px;}\n";
ptr +=".temperature-text{font-weight: 600;padding-left: 15px;font-size: 19px;width: 160px;text-align: left;}\n";
ptr +=".temperature{font-weight: 300;font-size: 60px;color: #f39c12;}\n";
ptr +=".superscript{font-size: 17px;font-weight: 600;position: absolute;right: -70px;top: 15px;}\n";
ptr +=".superscript_temperature{font-size: 17px;font-weight: 600;position: absolute;right: -20px;top: 15px;}\n";
ptr +=".data{padding: 10px;}\n";
ptr +="</style>\n";

ptr +="<script>\n";
ptr +="setInterval(loadDoc,200);\n";
ptr +="function loadDoc() {\n";
ptr +="var xhttp = new XMLHttpRequest();\n";
ptr +="xhttp.onreadystatechange = function() {\n";
ptr +="if (this.readyState == 4 && this.status == 200) {\n";
ptr +="document.getElementById(\"webpage\").innerHTML =this.responseText}\n";
ptr +="};\n";
ptr +="xhttp.open(\"GET\", \"/\", true);\n";
ptr +="xhttp.send();\n";
ptr +="}\n";
ptr +="</script>\n";

ptr +="</head>\n";
ptr +="<body>\n";
ptr +="<div id=\"webpage\">\n";
ptr +="<h1>ESP8266 Weather Report</h1>\n";
ptr +="<div class=\"data\">\n";
ptr +="<div class=\"side-by-side temperature-icon\">\n";
ptr +="<svg version=\"1.1\" id=\"Layer_1\" xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" x=\"0px\" y=\"0px\"\n";
ptr +="width=\"9.915px\" height=\"22px\" viewBox=\"0 0 9.915 22\" enable-background=\"new 0 0 9.915 22\" xml:space=\"preserve\">\n";
ptr +="<path fill=\"#FFFFFF\" d=\"M3.498,0.53c0.377-0.331,0.877-0.501,1.374-0.527C5.697-0.04,6.522,0.421,6.924,1.142\n";
ptr +="c0.237,0.399,0.315,0.871,0.311,1.33C7.229,5.856,7.245,9.24,7.227,12.625c1.019,0.539,1.855,1.424,2.301,2.491\n";
ptr +="c0.491,1.163,0.518,2.514,0.062,3.693c-0.414,1.102-1.24,2.038-2.276,2.594c-1.056,0.583-2.331,0.743-3.501,0.463\n";
ptr +="c-1.417-0.323-2.659-1.314-3.3-2.617C0.014,18.26-0.115,17.104,0.1,16.022c0.296-1.443,1.274-2.717,2.58-3.394\n";
ptr +="c0.013-3.44,0-6.881,0.007-10.322C2.674,1.634,2.974,0.955,3.498,0.53z\"/>\n";
ptr +="</svg>\n";
ptr +="</div>\n";
ptr +="<div class=\"side-by-side temperature-text\">Temperature</div>\n";
ptr +="<div class=\"side-by-side temperature\">";
ptr +=temperature;
ptr +="<span class=\"superscript_temperature\">°C</span></div>\n";
ptr +="</div>\n";

ptr +="<div class=\"data\">\n";
ptr +="<div class=\"side-by-side humidity-icon\">\n";
ptr +="<svg version=\"1.1\" id=\"Layer_2\" xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" x=\"0px\" y=\"0px\"\n\"; width=\"12px\" height=\"17.955px\" viewBox=\"0 0 13 17.955\" enable-background=\"new 0 0 13 17.955\" xml:space=\"preserve\">\n";
ptr +="<path fill=\"#FFFFFF\" d=\"M1.819,6.217C3.139,4.064,6.5,0,6.5,0s3.363,4.064,4.681,6.217c1.793,2.926,2.133,5.05,1.571,7.057\n";
ptr +="c-0.438,1.574-2.264,4.681-6.252,4.681c-3.988,0-5.813-3.107-6.252-4.681C-0.313,11.267,0.026,9.143,1.819,6.217\"></path>\n";
ptr +="</svg>\n";
ptr +="</div>\n";
ptr +="<div class=\"side-by-side humidity-text\">PM1</div>\n";
ptr +="<div class=\"side-by-side humidity\">";
ptr +=PM01Mean;
ptr +="<span class=\"superscript\">мкг/м 3</span></div>\n";
ptr +="</div>\n";

ptr +="<div class=\"data\">\n";
ptr +="<div class=\"side-by-side humidity-icon\">\n";
ptr +="<svg version=\"1.1\" id=\"Layer_2\" xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" x=\"0px\" y=\"0px\"\n\"; width=\"12px\" height=\"17.955px\" viewBox=\"0 0 13 17.955\" enable-background=\"new 0 0 13 17.955\" xml:space=\"preserve\">\n";
ptr +="<path fill=\"#FFFFFF\" d=\"M1.819,6.217C3.139,4.064,6.5,0,6.5,0s3.363,4.064,4.681,6.217c1.793,2.926,2.133,5.05,1.571,7.057\n";
ptr +="c-0.438,1.574-2.264,4.681-6.252,4.681c-3.988,0-5.813-3.107-6.252-4.681C-0.313,11.267,0.026,9.143,1.819,6.217\"></path>\n";
ptr +="</svg>\n";
ptr +="</div>\n";
ptr +="<div class=\"side-by-side humidity-text\">PM2.5</div>\n";
ptr +="<div class=\"side-by-side humidity\">";
ptr +=PM2_5Mean;
ptr +="<span class=\"superscript\">мкг/м 3</span></div>\n";
ptr +="</div>\n";

ptr +="<div class=\"data\">\n";
ptr +="<div class=\"side-by-side humidity-icon\">\n";
ptr +="<svg version=\"1.1\" id=\"Layer_2\" xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" x=\"0px\" y=\"0px\"\n\"; width=\"12px\" height=\"17.955px\" viewBox=\"0 0 13 17.955\" enable-background=\"new 0 0 13 17.955\" xml:space=\"preserve\">\n";
ptr +="<path fill=\"#FFFFFF\" d=\"M1.819,6.217C3.139,4.064,6.5,0,6.5,0s3.363,4.064,4.681,6.217c1.793,2.926,2.133,5.05,1.571,7.057\n";
ptr +="c-0.438,1.574-2.264,4.681-6.252,4.681c-3.988,0-5.813-3.107-6.252-4.681C-0.313,11.267,0.026,9.143,1.819,6.217\"></path>\n";
ptr +="</svg>\n";
ptr +="</div>\n";
ptr +="<div class=\"side-by-side humidity-text\">PM10</div>\n";
ptr +="<div class=\"side-by-side humidity\">";
ptr +=PM10Mean;
ptr +="<span class=\"superscript\">мкг/м 3</span></div>\n";
ptr +="</div>\n";

ptr +="<div class=\"data\">\n";
ptr +="<div class=\"side-by-side humidity-text\">Brightness</div>\n";
ptr +="<div class=\"side-by-side humidity\">";
ptr +=sensorValue;
ptr +="<span class=\"superscript\">ppg</span></div>\n"; //в попугаях, конечно же
ptr +="</div>\n";

ptr +="<div class=\"data\">\n";
ptr +="<div class=\"side-by-side humidity-text\">Narodmon: </div>\n";
ptr +="<div class=\"side-by-side humidity\">";
if (narm_isConnected) ptr +="yes\n"; else ptr +="no\n";
//ptr +(String)narm_isConnected;
ptr +="</div>\n";

ptr +="</div>\n";
ptr +="</body>\n";
ptr +="</html>\n";
return ptr;
}

void notFound(AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
}

bool SendToNarodmon() { // Собственно формирование пакета и отправка.
  temperature = temp_summ/t_count;  //устредняем измерения температуры для точности
  temp_summ = 0;  t_count = 0;      //обнуляем переменные для усреднения температуры
  WiFiClient client;
  String buf;
  buf = "#" + Hostname + "\n"; //mac адрес для авторизации датчика
  buf = buf + "#ds18b20#" + String(temperature) + "#Датчик температуры\n"; //показания температуры
  buf = buf + "#PM1#" + String(PM01Mean) + "#Датчик пыли PMS5003(PM1)\n"; //показания pm1
  buf = buf + "#PM2_5#" + String(PM2_5Mean) + "#Датчик пыли PMS5003(PM2.5)\n"; //показания pm2.5
  buf = buf + "#PM10#" + String(PM10Mean) + "#Датчик пыли PMS5003(PM10)\n"; //показания pm10
  buf = buf + "#foto10kOm#" + String(sensorValue) + "#Фоторезистор\n"; //показания освещенности
  /*int WIFIRSSI=(WiFi.RSSI()+100)*2;
  constrain(WIFIRSSI, 0, 100);
  buf = buf + "#WIFI#"  + String(WIFIRSSI) + "#Уровень WI-FI " + String(WiFi.SSID()) + "\n"; // уровень WIFI сигнала*/
  buf = buf + "##\n"; //окончание передачи

  // попытка подключения
 


  while(!client.connect("narodmon.ru", 8283)) {
   // Serial.println("connection failed"); return false; // не удалось;
   narodmon_conect_count++;
   if (narodmon_conect_count>5) {
     narodmon_conect_count=0;
     narm_isConnected = false;
     return false;
     };
  };
  narm_isConnected = true;

  if (narm_isConnected){
    client.print(buf); // и отправляем данные
    //Serial.print(buf);
    delay(100);
    while (client.available()) {
      String line = client.readStringUntil('\r'); // если что-то в ответ будет - все в Serial
    };
  };
  return true; //ушло
}


void temp_read(){
    // опрашиваем DS  
  if (tmr1.tick()) sensor.requestTemp();
  if (tmr2.tick()){
    if (sensor.readTemp()){ 
      temperature = sensor.getTemp();
      temp_summ = temp_summ + temperature;
      t_count++;
      //Serial.println(temperature);
    } /*else {Serial.println("error get temperature");};*/
  }; 
}

void brightness_read(){
  if (tmr4.tick()) {
    sensorValue = analogRead(analogInPin); 
    if (analogInPin != 0) sensorValue = map(sensorValue, 1024, 1, 1, 1024);
  };
}

void setup(){
   //Serial.begin(9600);
  swSer.begin(9600); 
  if (!SPIFFS.begin ()) {
    //Serial.println ("An Error has occurred while mounting SPIFFS");
    return;
  }
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(10000);
    //Serial.println("Connecting to WiFi..");
    conection_try++;
    if (conection_try > 5) break;
  }
  //Serial.println(WiFi.localIP());
  //запускаем web 
  
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/html", SendHTML(temperature, PM01Mean_loc, PM2_5Mean_loc, PM10Mean_loc, sensorValue, (byte)narm_isConnected));
    });
  server.onNotFound(notFound);
  //Serial.println("HTTP server started");
  AsyncElegantOTA.begin(&server); 
  server.begin();

  Hostname = "ESP" + WiFi.macAddress();
  Hostname.replace(":", "");
  WiFi.hostname(Hostname);
  //Serial.println(Hostname);
}

void loop(){
  PM_read();
  temp_read();
  brightness_read();
  if (tmr3.tick()) SendToNarodmon();
}

