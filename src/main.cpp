#include <Arduino.h>
#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#endif
#ifdef ESP32
#include <WiFi.h>
#endif
#include <SD.h>

#ifdef TOUCH_INPUTS
#define TOUCH_TH 16
#define PIN_PRESSED(pin) ((touchRead(pin) > 0) && (touchRead(pin) < TOUCH_TH))
#else
#define PIN_PRESSED(pin) (digitalRead(pin) == LOW)
#endif

#include <AudioFileSource.h>
#ifdef BOARD_HAS_PSRAM
#include <AudioFileSourceSPIRAMBuffer.h>
#else
#include <AudioFileSourceBuffer.h>
#endif
#include <AudioFileSourceICYStream.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutputI2S.h>
#include <AudioOutputI2SNoDAC.h>

#include <SPI.h>
#include <TFT_eSPI.h>
#define SCREEN_WIDTH (((TFT_ROTATION == 0) || (TFT_ROTATION == 2)) ? TFT_WIDTH : TFT_HEIGHT)
#define SCREEN_HEIGHT (((TFT_ROTATION == 0) || (TFT_ROTATION == 2)) ? TFT_HEIGHT : TFT_WIDTH)
#define SCREEN_HEIGHT8 (SCREEN_HEIGHT >> 3)

#include <spiram-fast.h>

#include "frame.h"
// #include "background.h"
#include "Orbitron_Medium_20.h"

TFT_eSPI tft = TFT_eSPI(); // Invoke library, pins defined in User_Setup.h
#define TFT_GREY 0x5AEB    // New colour

#ifdef ESP32
const int pwmFreq = 5000;
const int pwmResolution = 8;
const int pwmLedChannelTFT = 0;
#endif

char *arrayURL[5] = {
    "http://51.144.137.103:18000/fg\0",
    "http://51.144.137.103:18000/mah\0"
    "http://51.144.137.103:18000/em\0"
    "http://51.144.137.103:18000/jb\0"
    "http://51.144.137.103:18000/jd\0"};

String arrayStation[8] = {
    "Fabio and Grooverider",
    "Mary Anne Hobbs",
    "Essential Mix",
    "John B",
    "John Digweed",
};

AudioGeneratorMP3 *mp3;
AudioFileSourceICYStream *file;
#ifdef BOARD_HAS_PSRAM
AudioFileSourceSPIRAMBuffer *buff;
#else
AudioFileSourceBuffer *buff;
#endif
AudioOutputI2S *out;

#ifdef ESP8266
const int preallocateBufferSize = 5 * 1024;
const int preallocateCodecSize = 29192; // MP3 codec max mem needed
#else
#ifdef BOARD_HAS_PSRAM
const int preallocateBufferSize = 1024 * 1024;
const int preallocateCodecSize = 85332; // AAC+SBR codec max mem needed
#else
const int preallocateBufferSize = 16 * 1024;
const int preallocateCodecSize = 85332; // AAC+SBR codec max mem needed
#endif
#endif

const int numCh = sizeof(arrayURL) / sizeof(char *);
bool TestMode = false;
uint32_t LastTime = 0;
int playflag = 0;
int ledflag = 0;
float fgain = 4.0;
int sflag = 0;
char *URL = arrayURL[sflag];
String station = arrayStation[sflag];

int backlight[5] = {10, 30, 60, 120, 220};
byte b = 2;
int press1 = 0;
int press2 = 0;
bool inv = 0;

void MDCallback(void *cbData, const char *type, bool isUnicode, const char *string)
{
  const char *ptr = reinterpret_cast<const char *>(cbData);
  (void)isUnicode; // Punt this ball for now
  // Note that the type and string may be in PROGMEM, so copy them to RAM for printf
  char s1[32], s2[64];
  strncpy_P(s1, type, sizeof(s1));
  s1[sizeof(s1) - 1] = 0;
  strncpy_P(s2, string, sizeof(s2));
  s2[sizeof(s2) - 1] = 0;
  Serial.printf("METADATA(%s) '%s' = '%s'\n", ptr, s1, s2);
  Serial.flush();
}

void StatusCallback(void *cbData, int code, const char *string)
{
  const char *ptr = reinterpret_cast<const char *>(cbData);
  // Note that the string may be in PROGMEM, so copy it to RAM for printf
  char s1[64];
  strncpy_P(s1, string, sizeof(s1));
  s1[sizeof(s1) - 1] = 0;
  Serial.printf("STATUS(%s) '%d' = '%s'\n", ptr, code, s1);
  Serial.flush();
}

void StartPlaying()
{
  file = new AudioFileSourceICYStream(URL);
  file->RegisterMetadataCB(MDCallback, (void *)"ICY");
  #ifdef BOARD_HAS_PSRAM
  buff = new AudioFileSourceSPIRAMBuffer(file, preallocateBufferSize);
  #else
  buff = new AudioFileSourceBuffer(file, preallocateBufferSize);
  #endif
  buff->RegisterStatusCB(StatusCallback, (void *)"buffer");
  
  out = new AudioOutputI2S();
  //out->SetOutputModeMono(true);
  out->SetGain(fgain * 0.05);
  mp3 = new AudioGeneratorMP3(); //preallocateCodecSize);
  mp3->RegisterStatusCB(StatusCallback, (void *)"mp3");
  mp3->begin(buff, out);
  Serial.printf("STATUS(URL) %s \n", URL);
  Serial.flush();
  tft.drawString("Playing!   ", (SCREEN_WIDTH >> 1), SCREEN_HEIGHT8 * 2, 2);
}

void StopPlaying()
{
  if (mp3)
  {
    mp3->stop();
    delete mp3;
    mp3 = NULL;
  }
  if (buff)
  {
    buff->close();
    delete buff;
    buff = NULL;
  }
  if (file)
  {
    file->close();
    delete file;
    file = NULL;
  }
  Serial.printf("STATUS(Stopped)\n");
  Serial.flush();
}

void initWifi()
{
  WiFi.disconnect();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  int i = 0;
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print("STATUS(Connecting to WiFi) ");
    delay(1000);
    i = i + 1;
    if (i > 10)
    {
      ESP.restart();
    }
  }
  Serial.println("Wifi OK");
}

void setup()
{
  Serial.begin(SERIAL_BAUD);
  Serial.println("Starting...");

// pinMode(LED, OUTPUT);
// digitalWrite(LED, HIGH);

#ifndef TOUCH_INPUTS
  pinMode(PIN_BTN1, INPUT);
  pinMode(PIN_BTN2, INPUT);
  pinMode(PIN_BTN3, INPUT);
#endif

#if defined(ESP32) && defined(TFT_BL)
  ledcSetup(pwmLedChannelTFT, pwmFreq, pwmResolution);
  ledcAttachPin(TFT_BL, pwmLedChannelTFT);
  ledcWrite(pwmLedChannelTFT, backlight[b]);
#endif

  tft.init();
  tft.setRotation(TFT_ROTATION);
  tft.setSwapBytes(true);
  tft.fillScreen(TFT_BLACK);
  // tft.pushImage(0, 0, BG_WIDTH, BG_HEIGHT, background);

  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, TFT_RED);
  tft.setFreeFont(&Orbitron_Medium_20);
  tft.setCursor(14, SCREEN_HEIGHT8);
  tft.println("Radio");

  for (int i = 0; i < b + 1; i++)
    tft.fillRect(SCREEN_WIDTH - 32 + (i * 4), 8, 2, 6, TFT_GREEN);

  tft.drawLine(0, SCREEN_HEIGHT8 + 8, SCREEN_WIDTH, SCREEN_HEIGHT8 + 8, TFT_GREY);
  delay(500);

  tft.setTextSize(1);
  tft.setTextFont(2);
  tft.setTextColor(TFT_WHITE, TFT_BLUE);
  tft.setCursor(8, SCREEN_HEIGHT8 * 2, 2);
  tft.println("STATUS");

  tft.setTextColor(TFT_WHITE, TFT_RED);
  tft.setCursor(8, SCREEN_HEIGHT8 * 3, 2);
  tft.println("GAIN");

  initWifi();

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Ready   ", (SCREEN_WIDTH >> 1), SCREEN_HEIGHT8 * 2, 2);
  tft.drawString(String(fgain), (SCREEN_WIDTH >> 1), SCREEN_HEIGHT8 * 3, 2);
  tft.drawString(String(arrayStation[sflag]), 12, SCREEN_HEIGHT8 * 4, 2);

  tft.setTextFont(1);
  tft.setCursor(8, SCREEN_HEIGHT8 * 7, 1);
  tft.println(WiFi.localIP());

  Serial.printf("STATUS(System) Ready \n\n");
  out = new AudioOutputI2S();
  //out->SetOutputModeMono(true);
  out->SetGain(fgain * 0.05);

#ifdef PLAY_AUTO
  StartPlaying();
  playflag = 1;
#endif
}

float n = 0;

void loop()
{
  if (playflag == 1)
  {
    tft.pushImage(SCREEN_WIDTH >> 1, SCREEN_HEIGHT8 * 6, ANI_WIDTH, ANI_HEIGHT, frame[int(n)]);
    n = n + 0.05;
    if (int(n) == ANI_FRAMES)
      n = 0;
  }
  else
  {
    tft.pushImage(SCREEN_WIDTH >> 1, SCREEN_HEIGHT8 * 6, ANI_WIDTH, ANI_HEIGHT, frame[ANI_FRAMES - 1]);
  }

  #ifdef ESP32
  static int lastms = 0;
  if (playflag == 0)
  {
    if (PIN_PRESSED(PIN_BTN1))
    {
      StartPlaying();
      playflag = 1;
    }

    if (PIN_PRESSED(PIN_BTN2))
    {
      sflag = (sflag + 1) % numCh;
      URL = arrayURL[sflag];
      station = arrayStation[sflag];
      tft.setTextSize(1);
      tft.drawString(String(station), 12, SCREEN_HEIGHT8 * 4, 2);
      delay(300);
    }
  }

  if (playflag == 1)
  {
    if (mp3->isRunning())
    {
      if (millis() - lastms > 1000)
      {
        lastms = millis();
        Serial.printf("STATUS(Streaming) %d ms...\n", lastms);

        ledflag = ledflag + 1;
        if (ledflag > 1)
        {
          ledflag = 0;
          // digitalWrite(LED, HIGH);
        }
        else
        {
          // digitalWrite(LED, LOW);
        }
      }
      if (!mp3->loop())
        mp3->stop();
    }
    else
    {
      Serial.printf("MP3 done\n");
      playflag = 0;
      // digitalWrite(LED, HIGH);
    }

    if (PIN_PRESSED(PIN_BTN1))
    {
      StopPlaying();
      playflag = 0;
      tft.drawString("Stoped!   ", (SCREEN_WIDTH >> 1), SCREEN_HEIGHT8 * 2, 2);
      // digitalWrite(LED, HIGH);
      delay(200);
    }

    if (PIN_PRESSED(PIN_BTN2))
    {
      fgain = fgain + 1.0;
      if (fgain > 10.0)
      {
        fgain = 1.0;
      }
      out->SetGain(fgain * 0.05);
      tft.drawString(String(fgain), (SCREEN_WIDTH >> 1), SCREEN_HEIGHT8 * 3, 2);
      Serial.printf("STATUS(Gain) %f \n", fgain * 0.05);
      delay(200);
    }
  }


  // if (PIN_PRESSED(PIN_BTN3) == 0)
  // {
  //   if (press2 == 0)
  //   {
  //     press2 = 1;
  //     //tft.fillRect(108, 18, 25, 6, TFT_BLACK);
  //     tft.fillRect(SCREEN_WIDTH - 32, 8, 25, 6, TFT_BLACK);

  //     b++;
  //     if (b > 4)
  //       b = 0;

  //     for (int i = 0; i < b + 1; i++)
  //       //tft.fillRect(108 + (i * 4), 18, 2, 6, TFT_GREEN);
  //       tft.fillRect(SCREEN_WIDTH - 32 + (i * 4), 8, 2, 6, TFT_GREEN);
  //     ledcWrite(pwmLedChannelTFT, backlight[b]);
  //   }
  // }
  // else
  //   press2 = 0;
  #endif
}
