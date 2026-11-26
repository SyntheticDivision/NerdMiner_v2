#include "displayDriver.h"

#if defined ESP32_2432S028R || ESP32_2432S028_2USB

#include <TFT_eSPI.h>
#include <TFT_eTouch.h>
#include "media/myFonts.h"
#include "media/Free_Fonts.h"
#include "version.h"
#include "monitor.h"
#include "OpenFontRender.h"
#include <SPI.h>
#include <WiFi.h>
#include "rotation.h"
#include "drivers/storage/nvMemory.h"
#include "drivers/storage/storage.h"

#define WIDTH 130 //320
#define HEIGHT 170 

extern nvMemory nvMem;

OpenFontRender render;
TFT_eSPI tft = TFT_eSPI();                  // Invoke library, pins defined in platformio.ini
TFT_eSprite background = TFT_eSprite(&tft); // Invoke library sprite
SPIClass hSPI(HSPI);
TFT_eTouch<TFT_eSPI> touch(tft, ETOUCH_CS, 0xFF, hSPI); 

extern monitor_data mMonitor;
extern pool_data pData;
extern DisplayDriver *currentDisplayDriver;
extern bool invertColors; 
extern TSettings Settings;
bool hasChangedScreen = true;

void getChipInfo(void){
  Serial.print("Chip: ");
  Serial.println(ESP.getChipModel());
  Serial.print("ChipRevision: ");
  Serial.println(ESP.getChipRevision());
  Serial.print("Psram size: ");
  Serial.print(ESP.getPsramSize() / 1024);
  Serial.println("KB");
  Serial.print("Flash size: ");
  Serial.print(ESP.getFlashChipSize() / 1024);
  Serial.println("KB");
  Serial.print("CPU frequency: ");
  Serial.print(ESP.getCpuFreqMHz());
  Serial.println("MHz");  
}

void esp32_2432S028R_Init(void)
{ 
  // getChipInfo();  
  tft.init();
#ifdef ESP32_2432S028_2USB
  invertColors = true;
#endif
  if (nvMem.loadConfig(&Settings))
    invertColors = Settings.invertColors;
  tft.invertDisplay(invertColors);
  tft.setRotation(1);    
  tft.setSwapBytes(true); // Swap the colour byte order when rendering
  if (invertColors) {
    tft.writecommand(ILI9341_GAMMASET);
    tft.writedata(2);
    delay(120);
    tft.writecommand(ILI9341_GAMMASET); //Gamma curve selected
    tft.writedata(1); 
  }
  hSPI.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, ETOUCH_CS);
  touch.init();

  TFT_eTouchBase::Calibation calibation = { 233, 3785, 3731, 120, 2 };
  touch.setCalibration(calibation);

  // Configuring screen backlight brightness using ledcontrol channel 0.
  // Using 5000Hz in 8bit resolution, which gives 0-255 possible duty cycle setting.
  ledcSetup(0, 5000, 8);
  ledcAttachPin(TFT_BL, 0);
  ledcWrite(0, Settings.Brightness);
 
  //background.createSprite(WIDTH, HEIGHT); // Background Sprite
  //background.setSwapBytes(true);
  //render.setDrawer(background);  // Link drawing object to background instance (so font will be rendered on background)
  //render.setLineSpaceRatio(0.9); // Espaciado entre texto

  // Load the font and check it can be read OK
  // if (render.loadFont(NotoSans_Bold, sizeof(NotoSans_Bold)))
  if (render.loadFont(DigitalNumbers, sizeof(DigitalNumbers)))
  {
    Serial.println("Initialise error");
    return;
  }
  
  pinMode(LED_PIN, OUTPUT);
  pinMode(LED_PIN_B, OUTPUT);
  pinMode(LED_PIN_G, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  digitalWrite(LED_PIN_B, HIGH);
  digitalWrite(LED_PIN_G, HIGH);
  pData.bestDifficulty = "0";
  pData.workersHash = "0";
  pData.workersCount = 0;
  //Serial.println("=========== Fim Display ==============") ;
}

void esp32_2432S028R_AlternateScreenState(void)
{
  Serial.println("Switching display state");
  int screen_state_duty = ledcRead(0);
  // Switching the duty cycle for the ledc channel, where the TFT_BL pin is attached.
  if (screen_state_duty > 0) {
    ledcWrite(0, 0);
  } else {
    ledcWrite(0, Settings.Brightness);
  }
}

void esp32_2432S028R_AlternateRotation(void)
{
  tft.setRotation( flipRotation(tft.getRotation()) );
  hasChangedScreen = true;
}

bool bottomScreenBlue = true;

void printheap(){
  Serial.print("$$ Free Heap:");
  Serial.println(ESP.getFreeHeap()); 
  // Serial.printf("### stack WMark usage: %d\n", uxTaskGetStackHighWaterMark(NULL));
}

bool createBackgroundSprite(int16_t wdt, int16_t hgt){  // Set the background and link the render, used multiple times to fit in heap
  background.createSprite(wdt, hgt) ; //Background Sprite
  // printheap();
  if (background.created()) {
      background.setColorDepth(16);
      background.setSwapBytes(true);
      render.setDrawer(background); // Link drawing object to background instance (so font will be rendered on background)
      render.setLineSpaceRatio(0.9);      
  } else {
    Serial.println("#### Sprite Error ####");
    Serial.printf("Size w:%d h:%d \n", wdt, hgt);
    printheap();
  }
  return background.created();
}

static const uint16_t SYN_CYAN = 0x279F;
static const uint16_t SYN_YEL = 0xFF09;
static const uint16_t SYN_MAG = 0xF974;
static const uint16_t SYN_MUTE = 0x7BEF;

static void drawMark(int x, int y)
{
  tft.fillRoundRect(x, y, 28, 28, 5, TFT_BLACK);
  tft.drawRoundRect(x, y, 28, 28, 5, SYN_YEL);
  tft.fillTriangle(x + 15, y + 5, x + 23, y + 7, x + 16, y + 13, SYN_MAG);
  tft.fillRect(x + 7, y + 8, 6, 4, SYN_CYAN);
  tft.fillRect(x + 12, y + 10, 3, 12, SYN_CYAN);
}

static void drawTemp(const String &temp)
{
  tft.fillRect(220, 2, 98, 24, TFT_BLACK);
  tft.setTextFont(2);
  tft.setTextDatum(TR_DATUM);
  tft.setTextColor(SYN_YEL, TFT_BLACK);
  tft.drawString(temp + "c", 300, 6);
  tft.fillCircle(310, 14, 3, SYN_MAG);
}

static void drawIp(void)
{
  String ip = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : String("NO LINK");
  tft.fillRect(38, 6, 176, 18, TFT_BLACK);
  tft.setTextFont(2);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(SYN_CYAN, TFT_BLACK);
  tft.drawString(ip, 126, 6);
}

static void drawStat(int x, int w, const char *k, const String &v)
{
  tft.fillRect(x, 176, w, 62, TFT_BLACK);
  tft.setTextFont(2);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(SYN_MUTE, TFT_BLACK);
  tft.drawString(k, x + w / 2, 182);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  String shown = v;
  if (shown.length() > 9) shown = shown.substring(0, 9);
  tft.drawString(shown, x + w / 2, 204);
}

static int fitHeroSize(const char *text)
{
  int size = 44;
  while (size > 20)
  {
    render.setFontSize(size);
    if ((int)render.getTextWidth("%s", text) <= 300)
      break;
    size -= 2;
  }
  return size;
}

static void drawHero(const String &value)
{
  const int w = 320;
  const int h = 88;
  tft.fillRect(0, 36, 320, 96, TFT_BLACK);
  if (!createBackgroundSprite(w, h))
  {
    tft.setTextFont(4);
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(SYN_CYAN, TFT_BLACK);
    tft.drawString(value, 160, 64);
    return;
  }
  background.fillSprite(TFT_BLACK);
  render.setLineSpaceRatio(1.0);
  int size = fitHeroSize(value.c_str());
  render.setFontSize(size);
  render.setAlignment(Align::MiddleCenter);
  render.cdrawString(value.c_str(), w / 2, h / 2, SYN_CYAN);
  background.pushSprite(0, 40);
  background.deleteSprite();
}

static void drawUnit(const char *unit)
{
  tft.fillRect(0, 136, 320, 28, TFT_BLACK);
  tft.setTextFont(4);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(SYN_YEL, TFT_BLACK);
  tft.drawString(unit, 160, 136);
}

static void drawChrome(const String &temp)
{
  tft.fillScreen(TFT_BLACK);
  drawMark(6, 4);
  drawTemp(temp);
  drawIp();
  tft.drawFastHLine(8, 170, 304, SYN_YEL);
}

static const char *poolState(void)
{
  switch (mMonitor.NerdStatus)
  {
  case NM_hashing:
    return "OK";
  case NM_Connecting:
    return "LINK";
  case NM_waitingConfig:
    return "CFG";
  default:
    return "--";
  }
}

void esp32_2432S028R_MinerScreen(unsigned long mElapsed)
{
  mining_data data = getMiningData(mElapsed);
  if (hasChangedScreen)
    drawChrome(data.temp);
  else
    drawTemp(data.temp);
  drawIp();
  hasChangedScreen = false;

  drawHero(data.currentHashRate);
  drawUnit("kH/s");

  const int col = 80;
  drawStat(0, col, "BEST", data.bestDiff);
  drawStat(col, col, "SHARES", data.completedShares);
  drawStat(col * 2, col, "HEIGHT", getClockData(mElapsed).blockHeight);
  drawStat(col * 3, col, "POOL", poolState());

  Serial.printf(">>> SynMiner %s kH/s  shares %s  temp %s\n",
                data.currentHashRate.c_str(), data.completedShares.c_str(), data.temp.c_str());
}

void esp32_2432S028R_ClockScreen(unsigned long mElapsed)
{
  clock_data data = getClockData(mElapsed);
  clock_data_t tick = getClockData_t(mElapsed);
  if (hasChangedScreen)
    drawChrome(String(temperatureRead(), 0));
  else
    drawTemp(String(temperatureRead(), 0));
  drawIp();
  hasChangedScreen = false;

  String clock = data.currentTime;
  int colon = clock.indexOf(':');
  String hours = colon > 0 ? clock.substring(0, colon) : clock;
  String mins = colon > 0 ? clock.substring(colon + 1) : String("00");
  tft.fillRect(0, 36, 320, 96, TFT_BLACK);
  if (createBackgroundSprite(320, 88))
  {
    background.fillSprite(TFT_BLACK);
    render.setLineSpaceRatio(1.0);
    render.setFontSize(44);
    render.setAlignment(Align::MiddleCenter);
    render.cdrawString(hours.c_str(), 108, 44, SYN_CYAN);
    render.cdrawString(mins.c_str(), 212, 44, SYN_CYAN);
    background.fillCircle(160, 32, 3, SYN_CYAN);
    background.fillCircle(160, 52, 3, SYN_CYAN);
    background.pushSprite(0, 40);
    background.deleteSprite();
  }
  drawUnit(data.currentDate.c_str());

  char sec[8];
  char zone[12];
  snprintf(sec, sizeof(sec), "%02lu", tick.currentSeconds);
  snprintf(zone, sizeof(zone), "UTC%+d", Settings.Timezone);
  const int col = 106;
  drawStat(0, col, "SEC", sec);
  drawStat(col, col, "ZONE", zone);
  drawStat(col * 2, col, "RATE", data.currentHashRate);
}

void esp32_2432S028R_GlobalHashScreen(unsigned long mElapsed)
{
  coin_data data = getCoinData(mElapsed);
  if (hasChangedScreen)
    drawChrome(String(temperatureRead(), 0));
  else
    drawTemp(String(temperatureRead(), 0));
  drawIp();
  hasChangedScreen = false;

  String price = data.btcPrice;
  price.replace("$", "");
  price.replace(",", "");
  drawHero(price);
  drawUnit("BTC");

  const int col = 106;
  drawStat(0, col, "FEES", data.halfHourFee);
  drawStat(col, col, "DIFF", data.netwrokDifficulty);
  drawStat(col * 2, col, "LEFT", data.remainingBlocks);
}

void esp32_2432S028R_LoadingScreen(void)
{
  tft.fillScreen(TFT_BLACK);
  drawMark(146, 70);
  tft.setTextFont(4);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(SYN_CYAN, TFT_BLACK);
  tft.drawString("SYNMINER", 160, 112);
  tft.setTextFont(2);
  tft.setTextColor(SYN_YEL, TFT_BLACK);
  tft.drawString("SOLO", 160, 148);
  tft.setTextColor(SYN_MUTE, TFT_BLACK);
  tft.drawString(CURRENT_VERSION, 160, 172);
}

void esp32_2432S028R_SetupScreen(void)
{
  tft.fillScreen(TFT_BLACK);
  drawMark(8, 8);
  tft.setTextFont(4);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(SYN_CYAN, TFT_BLACK);
  tft.drawString("SYNMINER", 160, 70);
  tft.setTextFont(2);
  tft.setTextColor(SYN_YEL, TFT_BLACK);
  tft.drawString("WiFi setup", 160, 110);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("SynMinerAP", 160, 136);
  tft.setTextColor(SYN_MUTE, TFT_BLACK);
  tft.drawString("password  MineYourCoins", 160, 158);
  tft.drawString("then open  192.168.4.1", 160, 180);
}

void esp32_2432S028R_AnimateCurrentScreen(unsigned long frame)
{
}

// Variables para controlar el parpadeo con millis()
unsigned long previousMillis = 0;
unsigned long previousTouchMillis = 0;
char currentScreen = 0;

void esp32_2432S028R_DoLedStuff(unsigned long frame)
{
  unsigned long currentMillis = millis();    
  // / Check the touch coordinates 110x185 210x240
  if (currentMillis - previousTouchMillis >= 500)
    { 
      int16_t t_x , t_y;  // To store the touch coordinates
      bool pressed = touch.getXY(t_x, t_y);
      if (pressed) {                        
          if (((t_x > 109)&&(t_x < 211)) && ((t_y > 185)&&(t_y < 241))) {
            bottomScreenBlue ^= true;
            hasChangedScreen = true;
          } else if((t_x > 235) && ((t_y > 0)&&(t_y < 16))) {
            // Touching the top right corner of the screen, roughly in the gray status label.
            // Disabling the screen backlight. 
            esp32_2432S028R_AlternateScreenState();
          }
          else
            if (t_x > 160) {
              // next screen
             // Serial.printf("Next screen touch( x:%d y:%d )\n", t_x, t_y);              
              currentDisplayDriver->current_cyclic_screen = (currentDisplayDriver->current_cyclic_screen + 1) % currentDisplayDriver->num_cyclic_screens;
            } else if (t_x < 160)
            {
              // Previus screen
             // Serial.printf("Previus screen touch( x:%d y:%d )\n", t_x, t_y);              
              /* Serial.println(currentDisplayDriver->current_cyclic_screen); */
              currentDisplayDriver->current_cyclic_screen = currentDisplayDriver->current_cyclic_screen - 1;      
              if (currentDisplayDriver->current_cyclic_screen<0) currentDisplayDriver->current_cyclic_screen = currentDisplayDriver->num_cyclic_screens - 1;              
            }
      }
      previousTouchMillis = currentMillis;
    }

    if (currentScreen != currentDisplayDriver->current_cyclic_screen) hasChangedScreen ^= true;
    currentScreen = currentDisplayDriver->current_cyclic_screen;

  switch (mMonitor.NerdStatus)
  {
  case NM_waitingConfig:
    digitalWrite(LED_PIN, LOW); // LED encendido de forma continua
    break;

  case NM_Connecting:
    if (currentMillis - previousMillis >= 500)
    { // 0.5sec blink
      previousMillis = currentMillis;
      // Serial.print("C");
      digitalWrite(LED_PIN, HIGH);
      digitalWrite(LED_PIN_B, !digitalRead(LED_PIN)); // Cambia el estado del LED
    }
    break;

  case NM_hashing:
    if (currentMillis - previousMillis >= 500)
    { // 0.1sec blink
      // Serial.print("h");
      previousMillis = currentMillis;
      digitalWrite(LED_PIN_B, HIGH);
      digitalWrite(LED_PIN, !digitalRead(LED_PIN)); // Cambia el estado del LED      
    }
    break;
  }
  

}

CyclicScreenFunction esp32_2432S028RCyclicScreens[] = {esp32_2432S028R_MinerScreen, esp32_2432S028R_GlobalHashScreen, esp32_2432S028R_ClockScreen};

DisplayDriver esp32_2432S028RDriver = {
    esp32_2432S028R_Init,
    esp32_2432S028R_AlternateScreenState,
    esp32_2432S028R_AlternateRotation,
    esp32_2432S028R_LoadingScreen,
    esp32_2432S028R_SetupScreen,
    esp32_2432S028RCyclicScreens,
    esp32_2432S028R_AnimateCurrentScreen,
    esp32_2432S028R_DoLedStuff,
    SCREENS_ARRAY_SIZE(esp32_2432S028RCyclicScreens),
    0,
    WIDTH,
    HEIGHT};
#endif
