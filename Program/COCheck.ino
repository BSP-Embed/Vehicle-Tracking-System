#include  <Arduino.h>
#include  <TinyGPS.h>
#include  <LiquidCrystal.h>
#include  <SoftwareSerial.h>
#include  <avr/sleep.h>
#include  <TimerOne.h>

LiquidCrystal lcd(8,9,10,11,12,13);
SoftwareSerial GPS(5,6);
TinyGPS gps;

#define TESTING
//#define DEMO

#ifdef TESTING
  #define PHNUM1          "9980237552"
  #define PHNUM2          "9980237552"
#else
  #define PHNUM1          "9632166279"
  #define PHNUM2          "9632839510"
#endif

#define interval          1000
#define MQ7_CONST         0.02f
#define CO_THRES          6.0
#define TRUE              1
#define FALSE             0
#define LCD_NSCRL         3
#define LCD_DOT           '.'
#define LCD_SPC           ' '

#define MSG_WAIT_MSG      1
#define MSG_PH_NUM        2
#define MSG_COLL_MSG      3
#define MSG_RCV_MSG       4
#define LINE_FEED         0x0A

typedef unsigned char int8u;
typedef unsigned long int16u;

unsigned char PhNum[15], sbuf[100];
const char *SMSMsg[]      = {"High CO Content @", "Your Vehicle Got Accident @", "Your Vehicle is Located @", " Automated SMS By: SV"};
const char *MapLink       = " http://maps.google.com/maps?q=";

const int COSensPin       = A0;
const int BuzPin          = A5;
const byte interruptPin   = 2;

struct {
  unsigned char DispCo    : 1;
  unsigned char ClrDisp   : 1;
  unsigned char Acci      : 1;
  unsigned char Msg       : 1;
  unsigned char Monit     : 1;
} Flags;

#define BuzOn()           digitalWrite(BuzPin, HIGH)
#define BuzOff()          digitalWrite(BuzPin, LOW)

void DispTitle            (void);
void DispParam            (void);
void Beep                 (void);
void TaskCOMoni           (void);
void TaskAcci             (void);
void TaskAcciProc         (void);
void TaskMsg              (void);
void GSMInit              (void);
void SendSMS              (const char *PhNum, const char *Msg);
void GPSgetloc            (char *Lat, char *Lon);
void ftoa                 (double n, char *res, int afterpoint);
int intToStr              (int x, char str[], int d);
void rever                (char *str, int len);
void LCDDispInit          (void);
void COSensInit           (void);
int16u  ReadCO            (void);
void TimerOF              (void);

void setup() {
  pinMode(BuzPin, OUTPUT);
  Beep();
  pinMode(interruptPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(interruptPin), TaskAcci, RISING);
  lcd.begin(16,2);
  LCDDispInit();
  COSensInit();
  Serial.begin(9600);
  GSMInit();
  GPS.begin(9600);
  Timer1.initialize(100000);        /* 100 milli seconds or 0.1 seconds */
  Timer1.attachInterrupt(TimerOF); /* blinkLED to run every 0.15 seconds */
  DispTitle();
  sleep_enable();
  sleep_cpu();
} 
void loop(void) {
  TaskCOMoni();
  TaskAcciProc();
  TaskMsg();
  sleep_cpu();
} 
void TaskCOMoni(void) {
  if (Flags.Monit) {
    float CO;
    CO =  ReadCO() * MQ7_CONST;
    lcd.setCursor(9,0);
    lcd.print(CO,2);
    if (CO > CO_THRES) {
      if (!Flags.DispCo) {
        BuzOn();
        lcd.setCursor(0, 1); 
        lcd.print("HIGH CO Content");
        SendLinkLoc(PHNUM1, SMSMsg[0]);
        BuzOff();
        Flags.ClrDisp = FALSE;
        Flags.DispCo = TRUE;
      }
    } else 
      if (!Flags.ClrDisp) {
        Flags.DispCo = FALSE;
        Flags.ClrDisp = TRUE;
        lcd.setCursor(0, 1); 
        lcd.print("                ");
      }
  Flags.Monit = FALSE;
  }
}
void TaskAcciProc(void) {
  if (Flags.Acci) {
    lcd.clear(); 
    lcd.print("Accident Occured");
    SendLinkLoc(PHNUM1, SMSMsg[1]);
    lcd.setCursor(0,1);
    lcd.print("RESET THE VEHICLE");
    Flags.Acci = FALSE;
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    cli();
    sleep_enable();
    sleep_cpu();
  }
}
void TaskMsg(void) {
  if (Flags.Msg) {
    #ifdef DISP_PHNO_MSG
      String Ph = PhNum;
      String SMS = sbuf;
      lcd.clear();
      lcd.print(Ph);
      lcd.setCursor(0,1);
      lcd.print(SMS);
    #endif
    if (!strcmp(PHNUM1, PhNum) || !strcmp(PHNUM2, PhNum)) {
      Beep();
      SendLinkLoc(PhNum, SMSMsg[2]);
    }
    Flags.Msg = FALSE;
   }
}
void DispTitle(void){
  lcd.clear();
  lcd.setCursor(0, 0); 
  lcd.print(" SMART VEHICLE");
  delay(500);
  DispParam();
}
void DispParam(void) {
  lcd.setCursor(0,0);
  lcd.print("CO Value:    ppm");
}
void GSMInit(void) {
 Serial.println("AT\r");
 delay(250);
 Serial.println("AT+CMGF=1\r");
 delay(250);
 Serial.println("AT+CNMI=2,2,2,0,0\r");
 delay(250);
}
void SendLinkLoc(const char *PhNum, const char *GSMMsg) {
  char i, Lat[15], Lon[15], gsmmsg[150];
  gsmmsg[0] = '\0';
  strcat(gsmmsg,GSMMsg);
  strcat(gsmmsg, MapLink);
  #ifdef DEMO
    strcat(gsmmsg,"12.2766");
    strcat(gsmmsg,",");
    strcat(gsmmsg,"76.62022");
  #else 
    GPSgetloc(Lat,Lon);
    strcat(gsmmsg,Lat);
    strcat(gsmmsg,",");
    strcat(gsmmsg,Lon); 
  #endif
  strcat(gsmmsg, SMSMsg[3]);
  SendSMS(PhNum, gsmmsg);
}
void SendSMS (const char *PhNum, const char *Msg){
  Serial.print("AT+CMGS=\"0");
  Serial.print(PhNum);
  Serial.println("\"\r");
  delay(1000);
  Serial.println(Msg);
  delay(500);
  Serial.println((char)26);
}
void  GPSgetloc(char *Lat, char *Lon) {
  char c;
  float latitude, longitude;
  unsigned long previousMillis = millis();
  while (millis() - previousMillis <= interval) {
        if (GPS.available()) { 
          char c = GPS.read();
          if(gps.encode(c)) {
            gps.f_get_position(&latitude, &longitude);
            ftoa(latitude, Lat, 4);
            ftoa(longitude, Lon, 4); 
            #ifdef GPS_DISP_LOC
              lcd.clear();
              lcd.print(Lat);
              lcd.setCursor(0,1);
              lcd.print(Lon);
            #endif
            break;
        }
      }
  }
}
void Beep(void) {
  BuzOn();
  delay(250);
  BuzOff();
}

/* SerialEvent occurs whenever a new data */
/* comes in the hardware serial RX. */
void serialEvent() {
  static int8u i;
  static int8u msgcnt,phcnt;
  static int8u state = MSG_WAIT_MSG;
  
  while (Serial.available()) {
  char UDR = (char)Serial.read();
  switch (state) {
    case MSG_WAIT_MSG:
      if (UDR == '\"') state = MSG_PH_NUM;
      break;
    case MSG_PH_NUM:
     if (phcnt++ < 13)
        PhNum[phcnt-1] = UDR;
      else
        state = MSG_COLL_MSG;
      break;
    case MSG_COLL_MSG:
      if (UDR == LINE_FEED)
        state = MSG_RCV_MSG;
      break;
    case MSG_RCV_MSG:
      if ((sbuf[msgcnt++] = UDR) == LINE_FEED) {
        sbuf[msgcnt-2] = '\0';
        for (i = 0 ; i < 10; i++) /* eliminate +91 */
        PhNum[i] = PhNum[i+3];
        PhNum[i] = '\0';
        state = MSG_WAIT_MSG;
        msgcnt = 0;
        phcnt = 0;
        Flags.Msg = TRUE;
      }
    }
  }
}
void TaskAcci(void) {
  Flags.Acci = TRUE;
  BuzOn();
}
void TimerOF(void) {
  static int8u i = 0;
  if (!Flags.Monit && ++i >= 10) {
    i = 0;
    Flags.Monit = TRUE;
  }
}
void COSensInit(void) {
  lcd.setCursor(0,1);
  lcd.print("Sensor Init:");
  while ((analogRead(COSensPin) * MQ7_CONST) > CO_THRES) ;
  Beep();
  lcd.print("OK");
}
int16u ReadCO(void) {
  int8u i;
  int16u COValue = 0;
  for (i = 0; i < 32; i++)
    COValue += analogRead(COSensPin);
  return COValue >>= 5;
}
void LCDDispInit(void) {
  int8u i, j, adr;
  lcd.print("  INITIALIZING");
  lcd.setCursor(0,1);
  for ( j = 0; j < LCD_NSCRL; j++ ) {
    adr = 0xc0;               // 2nd row, first coloumn
    for ( i = 0; i < 16; i++ ) {
      lcd.setCursor(i,1);
      lcd.print(LCD_DOT);       
      if ( i < 8 ) delay(200+(50*i)); else delay(25);
      lcd.setCursor(i,1);     
      lcd.print(LCD_SPC);     
   }
  } 
}
// Converts a floating point number to string.
void ftoa(double n, char *res, int afterpoint){
  // Extract integer part
  int ipart = (int)n;
  // Extract floating part
  double fpart = n - (float)ipart;
  // convert integer part to string
  int i = intToStr(ipart, res, 0);
  // check for display option after point
  if (afterpoint != 0)  {
    res[i] = '.';  // add dot
    // Get the value of fraction part upto given no.
    // of points after dot. The third parameter is needed
    // to handle cases like 233.007
    fpart = fpart * pow(10, afterpoint);
    intToStr((int)fpart, res + i + 1, afterpoint);
  }
}
// Converts a given integer x to string str[].  d is the number
// of digits required in output. If d is more than the number
// of digits in x, then 0s are added at the beginning.
int intToStr(int x, char str[], int d){
  int i = 0;
  while (x) {
    str[i++] = (x%10) + '0';
    x = x/10;
  }
  // If number of digits required is more, then
  // add 0s at the beginning
  while (i < d)
  str[i++] = '0';
  
  rever(str, i);
  str[i] = '\0';
  return i;
}
void rever(char *str, int len){
  int i=0, j=len-1, temp;
  while (i<j) {
    temp = str[i];
    str[i] = str[j];
    str[j] = temp;
    i++; j--;
  }
}

