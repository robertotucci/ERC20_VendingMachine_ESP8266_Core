#include <Adafruit_ILI9341.h>
#include <Adafruit_GFX.h>
#include "qrcode.h"
#include <Button2.h>
#include <ESPRotary.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <Ticker.h>
#include <MQTT.h>
#include <PubSubClient.h>
#include <secrets.h>

uint8_t frame = 1;
uint8_t frameMax = 1;
uint8_t reminderFrame = 0;
uint8_t catalogueSize = 0;
uint8_t page = 1;
String balance;

uint8_t numElementForFrame = 4;
uint8_t yCursorItemDiff = 30;
uint8_t positionInFrame = 1;

boolean balanceChanged = false;

uint8_t last = 0;
int price = 0;

//Screen
#define TFT_DC 2
#define TFT_CS 15

Adafruit_ILI9341 display = Adafruit_ILI9341(TFT_CS, TFT_DC);

//Rotary encoder
#define ROTARY_PIN1 5
#define ROTARY_PIN2 16
#define BUTTON_PIN 4

ESPRotary r = ESPRotary(ROTARY_PIN1, ROTARY_PIN2, BUTTON_PIN);
Button2 b = Button2(BUTTON_PIN);

//Ticker loading
uint16_t loadingAxis = 0;
void loading();
Ticker timerLoading(loading, 8000);

String catalogue[15][2];

WiFiClient wifi_client;
PubSubClient mqtt_client(wifi_client, MQTT_SERVER, MQTT_BROKER_PORT);
bool mqtt_status;

void setup()
{

  Serial.begin(115200);
  display.begin();
  display.setRotation(3);
  loadingScreen("Loading...");
  wifiConnection();
  getTransaction();

  balanceChanged = false;
  getCatalogue();

  display.fillScreen(ILI9341_BLACK);
  drawMenu();
  r.setChangedHandler(rotate);

  //b.setClickHandler(showPosition);
  b.setClickHandler(push);
  //servo1.attach(14);
  mqtt_status = MQTT_init();
  if (!mqtt_status)
    Serial.println("MQTT connection ERROR....");
  else
    Serial.println("MQTT connection OK....");
}

void loop()
{
  r.loop();
  b.loop();

  switch (page)
  {
  case 2:
    if (loadingAxis == 320 || balanceChanged)
    {

      Serial.println("Stop timer");
      Serial.println("MOTORINO GO!!!");
      timerLoading.stop();
      rotateServo();
      resetAllData();
      display.fillScreen(ILI9341_BLACK);
      drawMenu();
    }
    else
    {
      timerLoading.update();
    }
    break;
  }
}

void rotate(ESPRotary &r)
{
  switch (page)
  {
  case 1:
    if (r.getPosition() < 0 || last > (catalogueSize - 1))
    {
      Serial.println("RESET");
      r.resetPosition();
      resetAllData();
      drawMenu();
      return;
    }

    Serial.println((String) "position -> " + r.getPosition());
    Serial.println((String) "positionInFrame -> " + positionInFrame);
    Serial.println((String) "last -> " + last);
    last = r.getPosition();

    if (r.getPosition() > last) //We have turned the Rotary Encoder Clockwise
    {
      Serial.println("VADO GIU");
      positionInFrame++;

      if (positionInFrame > numElementForFrame)
      {
        Serial.println("qui1");
        if (frame < frameMax)
        {
          Serial.println("qui2");
          Serial.println("aggiungo frame");
          display.fillScreen(ILI9341_BLACK);
          positionInFrame = 1;
          frame++;
          //numElementForFrame = numElementForFrame + numElementForFrame;
        }
      }
    }
    else
    {
      Serial.println("VADO SU");
      positionInFrame--;
      if (positionInFrame < 1)
      {
        if (frame > 1)
        {
          Serial.println("rimuovo frame");
          positionInFrame = numElementForFrame;
          frame--;
          display.fillScreen(ILI9341_BLACK);
          //numElementForFrame = numElementForFrame - numElementForFrame;
        }
      }
    }

    drawMenu();
    break;

  default:
    r.resetPosition();
    last = 0;
    break;
  }
}

void push(Button2 &btn)
{
  r.resetPosition();
  switch (page)
  {
  case 1:
    page = 2;
    mqtt_client.publish(MQTT_TOPIC, "yellow");
    Serial.println("MQTT message sent....");
    showQrCode();
    timerLoading.start();
    break;

  default:
    Serial.println("PUSH");
    break;
  }
}

void showQrCode()
{
  display.fillScreen(ILI9341_BLACK);

  QRCode qrcode;

  // Allocate a chunk of memory to store the QR code
  uint8_t qrcodeData[qrcode_getBufferSize(2000)];

  uint8_t qrx = 50;
  uint8_t qry = 10;

  qrcode_initText(&qrcode, qrcodeData, 6, ECC_LOW, QRCODE_ETH); //122
  for (uint8_t y = 0; y < qrcode.size; y++)
  {
    for (uint8_t x = 0; x < qrcode.size; x++)
    {
      if (qrcode_getModule(&qrcode, x, y))
      {
        //Serial.print("**");
        display.fillRect(qrx, qry, 5, 5, ILI9341_WHITE);
        qrx = qrx + 5;
      }
      else
      {
        //Serial.print("  ");
        display.fillRect(qrx, qry, 5, 5, ILI9341_BLACK);
        qrx = qrx + 5;
      }
    }
    qrx = 50;
    qry = qry + 5;
  }
}

void drawMenu()
{

  display.setTextSize(3);
  //display.clearDisplay();
  display.setTextColor(ILI9341_DARKGREEN, ILI9341_BLACK);
  display.setCursor(95, 0);
  display.println(TITLE);
  display.drawFastHLine(0, 30, 320, ILI9341_WHITE); //BLACK

  if (last > (catalogueSize - 1))
  {
    Serial.println("sei oltre!");
    Serial.println(catalogueSize - 1);
    r.resetPosition();
    resetAllData();
  }

  uint8_t maxIndex = 0;
  if (frame == frameMax)
  {
    maxIndex = catalogueSize - 1;
  }
  else
  {
    maxIndex = (frame * numElementForFrame) - 1;
  }
  uint8_t minIndex = (frame * numElementForFrame) - numElementForFrame;
  uint8_t curs = 40;

  Serial.println((String) "numElementForFrame -> " + numElementForFrame);
  Serial.println((String) "maxIndex -> " + maxIndex);
  Serial.println((String) "minIndex -> " + minIndex);
  Serial.println((String) "frame -> " + frame);

  for (uint8_t i = minIndex; i <= maxIndex; i++)
  {
    if (i == last)
    {
      price = catalogue[i][1].toInt();
      Serial.println(price);
      displayMenuItem(catalogue[i][0], curs, true);
    }
    else
    {
      displayMenuItem(catalogue[i][0], curs, false);
    }
    curs = curs + yCursorItemDiff;
  }
}

void displayMenuItem(String item, uint8_t position, boolean selected)
{
  if (selected)
  {
    display.setTextColor(ILI9341_BLACK, ILI9341_WHITE);
  }
  else
  {
    display.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  }
  display.setCursor(0, position);
  display.print("> " + item);
}

String getRestService(const char *host, const char *url, const char *fingerprint)
{
  WiFiClientSecure client;
  // Use WiFiClientSecure class to create TLS connection
  Serial.print("connecting to ");
  Serial.println(host);

  Serial.printf("Using fingerprint '%s'\n", fingerprint);
  client.setFingerprint(fingerprint);

  if (!client.connect(host, HTTPS_PORT))
  {
    Serial.println("connection failed");
    return "";
  }

  //String url = "/dev/getAll";
  Serial.print("requesting URL: ");
  Serial.println(url);

  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "User-Agent: BuildFailureDetectorESP8266\r\n" +
               "Connection: close\r\n\r\n");

  Serial.println("request sent");
  while (client.connected())
  {
    String line = client.readStringUntil('\n');
    if (line == "\r")
    {
      Serial.println("headers received");
      break;
    }
  }
  String line = client.readStringUntil('\n');

  Serial.println("reply was:");
  Serial.println("==========");
  Serial.println(line);
  Serial.println("==========");

  return line;
}

void getCatalogue()
{
  String catalogueJson = getRestService(HOST_AWS, URL_AWS, FINGERPRINT_AWS);

  const size_t capacity = JSON_ARRAY_SIZE(15) + 15 * JSON_OBJECT_SIZE(3) + 480;
  DynamicJsonDocument doc(capacity);

  const char *json = catalogueJson.c_str();

  deserializeJson(doc, json);

  catalogueSize = doc.size();
  for (int i = 0; i < catalogueSize; i++)
  {
    JsonObject root_0 = doc[i];
    uint8_t root_0_idcatalogue = root_0["idcatalogue"];
    String root_0_name = root_0["name"];
    String root_0_price = root_0["price"];
    catalogue[i][0] = root_0_name;
    catalogue[i][1] = root_0_price;
  }
  doc.clear();
  double value = doc.size() / numElementForFrame;
  int left_part, right_part;
  char buffer[50];
  sprintf(buffer, "%lf", value);
  sscanf(buffer, "%d.%d", &left_part, &right_part);
  reminderFrame = doc.size() % numElementForFrame;
  if (reminderFrame > 0)
  {
    left_part++;
  }
  frameMax = left_part;

  Serial.println((String) "frameMax -> " + frameMax);
}

void getTransaction()
{
  String bJson = getRestService(HOST_ETHSCAN, URL_ETHSCAN, FINGERPRINT_ETHSCAN);

  const size_t capacity = JSON_OBJECT_SIZE(3) + 60;
  DynamicJsonDocument doc(capacity);

  const char *json = bJson.c_str();

  deserializeJson(doc, json);

  String balanceTemp = doc["result"];
  doc.clear();
  Serial.println((String) "balanceTemp -> " + balanceTemp);
  if (balanceTemp != NULL && (balanceTemp != balance) && page == 2)
  {
    Serial.println((String) "Balance cambiato!!");
    balanceChanged = true;
  }
  balance = balanceTemp;
  //String bal = String(b);
  Serial.println((String) "balance -> " + balance);
}

void loading()
{
  digitalWrite(0, HIGH);
  if (loadingAxis < 320)
  {
    display.fillRect(loadingAxis, 235, 40, 5, ILI9341_RED);
    loadingAxis = loadingAxis + 40;
  }
  getTransaction();
  digitalWrite(0, LOW);
}

void resetAllData()
{
  loadingAxis = 0;
  balanceChanged = false;
  //countLoading = 1;
  page = 1;
  last = 0;
  frame = 1;
  positionInFrame = 1;
}
void rotateServo()
{

  for (int pos = 0; pos <= 360; pos += 1)
  { // goes from 0 degrees to 180 degrees
    // in steps of 1 degree
    //servo1.write(pos);              // tell servo to go to position in variable 'pos'
  }
}

void loadingScreen(String text)
{
  display.fillScreen(ILI9341_DARKGREEN);
  display.setTextSize(3);
  display.setCursor(75, 110);
  display.print(text);
}
void wifiConnection()
{
  Serial.println();
  Serial.print("connecting to ");
  Serial.println(SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWORD);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

//MQTT callback function invoked for every MQTT received message on a subscribed topic
void mqtt_callback(const MQTT::Publish &pub)
{
  Serial.println("MQTT receiving a message:");
  Serial.println(pub.payload_string());
}

int MQTT_init()
{
  Serial.println("Initializing MQTT communication.........");

  mqtt_client.set_callback(mqtt_callback); //set callback on received messages
  mqtt_client.set_max_retries(255);

  //here we connect to MQTT broker and we increase the keepalive for more reliability
  if (mqtt_client.connect(MQTT::Connect(MQTT_CLIENT_ID).set_keepalive(90).set_auth(String(MQTT_UNAME), String(MQTT_PASSW))))
  {
    Serial.println("Connection to MQTT broker SUCCESS..........");
  }
  else
  {
    Serial.println("Connection to MQTT broker ERROR..........");
  }

  return mqtt_client.connected();
}
