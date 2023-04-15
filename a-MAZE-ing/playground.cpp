#include <M5Core2.h>

void setup() {
  M5.Lcd.begin();  //Initialize M5Core2
  M5.Lcd.fillScreen(M5.Lcd.alphaBlend(128, 0X00FF00, 0XFF0000));
  //Set foreground、background color to 0X00FF00,0XFF0000 respectively and transparency to 128, and fill out the entire screen.
}

void loop() {
}