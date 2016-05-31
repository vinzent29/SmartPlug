#include "filter.h"
#include <AuthClient.h>
#include <MicroGear.h>
#include <MQTTClient.h>
#include <SHA1.h>
#include <Arduino.h>
#include <EEPROM.h>

#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>

#define OP1   D7
#define OP2   D6
#define EEPROM_STATE_ADDRESS 128

WiFiClient client;
AuthClient *authclient;

#define APPID   "piesensor"
#define KEY     "UOstvEfmL7JF6HJ"
#define SECRET  "4DvVmXFO7ABvIpKN7FI8tEsFQmRLRM"
#define ALIAS   "devplug"

char state = 0;
char state2 = 0;
char tosendstate = 0;
char buff[16];

int sensSmoothArray [filterSamples]; 
int smoothData; 

boolean cal_complete = 0;

/*
Measuring Current Using ACS712
*/
const int sensorIn = A0;
int mVperAmp = 185; // use 100 for 20A Module and 66 for 30A Module


double Voltage = 0;
double VRMS = 0;
double AmpsRMS = 0;

char data[50];

float getVPP(void);
void  calI(void);
 
MicroGear microgear(client);

void sendState() {
  tosendstate = 0;
  sprintf(buff,"%d",state);
  microgear.publish("/devplug/state",buff);
}

void updateIO() {
  if ((state == 1)&&(state2 == 1)) 
  {
    digitalWrite(OP1, HIGH);
    digitalWrite(OP2, HIGH);
  }
  else if((state == 0)&&(state2 == 0))
  {
    digitalWrite(OP1, LOW);
    digitalWrite(OP2, LOW);
  }
  else if((state == 1)&&(state2 == 0))
  {
    digitalWrite(OP1, HIGH);
    digitalWrite(OP2, LOW);
  }
  else if((state == 0)&&(state2 == 1))
  {
    digitalWrite(OP1, LOW);
    digitalWrite(OP2, HIGH);
  }
}

void onMsghandler(char *topic, uint8_t* msg, unsigned int msglen) {
  int s;
  char *m = (char *)msg;
  m[msglen] = '\0';

  Serial.println(m);
  s = atoi(m);
  
  if (s == 0) 
  {
    state = 0;
  }
  else if(s == 1)
  {
    state = 1;
  }
  else if(s == 2)
  {
    state2 = 0;
  }
  else if(s == 3)
  {
    state2 = 1;
  }

  updateIO();
  tosendstate = 1;
}

void onConnected(char *attribute, uint8_t* msg, unsigned int msglen) {
  Serial.println("Connected to NETPIE...");
  tosendstate = 1;
}



void setup(){ 
 Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);     // Initialize the LED_BUILTIN pin as an output
  pinMode(D6, OUTPUT);     // Relay2 (OP2)
  pinMode(D7, OUTPUT);     // Relay1 (OP1)
  digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED off by making the voltage HIGH

    WiFiManager wifiManager;
    wifiManager.setTimeout(180);

    if(!wifiManager.autoConnect("pieplug")) {
      Serial.println("Failed to connect and hit timeout");
      delay(3000);
      ESP.reset();
      delay(5000);
    }
    
  microgear.on(MESSAGE,onMsghandler);
  microgear.on(CONNECTED,onConnected);
/*
  if (WiFi.begin(ssid, password)) {
    while (WiFi.status() != WL_CONNECTED) {
          delay(500);
          Serial.print(".");
  }
  Serial.println("WiFi connected");  
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
*/
  //microgear.resetToken();
  microgear.init(KEY,SECRET,ALIAS);
  microgear.connect(APPID);
  //}
  

  
}

void loop(){
  
  calI();
  
  if (microgear.connected()) 
  {
    if (tosendstate) sendState();
    microgear.loop();
    microgear.publish("/devplughtml/devplug",data);
  }
  else 
  {
    Serial.println("connection lost, reconnect...");
    microgear.connect(APPID);
  }
}

float getVPP()
{
  float result;
  
  int readValue;             //value read from the sensor
  int maxValue = 0;          // store max value here
  int minValue = 1024;          // store min value here
  uint32_t start_time = millis();

   while((millis()-start_time) < 1000) //sample for 1 Sec
   {
       readValue = analogRead(sensorIn);
       smoothData = digitalSmooth(readValue, sensSmoothArray); 
       // see if you have a new maxValue
       if (smoothData > maxValue) 
       {
           /*record the maximum sensor value*/
           maxValue = smoothData;
       }
       if (smoothData < minValue) 
       {
           /*record the maximum sensor value*/
           minValue = smoothData;
       }

       delay(10);
   } 
   // Subtract min from max
   result = ((maxValue - minValue) * 5.0)/1024.0;
      
   return result;
 }

 void calI(void)
 {
    double app_power = 0;
    double real_power = 0;
    static double kwh = 0;
    static double kwh_sum = 0;
    
    char I_buff[10];
    char P_buff[10];
    char kwh_buff[10];


      Voltage = getVPP();
      VRMS = (Voltage/2.0) *0.707; 
      AmpsRMS = (VRMS * 1000)/mVperAmp;
      //Serial.print(AmpsRMS);
      //Serial.println(" Amps RMS");

      app_power = AmpsRMS*230.0;
      real_power = (app_power*0.8);
      kwh_sum = kwh_sum+kwh;
      kwh = real_power*0.00027;

      Serial.print("Irms = ");                  // Irms 
      Serial.print(AmpsRMS);                       // Irms 
      Serial.print("A : Apparent Power = ");    // Irms 
      Serial.print(app_power);                  // Apparent power
      Serial.print("VA : Power = ");            // Apparent power
      Serial.print(real_power/1000);            // Real power
      Serial.print("kW : kWh = ");              // Real power
      Serial.print(kwh_sum/1000);               // Real power
      Serial.println("kWh");                    // Real power
  
      dtostrf(AmpsRMS, 6, 2, I_buff);
      dtostrf((real_power/1000), 6, 2, P_buff);
      dtostrf((kwh_sum/1000), 6, 2, kwh_buff);

      sprintf(data,"%s:229.40:%s:%s",I_buff,P_buff,kwh_buff);
      Serial.println(data);                        // Real power
    
 }

