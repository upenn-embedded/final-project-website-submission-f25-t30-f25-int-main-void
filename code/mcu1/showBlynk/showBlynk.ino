// template info
#define BLYNK_TEMPLATE_ID "TMPL2-_KHyhYj"
#define BLYNK_TEMPLATE_NAME "T30 Final Project"
#define BLYNK_AUTH_TOKEN "sIwb0-DoeCb027ji-w9a5njIlXHLbELb"

// WiFi
#define MY_SSID                     "AirPennNet-Device"
#define MY_PASSWORD                 "penn1740wifi"

#include <BlynkSimpleEsp32.h>

#define BAUD 9600 // since atmega328pb buad rate 9600
// from board's pin mapping: RX 44, Tx 43
#define Rx 44
#define Tx 43

// store temperature and humidity data as string
char message[101];
// how many bytes read
int n = 0;

// prevent disconnecting
BlynkTimer timer;
void Timer()
{
  Blynk.virtualWrite(1, message);
}

void setup()
{
  Serial.begin(BAUD);
  Serial1.begin(BAUD, SERIAL_8N1, Rx, Tx);
  Blynk.begin(BLYNK_AUTH_TOKEN, MY_SSID, MY_PASSWORD);
  timer.setInterval(10000L, Timer); // upload message every 10 sconds
}

void loop()
{
  if(Serial1.available() > 0)
  {
    n = Serial1.readBytesUntil('\n', message, 100);
    message[n] = '\0'; // add the END
  }

  // test
  Serial.println(message);

  Blynk.run();
  timer.run();

  delay(1000);
}
