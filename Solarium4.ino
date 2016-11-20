// Master solarium unit - based on:
// TrackControl - the master or the transmitter

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

typedef enum commands {  cmd_status, cmd_start, cmd_set_pre_time, cmd_set_cool_time, cmd_stop, cmd_set_main_time };

// NOTE: the "LL" at the end of the constant is "LongLong" type
// These are the IDs of each of the slaves
const uint64_t slaveID = 0xE8E8F0F0E1LL;

RF24 radio(CE_PIN, CSN_PIN); // Create a Radio

int dataToSend[2];

unsigned long currentMillis;
unsigned long prevMillis;
unsigned long txIntervalMillis = 1000;
int txVal = 0;
int ackMessg[4];
byte ackMessgLen = 2;

#include <avr/wdt.h>
#include "TimerOne.h"

#define ledPin 13

#define STATUS_FREE    0
#define STATUS_WORKING 1
#define STATUS_COOLING 2
#define STATUS_WAITING 3
#define STATUS_OFFLINE 7

#define STATE_WAIT_COMMAND 0
#define STATE_WAIT_PRE_TIME 1
#define STATE_WAIT_MAIN_TIME 2
#define STATE_WAIT_COOL_TIME 3
#define STATE_WAIT_CHECKSUM 4
#define STATE_WAIT_VALIDATE_START 5

// set up a new serial port

byte pinState = 0;

unsigned char device = 255;
unsigned char status;
unsigned char result_1;
unsigned char result_2;
signed char pre_time[16];
signed char main_time[16];
signed char cool_time[16];
signed char curr_time[16];
unsigned char device_status[16];

int receiver_timeout;
char receiver_state;


void setup () {
  // define pin modes for led pin: 
  pinMode(ledPin, OUTPUT);

  Serial.begin(1200);
  wdt_enable(WDTO_4S);
  Timer1.initialize(1000000);  // Initialize Timer1 to 1S period
  Timer1.attachInterrupt(callback);  // attaches callback() as a timer overflow interrupt
  radio.begin();
  radio.setDataRate( RF24_250KBPS );
  radio.enableAckPayload();
  radio.setRetries(3,5); // delay, count
}

void callback()
{
  
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

bool sendCommand(unsigned char device, unsigned char command, unsigned char data)
{
  dataToSend[0] = command; // this gets incremented so you can see that new data is being sent
  dataToSend[1] = data;
  radio.setChannel(device);
  bool rslt;
  radio.openWritingPipe(slaveID); // calls each slave in turn
  rslt = radio.write( dataToSend, sizeof(dataToSend) );
  if ( radio.isAckPayloadAvailable() ) 
  {
    radio.read(ackMessg,ackMessgLen);
  }
  return rslt;
}

bool updateDeviceStatus(unsigned char device)
{
  if(sendCommand(device, cmd_set_pre_time, pre_time[device]))
  {
    if(sendCommand(device, cmd_set_pre_time, cool_time[device]))
    {
       if(sendCommand(device, cmd_set_pre_time, main_time[device]))
       {
         return true;
       }   
    }
  } 
}


unsigned char getDeviceStatus(unsigned char device)
{
  if(sendCommand(device, cmd_status,0))
  {
    device_status[device] = ackMessg[0];
    curr_time[device] = ackMessg[1];
  }
  else
  {
     device_status[device] = STATUS_OFFLINE;
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
  if (Serial.available() > 0)
  {
    data = Serial.read();
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
    if (device<6)
      switch(data & 0x07)
      {
        case 0: // status
          getDeviceStatus(device);
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
          if(STATUS_OFFLINE != device_status[device])
          {
            Serial.write(data);
          }
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
        sendCommand(device, cmd_stop,0);
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
          receiver_state = STATE_WAIT_COMMAND;
          break;
        case STATE_WAIT_MAIN_TIME:                              
          main_time[device] = FromBCD(data);       
          Serial.write(main_time[device]);
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
          Serial.write(checksum);
          receiver_state = STATE_WAIT_CHECKSUM;                                                          
          break;
        case STATE_WAIT_CHECKSUM:   
          time_in_hex = ToBCD(main_time[device]);
          checksum = (pre_time[device] + cool_time[device] - time_in_hex - 5) & 0x7F;     
          if((char)data == (char)checksum)
          {
//             Serial.print(F("\nchecksum OK!!!!!"));
            if(main_time[device] > 60) main_time[device] = 60;
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

  delay(4);
  digitalWrite(ledPin,0);
  
}


