#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include "printf.h"
#include "lcdgfx.h"
#include <EEPROM.h>


//
// Hardware configuration
//

// Set up nRF24L01 radio on SPI bus plus pins 7 & 8

RF24 radio(7, 8);
int channel = 115;
int payloadSize = 6;

const int l_bump_pin = 3;
const int r_bump_pin = 4;
const int l_switch_pin = 6;
const int r_switch_pin = 5;
const int screen_switch_pin = 9;

bool expMode = true;

int yTrim = 0; //(char)EEPROM.read(0) ; // options are 0 -> 3 and toggled via the bumpers. Can be used for gearing sensitivity etc.
int maxMode = 50;
int minMode = -50;

//
// Topology
//

// Radio pipe addresses for the 2 nodes to communicate.
const uint64_t pipes[2] = { 0xF0F0F0F0E1LL, 0xF0F0F0F0D2LL };

DisplaySH1106_128x64_I2C display(-1);

int last_l_bump = LOW;
int last_r_bump = LOW;
void manage_bumpers(){
  int l_val = digitalRead(l_bump_pin);
  int r_val = digitalRead(r_bump_pin);
  if (last_l_bump != l_val){
    if (l_val == HIGH){
      yTrim--;
      yTrim = max(yTrim, minMode);
    }
    last_l_bump = l_val;
  }

  if (last_r_bump != r_val){
    if (r_val == HIGH){
      yTrim++;
      yTrim = min(yTrim, maxMode);
    }
    last_r_bump = r_val;
  }
}

void setup(void)
{
  display.begin();
  display.clear();
  
  // set up the role pin
  pinMode(l_bump_pin, INPUT);
  pinMode(r_bump_pin, INPUT);

  //
  // Print preamble
  //
  Serial.begin(9600);
  printf_begin();

  //
  // Setup and configure rf radio
  //
  radio.begin();
  radio.setChannel(channel);

  // Optionally, increase the delay between retries & # of retries
  radio.setRetries(5, 15); // 5, 15 is default
  radio.setPALevel(RF24_PA_HIGH);
  radio.setDataRate(RF24_250KBPS);
  radio.enableDynamicPayloads();
  radio.setAutoAck(true);

  radio.openWritingPipe(pipes[0]);
  radio.openReadingPipe(1, pipes[1]);

  // First, stop listening so we can talk.
  radio.stopListening();

  //
  // Dump the configuration of the rf unit for debugging
  //
  radio.printDetails();
  display.setFixedFont(ssd1306xled_font6x8);
  
}

void loop(void)
{
  int highs = !digitalRead(r_switch_pin);
  expMode = digitalRead(l_switch_pin);
  //manage_bumpers();
  
  short inputs[4]; 
  inputs[0] = 1023 - analogRead(3);
  inputs[1] = analogRead(2);
  inputs[2] = 1023 - analogRead(0);
  inputs[3] = 1023-analogRead(1);
  

  for (int i = 1; i < 4; i++){
    int s = expMode && (inputs[i] - 512 < 0) ? -1 : 1;
    inputs[i] = (pow(((float)(inputs[i] - 512)) / 512, expMode ? 2 : 1)*s / (2 - highs) + 1) * 512;
  }

  inputs[2] += yTrim * 10;
  inputs[2] = max(inputs[2], 0);
  inputs[2] = min(inputs[2], 1023);

  //Serial.println(inputs[0]);
  //Serial.println(inputs[1]);
  //Serial.println(inputs[2]);

  byte *radioData = (byte *) inputs;
  for (int i = 0; i < 8; i++){
    //Serial.print(radioData[i]);
    //Serial.print(" ");
  }
  //Serial.println(" ");
  
  bool success = radio.write(radioData, 8);

  short arc = radio.getARC();
  
  if (success){
    //Serial.println("success");
  }
  else{
    arc = 16;
    //Serial.println("no ack received");
  }
  
  //Serial.print("arc: ");
  //Serial.println(arc);
  //Serial.print("con: ");
  //Serial.println(radio.isChipConnected());

  display.printFixed(0, 32, "In:", STYLE_NORMAL);
  display.printFixed(20, 32, "    ", STYLE_NORMAL);
  display.printFixed(20, 32, String(inputs[0]).c_str(), STYLE_NORMAL);
  display.printFixed(44, 32, "    ", STYLE_NORMAL);
  display.printFixed(44, 32, String(inputs[1]).c_str(), STYLE_NORMAL);
  display.printFixed(68, 32, "    ", STYLE_NORMAL);
  display.printFixed(68, 32, String(inputs[2]).c_str(), STYLE_NORMAL);
  display.printFixed(96, 32, "    ", STYLE_NORMAL);
  display.printFixed(96, 32, String(inputs[3]).c_str(), STYLE_NORMAL);
  display.printFixed(0, 48, "Arc: ");
  display.printFixed(30, 48, "  ", STYLE_NORMAL);
  display.printFixed(30, 48, String(arc).c_str(), STYLE_NORMAL);
  display.printFixed(54, 48, "yTrim: ");
  display.printFixed(96, 48, "    ", STYLE_NORMAL);
  display.printFixed(96, 48, String(yTrim).c_str(), STYLE_NORMAL);
  display.printFixed(0, 16, "         ", STYLE_NORMAL);
  display.printFixed(16, 16, String(highs).c_str(), STYLE_NORMAL);
  display.printFixed(0, 16, String(digitalRead(l_switch_pin)).c_str(), STYLE_NORMAL);
  display.printFixed(32, 16, String(digitalRead(screen_switch_pin)).c_str(), STYLE_NORMAL);

  EEPROM.write(0, yTrim);
  // Try again 1s later
  delay(25);
}
// vim:cin:ai:sts=2 sw=2 ft=cpp
