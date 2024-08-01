// Visual Micro is in vMicro>General>Tutorial Mode
// 
/*
    Name:       Rousis_Matrix_IoT.ino
    Created:	2/8/2023 9:39:52 μμ
    Author:     ROUSIS_FACTORY\user
*/

#define PIXELS_X 96
#define PIXELS_Y 16
#define PIXELS_ACROSS 96      //pixels across x axis (base 2 size expected)
#define PIXELS_DOWN	16      //pixels down y axis
#define DRIVER_PIN_EN 26

#include <Arduino.h>
#include <EEPROM.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <String.h>
#include <RousisMatrix16.h>
#include <fonts/Big_font.h>
#include <fonts/Big_font_2.h>
#include <fonts/SystemFont5x7_greek.h>
#include <WiFiClientSecure.h>
#include <Arduino_JSON.h>
#include "time.h"
#include <OneWire.h>
#include <DallasTemperature.h>

#define OPEN_HOT_SPOT 36
#define REG_BRIGHTNESS 20
#define EEP_BRIGHT_ADDRESS 0X100

// GPIO where the DS18B20 is connected to
const int oneWireBus = 32;
// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(oneWireBus);
// Pass our oneWire reference to Dallas Temperature sensor 
DallasTemperature sensors(&oneWire);

const char* ssid = "rousis";
const char* passphrase = "rousis074520198";

uint8_t Clock_Updated_once = 0;
uint8_t Connect_status = 0;
uint8_t Show_IP = 0;
uint8_t Last_Update = 0xff;
char mac_address[18];
String st;
String content;
String esid;
String epass = "";
int i = 0;
int statusCode;

static char page_buf[250];
static char page_line2[250];

uint32_t page_len;
char timedisplay[8];
char datedisplay[8];

bool testWifi(void);
void launchWeb(void);
void setupAP(void);

//Establishing Local server at port 80
WebServer server(80);

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 0; // 7200;
const int   daylightOffset_sec = 0; // 3600;

const char Company[] = { "Rousis LTD" };
const char Device[] = { "Matrix 16 ΙοΤ" };
const char Version[] = { "V.1.2    " };
const char Init_start[] = { "ROUSIS SYSTEMS" };
static char  receive_packet[256] = { 0 };
//uint8_t incoming_bytes = 0;
uint16_t  _cnt_byte = 0;
uint16_t packet_cnt = 0;
uint8_t Address = 1;
byte Select_font = '0';
bool messages_enable = false;
bool test_enable = false;
uint8_t flash_l1 = 0;
uint8_t flash_l2 = 0;
uint8_t double_line = 0;
char page[128] = { 0 };
char page_b[256] = { 0 };
bool flash_on = false;
uint8_t flash_cnt = 0;
uint16_t center_1l = 0;
uint16_t center_2l = 0;
uint8_t char_count_1L = 0; uint8_t char_count_2L = 0;
uint16_t page_center;

//RousisMatrix16 myLED(PIXELS_X, PIXELS_Y, 12, 14, 27, 26, 25, 33); 
RousisMatrix16 myLED(PIXELS_X, PIXELS_Y, 12, 14, 27, 32, 25, 33);    // Uncomment if not using OE pin

#define LED_PIN 22
#define BUZZER 15
#define RS485_PIN_DIR 4
#define RXD2 16
#define TXD2 17

HardwareSerial rs485(1);
#define RS485_WRITE     1
#define RS485_READ      0

hw_timer_t* timer = NULL;
hw_timer_t* flash_timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
portMUX_TYPE falshMux = portMUX_INITIALIZER_UNLOCKED;
// Code with critica section
void IRAM_ATTR onTime() {
    portENTER_CRITICAL_ISR(&timerMux);
    myLED.scanDisplay();
    //digitalWrite (LED_PIN, !digitalRead(LED_PIN)) ;
    portEXIT_CRITICAL_ISR(&timerMux);
}

void IRAM_ATTR FlashInt()
{
    portENTER_CRITICAL_ISR(&falshMux);

    if (flash_on)
    {
        if (double_line && flash_l1)
        {
            myLED.drawString(center_1l, 0, page, char_count_1L, 2);
        }
        else {
            if (flash_l1) { myLED.drawString(center_1l, 0, page, char_count_1L, 1); }
            if (flash_l2) { myLED.drawString(center_2l, 9, page_b, char_count_2L, 1); }
        }
        flash_on = false;
    }
    else {
        if (double_line)
        {
            if (flash_l1) { myLED.clearDisplay(); }
        }
        else {
            if (flash_l1) { myLED.drawFilledBox(0, 0, PIXELS_X, 8, GRAPHICS_OFF); }
            if (flash_l2) { myLED.drawFilledBox(0, 9, PIXELS_X, 16, GRAPHICS_OFF); }
        }
        flash_on = true;
    }

    portEXIT_CRITICAL_ISR(&falshMux);
}

//========================================================================================
void test_patern_slash() {
    uint8_t n = 0x55;

}

char* remove_quotes(char* s1) {
    size_t len = strlen(s1);
    if (s1[0] == '"' && s1[len - 1] == '"') {
        s1[len - 1] = '\0';
        memmove(s1, s1 + 1, len - 1);
    }
    return s1;
}

const char* Host = "app.smart-q.eu";  // Server URL
String jsonBuffer;

void setTimezone(String timezone) {
    Serial.printf("  Setting Timezone to %s\n", timezone.c_str());
    setenv("TZ", timezone.c_str(), 1);  //  Now adjust the TZ.  Clock settings are adjusted to show the new local time
    tzset();
}

WiFiClientSecure client;
//========================================================================================

void setup()
{
    Serial.begin(115200);
    rs485.begin(9600, SERIAL_8N1, RXD2, TXD2);
    pinMode(RS485_PIN_DIR, OUTPUT);
    digitalWrite(RS485_PIN_DIR, RS485_READ);
    sensors.begin();

    myLED.displayEnable();     // This command has no effect if you aren't using OE pin
    myLED.selectFont(SystemFont5x7_greek); //font1

    uint8_t cpuClock = ESP.getCpuFreqMHz();
    //myLED.displayBrightness(0);
    // 
    myLED.displayBrightness(255);
    //myLED.normalMode();    
    // Configure the Prescaler at 80 the quarter of the ESP32 is cadence at 80Mhz
    // 80000000 / 80 = 1000 tics / seconde
    timer = timerBegin(0, 20, true);
    timerAttachInterrupt(timer, &onTime, true);
    Serial.println("Initialize LED matrix display");
    // Sets an alarm to sound every second
    timerAlarmWrite(timer, 10000, true);
    timerAlarmEnable(timer);

    flash_timer = timerBegin(1, cpuClock, true);
    timerAttachInterrupt(flash_timer, &FlashInt, true);
    timerAlarmWrite(flash_timer, 100000, true);
    timerAlarmEnable(flash_timer);

    delay(200);
    //--------------------------------------------------------------------------------
    myLED.drawString(0, 0, "Initialize WiFi", 15, 1);
    delay(3000);
    myLED.clearDisplay();
    timerAlarmDisable(timer);
    //dmd.drawString(0, 0, "Disable Display", 15, GRAPHICS_NORMAL);


    Serial.println("Disconnecting current wifi connection");
    WiFi.disconnect();
    EEPROM.begin(512); //Initialasing EEPROM
    delay(10);
    pinMode(OPEN_HOT_SPOT, INPUT_PULLUP);
    pinMode(OPEN_HOT_SPOT, INPUT);
    Serial.println();
    Serial.println();
    Serial.println("Startup");
    myLED.drawString(0, 0, "Initialize WiFi", 15, 1);
    delay(3000);
    myLED.clearDisplay();
    timerAlarmDisable(timer);
    //dmd.drawString(0, 0, "Disable Display", 15, GRAPHICS_NORMAL);


    Serial.println("Disconnecting current wifi connection");
    WiFi.disconnect();
    EEPROM.begin(512); //Initialasing EEPROM
    delay(10);
    pinMode(OPEN_HOT_SPOT, INPUT_PULLUP);
    pinMode(OPEN_HOT_SPOT, INPUT);
    Serial.println();
    Serial.println();
    Serial.println("Startup");

    Serial.println("Reading EEPROM ssid");


    for (int i = 0; i < 32; ++i)
    {
        esid += char(EEPROM.read(i));
    }
    Serial.println();
    Serial.print("SSID: ");
    Serial.println(esid);
    Serial.println("Reading EEPROM pass");

    for (int i = 32; i < 96; ++i)
    {
        epass += char(EEPROM.read(i));
    }
    Serial.print("PASS: ");
    Serial.println(epass);


    WiFi.begin(esid.c_str(), epass.c_str());
    //Connect_status = testWifi();
    if (testWifi() && digitalRead(OPEN_HOT_SPOT))
    {
        Connect_status = 1;
        Serial.println(" connection status positive");
        //return;
    }
    else
    {
        Serial.print("ESP Board MAC Address:  ");
        Serial.println(WiFi.macAddress());
        Connect_status = 0;
        Serial.println("Connection Status Negative / D15 HIGH");
        Serial.println("Turning the HotSpot On");
        launchWeb();
        setupAP();// Setup HotSpot
    }

    Serial.println();
    Serial.println("Waiting.");

    while ((WiFi.status() != WL_CONNECTED))
    {
        Serial.print(".");
        delay(100);
        server.handleClient();
    }

    if (WiFi.status() == WL_CONNECTED) {
        Connect_status = 1;
    }

    Serial.print("RRSI: ");
    Serial.println(WiFi.RSSI());
    char rssi[] = { "RSSI:           " };

    itoa(WiFi.RSSI(), &rssi[8], 10);

    timerAlarmEnable(timer);
    myLED.clearDisplay();
    myLED.drawString(0, 0, "WiFi Connected!", 15, 1);
    myLED.drawString(0, 8, rssi, 16, 1);
    delay(2000);
    
    Serial.println();
    Serial.print("ESP Board MAC Address:  ");
    Serial.println(WiFi.macAddress());
    String helpmac = WiFi.macAddress();
    mac_address[0] = helpmac[0]; mac_address[1] = helpmac[1];
    mac_address[2] = helpmac[3]; mac_address[3] = helpmac[4];
    mac_address[4] = helpmac[6]; mac_address[5] = helpmac[7];
    mac_address[6] = helpmac[9]; mac_address[7] = helpmac[10];
    mac_address[8] = helpmac[12]; mac_address[9] = helpmac[13];
    mac_address[10] = helpmac[15]; mac_address[11] = helpmac[16];
    mac_address[12] = 0;

    //-------------------------------------------------------------
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    //Europe/Athens	EET-2EEST,M3.5.0/3,M10.5.0/4
    setTimezone("EET-2EEST,M3.5.0/3,M10.5.0/4");
    printLocalTime();
    //--------------------------------------------------------------------------------

    Serial.println("Display Initial Message");

    pinMode(DRIVER_PIN_EN, OUTPUT);
    digitalWrite(DRIVER_PIN_EN, LOW);
    myLED.clearDisplay();
    myLED.drawString(0, 0, Company, 10, 2);
    myLED.drawString(0, 8, Device, 10, 2);
    delay(3000);
    myLED.drawString(0, 8, Version, 10, 2);
    delay(3000);
    myLED.clearDisplay();
    myLED.drawString(0, 0, Init_start, sizeof(Init_start), 2);
    
    myLED.clearDisplay();
    myLED.drawString(0, 0, "ROUSIS LTD - ID:", 16, 1);
    myLED.drawString(0, 8, mac_address, sizeof(mac_address), 1);
    delay(3000);

    Serial.println();
    Serial.println("Waiting.");

    while ((WiFi.status() != WL_CONNECTED))
    {
        Serial.print(".");
        delay(100);
        server.handleClient();
    }

    if (WiFi.status() == WL_CONNECTED) {
        Connect_status = 1;
    }
}


// Add the main program code into the continuous loop() function
void loop()
{
    
    //test_patern_slash();

    Serial.println("\nStarting connection to server...");
    client.setInsecure();//skip verification
    if (!client.connect(Host, 443)) {
        Serial.println("Connection failed!");
        //NotDisplay = 1;
    }
    else {
        Serial.println("Connected to server!");
       
        // Make a HTTP request:
        client.print("GET /djson?mac=");
        client.print(mac_address);
        client.print("&ver=");
        client.print("V.1.1");
        client.println(" HTTP/1.1");
        client.println("Host: app.smart-q.eu");
        client.println("Connection: close");
        client.println();
             
        while (client.connected()) {
            String line = client.readStringUntil('\n');
            if (line == "\r") {
                Serial.println("headers received");
                break;
            }
        }

        // if there are incoming bytes available
        // from the server, read them and print them:
        uint8_t startjson = 0;
        jsonBuffer = "";
        while (client.available()) {
            char c = client.read();
            //Serial.write(c);
            if (c == '{' || startjson) {
                jsonBuffer += c;
                startjson = 1;
            }

        }
        client.stop();

    }

    JSONVar myObject = JSON.parse(jsonBuffer);
    JSONVar value;
    // JSON.typeof(jsonVar) can be used to get the type of the var
    if (JSON.typeof(myObject) == "undefined") {
        Serial.println("Parsing input failed!");
        return;
    }
    /*Serial.print("enable: ");
    Serial.println(myObject["page1"]["enable"]);*/
    String page; String enable;
    int i;
    //------------------------------------------------------------------------------------------------------
        //String page1 = JSON.stringify(myObject["page1"]["text"]);    
    digitalWrite(LED_PIN, LOW);
    //Reading Settings
    Serial.println("Reading Settings:");
    Serial.print("Active: ");
    Serial.println(myObject["settings"]["active"]);
    Serial.print("Clock Display: ");
    Serial.println(myObject["settings"]["clockcheck"]);
    Serial.print("Date Display: ");
    Serial.println(myObject["settings"]["datecheck"]);
    Serial.print("Brightness: ");
    Serial.println(myObject["settings"]["brightness"]);
    Serial.println("--------------------------------------------");
    //------------------------------------------------------------------------------------------------------
    page = myObject["settings"]["active"];
    if (page == "0") {
        Serial.print("Sign is inactive...");
        myLED.clearDisplay();
        myLED.drawString(0, 0, "inactive", 8, 1);
        return;
    }
    //------------------------------------------------------------------------------------------------------
    
    page = myObject["page1"]["enable"];
    if (page == "1") {
        page = JSON.stringify(myObject["page1"]["text"]);
        display_page(page);
        /*int str_len = page.length() + 1;
        char char_array[str_len];
        page.toCharArray(char_array, str_len);
        remove_quotes(char_array);
        decodeUTF8(char_array);
        Serial.print("Printed page len: ");
        Serial.println(page_len);
        Serial.println(page);
        Serial.println("--------------------------------------------");
        myLED.clearDisplay();


        if (page_len > 16) {
            myLED.scrollingString(0, 0, page_buf, page_len, 1, 2);
        }
        else {
            myLED.drawString(0, 0, page_buf, page_len, 1);
        }
        Serial.println();
        delay(3000);*/

    }

    page = myObject["page2"]["enable"];
    if (page == "1") {
        page = JSON.stringify(myObject["page2"]["text"]);
        display_page(page);
        /*int str_len = page.length() + 1;
        char char_array[str_len];
        page.toCharArray(char_array, str_len);
        remove_quotes(char_array);
        decodeUTF8(char_array);
        Serial.print("Printed page len: ");
        Serial.println(page_len);
        Serial.println(page);
        Serial.println("--------------------------------------------");
        myLED.clearDisplay();
        if (page_len > 16) {
            myLED.scrollingString(0, 0, page_buf, page_len, 1, 2);
        }
        else {
            myLED.drawString(0, 0, page_buf, page_len, 1);
        }
        Serial.println();
        delay(3000);*/
    }

    page = myObject["page3"]["enable"];
    if (page == "1") {
        page = JSON.stringify(myObject["page3"]["text"]);
        display_page(page);
        /*int str_len = page.length() + 1;
        char char_array[str_len];
        page.toCharArray(char_array, str_len);
        remove_quotes(char_array);
        decodeUTF8(char_array);
        Serial.print("Printed page len: ");
        Serial.println(page_len);
        Serial.println(page);
        Serial.println("--------------------------------------------");
        myLED.clearDisplay();
        if (page_len > 16) {
            myLED.scrollingString(0, 0, page_buf, page_len, 1, 2);
        }
        else {
            myLED.drawString(0, 0, page_buf, page_len, 1);
        }
        Serial.println();
        delay(3000);*/
    }
    printLocalTime();
    //------------------------------------------------------------------------------------------------------
    page = myObject["settings"]["clockcheck"];
    if (page == "1") {
        myLED.selectFont(Big_font);

        myLED.clearDisplay();
        page_center = PIXELS_X - count_line(timedisplay, sizeof(timedisplay), 2);
        myLED.drawString(page_center/2, 0, timedisplay, sizeof(timedisplay), 2);
        delay(1000);
        printLocalTime();
        myLED.clearDisplay();
        page_center = PIXELS_X - count_line(timedisplay, sizeof(timedisplay), 2);
        myLED.drawString(page_center / 2, 0, timedisplay, sizeof(timedisplay), 2);
        delay(1000);
        printLocalTime();
        myLED.clearDisplay();
        page_center = PIXELS_X - count_line(timedisplay, sizeof(timedisplay), 2);
        myLED.drawString(page_center / 2, 0, timedisplay, sizeof(timedisplay), 2);
        delay(1000);
    }
    //---------------------------------------------------------------------------------------
    page = myObject["settings"]["datecheck"];
    if (page == "1") {
        myLED.selectFont(Big_font);
        myLED.clearDisplay();
        myLED.clearDisplay();
        page_center = PIXELS_X - count_line(datedisplay, sizeof(datedisplay), 2);
        myLED.drawString(page_center/2, 0, datedisplay, sizeof(datedisplay), 2);
        delay(3000);
    }
    page = myObject["settings"]["tempcheck"];
    if (page == "1") {
        myLED.selectFont(Big_font);        
        sensors.requestTemperatures();
        float temperature = sensors.getTempCByIndex(0);
        Serial.print(temperature);
        Serial.println("ºC");
        char buf[10] = { 0 };
        snprintf(buf, sizeof(buf), "%g", temperature);
        uint8_t count;
        for (count = 0; count < sizeof(buf); count++)
        {
            if (buf[count] == '.')
            {
                buf[count + 2] = 0;
            }
            if (buf[count]==0)
                break;
                
        }
        buf[count++] = 176; buf[count++] = 'C'; buf[count++] = 0; //176 = º
        
        myLED.clearDisplay();
        page_center = PIXELS_X - count_line(buf, count, 2) ;

        myLED.drawString(page_center/2, 0, buf, count, 2);
        delay(3000);
    }
}


void printLocalTime() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Failed to obtain time");
        return;
    }

    Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");

    char timehelp[3];
    strftime(timehelp, 3, "%H", &timeinfo);
    timedisplay[0] = timehelp[0]; timedisplay[1] = timehelp[1]; timedisplay[2] = ':';
    strftime(timehelp, 3, "%M", &timeinfo);
    timedisplay[3] = timehelp[0]; timedisplay[4] = timehelp[1]; timedisplay[5] = ':';
    strftime(timehelp, 3, "%S", &timeinfo);
    timedisplay[6] = timehelp[0]; timedisplay[7] = timehelp[1];

    strftime(timehelp, 3, "%d", &timeinfo);
    datedisplay[0] = timehelp[0]; datedisplay[1] = timehelp[1]; datedisplay[2] = '/';
    strftime(timehelp, 3, "%m", &timeinfo);
    datedisplay[3] = timehelp[0]; datedisplay[4] = timehelp[1]; datedisplay[5] = '/';
    strftime(timehelp, 3, "%y", &timeinfo);
    datedisplay[6] = timehelp[0]; datedisplay[7] = timehelp[1];
}

//----------------------------------------------- Fuctions used for WiFi credentials saving and connecting to it which you do not need to change
bool testWifi(void)
{
    int c = 0;
    Serial.println("Waiting for Wifi to connect");
    while (c < 20) {
        if (WiFi.status() == WL_CONNECTED)
        {
            Serial.println("");
            return true;
        }
        delay(500);
        Serial.print("*");
        c++;
    }
    Serial.println("");
    Serial.println("Connect timed out, opening AP");
    return false;
}

void launchWeb()
{
    Serial.println("");
    if (WiFi.status() == WL_CONNECTED)
        Serial.println("WiFi connected");
    Serial.print("Local IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("SoftAP IP: ");
    Serial.println(WiFi.softAPIP());
    createWebServer();
    // Start the server
    server.begin();
    Serial.println("Server started");
}

void setupAP(void)
{
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    int n = WiFi.scanNetworks();
    Serial.println("scan done");
    if (n == 0)
        Serial.println("no networks found");
    else
    {
        Serial.print(n);
        Serial.println(" networks found");
        for (int i = 0; i < n; ++i)
        {
            // Print SSID and RSSI for each network found
            Serial.print(i + 1);
            Serial.print(": ");
            Serial.print(WiFi.SSID(i));
            Serial.print(" (");
            Serial.print(WiFi.RSSI(i));
            Serial.print(")");
            //Serial.println((WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*");
            delay(10);
        }
    }
    Serial.println("");
    st = "<ol>";
    for (int i = 0; i < n; ++i)
    {
        // Print SSID and RSSI for each network found
        st += "<li>";
        st += WiFi.SSID(i);
        st += " (";
        st += WiFi.RSSI(i);

        st += ")";
        //st += (WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*";
        st += "</li>";
    }
    st += "</ol>";
    delay(100);
    WiFi.softAP("Rousis_ESP32", "");
    Serial.println("Initializing_softap_for_wifi credentials_modification");
    launchWeb();
    Serial.println("over");
}

void createWebServer()
{
    {
        server.on("/", []() {

            IPAddress ip = WiFi.softAPIP();
            String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
            content = "<!DOCTYPE HTML>\r\n<html>Welcome to ROUSIS_Wifi Credentials Update page";
            content += "<form action=\"/scan\" method=\"POST\"><input type=\"submit\" value=\"scan\"></form>";
            content += ipStr;
            content += "<p>";
            content += st;
            content += "</p><form method='get' action='setting'><label>SSID: </label><input name='ssid' length=32><input name='pass' length=64><input type='submit'></form>";
            content += "</html>";
            server.send(200, "text/html", content);
        });
        server.on("/scan", []() {
            //setupAP();
            IPAddress ip = WiFi.softAPIP();
            String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);

            content = "<!DOCTYPE HTML>\r\n<html>go back";
            server.send(200, "text/html", content);
        });

        server.on("/setting", []() {
            String qsid = server.arg("ssid");
            String qpass = server.arg("pass");
            if (qsid.length() > 0 && qpass.length() > 0) {
                Serial.println("clearing eeprom");
                for (int i = 0; i < 96; ++i) {
                    EEPROM.write(i, 0);
                }
                Serial.println(qsid);
                Serial.println("");
                Serial.println(qpass);
                Serial.println("");

                Serial.println("writing eeprom ssid:");
                for (int i = 0; i < qsid.length(); ++i)
                {
                    EEPROM.write(i, qsid[i]);
                    Serial.print("Wrote: ");
                    Serial.println(qsid[i]);
                }
                Serial.println("writing eeprom pass:");
                for (int i = 0; i < qpass.length(); ++i)
                {
                    EEPROM.write(32 + i, qpass[i]);
                    Serial.print("Wrote: ");
                    Serial.println(qpass[i]);
                }
                EEPROM.commit();

                content = "{\"Success\":\"saved to eeprom... reset to boot into new wifi\"}";
                statusCode = 200;
                ESP.restart();
            }
            else {
                content = "{\"Error\":\"404 not found\"}";
                statusCode = 404;
                Serial.println("Sending 404");
            }
            server.sendHeader("Access-Control-Allow-Origin", "*");
            server.send(statusCode, "application/json", content);

        });
    }
}

void decodeUTF8(char str[]) {
    page_buf[0] = 0; page_len = 0;
    unsigned int a = 0;
    char get_C = 0xff;
    size_t i = 0;
    while (get_C != 0)
    {
        get_C = str[i];
        if (get_C == 0xCE) { //Greek Windows-1253 or ISO/IEC 8859-7 -> UTF-8
            i++;
            get_C = str[i];
            page_buf[a++] = (get_C + 0x30);
        }
        else if (get_C == 0xCF) { //Greek Windows-1253 or ISO/IEC 8859-7 -> UTF-8
            i++;
            get_C = str[i];
            page_buf[a++] = (get_C + 0x70);
        }
        else {
            page_buf[a++] = get_C;
        }
        i++;
        page_len++;
    }
    page_buf[a++] = 0;
    page_len--;
    Serial.println("/");
}

uint16_t count_line(char str[], uint16_t size_p, uint8_t space) {
    uint16_t Legth_p = 0;

    for (size_t a = 0; a < size_p; a++)
    {
        Legth_p += myLED.charWidth(str[a]) + space;
    }
    return Legth_p;
}

void display_page(String page_str) {
    int str_len = page_str.length() + 1;
    char char_array[str_len];
    page_str.toCharArray(char_array, str_len);
    remove_quotes(char_array);
    decodeUTF8(char_array);
    Serial.print("Printed page len: ");
    Serial.println(page_len);
    Serial.println(page_str);
    
    myLED.clearDisplay();
    double_line = 0;
    uint8_t l2_cnt = 0;
    uint8_t l1_cnt = page_len;
    for (size_t i = 0; i < sizeof(page_buf); i++)
    {
        if (page_buf[i] == '|')
        {
            double_line = 1;
            page_buf[i] = 0;
            l1_cnt = i;
        }
        else if (page_buf[i] == 0) {
            break;
        }
        else if (double_line) {
            page_line2[l2_cnt++] = page_buf[i];
        }
    }

    if (double_line)
    {
        myLED.selectFont(SystemFont5x7_greek);
        page_center = PIXELS_X - count_line(page_buf, l1_cnt, 1);
        myLED.drawString(page_center/2, 0, page_buf, l1_cnt, 1);
        page_center = count_line(page_line2, l2_cnt, 1);
        if (page_center <= PIXELS_X) {
            page_center = PIXELS_X - page_center;
            myLED.drawString(page_center / 2, 9, page_line2, l2_cnt, 1);
            delay(3000);
        }
        else {
            myLED.scrollingString(0, 9, page_buf, page_len, 1, 2);
        }
        
    }
    else {
        myLED.selectFont(Big_font);
        page_center = count_line(page_buf, page_len, 2);
        if (page_center <= PIXELS_X)
        {
            page_center = PIXELS_X - page_center;
            myLED.drawString(page_center / 2, 0, page_buf, page_len, 2);
            delay(3000);
        }
        else {
            myLED.scrollingString(0, 0, page_buf, page_len, 2, 2);
        }
    }   

    Serial.print("double_line: ");
    Serial.println(double_line);
    Serial.print("line 1 len: ");
    Serial.println(l1_cnt);
    Serial.print("line 2 len: ");
    Serial.println(l2_cnt);
    Serial.print("page_center: ");
    Serial.println(page_center);
    Serial.println("--------------------------------------------");  
}