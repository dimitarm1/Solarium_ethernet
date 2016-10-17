// This is a demo of the RBBB running as webserver with the Ether Card
// 2010-05-28 <jc@wippler.nl> http://opensource.org/licenses/mit-license.php

#include <SoftwareSerial.h>
#include <avr/wdt.h>
#include "TimerOne.h"

#define rxPin 11
#define txPin 12
#define ledPin 6

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

// set up a new serial port
//SoftwareSerial mySerial = SoftwareSerial(rxPin, txPin); 
#define mySerial Serial
byte pinState = 0;

unsigned char device = 255;
unsigned char status;
unsigned char result_1;
unsigned char result_2;
signed char pre_time[16];
signed char main_time[16];
signed char cool_time[16];
unsigned char device_status[16];
unsigned char prescalers[16] = { 60,60,60,60,60,60,60,60,60,60,60,60,60,60,60,60  };
signed const char output_pin_LUT[16] = {2,3,4,5,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};  
signed const char input_pin_LUT[16] = {7,8,9,10,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
unsigned char key_readings[16];
int receiver_timeout;
char receiver_state;


void setup () {
  // define pin modes for tx, rx, led pins:
  pinMode(rxPin, INPUT);
  pinMode(txPin, OUTPUT);
  pinMode(ledPin, OUTPUT);

  pinMode(2, OUTPUT);
  pinMode(3, OUTPUT);        
  pinMode(4, OUTPUT);
  pinMode(5, OUTPUT);

  pinMode(7, INPUT_PULLUP);
  pinMode(8, INPUT_PULLUP);
  pinMode(9, INPUT_PULLUP);
  pinMode(10, INPUT_PULLUP);
  //digitalWrite(txPin, LOW);
  // set the data rate for the SoftwareSerial port
  mySerial.begin(1200);
  //Serial.begin(115200);
  //Serial.println("\nHello");
  char someChar = 'r';
  //mySerial.print(someChar);
  wdt_enable(WDTO_4S);
  Timer1.initialize(1000000);  // Initialize Timer1 to 1S period
  Timer1.attachInterrupt(callback);  // attaches callback() as a timer overflow interrupt
}

void callback()
{
  for(char i=0; i<16; i++)
  {
    prescalers[i]--;
    if (prescalers[i] == 0)
    {
      prescalers[i] = 60;
      if (pre_time[i] > 0) pre_time[i]--;
      else if (main_time[i] > 0) main_time[i]--;
      else if(cool_time[i] > 0) cool_time[i]--;
      updateDeviceStatus(i);
    }
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



void updateDeviceStatus(unsigned char device)
{
  if(pre_time[device] > 0)
  {
      device_status[device] = STATUS_WAITING;
  }
  else if(main_time[device] > 0)
  {
      device_status[device] = STATUS_WORKING;
  }
  else if(cool_time[device] > 0)
  {
      device_status[device] = STATUS_COOLING;
  }
  else
  {
      device_status[device] = STATUS_FREE;
  }
  
  if(output_pin_LUT[device] != -1)
  {
      if( device_status[device] == STATUS_WORKING)
      {
          digitalWrite(output_pin_LUT[device],1);
      }
      else
      {
          digitalWrite(output_pin_LUT[device],0);
      }
  }
//  Serial.print(F("\nNew device status:"));
//  Serial.print( device_status[device]);
}


void loop() 
{
  signed char data;
  signed char checksum;
  unsigned char remote_check_sum = 220;
  signed char time_in_hex;

  wdt_reset();
  if(receiver_state)
  {
    if(receiver_timeout)
    {
      receiver_timeout--;
    }
    else
    {
      receiver_state = STATE_WAIT_COMMAND;
    }
  }
  if (mySerial.available() > 0)
  {
    data = mySerial.read();
    if(data & 0x80)  // Command received
    {
      digitalWrite(ledPin,1);
      device = (data & 0x78)>>3;    
//      Serial.print(F("\nCommand received:"));
//      Serial.print(data & 0x07,HEX);  
//      Serial.print(F("\nDevice:"));
//      Serial.print(device,HEX);   
      receiver_timeout = 40;       
      /*
 * commads:
       * 0 - status 0-free, 1-Working, 2-COOLING, 3-WAITING
       * 1 - start
       * 2 - set pre-time
       * 3 - set cool-time
       * 4 - stop - may be not implemented in some controllers
       * 5 - set main time
       */
if (device<4)
      switch(data & 0x07)
      {
      case 0: // ststus
        data = device_status[device]<<6;
        switch (device_status[device])
        {
          case STATUS_FREE:
            break;
          case STATUS_WORKING:
            data = data | ToBCD(main_time[device]);
            break;
          case STATUS_COOLING:
            data = data | ToBCD(cool_time[device]);
            break;
          case STATUS_WAITING:
            data = data | ToBCD(pre_time[device]);
            break;
          default:
            break;
        }
        mySerial.write(data);
//        Serial.print(F("\nDevice status sent:"));
//        Serial.print(device_status[device]);
        receiver_state = STATE_WAIT_COMMAND;
        break;
      case 1: // start
        receiver_state = STATE_WAIT_VALIDATE_START;                        
        break;
      case 2: // set pre_time
        receiver_state = STATE_WAIT_PRE_TIME;                      
        break;
      case 3: // set cool_time
        receiver_state = STATE_WAIT_COOL_TIME;                        
        break;
      case 4: // stop
        receiver_state = STATE_WAIT_COMMAND;
        if(pre_time[device] == 0 && main_time[device] > 0)
        {
          main_time[device] = 0;                          
        }
        else
        {
          cool_time[device] = 0;
          main_time[device] = 0;
          pre_time[device] = 0;                            
        }
        if(device_status[device] > 0)
        {
          prescalers[device] = 60;
          updateDeviceStatus(device);
        }
        break;
      case 5: // set main time
        receiver_state = STATE_WAIT_MAIN_TIME;            
        break;
      default:
        break;
      }
    }
    else // Data recevied ?
    if(receiver_state != STATE_WAIT_COMMAND)
    {
//       Serial.print(F("\nData received:"));
//       Serial.print(data,HEX);
      switch(receiver_state)
      {
        case STATE_WAIT_PRE_TIME:          
          if(data>9)data = 9;
          pre_time[device] = (data);
          prescalers[device] = 60;        
          receiver_state = STATE_WAIT_COMMAND;
          break;
        case STATE_WAIT_MAIN_TIME:                              
          prescalers[device] = 60;
          main_time[device] = FromBCD(data);       
          mySerial.write(main_time[device]);
//          Serial.print(F("\nMain_time:"));
//          Serial.print(main_time[device]);
          receiver_state = STATE_WAIT_COMMAND;
          break;
        case STATE_WAIT_COOL_TIME:       
          if(data>9) data = 9;
          cool_time[device] = (data);
          time_in_hex = ToBCD(main_time[device]);
//          Serial.print(F("\nTime in HEX:"));
//          Serial.print(time_in_hex, HEX);
          checksum = (pre_time[device] + cool_time[device] - time_in_hex - 5) & 0x7F;
//          Serial.print(F("\nchecksum sent:"));
//          Serial.print(checksum, HEX);
          mySerial.write(checksum);
          receiver_state = STATE_WAIT_CHECKSUM;                                                          
          break;
        case STATE_WAIT_CHECKSUM:   
          time_in_hex = ToBCD(main_time[device]);
          checksum = (pre_time[device] + cool_time[device] - time_in_hex - 5) & 0x7F;     
          if((char)data == (char)checksum)
          {
//             Serial.print(F("\nchecksum OK!!!!!"));
            if(main_time[device] > 60) main_time[device] = 60;
            prescalers[device] = 60;
            updateDeviceStatus(device);
          }
//          Serial.print(F("\nPre_time/Main_time/Cool_time:"));
//          Serial.print(pre_time[device]);
//          Serial.print(F("/"));
//          Serial.print(main_time[device]);
//          Serial.print(F("/"));
//          Serial.print(cool_time[device]);
//          Serial.print(F("\nchecksum1:"));
//          Serial.print(checksum, HEX);
//          Serial.print(F("\nchecksum2:"));
//          Serial.print(data,HEX);    
          receiver_state = STATE_WAIT_COMMAND;
          break;
        case STATE_WAIT_VALIDATE_START:        
          if(data == 0x55) //validate start
          {
            pre_time[device] = 0;
            if(device_status[device] > 0)
            {             
              prescalers[device] = 60;
              updateDeviceStatus(device);
            }                                
          }
          receiver_state = STATE_WAIT_COMMAND;                                    
          break;
        default:
          break;
      }
    }
  }

  //	if (Serial.available()) {
  //		mySerial.write(Serial.read());
  //	}

  for (char i = 0; i<16;i++)
  {
    if(input_pin_LUT[i] != -1)
    {
      key_readings[i] = (key_readings[i]<<1) | digitalRead(input_pin_LUT[i]);
      if(key_readings[i] == 0x00 && device_status[i] == STATUS_WAITING)
      {
          prescalers[device] = 60;
          pre_time[i] = 0;
          updateDeviceStatus(i);
      }
    }
  }
  delay(4);
  digitalWrite(ledPin,0);
  
}


