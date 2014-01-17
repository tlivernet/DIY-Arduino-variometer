//#include <SPI.h>
//#include <PCD8544_SPI.h>
#include <EEPROM.h>
#include <EEPROMAnything.h>
#include <MenuBackend.h>
#include <Encoder.h>
#include <Wire.h>  //i2c library
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP085_U.h>                 //bmp085 library, download from url link (1)
#include <toneAC.h>
#include <Adafruit_GFX.h>  
#include <Adafruit_PCD8544.h>
#include <RTClib.h>
//#include <avr/pgmspace.h>
//#include <Flash.h>
#include <Arduino.h> 

RTC_DS1307 rtc;
uint8_t hour;
uint8_t minute;

//////////////////ENCODER///////////////////////
#define ENCODER_OPTIMIZE_INTERRUPTS
Encoder knobLeft(2, 3);
long positionLeft  = 0;
#define Enter 12
int lastEnterState = HIGH;
long enterDebounceTime = 0;
long debounceDelay = 200;

//////////////////MENU/////////////////////////
bool menuUsed = false;
bool menuUsed_last = false;
bool varioUsed = false;

#define MENU_RIGHT 0
#define MENU_LEFT 1
#define MENU_VARIO 2
//#define MENU_TARE 10
#define MENU_ALTITUDE 10
#define MENU_MONTEE 11
#define MENU_DESCENTE 12
#define MENU_LIGHT 13
#define MENU_CONTRASTE 14
#define MENU_HEURE 15
#define MENU_MINUTE 16
#define MENU_CHRONO 20
#define MENU_ALTIMAX 21
#define MENU_ALTIMIN 22
#define MENU_TXCHUTEMAX 23
#define MENU_TXCHUTEMIN 24
#define MENU_RECRESET 25

//this controls the menu backend and the event generation
MenuBackend menu = MenuBackend(menuUseEvent,menuChangeEvent);

MenuItem m_vario = MenuItem(NULL, MENU_VARIO); //Vario
  MenuItem m_options = MenuItem(NULL, MENU_RIGHT); //Options
  MenuItem m_stats = MenuItem(NULL, MENU_RIGHT); //Records
      MenuItem m_retour = MenuItem(NULL, MENU_LEFT); //Retour
      
      //MenuItem m_tare = MenuItem(NULL, MENU_TARE); //Tare
      MenuItem m_altitude = MenuItem(NULL, MENU_ALTITUDE); //Altitude
      MenuItem m_montee = MenuItem(NULL, MENU_MONTEE); //Montée
      MenuItem m_descente = MenuItem(NULL, MENU_DESCENTE); //Descente
      MenuItem m_light = MenuItem(NULL, MENU_LIGHT); //eclairage
      MenuItem m_contrast = MenuItem(NULL, MENU_CONTRASTE); //contrast
      MenuItem m_heure = MenuItem(NULL, MENU_HEURE); //heure
      MenuItem m_minute = MenuItem(NULL, MENU_MINUTE); //minute
        
      MenuItem m_retour2 = MenuItem(NULL, MENU_LEFT); //Retour
      MenuItem m_chrono = MenuItem(NULL, MENU_CHRONO); //Chrono
      MenuItem m_altimin = MenuItem(NULL, MENU_ALTIMIN); //Altitude min
      MenuItem m_altimax = MenuItem(NULL, MENU_ALTIMAX); //Altitude max      
      MenuItem m_txchutemax = MenuItem(NULL, MENU_TXCHUTEMAX); //Taux de chute max 
      MenuItem m_txchutemin = MenuItem(NULL, MENU_TXCHUTEMIN); //Taux de chute min 
      MenuItem m_recreset = MenuItem(NULL, MENU_RECRESET); //Reset records
      
//////////////////ECRAN///////////////////////
#define enablePartialUpdate
#define PIN_SCLK  8
#define PIN_LIGHT  11
#define PIN_SCE   7
#define PIN_RESET 6
#define PIN_DC    5
#define PIN_SDIN  4

Adafruit_PCD8544 display = Adafruit_PCD8544(PIN_SCLK, PIN_SDIN, PIN_DC, PIN_SCE, PIN_RESET);  

///////////////////////////////////////// variables that You can test and try
//float vario_climb_rate_start = 1;    //minimum climb beeping value(ex. start climbing beeping at 0.4m/s)
//float vario_sink_rate_start = -2;    //maximum sink beeping value (ex. start sink beep at -1.1m/s)
#define  MAX_SAMPLES 10
//#define UART_SPEED 9600                //define serial transmision speed (9600,19200, etc...)
//uint16_t currentAltitude = 50;

//uint8_t contrast_default = 50;
bool initialisation = false;

/////////////////////////////////////////

/////////////////////VARIO/////////////////////////
//Adafruit_BMP085_Unified bmp085;            //set up bmp085 sensor
Adafruit_BMP085_Unified bmp085 = Adafruit_BMP085_Unified(10085);

#define ALTI_TRIGGER 2
float    Altitude;
int altitude_temp;
uint32_t chrono_start = 0;
uint32_t chrono_stop = 0;
uint8_t chrono_cpt = 0;

float vario = 0;
bool is_vario_button_push = false;
float Battery_Vcc = 0;             //variable to hold the value of Vcc from battery
const float p0 = 1040.00;              //Pressure at sea level (Pa)
double average_pressure;
unsigned long get_time1 = millis();
unsigned long get_time2 = millis();
unsigned long get_time3 = millis();
unsigned long get_time4 = millis();
unsigned long get_time5 = millis();
boolean push_write_eeprom = false;
boolean  thermalling = false;
float      my_temperature = 1;
float    alt[(MAX_SAMPLES + 1)];
float    tim[(MAX_SAMPLES + 1)];

#define memoryBase 32
struct Conf
{
    float vario_climb_rate_start;    //minimum climb beeping value(ex. start climbing beeping at 0.4m/s)
    float vario_sink_rate_start;    //maximum sink beeping value (ex. start sink beep at -1.1m/s)
    int currentAltitude;
    uint8_t light_cpt;
    uint8_t contrast_default;
    int alti_max;
    int alti_min;
    float txchutemax;
    float txchutemin;
    uint8_t volume;
} conf = { 
  0.4 , -1.1 , 0, 0, 50, 0, 0, 0, 0, 10
};


inline float Averaging_Filter(float input) // moving average filter function
{  
  return average_pressure * 0.94 + input * 0.06;
}

void renderVario(){
      
      display.fillRect(0, 0, 84, 32, WHITE);
      // text display tests        
      display.setCursor(0,0);            
      
      display.setTextColor(BLACK);  
      display.setTextSize(2);  
      display.print((int)Altitude);
      display.setTextSize(1); 
      display.print(F("m"));
      
      DateTime now = rtc.now();
      if (now.second()%2 == 0){       
        
        display.setCursor(55,0);
        renderZero(now.hour());
        display.print(now.hour());
        display.setCursor(66,0);
        display.print(F(":"));      
        display.setCursor(72,0);
        renderZero(now.minute());
        display.print(now.minute());
      } 
      else {
        display.setCursor(62,0);
        display.print(round(my_temperature));
        display.drawCircle(75, 1, 1, BLACK);
        display.setCursor(72,0);
        display.print(F(" C"));
      }   
      
      display.setCursor(62,18);
      Battery_Vcc = readVcc();    // get voltage and prepare in percentage
      uint8_t v = round(floor(Battery_Vcc));
      display.print(F("."));
      display.print(round(10 * Battery_Vcc) - (10 * v));
      display.print(F("V"));
     
      
      display.setTextSize(2);  
      display.setCursor(0,16);
      
      display.setTextColor(WHITE, BLACK);

      float vario_abs = abs(vario);
      display.print((vario < 0)? F("-"): F("+"));
      uint8_t m = round(floor(vario_abs));
      display.print(m);
      display.print(F("."));
      display.print(round(10 * vario_abs) - (10 * m));
      display.setTextSize(1);  
      display.setCursor(48,24);
      display.print(F("m/s")); 
            
      display.setTextSize(1);  
      display.setTextColor(BLACK);
      display.setCursor(0,41);
      renderChrono();   

      display.display();      
}

void renderVarioBar(){
  /*
  float vario_abs = abs(vario);
  display.fillRect(0, 32, 84, 16, WHITE);
  if (vario >= 0)
    display.fillRect(42, 32, round(vario_abs * 15), 16, BLACK);
  else
    display.drawRect(42, 32, -round(vario_abs * 15), 16, BLACK);
    
  display.display(); */
}

void renderVolume(uint8_t dir = MENU_RIGHT){

    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(WHITE, BLACK);
    
    if  (dir == MENU_RIGHT)
      (conf.volume == 10)? conf.volume = 10 : conf.volume += 2;
    else if (dir == MENU_LEFT)
      (conf.volume == 0)? conf.volume = 0 : conf.volume -= 2;
    
    push_write_eeprom = true;
    get_time3 = millis();
    get_time2 = millis();  //stop the refresh rendering vario
    
    display.print(F("Volume:"));
    (conf.volume == 0)? display.print(F("Off")) : display.print(conf.volume);
    display.display();
}

float updateConfItem(float var, uint8_t dir = 2, float increment = 1){
    
    //enregistrement de la conf si validation d'un parametre
    if (menuUsed_last == true && menuUsed == false){
      menuUsed_last = false;
      EEPROM_writeAnything(0, conf);
    }
    //sinon modification de ce paramêtre    
    else if (menuUsed){
      menuUsed_last = menuUsed;
      if  (dir == MENU_RIGHT)
        var += increment;
      else if (dir == MENU_LEFT)
        var -= increment;              
    }    
    return var;
}

void renderMenuDisplayedItem(float value, const __FlashStringHelper *unit, bool integer = false){
   menuUsed = false;
   display.setTextColor(BLACK); 
   if (integer == true)
     value = (int)value;
   display.print(value);
   display.setTextSize(1);  
   display.print(unit);
}

void renderZero(int value){
  if (value == 0)
    display.print(F("0"));
}

void renderChrono(){
   if (chrono_start != 0){
     uint32_t chrono_s = chrono_stop;
     if (chrono_s == 0){
       DateTime now = rtc.now();
       chrono_s = now.unixtime();
     }
     uint32_t s = chrono_s - chrono_start;
     uint8_t h = floor(s/3600);
     s -= h * 3600;
     uint8_t m = floor(s/60);
     s -= m * 60;
     
     //renderZero(h);
     display.print(h);
     display.print(F(":"));
     renderZero(m);
     display.print(m);
     display.print(F(":"));
     renderZero((int)s);
     display.print(s);
   }
}

void resetStats(){
   conf.alti_min = 20000;
   conf.alti_max = -20000;
   conf.txchutemax = 0;
   conf.txchutemin = 0;
   
   altitude_temp = Altitude;
   chrono_start = chrono_stop = 0;          
   chrono_cpt = 0;
}

void renderMenu(MenuItem newMenuItem, uint8_t dir = 2){
  
        display.clearDisplay();
        display.setTextSize(1);  
        display.setTextColor(BLACK);  
        display.setCursor(0,0);
        //display.println(title); 
        display.setTextSize(1);
        display.setTextColor(WHITE, BLACK);
        
        if (newMenuItem.getShortkey() < 10)
          display.println(F("Accueil"));      
        else if (newMenuItem.getShortkey() >= 10 && newMenuItem.getShortkey() < 20)
          display.println(F("Options"));   
        else if (newMenuItem.getShortkey() >= 20 && newMenuItem.getShortkey() < 30)
          display.println(F("Records"));  
          
        display.setTextSize(2);
        display.println(newMenuItem.getName());
        
        if (!menuUsed)
           display.setTextColor(BLACK); 
         
         if (varioUsed && menuUsed){
           varioUsed = false;
           menuUsed = false;
         }         
         
         //sous-menu (valeur)
         if (varioUsed == false){

              if (newMenuItem.getShortkey() == MENU_VARIO && menuUsed){
                     varioUsed = true;
                     menuUsed = false;                   
              }              
              /*
              else if (newMenuItem.getShortkey() == MENU_TARE){
                 display.println(newMenuItem.getName());
              }
              */
              else if (newMenuItem.getShortkey() == MENU_ALTITUDE){
              
                conf.currentAltitude = updateConfItem(conf.currentAltitude, dir, 5);

                display.print(conf.currentAltitude);
                display.setTextSize(1);  
                display.print(F("m"));
              }
              
              else if (newMenuItem.getShortkey() == MENU_MONTEE){
              
                conf.vario_climb_rate_start = updateConfItem(conf.vario_climb_rate_start, dir, 0.1);

                if (conf.vario_climb_rate_start < 0.1){
                  conf.vario_climb_rate_start = 0;
                  display.print(F("Off"));
                }
                 else {
                  display.print(conf.vario_climb_rate_start);
                  display.setTextSize(1);  
                  display.print(F("m/s"));
                }       
              }
              
              else if (newMenuItem.getShortkey() == MENU_DESCENTE){

                conf.vario_sink_rate_start = updateConfItem(conf.vario_sink_rate_start, dir, 0.1);

                if (conf.vario_sink_rate_start >= 0){
                  conf.vario_sink_rate_start = 0;
                  display.print(F("Off"));
                }
                 else{
                  display.print(conf.vario_sink_rate_start);
                  display.setTextSize(1);  
                  display.print(F("m/s"));
                }  
              }  
              
              else if (newMenuItem.getShortkey() == MENU_LIGHT){
              
                conf.light_cpt = updateConfItem(conf.light_cpt, dir, -1);
                
                if  (conf.light_cpt <= 0)
                  conf.light_cpt = 0;    
                
                if (conf.light_cpt >= 5){
                  conf.light_cpt = 5;
                  display.print(F("Off"));
                }
                else{
                  display.print(5 - conf.light_cpt);
                }
              }
              
              else if (newMenuItem.getShortkey() == MENU_CONTRASTE){
              
                conf.contrast_default = updateConfItem(conf.contrast_default, dir, 1);
                  
                if (conf.contrast_default <= 0){
                  conf.contrast_default = 0;
                }
                else if (conf.contrast_default >= 100){
                  conf.contrast_default = 100;
                } 
                
                display.print(conf.contrast_default);
                display.setContrast(conf.contrast_default); 
              }        
              
              else if (newMenuItem.getShortkey() == MENU_HEURE || newMenuItem.getShortkey() == MENU_MINUTE){
                
                DateTime now = rtc.now();
                if (menuUsed_last == false){
                  hour = now.hour();
                  minute = now.minute();
                }
                if (menuUsed_last == true && menuUsed == false){
                  menuUsed_last = false;
                  rtc.adjust(DateTime(now.year(), now.month(),now.day(), hour, minute, 0));
                }
                //sinon modification de ce paramêtre    
                else if (menuUsed){
                  menuUsed_last = menuUsed;
                  if  (dir == MENU_RIGHT)
                    (newMenuItem.getShortkey() == MENU_HEURE)? (hour = 23)? hour = 0 : hour ++ : (minute == 59)? minute = 0 : minute ++;
                  else if (dir == MENU_LEFT)
                    (newMenuItem.getShortkey() == MENU_HEURE)? (hour == 0)? hour = 23: hour -- : (minute == 0)? minute = 59 : minute --;
                }                    
                display.print((newMenuItem.getShortkey() == MENU_HEURE)? hour : minute);
              }  
              
              else if (newMenuItem.getShortkey() == MENU_CHRONO){
                 menuUsed = false;
                 display.setTextColor(BLACK);                 
                 renderChrono();
              }
              
              else if (newMenuItem.getShortkey() == MENU_ALTIMIN){                 
                 //menuUsed = false;
                 //display.setTextColor(BLACK); 
                 //display.print(conf.alti_min);
                 //display.setTextSize(1);  
                 //display.print(F("m"));
                 renderMenuDisplayedItem(conf.alti_min, F("m"), true);
              }
              else if (newMenuItem.getShortkey() == MENU_ALTIMAX){
                 //menuUsed = false;
                 //display.setTextColor(BLACK); 
                 //display.print(conf.alti_max);
                 //display.setTextSize(1);  
                 //display.print(F("m"));
                 renderMenuDisplayedItem(conf.alti_max, F("m"), true);
              }  
              else if (newMenuItem.getShortkey() == MENU_RECRESET){
                 if (menuUsed){
                   menuUsed = false;
                   resetStats();                   
                   display.print(F("Ok"));
                 }
              } 
              else if (newMenuItem.getShortkey() == MENU_TXCHUTEMAX){
                 //menuUsed = false;
                 //display.setTextColor(BLACK); 
                 //display.print(conf.txchutemax);
                 //display.setTextSize(1);  
                 //display.print(F("m/s"));
                 renderMenuDisplayedItem(conf.txchutemax, F("m/s"));
              }  
              else if (newMenuItem.getShortkey() == MENU_TXCHUTEMIN){
                 //menuUsed = false;
                 //display.setTextColor(BLACK); 
                 //display.print(conf.txchutemin);
                 //display.setTextSize(1);  
                 //display.print(F("m/s"));
                 renderMenuDisplayedItem(conf.txchutemin, F("m/s"));
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
      //m_tare.name = F("Tare"); //Tare
      m_altitude.name = F("Alti"); //Altitude
      m_montee.name = F("Montee"); //Montée
      m_descente.name = F("Desc"); //Descente
      m_light.name = F("Light"); //eclairage
      m_contrast.name = F("Contra"); //eclairage
      m_heure.name = F("Heure"); //heure
      m_minute.name = F("Minute"); //minute      
      m_retour2.name = F("Retour"); //Retour
      m_chrono.name = F("Chrono"); //Chrono
      m_altimin.name = F("AltMin"); //Altitude min
      m_altimax.name = F("AltMax"); //Altitude max  
      m_recreset.name = F("Reset"); //Reset records
      m_txchutemax.name = F("Tx max"); //Taux de chute max
      m_txchutemin.name = F("Tx min"); //Taux de chute min

  /*
  	This is the structure of the modelled menu
  	
          Vario
          Options
                  Retour
                  Tare
                  Altitude
                  Montée
                  Descente
                  Light     
                  Contra           
  
  */
    
    m_vario.addAfter(m_stats);
    
    m_stats.addAfter(m_options);                             
      m_stats.addRight(m_chrono);
        m_chrono.addBefore(m_retour2);
        m_chrono.addAfter(m_altimax);
        m_altimax.addAfter(m_altimin);
        m_altimin.addAfter(m_txchutemin);
        m_txchutemin.addAfter(m_txchutemax);
        m_txchutemax.addAfter(m_recreset);
        m_retour2.addLeft(m_vario);
        
    m_options.addRight(m_altitude);
        m_altitude.addBefore(m_retour);                
        m_altitude.addAfter(m_montee);
        m_montee.addAfter(m_descente);
        m_descente.addAfter(m_light);
        m_light.addAfter(m_contrast);
        m_contrast.addAfter(m_heure);
        m_heure.addAfter(m_minute);                 
        m_retour.addLeft(m_vario);
        
    menu.use(m_vario);
}

/*
	This is an important function
	Here all use events are handled
	
	This is where you define a behaviour for a menu item
*/
void menuUseEvent(MenuUseEvent used)
{
  if (used.item.getShortkey() == MENU_RIGHT){
    menu.moveRight();    
  }
  else if (used.item.getShortkey() == MENU_LEFT){
    menu.moveLeft();
  }
  else
    menuUsed = !menuUsed;    
  
  renderMenu(menu.getCurrent());
}

/*
	This is an important function
	Here we get a notification whenever the user changes the menu
	That is, when the menu is navigated
*/
void menuChangeEvent(MenuChangeEvent changed)
{    
  renderMenu(changed.to);
}

float readVcc() {
  long result;
  // Read 1.1V reference against AVcc
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  delay(2); // Wait for Vref to settle
  ADCSRA |= _BV(ADSC); // Convert
  while (bit_is_set(ADCSRA,ADSC));
  result = ADCL;
  result |= ADCH<<8;
  return (1126400L / result)/1000; // Back-calculate AVcc in V
}

void setup()
{
	//Serial.begin(9600);

  //chargement de la configuration
  //EEPROM_writeAnything(0, conf);    
  EEPROM_readAnything(0, conf);

  Wire.begin();
  rtc.begin();
  if (!rtc.isrunning()) {
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(__DATE__, __TIME__));
  }

  pinMode(PIN_LIGHT, OUTPUT);

  bmp085.begin();
  /* Get a new sensor event */ 
  sensors_event_t event;
  bmp085.getEvent(&event);
  average_pressure = event.pressure;                   //put it in filter and take average
  bmp085.getTemperature(&my_temperature);
  Altitude = bmp085.pressureToAltitude(p0, average_pressure, my_temperature);  //take new altitude in meters
  altitude_temp = Altitude;
  
  display.begin();  
  display.setContrast(conf.contrast_default);   

	menuSetup();
	//Serial.println("Starting navigation:\r\nLeft: 4   Right: 6   Use: 5");
}

void loop()
{
  readButtons();
  analogWrite(PIN_LIGHT, conf.light_cpt * 51);  // analogRead values go from 0 to 1023, analogWrite values from 0 to 255

  float tempo=millis();
  float N1=0;
  float N2=0;
  float N3=0;
  float D1=0;
  float D2=0;
  
  /* Get a new sensor event */ 
  sensors_event_t event;
  bmp085.getEvent(&event);
  
  average_pressure = Averaging_Filter(event.pressure);                   //put it in filter and take average
  bmp085.getTemperature(&my_temperature);
  Altitude = bmp085.pressureToAltitude(p0, average_pressure, my_temperature);  //take new altitude in meters
  
  if (millis() >= (get_time4+1000)) // auto start/stop chrono
  {
    get_time4 = millis();
    if (chrono_start == 0){
      if (Altitude > altitude_temp + ALTI_TRIGGER || Altitude < altitude_temp - ALTI_TRIGGER){ //si l'altitude sort de sa "zone", le chrono est lancé
        DateTime now = rtc.now();
        chrono_start = now.unixtime();
      }
      else { // toutes les 15 secondes, la zone d'altitude est mise à jour      
        chrono_cpt++;      
        if (chrono_cpt >= 15){
          chrono_cpt = 0;
          altitude_temp = Altitude;
        }
      }
    }
    else if (chrono_start != 0 && chrono_stop == 0){
      if (altitude_temp - ALTI_TRIGGER/2 < Altitude && altitude_temp + ALTI_TRIGGER/2 > Altitude){  // si l'altitude reste dans la même "zone" 15 secondes, le chrono est stoppé
        chrono_cpt++;      
        if (chrono_cpt >= 15){
          DateTime now = rtc.now();
          chrono_stop = now.unixtime();
        }
      }
      else {
        chrono_cpt = 0;
        altitude_temp = Altitude;
      }
    }
  }
  
  if (Altitude > conf.alti_max)
    conf.alti_max = Altitude;
  if (Altitude < conf.alti_min)
    conf.alti_min = Altitude;
    
  for(uint8_t cc=1;cc<=MAX_SAMPLES;cc++){                                   //samples averaging and vario algorithm
    alt[(cc-1)]=alt[cc];
    tim[(cc-1)]=tim[cc];
  };
  alt[MAX_SAMPLES]=Altitude;
  tim[MAX_SAMPLES]=tempo;
  float stime=tim[0];
  for(uint8_t cc=0;cc<MAX_SAMPLES;cc++){
    N1+=(tim[cc]-stime)*alt[cc];
    N2+=(tim[cc]-stime);
    N3+=(alt[cc]);
    D1+=(tim[cc]-stime)*(tim[cc]-stime);
    D2+=(tim[cc]-stime);
  };

  vario=1000*((MAX_SAMPLES*N1)-N2*N3)/(MAX_SAMPLES*D1-D2*D2);
  
  if (vario < conf.txchutemax)
    conf.txchutemax = vario;
    
  if (vario > conf.txchutemin)
    conf.txchutemin = vario;
    
  // make some beep   
  if (vario < 15 && vario > -15){
    if (vario > conf.vario_climb_rate_start && conf.vario_climb_rate_start != 0)
    {
      toneAC(900+(100*vario), conf.volume, 200-(vario*10)); //when climbing make faster and shorter beeps
      thermalling = true;                               //ok,we have thermall in our hands
    } else if (vario < 0  && thermalling == true)   //looks like we jump out the thermall
    {
     // play_siren();                                   //oo, we lost thermall play alarm
      thermalling = false;
    } else if (vario < conf.vario_sink_rate_start && conf.vario_sink_rate_start != 0){           //if you have high performace glider you can change sink beep to -0.95m/s ;)
      toneAC(900+(100*vario), conf.volume, 200-(vario*10));
      thermalling = false;
    }
  }

  if (millis() >= (get_time1+67) && varioUsed)      //every second get temperature and battery level
  {
    get_time1 = millis();
    renderVarioBar();
  }  
  if (millis() >= (get_time2+1000) && varioUsed)      //every second get temperature and battery level
  {
    get_time2 = millis();
    renderVario();
  }  
  if (millis() >= (get_time3+5000) && push_write_eeprom) //Write conf in eeprom if request
  {
    push_write_eeprom = false;
    EEPROM_writeAnything(0, conf);
  }

}

void readButtons(){   

    long newLeft = knobLeft.read();
    if (newLeft != positionLeft) {
      if (newLeft%2==0) {
        if (newLeft > positionLeft){ //Right
          if (!menuUsed && varioUsed == false)
            menu.moveDown(); 
          else if (varioUsed == false)
            renderMenu(menu.getCurrent(), MENU_RIGHT);
          else if (varioUsed == true)
            renderVolume(MENU_RIGHT);
        }
        else { //Left
          if (!menuUsed && varioUsed == false)
            menu.moveUp(); 
          else if (varioUsed == false)
            renderMenu(menu.getCurrent(), MENU_LEFT);            
          else if (varioUsed == true)
            renderVolume(MENU_LEFT);
        }
      }
      positionLeft = newLeft;
    }       

    //if button enter is pressed    
    int reading = digitalRead(Enter);
    if ((millis() - enterDebounceTime) > debounceDelay){

      // in menu, clic an item
      if (reading == LOW && lastEnterState == HIGH && varioUsed == false){
        enterDebounceTime = millis();
        menu.use();
      }
      // in vario, button enter init timer
      if (reading == LOW && lastEnterState == HIGH && varioUsed == true){ 
        enterDebounceTime = millis();
        get_time5 = millis();
        is_vario_button_push = true;
      }
      // in vario, stop button enter and go back to menu
      if (reading == HIGH && lastEnterState == LOW && varioUsed == true && is_vario_button_push == true){
        enterDebounceTime = millis();
        is_vario_button_push = false;
        menu.use();
      }
    }
    lastEnterState = reading;

    // in vario, if button enter is pressed 2 seconds, reset stats
    if (millis() >= (get_time5+2000) && is_vario_button_push == true){

      is_vario_button_push = false;
      resetStats();
      
      display.clearDisplay();
      display.setTextSize(2);
      display.setTextColor(WHITE, BLACK);
      get_time2 = millis();  //stop the refresh rendering vario      
      display.println(F("Reset"));
      display.println(F("stats"));
      display.print(F("Ok"));
      display.display();
      
      toneAC(500, 10, 100);
      toneAC(500, 10, 100);
    }

}//end read button
