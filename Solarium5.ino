// Receiver for single solarium using NRF24L01

// Thanks to HandController - the slave or the receiver

// http://maniacbug.github.io/RF24/classRF24.html
    //~ - CONNECTIONS: nRF24L01 Modules See:
    //~ http://arduino-info.wikispaces.com/Nrf24L01-2.4GHz-HowTo
    //~ 1 - GND
    //~ 2 - VCC 3.3V !!! NOT 5V
    //~ 3 - CE to Arduino pin 9
    //~ 4 - CSN to Arduino pin 10
    //~ 5 - SCK to Arduino pin 13
    //~ 6 - MOSI to Arduino pin 11
    //~ 7 - MISO to Arduino pin 12
    //~ 8 - UNUSED

#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
	// NOTE the file RF24.cpp has been modified by commenting out the call to powerDown() in the write() function
	//      that is line 506 in my copy of the file

#define CE_PIN   9
#define CSN_PIN 10

// NOTE: the "LL" at the end of the constant is "LongLong" type

const uint64_t   deviceID = 0xE8E8F0F0E1LL; // Define the ID for this slave

int valChange = 1; 

RF24 radio(CE_PIN, CSN_PIN);

int dataReceived[2];  
int ackData[2] = {12,0};
byte ackLen = 2;


#include <avr/wdt.h>
#include "TimerOne.h"

#define ledPin 13

#define STATUS_FREE    0
#define STATUS_WORKING 1
#define STATUS_COOLING 2
#define STATUS_WAITING 3

#define STATE_WAIT_COMMAND 0
#define STATE_WAIT_PRE_TIME 1
#define STATE_WAIT_MAIN_TIME 2
#define STATE_WAIT_COOL_TIME 3
#define STATE_WAIT_CHECKSUM 4
#define STATE_WAIT_VALIDATE_START 5

#define INPUT_PIN            7
#define OUTPUT_LAMPI_PIN     2
#define OUPTUT_OHLAJDANE_PIN 3
#define CH_SELECTION_PIN_1   4
#define CH_SELECTION_PIN_2   5
#define CH_SELECTION_PIN_3   6

// set up a new serial port

byte pinState = 0;

unsigned char device = 255;
unsigned char status;
unsigned char result_1;
unsigned char result_2;
signed char pre_time;
signed char main_time;
signed char cool_time;
unsigned char device_status;
unsigned char prescaler =  60;
unsigned char key_reading;
int receiver_timeout;
char receiver_state;


void setup () {
  unsigned char channel = 0;
  // define pin modes for led pin: 
  pinMode(ledPin, OUTPUT);

  pinMode(OUTPUT_LAMPI_PIN, OUTPUT);
  pinMode(OUPTUT_OHLAJDANE_PIN, OUTPUT);        

  pinMode(INPUT_PIN, INPUT_PULLUP);

  wdt_enable(WDTO_4S);
  Timer1.initialize(1000000);  // Initialize Timer1 to 1S period
  Timer1.attachInterrupt(callback);  // attaches callback() as a timer overflow interrupt
  radio.begin();
  radio.setDataRate( RF24_250KBPS );
  channel = (channel<<1) | digitalRead(CH_SELECTION_PIN_3);
  channel = (channel<<1) | digitalRead(CH_SELECTION_PIN_2);
  channel = (channel<<1) | digitalRead(CH_SELECTION_PIN_1);
  radio.setChannel(channel);
  radio.openReadingPipe(1,deviceID);
  radio.enableAckPayload();
  radio.writeAckPayload(1, ackData, ackLen);
  radio.startListening();
}

void callback()
{
  prescaler--;
  if (prescaler == 0)
  {
      prescaler = 60;
      if (pre_time > 0) pre_time--;
      else if (main_time > 0) main_time--;
      else if(cool_time > 0) cool_time--;
      updateDeviceStatus();
  }
}


int ToBCD(int value)
{  
  int digits[3];
  int result;
  digits[0] = value %10;
  digits[1] = (value/10) % 10;
  digits[2] = (value/100) % 10;
  result = digits[0] | (digits[1]<<4) | (digits[2]<<8);
  return result;
}

int FromBCD(int value){
  int digits[3];
  int result;
  digits[0] = value & 0x0F;
  digits[1] = (value>>4) & 0x0F;
  digits[2] = (value>>8) & 0x0F;
  result = digits[0] + digits[1]*10 + digits[2]*100;
  return result;
}



void updateDeviceStatus()
{
  if(pre_time > 0)
  {
      device_status = STATUS_WAITING;
  }
  else if(main_time > 0)
  {
      device_status = STATUS_WORKING;
  }
  else if(cool_time > 0)
  {
      device_status = STATUS_COOLING;
  }
  else
  {
      device_status = STATUS_FREE;
  }
  
  if( device_status == STATUS_WORKING)
  {
      digitalWrite(OUTPUT_LAMPI_PIN,1);
  }
  else
  {
      digitalWrite(OUTPUT_LAMPI_PIN,0);
  }
  if( device_status == STATUS_WORKING || device_status == STATUS_COOLING)
  {
      digitalWrite(OUPTUT_OHLAJDANE_PIN,1);
  }
  else
  {
      digitalWrite(OUPTUT_OHLAJDANE_PIN,0);
  }
//  Serial.print(F("\nNew device status:"));
//  Serial.print( device_status);
}


void loop() 
{
  signed char time_in_hex;

  wdt_reset();
  if ( radio.available() ) 
  {
        radio.read( dataReceived, sizeof(dataReceived) );
  //      Serial.print("Data0 ");
  //      Serial.print(dataReceived[0]);
  //      Serial.print(" Data1 ");      
  //      Serial.println(dataReceived[1]);
        radio.writeAckPayload(1, ackData, ackLen);
//        ackData[0] += valChange; // this just increments so you can see that new data is being sent
 
      digitalWrite(ledPin,1);
      device = (dataReceived[0] & 0x78)>>3;    
//      Serial.print(F("\nCommand received:"));
//      Serial.print(data & 0x07,HEX);  
//      Serial.print(F("\nDevice:"));
//      Serial.print(device,HEX);      
      /*
 * commads:
       * 0 - status 0-free, 1-Working, 2-COOLING, 3-WAITING
       * 1 - start
       * 2 - set pre-time
       * 3 - set cool-time
       * 4 - stop - may be not implemented in some controllers
       * 5 - set main time
       */
      switch(dataReceived[0] & 0x07)
      {
      case 0: // ststus
        ackData[0] = device_status;
        switch (device_status)
        {
          case STATUS_FREE:
            break;
          case STATUS_WORKING:
            ackData[1] =  ToBCD(main_time);
            break;
          case STATUS_COOLING:
            ackData[1] = ToBCD(cool_time);
            break;
          case STATUS_WAITING:
            ackData[1] = ToBCD(pre_time);
            break;
          default:
            break;
        }
//        Serial.print(F("\nDevice status sent:"));
//        Serial.print(device_status[device]);        
        break;
      case 1: // start
        pre_time = 0;
        if(device_status > 0)
        {             
          prescaler = 60;
          updateDeviceStatus();
        }
        break;
      case 2: // set pre_time
        pre_time = FromBCD(dataReceived[1]);        
        break;
      case 3: // set cool_time
        cool_time = dataReceived[1];                        
        break;
      case 4: // stop        
        if(pre_time == 0 && main_time > 0)
        {
          main_time = 0;                          
        }
        else
        {
          cool_time = 0;
          main_time = 0;
          pre_time = 0;                            
        }
        if(device_status > 0)
        {
          prescaler = 60;
          updateDeviceStatus();
        }
        break;
      case 5: // set main time
        main_time = dataReceived[1];
        prescaler = 60;
        main_time = FromBCD(dataReceived[1]);       
//          Serial.print(F("\nMain_time:"));
//          Serial.print(main_time[device]);
        break;
      default:
        break;
      }
  }
  delay(2);
  //	if (Serial.available()) {
  //		mySerial.write(Serial.read());
  //	}

  key_reading = (key_reading<<1) | digitalRead(INPUT_PIN);
  if(key_reading == 0x00 && device_status == STATUS_WAITING)
  {
    prescaler = 60;
    pre_time = 0;
    updateDeviceStatus();
  }
  
  digitalWrite(ledPin,0);
  
}


