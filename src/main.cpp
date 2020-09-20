#define CONFIG_USE_ONLY_LWIP_SELECT 1

// edit the config.h file and enter your Adafruit IO credentials
// and any additional configuration needed for WiFi, cellular,
// or ethernet clients.
#include "config.h"

#include <TimeLib.h>
#include <ArduinoOTA.h>

#define FASTLED_RMT_MAX_CHANNELS 1
#include "FastLED.h"

FASTLED_USING_NAMESPACE

#if defined(FASTLED_VERSION) && (FASTLED_VERSION < 3001000)
#warning "Requires FastLED 3.1 or later; check github for latest code."
#endif

#define DATA_PIN    13
//#define CLK_PIN   4
#define LED_TYPE    WS2811
#define COLOR_ORDER GRB
#define NUM_LEDS    101
#define FRAMES_PER_SECOND  120

CRGB leds[NUM_LEDS];

// KivSEE sign LED positional defines
#define E2Top  0
#define E2Bot 5
#define E1Top  5
#define E1Bot  10
#define STop  10
#define SBot  15
#define VTop  15
#define VBot  19
#define ITop  19
#define IBot  24
#define KTop  24
#define KBot  29
#define SheepStart  30
#define SheepEnd  101

const int lettersStart[] = {SheepStart,KTop,ITop,VTop,STop,E1Top, E2Top};
const int lettersEnd[] = {SheepEnd,KBot,IBot,VBot,SBot,E1Bot, E2Bot};

// Animations functions declarations
void quiet(byte color);
void nextPattern();
void paintRange(int start, int end, CRGB color);
void paintAll(CRGB color);
void confettiLetters();
void fill();
void blink();
void splash();
void changeHue(int &currHue, int &destHue, int iStart, int iEnd);
void singleColor();
// void toggleSegment(int iStart, int iEnd, int hue, int sat);
// void toggle();
void rainbow();
void rainbowWithGlitter();
void addGlitter(fract8 chanceOfGlitter);
void confetti();
void sinelon();
void bpm();
void juggle();
void dotted();

int brightness = 20;
int animSel = 0;
bool isPlain = true;

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

// List of patterns to cycle through.  Each is defined as a separate function below.
typedef void (*SimplePatternList[])();
SimplePatternList gPatterns = { confetti, rainbow, singleColor, splash, blink, confettiLetters, dotted, bpm };
// SimplePatternList gPatterns = { confettiLetters, fill, dotted, bpm, sinelon, blink, splash, singleColor, rainbow, confetti };

uint8_t gCurrentPatternNumber = 0; // Index number of which pattern is current
uint8_t gHue = 0; // rotating "base color" used by many of the patterns

bool setBrightnessValue = false;
int brightnessValue = 0;
bool setAnimSelValue = false;
int animSelValue = 0;
bool resetFlag = false;

unsigned int lastReportTime = 0;
unsigned int lastMonitorTime = 0;

// set up the 'time/seconds' topic
AdafruitIO_Time *seconds = io.time(AIO_TIME_SECONDS);
time_t secTime = 0;

// set up the 'time/milliseconds' topic
//AdafruitIO_Time *msecs = io.time(AIO_TIME_MILLIS);

// set up the 'time/ISO-8601' topic
// AdafruitIO_Time *iso = io.time(AIO_TIME_ISO);
// char *isoTime;

// this int will hold the current count for our sketch
// int count = 0;

// set up the 'counter' feed
// AdafruitIO_Feed *counter = io.feed("counter");
// set up mqtt feeds
AdafruitIO_Feed *rssi = io.feed("rssi_kivsee_sign");
AdafruitIO_Feed *animSelFeed = io.feed("button");
AdafruitIO_Feed *brightnessFeed = io.feed("brightness");

TaskHandle_t Task1;

QueueHandle_t brightnessQueue;
const int brightnessQueueSize = 10;

QueueHandle_t animationQueue;
const int animationQueueSize = 10;

// message handler for the seconds feed
void handleSecs(char *data, uint16_t len) {
  // Serial.print("Seconds Feed: ");
  // Serial.println(data);
  secTime = atoi (data);
}

// message handler for the milliseconds feed
// void handleMillis(char *data, uint16_t len) {
//   Serial.print("Millis Feed: ");
//   Serial.println(data);
// }

// message handler for the ISO-8601 feed
// void handleISO(char *data, uint16_t len) {
//   Serial.print("ISO Feed: ");
//   Serial.println(data);
//   isoTime = data;
// }

void setBrightness(AdafruitIO_Data *data) {
  Serial.print("[0] received new brightness value from web ");
  Serial.println(data->value());
  brightnessValue = atoi(data->value());
  xQueueSend(brightnessQueue, &brightnessValue, portMAX_DELAY);
  // setBrightnessValue = true;
}

void setAnimation(AdafruitIO_Data *data) {
  Serial.print("[0] received new animation selected value from web ");
  Serial.println(data->value());
  animSelValue = atoi(data->value());
  xQueueSend(animationQueue, &animSelValue, portMAX_DELAY);
  // setAnimSelValue = true;
}

void printDigits(int digits){
  // utility function for digital clock display: prints preceding colon and leading 0
  Serial.print(":");
  if(digits < 10)
    Serial.print('0');
  Serial.print(digits);
}

void digitalClockDisplay(){
  // digital clock display of the time
  Serial.print(hour());
  printDigits(minute());
  printDigits(second());
  Serial.print(" ");
  Serial.print(day());
  Serial.print(" ");
  Serial.print(month());
  Serial.print(" ");
  Serial.print(year()); 
  Serial.println(); 
}

// time sync function
time_t timeSync()
{
  if (secTime == 0) {
    return 0;
  }
  return (secTime + TZ_HOUR_SHIFT * 3600);
}

void MonitorLoop( void * parameter) {

  Serial.print("Connecting to Adafruit IO");

  // connect to io.adafruit.com
  io.connect();

  // attach message handler for the seconds feed
  seconds->onMessage(handleSecs);

  // attach a message handler for the msecs feed
  //msecs->onMessage(handleMillis);

  // attach a message handler for the ISO feed
  // iso->onMessage(handleISO);

  // attach message handler for the brightness feed
  brightnessFeed->onMessage(setBrightness);

  // attach message handler for the button animation selector feed
  animSelFeed->onMessage(setAnimation);

  // wait for a connection
  while(io.status() < AIO_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  // we are connected
  Serial.println();
  Serial.println(io.statusText());

  // Because Adafruit IO doesn't support the MQTT retain flag, we can use the
  // get() function to ask IO to resend the last value for this feed to just
  // this MQTT client after the io client is connected.
  brightnessFeed->get();
  animSelFeed->get();

  setSyncProvider(timeSync);
  setSyncInterval(60); // sync interval in seconds, consider increasing
  
  //// Over The Air section ////
  // Port defaults to 3232
  // ArduinoOTA.setPort(3232);

  // Hostname defaults to esp3232-[MAC]
  ArduinoOTA.setHostname(THING_NAME);

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.begin();

  for(;;) {
    unsigned int currTime = millis();

    ArduinoOTA.handle();

    // io.run(); is required for all sketches.
    // it should always be present at the top of your loop
    // function. it keeps the client connected to
    // io.adafruit.com, and processes any incoming data.
    io.run();

    // don't do anything before we get a time read and set the clock
    if (timeStatus() == timeNotSet) {
      if (secTime > 0) {
        setTime(timeSync());
        Serial.print("Time set, time is now <- ");
        digitalClockDisplay();
      }
      // else {
      //   return;
      // }
    }
    
    if (currTime - lastMonitorTime >= (MONITOR_SECS * 1000)) {
      // save count to the 'counter' feed on Adafruit IO
      Serial.print("sending rssi value -> ");
      // Serial.println(count);
      // counter->save(count);
      // save the wifi signal strength (RSSI) to the 'rssi' feed on Adafruit IO
      rssi->save(WiFi.RSSI());
      
      Serial.print("Time is: ");
      digitalClockDisplay();

      Serial.print("Brightness value # ");
      Serial.println(brightnessValue);

      Serial.print("Animation selected value # ");
      Serial.println(animSelValue);

      // increment the count by 1
      // count++;
      lastMonitorTime = currTime;
    }
    
    // // counter disabled to not overload adfruit IO for no need
    // // reset the count at some hour of the day
    // if ((hour() == RESET_HOUR) && (minute() == 0) && (second() == 0) && (!resetFlag)) {
    //   Serial.print("sending count and presses reset at time -> ");
    //   digitalClockDisplay();
    //   count = 0;
    //   counter->save(count);
    //   // remember a reset happened so we don't do it again and bombard Adafruit IO with requests
    //   resetFlag = true;
    // }

    //// reallow reset to occur after reset time has passed
    // if (second() == 1) {
    //   resetFlag = false;
    // }

    vTaskDelay(5);
  }
}

void setup() { 
  // tell FastLED about the LED strip configuration
  FastLED.addLeds<LED_TYPE,DATA_PIN,COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  
  // start the serial connection
  Serial.begin(115200);
  disableCore0WDT();

  brightnessQueue = xQueueCreate( brightnessQueueSize, sizeof( int ) );
  animationQueue = xQueueCreate( animationQueueSize, sizeof( int ) );

  // wait for serial monitor to open
  while(! Serial);

  xTaskCreatePinnedToCore(
      MonitorLoop, /* Function to implement the task */
      "MonitorTask", /* Name of the task */
      16384,  /* Stack size in words */
      NULL,  /* Task input parameter */
      0,  /* Priority of the task */
      &Task1,  /* Task handle. */
      0); /* Core where the task should run */

}

void loop()
{
  // check queues for new values
  if(xQueueReceive(brightnessQueue, &brightness, 0) == pdTRUE) {
    Serial.print("[1] received new brightness value from queue: ");
    Serial.println(brightness);
  }
  
  if(xQueueReceive(animationQueue, &animSel, 0) == pdTRUE) {
    Serial.print("[1] received new animSel value from queue: ");
    Serial.println(animSel);
  }
  // start LED handling code
  // if (setBrightnessValue && (brightnessValue >= 0 && brightnessValue < 256)) { 
  //   brightness = brightnessValue;
  //   setBrightnessValue = false;
  // }

  // set master brightness control
  FastLED.setBrightness(brightness);
 
  // disabling isPlain for now until needed (isPlain)
  if(animSel > ( ARRAY_SIZE( gPatterns))) {
    paintAll(CRGB::Red);
  }
  else if (animSel > 0) {
    // gCurrentPatternNumber = animSelValue-1;
    gPatterns[animSelValue-1]();
  }
  else {
    // Call the current pattern function once, updating the 'leds' array
    gPatterns[gCurrentPatternNumber]();    
  }
    
  // gPatterns[gCurrentPatternNumber]();    

  // send the 'leds' array out to the actual LED strip
  FastLED.show();  
  // insert a delay to keep the framerate modest
  FastLED.delay(1000/FRAMES_PER_SECOND); 

  // do some periodic updates
  EVERY_N_MILLISECONDS( 30 ) { gHue++; } // slowly cycle the "base color" through the rainbow
  EVERY_N_SECONDS( 30 ) { nextPattern(); } // change patterns periodically
}


void quiet(byte color) {
  for(int i=0; i<NUM_LEDS; i++) {
    leds[i] = CHSV(color, 196, 255);
  }
}

void nextPattern()
{
  // add one to the current pattern number, and wrap around at the end
  gCurrentPatternNumber = (gCurrentPatternNumber + 1) % ARRAY_SIZE( gPatterns);
  Serial.print("CurrentPatternNumber is now at: ");
  Serial.println(gCurrentPatternNumber);
}

void paintRange(int start, int end, CRGB color) {
  for(int i=start; i<end; i++) {
    leds[i] = color;
  }
}

void paintAll(CRGB color) {
  paintRange(0, NUM_LEDS, color);
}

void confettiLetters() {
  fadeToBlackBy( leds, NUM_LEDS, 10); 
  if(random(255) < 16) {
    int currentLetter = random(7);
    paintRange(lettersStart[currentLetter], lettersEnd[currentLetter], CHSV(gHue, 255, 255));
  }
}

void fill() {

  static int sat = 0;
  static uint8_t currentPixel = NUM_LEDS;

  leds[currentPixel] = CHSV( (255 * 2) / 3, sat, 255);
  EVERY_N_MILLISECONDS( 20 ) { 
    currentPixel = (currentPixel - 1) % NUM_LEDS; 
    if(currentPixel == 0) {
      sat = (sat == 0? 255: 0);
      currentPixel = NUM_LEDS;
    }
  }
}

void blink() {
  static uint8_t currentBrightness = 0;
  static int dir = 3;
  currentBrightness = currentBrightness + dir;
  if(currentBrightness == 255) {
    dir = -3;
  }
  if(currentBrightness == 0) {
    dir = 3;
  }
  paintAll(CHSV(gHue, 255, currentBrightness));
}

void splash() {
  static int currentLetter = 0;
  static uint8_t currentHue = 0;

  paintAll(CRGB::Black);

  EVERY_N_MILLISECONDS( 330 ) { 
    currentLetter = (currentLetter + 1 ) % 7; 
    currentHue = currentHue + 16;
  }
  
  paintRange(lettersStart[currentLetter], lettersEnd[currentLetter], CHSV(currentHue, 255, 255));
}

void changeHue(int &currHue, int &destHue, int iStart, int iEnd) {
  
  if(currHue == destHue) {
    destHue = random(255);
  }

  if(((currHue - destHue) % 256) < 128) {
    currHue--;
  }
  else {
    currHue++;
  }

  for(int i=iStart; i<iEnd; i++) {
    leds[i] = CHSV(currHue, 255, 255);
  }

}

void singleColor() {

  static int sHue = 0;
  static int sDestHue = 0;
  static int aHue = 30;
  static int aDestHue = 0;
  static int lHue = 60;
  static int lDestHue = 0;
  static int eHue = 90;
  static int eDestHue = 0;
  
  changeHue(sHue, sDestHue, SheepStart, SheepEnd);
  changeHue(aHue, aDestHue, KTop, KBot);
  changeHue(lHue, lDestHue, ITop, IBot);
  changeHue(lHue, lDestHue, VTop, VBot);
  changeHue(eHue, eDestHue, STop, SBot);
  changeHue(eHue, eDestHue, E1Top, E1Bot);
  changeHue(eHue, eDestHue, E2Top, E2Bot);
}

// void toggleSegment(int iStart, int iEnd, int hue, int sat) {
//   int pos = iStart + random16(iEnd - iStart);
//   leds[pos] += CHSV( hue + random8(16), sat, 255);    
// }

void rainbow() 
{
  // FastLED's built-in rainbow generator
  fill_rainbow( leds, NUM_LEDS, gHue, 7);
  for(int i=0; i<NUM_LEDS; i++) {
    uint8_t hue = (gHue + (uint8_t)((double)i / (double)NUM_LEDS) * 256) % 256;
    leds[i] = CHSV(hue, 255, 255);
  }
}

void rainbowWithGlitter() 
{
  // built-in FastLED rainbow, plus some random sparkly glitter
  rainbow();
  addGlitter(80);
}

void addGlitter( fract8 chanceOfGlitter) 
{
  if( random8() < chanceOfGlitter) {
    leds[ random16(NUM_LEDS) ] += CRGB::White;
  }
}

void confetti() 
{
  // random colored speckles that blink in and fade smoothly
  fadeToBlackBy( leds, NUM_LEDS, 10);
  for(int i=0; i<2; i++) {
    int pos = random16(NUM_LEDS);
    leds[pos] += CHSV( gHue + random8(64), 200, 255);
  }
}

void sinelon()
{
  // a colored dot sweeping back and forth, with fading trails
  fadeToBlackBy( leds, NUM_LEDS, 20);
  int pos = beatsin16(13,0,NUM_LEDS);
  leds[pos] += CHSV( gHue, 255, 192);
}

void bpm()
{
  // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
  uint8_t BeatsPerMinute = 30;
  CRGBPalette16 palette = PartyColors_p;
  uint8_t beat = beatsin8( BeatsPerMinute, 64, 255);
  for( int i = 0; i < NUM_LEDS; i++) { //9948
    leds[i] = ColorFromPalette(palette, gHue+(i*2), beat-gHue+(i*10));
  }
}

void juggle() {
  // eight colored dots, weaving in and out of sync with each other
  fadeToBlackBy( leds, NUM_LEDS, 20);
  byte dothue = 0;
  for( int i = 0; i < 8; i++) {
    leds[beatsin16(i+7,0,NUM_LEDS)] |= CHSV(dothue, 200, 255);
    dothue += 32;
  }
}

void dotted() {

  static int moveFactor = 0;
  EVERY_N_MILLISECONDS( 500 ) { moveFactor++; } // slowly cycle the "base color" through the rainbow
  
  const int numberOfDots = 2;
  for(int i=0; i<NUM_LEDS; i++) {
    uint8_t hue = gHue + ( (i + moveFactor) % numberOfDots) * (255 / numberOfDots);
    leds[i] =   CHSV(hue, 255, 255);
  }
}
