#include <EEPROM.h>
#include <EEPROMAnything.h>
#include <MenuBackend.h>
#include <Encoder.h>
#include <Wire.h>  //i2c library
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP085_U.h>
#include <toneAC.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>
#include <RTClib.h>
#include <Arduino.h>

//////////////////RTC///////////////////////
RTC_DS1307 rtc;
#define DATE_DAY 1
#define DATE_MONTH 2
#define DATE_YEAR 3
#define DATE_HOUR 4
#define DATE_MINUTE 5
uint8_t conf_date_displayed = 0;

uint16_t date_year;
uint8_t date_month;
uint8_t date_day;
uint8_t date_hour;
uint8_t date_minute;
/////////////////////////////////////////

//////////////////ENCODER///////////////////////
#define ENCODER_OPTIMIZE_INTERRUPTS
#define Enter 12
#define ENCODER_STEP 4

Encoder knob(2, 3);
long knobPosition = 0;
int lastEnterState = HIGH;
/////////////////////////////////////////

//////////////////MENU/////////////////////////
bool menuUsed = false;
bool menuUsed_last = false;
bool varioState = false;
uint8_t stat_displayed = 1;
//bool stat_blink_status = false;

#define MENU_RIGHT 0
#define MENU_LEFT 1
#define MENU_VARIO 2
#define MENU_TARE 10
#define MENU_ALTITUDE 11
#define MENU_MONTEE 12
#define MENU_DESCENTE 13
#define MENU_LIGHT 14
#define MENU_CONTRASTE 15
#define MENU_DATE 16
#define MENU_STAT 20
#define MENU_RECRESET 21

//this controls the menu backend and the event generation
MenuBackend menu = MenuBackend(menuUseEvent, menuChangeEvent);

MenuItem m_vario = MenuItem(NULL, MENU_VARIO); //Vario
MenuItem m_options = MenuItem(NULL, MENU_RIGHT); //Options
MenuItem m_stats = MenuItem(NULL, MENU_RIGHT); //Records
MenuItem m_retour = MenuItem(NULL, MENU_LEFT); //Retour

MenuItem m_tare = MenuItem(NULL, MENU_TARE); //Tare
MenuItem m_altitude = MenuItem(NULL, MENU_ALTITUDE); //Altitude
MenuItem m_montee = MenuItem(NULL, MENU_MONTEE); //Montée
MenuItem m_descente = MenuItem(NULL, MENU_DESCENTE); //Descente
MenuItem m_light = MenuItem(NULL, MENU_LIGHT); //eclairage
MenuItem m_contrast = MenuItem(NULL, MENU_CONTRASTE); //contrast
MenuItem m_date = MenuItem(NULL, MENU_DATE); //date

MenuItem m_retour2 = MenuItem(NULL, MENU_LEFT); //Retour
MenuItem m_stat = MenuItem(NULL, MENU_STAT); //Stat
MenuItem m_recreset = MenuItem(NULL, MENU_RECRESET); //Reset records

/////////////////////////////////////////

//////////////////ECRAN///////////////////////
#define enablePartialUpdate
#define PIN_SCLK  8
#define PIN_LIGHT  11
#define PIN_SCE   7
#define PIN_RESET 6
#define PIN_DC    5
#define PIN_SDIN  4

Adafruit_PCD8544 display = Adafruit_PCD8544(PIN_SCLK, PIN_SDIN, PIN_DC, PIN_SCE, PIN_RESET);
/////////////////////////////////////////

/////////////////////////////////////////
bool initialisation = false;  //If true, reset and update eeprom memory at arduino start
/////////////////////////////////////////

/////////////////////VARIO/////////////////////////
Adafruit_BMP085_Unified bmp085 = Adafruit_BMP085_Unified(10085); //set up bmp085 sensor
#define ALTI_TRIGGER 4 //Trigger to start and stop chrono in meter
float Altitude;
int altitude_temp;
uint8_t chrono_cpt = 0;

float vario = 0;
bool is_vario_button_push = false;
uint16_t average_vcc = 0;             //variable to hold the value of Vcc from battery
double average_pressure;
unsigned long get_time1 = millis();
unsigned long get_time2 = millis();
unsigned long get_timeBeep = millis();
uint8_t beebLatency = 0;
uint8_t push_write_eeprom = 6;
float    my_temperature;
float    alt;
float    tim;

#define memoryBase 32
// Configuration structure (144 bits)
struct Conf
{
  float vario_climb_rate_start;    //minimum climb beeping value(ex. start climbing beeping at 0.4m/s)
  float vario_sink_rate_start;    //maximum sink beeping value (ex. start sink beep at -1.1m/s)
  int currentAltitude;
  uint8_t light_cpt;
  uint8_t contrast_default;
  uint8_t volume;
  float p0;
  uint8_t stat_index;
} conf = {
  0.8 , -1.1 , 0, 0, 50, 10, 1040.00, 0
};

// Statistic structure (176 bits)
#define  NB_STATS 5
struct Stat
{
  uint32_t chrono_start;
  uint16_t chrono;
  int alti_max;
  int alti_min;
  float txchutemax;
  float txchutemin;
  float cumul_alt;
}  stat = {
  0, 0, -20000, 20000, 0, 0, 0
};


void readStat(uint8_t index = conf.stat_index, Stat &value = stat)
{
  EEPROM_readAnything(sizeof(Conf) + sizeof(Stat) * index, value);
}
void writeStat(uint8_t index = conf.stat_index, Stat &value = stat)
{
  EEPROM_writeAnything(sizeof(Conf) + sizeof(Stat) * index, value);
}

void incrementStatIndex()
{
  conf.stat_index++;
  if (conf.stat_index > NB_STATS - 1)
    conf.stat_index = 0;
  EEPROM_writeAnything(0, conf);
  readStat();
}

void resetStat(uint8_t index = conf.stat_index, bool sound = true)
{
  stat.chrono_start = 0;
  stat.chrono = 0;
  stat.alti_max = -20000;
  stat.alti_min = 20000;
  stat.txchutemax = 0;
  stat.txchutemin = 0;
  stat.cumul_alt = 0;
  writeStat(index);

  altitude_temp = Altitude;

  chrono_cpt = 0;

  if (sound) {
    playConfirmMelody();
  }
}

void resetAllStats()
{
  for (uint8_t i = 0; i < NB_STATS; i++) {
    readStat(i);
    resetStat(i, false);
  }
  conf.stat_index = 0;
  EEPROM_writeAnything(0, conf);
  readStat(0);

  playConfirmMelody();
}

void playConfirmMelody()
{
  toneAC(700, conf.volume, 150);
  toneAC(500, conf.volume, 150);
}

void initEeprom()
{
  EEPROM_writeAnything(0, conf);
  resetAllStats();
}


void renderChrono(Stat value = stat)
{
  uint16_t s;
  if (value.chrono == 0 && value.chrono_start != 0) {
    DateTime now = rtc.now();
    s = now.unixtime() - value.chrono_start;
  }
  else
    s = value.chrono;

  uint8_t h = floor(s / 3600);
  s -= h * 3600;
  uint8_t m = floor(s / 60);
  s -= m * 60;

  display.print(h);
  display.print(F(":"));
  renderZero(m);
  display.print(m);
  display.print(F(":"));
  renderZero((int)s);
  display.print(s);
}

void renderVario()
{
  display.fillRect(0, 0, 84, 32, WHITE);
  // text display tests
  display.setCursor(0, 0);

  display.setTextColor(BLACK);
  display.setTextSize(2);
  display.print((int)Altitude);
  display.setTextSize(1);
  display.print(F("m"));
  
  DateTime now = rtc.now();
    
  display.setCursor(55, 0);  
  renderZero(now.hour());
  display.print(now.hour());
  display.setCursor(66, 0);
  display.print(F(":"));
  display.setCursor(72, 0);
  renderZero(now.minute());
  display.print(now.minute());

  if (now.second() % 2 == 0) {   

    uint8_t vcc = readVccPercent();
    uint8_t bat_x = 72;
    uint8_t bat_y = 9;
    display.drawRect(bat_x + 2, bat_y, 10, 6, BLACK);
    display.fillRect(bat_x, bat_y + 2, 2, 2, BLACK);
    display.fillRect(bat_x + 3 + (int)(99 - vcc) / 12, bat_y + 1, 8 - (int)(99 - vcc) / 12, 4, BLACK);   
  }
  else {
    display.setCursor(62, 9);
    display.print((int)my_temperature);
    display.drawCircle(75, 10, 1, BLACK);
    display.setCursor(72, 9);
    display.print(F(" C"));
  }

  display.setTextSize(2);
  display.setCursor(0, 16);

  display.setTextColor(WHITE, BLACK);

  float vario_abs = abs(vario);
  display.print((vario <= -0.05) ? F("-") : (vario >= 0.05) ? F("+") : F(" "));
  uint8_t m = floor(vario_abs);
  display.print(m);
  display.print(F("."));
  display.print(round(10 * vario_abs) - (10 * m));
  display.setTextSize(1);
  display.setCursor(48, 24);
  display.print(F("m/s"));

  display.setTextSize(1);
  display.setTextColor(BLACK);
  display.setCursor(0, 41);
  display.fillRect(0, 41, 84, 7, WHITE);

  display.print(F("M"));
  display.print(conf.stat_index + 1);
  display.print(F(" "));
  renderChrono();

  display.display();
}

void renderVarioBar()
{
  float vario_abs = abs(vario);
  display.fillRect(0, 32, 84, 9, WHITE);
  if (vario >= 0)
    display.fillRect(42, 32, round(vario_abs * 15), 8, BLACK);
  else
    display.drawRect(42, 32, -round(vario_abs * 15), 8, BLACK);

  display.display();
}

void renderVolume(uint8_t dir = MENU_RIGHT)
{
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE, BLACK);

  if  (dir == MENU_RIGHT)
    (conf.volume == 10) ? conf.volume = 10 : conf.volume += 2;
  else if (dir == MENU_LEFT)
    (conf.volume == 0) ? conf.volume = 0 : conf.volume -= 2;

  push_write_eeprom = 0;
  get_time2 = millis();  //stop the refresh rendering vario

  display.println(F("Volume:"));
  (conf.volume == 0) ? display.print(F("Off")) : display.print(conf.volume);
  display.display();
}

float updateConfItem(float var, uint8_t dir = 2, float increment = 1)
{
  // save the configuration if a parameter is validate.
  if (menuUsed_last == true && menuUsed == false) {
    menuUsed_last = false;
    EEPROM_writeAnything(0, conf);
  }
  // otherwise, update the parameter.
  else if (menuUsed) {
    menuUsed_last = menuUsed;
    if  (dir == MENU_RIGHT)
      var += increment;
    else if (dir == MENU_LEFT)
      var -= increment;
  }
  return var;
}

void renderStatItem(float value, const __FlashStringHelper *unit, bool integer = false)
{
  menuUsed = false;
  display.setTextColor(BLACK);

  if (integer == true) {
    display.print((int)value);
  }
  else {
    if (value < 0)
      display.print(F("-"));
    float value_abs = abs(value);
    uint8_t m = floor(value_abs);
    display.print(m);
    display.print(F("."));
    display.print(round(10 * value_abs) - (10 * m));
  }
  display.setTextSize(1);
  display.print(unit);
}

void renderZero(int value)
{
  if (value <  10)
    display.print(F("0"));
}


void renderDateTime(DateTime d, uint8_t bold = 0)
{
  //display date
  if (bold == DATE_DAY)
    display.setTextColor(WHITE, BLACK);
  renderZero(d.day());
  display.print(d.day());

  if (bold != 0)
    display.setTextColor(BLACK);
  display.print(F("/"));

  if (bold == DATE_MONTH)
    display.setTextColor(WHITE, BLACK);
  renderZero(d.month());
  display.print(d.month());

  if (bold != 0)
    display.setTextColor(BLACK);
  display.print(F("/"));

  if (bold == DATE_YEAR)
    display.setTextColor(WHITE, BLACK);
  renderZero(d.year() - 2000);
  display.print(d.year() - 2000); //reduces the length of the year string

  if (bold != 0)
    display.setTextColor(BLACK);
  display.print(F(" "));

  //display time
  if (bold == DATE_HOUR)
    display.setTextColor(WHITE, BLACK);
  renderZero(d.hour());
  display.print(d.hour());

  if (bold != 0)
    display.setTextColor(BLACK);
  display.print(F(":"));

  if (bold == DATE_MINUTE)
    display.setTextColor(WHITE, BLACK);
  renderZero(d.minute());
  display.print(d.minute());

  if (bold != 0)
    display.setTextColor(BLACK);
}

void renderMenu(MenuItem newMenuItem = menu.getCurrent(), uint8_t dir = 2)
{
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(BLACK);
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.setTextColor(WHITE, BLACK);

  if (newMenuItem.getShortkey() != MENU_LEFT)
    if (newMenuItem.getShortkey() < 10)
      display.println(F("Accueil"));
    else if (newMenuItem.getShortkey() >= 10 && newMenuItem.getShortkey() < 20)
      display.println(F("Options"));
    else if (newMenuItem.getShortkey() >= 20 && newMenuItem.getShortkey() < 30)
      display.println(F("Statistiques"));

  display.setTextSize(2);
  display.println(newMenuItem.getName());

  if (!menuUsed)
    display.setTextColor(BLACK);

  if (varioState == true && menuUsed) {
    varioState = false;
    menuUsed = false;
  }

  //sous-menu (valeur)
  if (varioState == false) {

    switch (newMenuItem.getShortkey()) {

      case MENU_VARIO:
        {
          if (menuUsed) {
            varioState = true;
            menuUsed = false;
          }
        }
        break;

      case MENU_TARE:
        {
          if (menuUsed) {
            menuUsed = false;
            display.print(F("Ok"));

            sensors_event_t event;
            bmp085.getEvent(&event);
            conf.p0 = event.pressure;

            //prevent chrono start and beeping
            resetAltitudeSensor();

            EEPROM_writeAnything(0, conf);
          }
        }
        break;

      case MENU_ALTITUDE:
        {
          conf.currentAltitude = updateConfItem(conf.currentAltitude, dir, 5);
          //prevent chrono start and beeping
          resetAltitudeSensor();

          display.print(conf.currentAltitude);
          display.setTextSize(1);
          display.print(F("m"));
        }
        break;

      case MENU_MONTEE:
        {
          conf.vario_climb_rate_start = updateConfItem(conf.vario_climb_rate_start, dir, 0.1);

          if (conf.vario_climb_rate_start < 0.1) {
            conf.vario_climb_rate_start = 0;
            display.print(F("Off"));
          }
          else {
            display.print(conf.vario_climb_rate_start);
            display.setTextSize(1);
            display.print(F("m/s"));
          }
        }
        break;

      case MENU_DESCENTE:
        {
          conf.vario_sink_rate_start = updateConfItem(conf.vario_sink_rate_start, dir, 0.1);

          if (conf.vario_sink_rate_start >= 0) {
            conf.vario_sink_rate_start = 0;
            display.print(F("Off"));
          }
          else {
            display.print(conf.vario_sink_rate_start);
            display.setTextSize(1);
            display.print(F("m/s"));
          }
        }
        break;

      case MENU_LIGHT:
        {
          conf.light_cpt = updateConfItem(conf.light_cpt, dir, -1);

          if  (conf.light_cpt <= 0)
            conf.light_cpt = 0;

          if (conf.light_cpt >= 5) {
            conf.light_cpt = 5;
            display.print(F("Off"));
          }
          else {
            display.print(5 - conf.light_cpt);
          }
        }
        break;

      case MENU_CONTRASTE:
        {
          conf.contrast_default = updateConfItem(conf.contrast_default, dir, 1);

          if (conf.contrast_default <= 0) {
            conf.contrast_default = 0;
          }
          else if (conf.contrast_default >= 100) {
            conf.contrast_default = 100;
          }

          display.print(conf.contrast_default);
          display.setContrast(conf.contrast_default);
        }
        break;

      case MENU_DATE:
        {
          if (menuUsed_last == false) {

            menuUsed_last = true;
            display.setTextSize(1);

            DateTime now = rtc.now();
            renderDateTime(now);

            date_year = now.year();
            date_month = now.month();
            date_day = now.day();
            date_hour = now.hour();
            date_minute = now.minute();
          }
          else {

            display.setTextSize(1);

            if  (dir != MENU_RIGHT && dir != MENU_LEFT) {

              if (conf_date_displayed < 5) {

                menuUsed_last = true;
                menuUsed = true;
                conf_date_displayed++;
              }
              else {
                menuUsed = false;
                menuUsed_last = true;
                conf_date_displayed = 0;
                display.setTextColor(BLACK);
                rtc.adjust(DateTime(date_year, date_month, date_day, date_hour, date_minute, 0));
                display.setTextSize(1);
                renderDateTime(rtc.now());
              }
            }

            switch (conf_date_displayed) {

              case DATE_DAY:
                if (dir == MENU_RIGHT) (date_day >= 31) ? date_day = 1 : date_day++;
                if (dir == MENU_LEFT) (date_day == 1) ? date_day = 31 : date_day--;
                //display.println(F("Jour"));
                break;
              case DATE_MONTH:
                if (dir == MENU_RIGHT) (date_month >= 12) ? date_month = 1 : date_month++;
                if (dir == MENU_LEFT) (date_month == 1) ? date_month = 12 : date_month--;
                //display.println(F("Mois"));
                break;
              case DATE_YEAR:
                if (dir == MENU_RIGHT) (date_year >= 2099) ? date_year = 2000 : date_year++;
                if (dir == MENU_LEFT) (date_year == 2000) ? date_year = 2099 : date_year--;
                //display.println(F("Annee"));
                break;
              case DATE_HOUR:
                if (dir == MENU_RIGHT) (date_hour >= 23) ? date_hour = 0 : date_hour++;
                if (dir == MENU_LEFT) (date_hour == 0) ? date_hour = 23 : date_hour--;
                //display.println(F("Heure"));
                break;
              case DATE_MINUTE:
                if (dir == MENU_RIGHT) (date_minute >= 59) ? date_minute = 0 : date_minute++;
                if (dir == MENU_LEFT)(date_minute == 0) ? date_minute = 59 : date_minute--;
                //display.println(F("Minute"));
                break;
            }

            display.setTextColor(BLACK);
            renderDateTime(DateTime(date_year, date_month, date_day, date_hour, date_minute, 0), conf_date_displayed);
            //display.print(date_conf[conf_date_displayed]);
          }
        }
        break;

      case MENU_STAT:
        {
          menuUsed = false;
          Stat stat_to_display;
          readStat(stat_displayed - 1, stat_to_display);

          display.clearDisplay();
          display.setTextSize(1);
          display.setTextColor(WHITE, BLACK);

          if (stat_to_display.chrono == 0) {
            display.print(F("M"));
            display.print(stat_displayed);
            display.print(F(" vide"));
          }
          else {

            //if (stat_blink_status) {

              renderDateTime(DateTime(stat_to_display.chrono_start));
              display.println();
            //}
            //else {
              
            display.setTextColor(BLACK);
            display.print(F("Chrono:"));
            renderChrono(stat_to_display);
            display.println();
            
            //}
            //stat_blink_status = !stat_blink_status;

            
            display.print(F("Alt max:"));
            renderStatItem(stat_to_display.alti_max, F("m"), true);
            display.println();
            
            display.print(F("Alt min:"));
            renderStatItem(stat_to_display.alti_min, F("m"), true);
            display.println();
            
            display.print(F("m/s:"));
            float m = floor(stat_to_display.txchutemax);
            m = m + (round(10 * stat_to_display.txchutemax) - (10 * m)) / 10;
            renderStatItem(m, F("|"));
            m = floor(stat_to_display.txchutemin);
            m = m + (round(10 * stat_to_display.txchutemin) - (10 * m)) / 10;
            renderStatItem(m, F(""));
            display.println();
            
            display.print(F("Cumul:"));
            renderStatItem(round(stat_to_display.cumul_alt), F("m"), true);
            display.println();
          }
        }
        break;

      case MENU_RECRESET:
        {
          if (menuUsed) {
            menuUsed = false;
            resetAllStats();
            display.setTextColor(BLACK);
            display.print(F("Ok"));
          }
        }
        break;
    }
  }
  display.display();
}

//this function builds the menu and connects the correct items together
void menuSetup()
{
  m_vario.name = F("Vario"); //Vario
  m_options.name = F("Options"); //Options
  m_stats.name = F("Stats"); //Stats
  m_retour.name = F("Retour"); //Retour
  m_tare.name = F("Tare"); //Tare
  m_altitude.name = F("Alti"); //Altitude
  m_montee.name = F("Montee"); //Montée
  m_descente.name = F("Desc"); //Descente
  m_light.name = F("Light"); //eclairage
  m_contrast.name = F("Contra"); //eclairage
  m_date.name = F("Date"); //date
  m_retour2.name = F("Retour"); //Retour
  //m_stat.name = F("Stat"); //Stats
  m_recreset.name = F("Reset"); //Reset records

  /*
  	This is the structure of the modelled menu

          Vario
          Stats
                  Retour
                  Statistiques
                  Reset
          Options
                  Retour
                  Tare
                  Altitude
                  Montee
                  Descente
                  Light
                  Contra
                  Date

  */
  m_vario.addAfter(m_stats);

  m_stats.addAfter(m_options);
  m_stats.addRight(m_stat);
  m_stat.addBefore(m_retour2);
  m_stat.addAfter(m_recreset);
  m_retour2.addLeft(m_vario);

  m_options.addRight(m_tare);
  m_tare.addBefore(m_retour);
  m_tare.addAfter(m_altitude);
  m_altitude.addAfter(m_montee);
  m_montee.addAfter(m_descente);
  m_descente.addAfter(m_light);
  m_light.addAfter(m_contrast);
  m_contrast.addAfter(m_date);
  m_retour.addLeft(m_vario);

  menu.use(m_vario);
}

/*
  Here all menu use events are handled
*/
void menuUseEvent(MenuUseEvent used)
{
  if (used.item.getShortkey() == MENU_RIGHT) {
    menu.moveRight();
  }
  else if (used.item.getShortkey() == MENU_LEFT) {
    menu.moveLeft();
  }
  else
    menuUsed = !menuUsed;

  renderMenu();
}

/*
  That is when the user changes the menu
*/
void menuChangeEvent(MenuChangeEvent changed)
{
  renderMenu(changed.to);
}

int readVccPercent()
{
  uint16_t real_bat = (int)(4.89 * analogRead(A0));
  //Serial.println(analogRead(A0));
  //Serial.println(real_bat);
  average_vcc = (average_vcc == 0) ? real_bat : (int)(average_vcc * 0.94 + real_bat * 0.06);

  uint8_t percent = round((average_vcc - 3300) * 100 / (3700 - 3300));
  if (percent >= 100)
    percent = 99;
  else if (percent < 1)
    percent = 1;

  return percent;
}

uint8_t getBeepLatency()
{  
   int latency = 240 - (vario * 60);
   return (latency < 130)? 130: (latency > 240)? 240: latency;
}

uint16_t getBeepFrequency()
{  
   int frequency = 790 + (200 * vario);   
   return (frequency < 100)? 100: (frequency > 1300)? 1300 :frequency;
}

void resetAltitudeSensor()
{
  // get a new sensor event
  sensors_event_t event;
  bmp085.getEvent(&event);
  average_pressure = event.pressure;                   //put it in filter and take average
  bmp085.getTemperature(&my_temperature);
  Altitude = bmp085.pressureToAltitude(conf.p0, average_pressure, my_temperature) + conf.currentAltitude;  //take new altitude in meters
  altitude_temp = Altitude;
  alt = Altitude;
  tim = millis();
}

void setup()
{
  //Serial.begin(9600);

  // load the configuration
  if (initialisation)
    initEeprom();

  EEPROM_readAnything(0, conf);
  readStat();

  Wire.begin();
  rtc.begin();
  if (!rtc.isrunning()) {
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(__DATE__, __TIME__));
  }

  pinMode(PIN_LIGHT, OUTPUT);

  bmp085.begin();
  resetAltitudeSensor();

  display.begin();
  display.setContrast(conf.contrast_default);
  display.setTextWrap(false);
  //display.setRotation(0);

  menuSetup();
  //Serial.println("Starting navigation:\r\nLeft: 4   Right: 6   Use: 5");
}

void loop()
{
  readButtons();
  // screen brightness. AnalogWrite values from 0 to 255
  analogWrite(PIN_LIGHT, conf.light_cpt * 51);

  // get a new sensor event
  sensors_event_t event;
  bmp085.getEvent(&event);
  // put it in filter and take average
  average_pressure = average_pressure * 0.94 + event.pressure * 0.06;
  // set up my_temperature
  bmp085.getTemperature(&my_temperature);
  // take new altitude in meters
  Altitude = bmp085.pressureToAltitude(conf.p0, average_pressure, my_temperature) + conf.currentAltitude;
  
  float tempo = millis();
  // put it in filter and take average
  vario = vario * 0.8 + (1000 * 0.2 * ((alt - Altitude) / (tim - tempo)));
  alt = Altitude;
  tim = tempo;

  // Update stats if chrono is running
  if (stat.chrono_start != 0) {

    if (Altitude > stat.alti_max)
      stat.alti_max = Altitude;
    if (Altitude < stat.alti_min)
      stat.alti_min = Altitude;

    if (vario < stat.txchutemax)
      stat.txchutemax = vario;

    if (vario > stat.txchutemin)
      stat.txchutemin = vario;
  }

  // make some beep...
  if (millis() >= (get_timeBeep + beebLatency))
  {
    get_timeBeep = millis();
    beebLatency = getBeepLatency();
   
    if (vario > conf.vario_climb_rate_start && conf.vario_climb_rate_start != 0) {
      //when climbing make faster and shorter beeps
      toneAC(getBeepFrequency(), conf.volume, beebLatency, true);

    } else if (vario < conf.vario_sink_rate_start && conf.vario_sink_rate_start != 0) {

      toneAC(getBeepFrequency(), conf.volume, beebLatency, true);
    }
  }

  //every 67 milliseconds, update vario bar
  if (millis() >= (get_time1 + 67) && varioState == true)
  {
    get_time1 = millis();
    renderVarioBar();
  }

  //every second
  if (millis() >= (get_time2 + 1000))
  {
    get_time2 = millis();

    if (stat.chrono_start != 0 && vario > 0) {
      stat.cumul_alt += vario;
    }

    /*
    if (menu.getCurrent().getShortkey() == MENU_STAT) {
      menu.use();
    }
    */
    // proceedings of the dynamic display of vario
    if (varioState == true)
      renderVario();

    // eeprom saves after 5 seconds
    if (push_write_eeprom == 5) {
      push_write_eeprom++;
      EEPROM_writeAnything(0, conf);
    }
    else if (push_write_eeprom < 5)
      push_write_eeprom++;


    // in vario, if button enter is pressed 1 seconds, reset stats
    if (is_vario_button_push == true) {

      is_vario_button_push = false;
      resetStat();

      display.setTextSize(1);
      display.setTextColor(BLACK);
      display.setCursor(0, 41);
      display.fillRect(0, 41, 84, 7, WHITE);
      get_time2 = millis();  //stop the refresh rendering vario
      display.print(F("R.A.Z. stat M"));
      display.print(conf.stat_index + 1);
      display.display();
    }


    // if the altitude out of his "zone", the timer is started
    //if (stat.chrono_start == 0) {
      if (Altitude > altitude_temp + ALTI_TRIGGER || Altitude < altitude_temp - ALTI_TRIGGER) {
        resetStat();
        DateTime now = rtc.now();
        stat.chrono_start = now.unixtime();
      }
      else { // every 15 seconds, the altitude "zone" is updated
        chrono_cpt++;
        if (chrono_cpt >= 15) {
          chrono_cpt = 0;
          altitude_temp = Altitude;
        }
      }
    //}
    if (stat.chrono_start != 0 && stat.chrono == 0) {
      // if left in the same altitude "zone" 15 seconds, the timer is stopped
      if (altitude_temp - ALTI_TRIGGER / 2 < Altitude && altitude_temp + ALTI_TRIGGER / 2 > Altitude) {
        chrono_cpt++;
        if (chrono_cpt >= 15) {
          DateTime now = rtc.now();
          stat.chrono = now.unixtime() - stat.chrono_start;
          writeStat();
          incrementStatIndex();
        }
      }
      else {
        chrono_cpt = 0;
        altitude_temp = Altitude;
      }
    }
  }
}

void readButtons()
{
  long newKnobPosition = knob.read();
  if (newKnobPosition != knobPosition) {
    //Serial.print(newKnobPosition);
    //Serial.print(F(" / "));
    //Serial.println(knobPosition);
    if (abs(newKnobPosition - knobPosition) >= ENCODER_STEP) {
      if (newKnobPosition > knobPosition) { //Right
        if (!menuUsed && varioState == false) {
          if (menu.getCurrent().getShortkey() == MENU_STAT && stat_displayed < NB_STATS) {
            get_time2 += 1000;
            //stat_blink_status = true;
            stat_displayed++;
            renderMenu();
          }
          else
            menu.moveDown();
        }
        else if (varioState == false)
          renderMenu(menu.getCurrent(), MENU_RIGHT);
        else if (varioState == true)
          renderVolume(MENU_RIGHT);
      }
      else { //Left
        if (!menuUsed && varioState == false) {
          if (menu.getCurrent().getShortkey() == MENU_STAT && stat_displayed > 1) {
            get_time2 += 1000;
            //stat_blink_status = true;
            stat_displayed--;
            renderMenu();
          }
          else
            menu.moveUp();
        }
        else if (varioState == false)
          renderMenu(menu.getCurrent(), MENU_LEFT);
        else if (varioState == true)
          renderVolume(MENU_LEFT);
      }
      knobPosition = newKnobPosition;
    }
  }

  //if button enter is pressed
  int reading = digitalRead(Enter);
  if (reading == LOW && lastEnterState == HIGH) {

    // in menu, clic an item
    if (varioState == false) {
      get_time2 = millis() - 1000;
      menu.use();
    }
    // in vario, button enter init timer
    else {
      get_time2 = millis();
      is_vario_button_push = true;
    }
  }

  // in vario, stop button enter and go back to menu
  if (reading == HIGH && lastEnterState == LOW && varioState == true && is_vario_button_push == true) {
    is_vario_button_push = false;
    menu.use();
  }

  lastEnterState = reading;

}//end read button
