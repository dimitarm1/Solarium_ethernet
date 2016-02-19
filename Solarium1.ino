// This is a demo of the RBBB running as webserver with the Ether Card
// 2010-05-28 <jc@wippler.nl> http://opensource.org/licenses/mit-license.php

#include <EtherCard.h>
#include <SoftwareSerial.h>
#include <avr/wdt.h>
#include "TimerOne.h"

// ethernet interface mac address, must be unique on the LAN
static byte mymac[] = { 0x74,0x69,0x69,0x2D,0x30,0x31 };
static byte myip[] = { 192,168,0,150 };

#define rxPin 9
#define txPin 10
#define ledPin 6

byte Ethernet::buffer[500];
BufferFiller bfill;
// set up a new serial port
SoftwareSerial mySerial = SoftwareSerial(rxPin, txPin);
byte pinState = 0;

int pre_time;
int work_time;
int cool_time;
int retry;
unsigned char device = 255;
unsigned char status;
unsigned char result_1;
unsigned char result_2;
unsigned char demo_mode;
unsigned char demo_pre_time = 0;
unsigned char demo_work_time = 0;
unsigned char demo_cool_time = 0;
unsigned char prescaler = 60;

void setup () {
	// define pin modes for tx, rx, led pins:
	pinMode(rxPin, INPUT);
	pinMode(txPin, OUTPUT);
	pinMode(ledPin, OUTPUT);
	//digitalWrite(txPin, LOW);
	// set the data rate for the SoftwareSerial port
	mySerial.begin(1200);
	Serial.begin(1200);
	//Serial.println("\nHello");
	if (ether.begin(sizeof Ethernet::buffer, mymac) == 0)
		;//Serial.println(F("Failed to access Ethernet controller"));
	if (!ether.dhcpSetup())
		;//	  Serial.println(F("DHCP failed"));
	ether.netmask[0] = 255;
	ether.netmask[1] = 255;
	ether.netmask[2] = 255;
	if (!ether.staticSetup(myip))
		; // Serial.println("Failed to set IP address");
 
	ether.printIp("IP:  ", ether.myip);
	ether.printIp("Netmask:  ", ether.netmask);
	char someChar = 'r';
	//mySerial.print(someChar);
  //ether.staticSetup(myip);
	wdt_enable(WDTO_4S);
  Timer1.initialize(1000000);  // Initialize Timer1 to 1S period
  Timer1.attachInterrupt(callback);  // attaches callback() as a timer overflow interrupt
}

void callback()
{
  prescaler--;
  if (prescaler == 0)
  {
    prescaler = 60;
    if (demo_pre_time > 0) demo_pre_time--;
    else if (demo_work_time > 0) demo_work_time--;
    else if(demo_cool_time > 0) demo_cool_time--;
  }
}
static word homePage() {
  long t = millis() / 1000;
  word h = t / 3600;
  byte m = (t / 60) % 60;
  byte s = t % 60;
  bfill = ether.tcpOffset();
  /*bfill.emit_p(PSTR(
    "HTTP/1.0 200 OK\r\n"
    "Content-Type: text/html\r\n"
    "Pragma: no-cache\r\n"
    "\r\n"
    "<meta http-equiv='refresh' content='1000'/>"
    "<title>RBBB server</title>" 
    "<h1>$D$D:$D$D:$D$D</h1>"),
      h/10, h%10, m/10, m%10, s/10, s%10);*/
  bfill.emit_p(PSTR(
	  "HTTP/1.0 200 OK\r\n"
	  "Content-Type: text/csv\r\n"
	  "Pragma: no-cache\r\n"
	  "\r\n"
	  "OPERATION_STATUS,$D,SOLARIUM_STATUS,$D,SOLARIUM_TIME,$D$D"
	  ),
	  status, result_1, result_2 / 10, result_2 % 10);
  return bfill.position();
}

int ToBCD(int val){
	if (val > 9) val += 6;
	return val;
}

signed char get_solarium_status(int n){

	signed char data;	
	// send conmmand for status request
	
	mySerial.print(0x80 | ((n & 0x0f) << 3) | 0);
	
	delay(70);

	if (mySerial.available() > 0)
	{
		char data = mySerial.read();
		return (data);
	}
	return -1;
}

void SendTime()
{
	// checksum: CoolTime + Pre-Time - 5 - MainTime
	
	int time_in_hex = /*ToBCD*/(work_time);
	while (retry < 20){
		// clear in FIFO
		unsigned char checksum = (pre_time + cool_time - time_in_hex - 5) & 0x7F;
		unsigned char  remote_check_sum = 220;

		mySerial.write(0x80U | ((device & 0x0fU) << 3U) | 2U); //Command 2 == Pre_time_set
		delay(2);
		mySerial.write(pre_time);

		delay(2);
		mySerial.write(0x80U | ((device & 0x0fU) << 3U) | 5U); //Command 5 == Main time set

		delay(2);
		mySerial.write(time_in_hex);

		delay(2);

		mySerial.read(); //"Read" old time

		delay(2);
		mySerial.write(0x80U | ((device & 0x0fU) << 3U) | 3U); //Command 3 == Cool Time set

		delay(2);
		mySerial.write(cool_time);

		delay(4);

		remote_check_sum = mySerial.read(); //"Read" checksum

		delay(20);
		//			checksum = remote_check_sum;
		if (remote_check_sum == checksum){
			delay(2);
			mySerial.write(checksum);
			retry = 22;
		}
		retry++;
	}
}

void loop() {
	
	wdt_reset();
	word len = ether.packetReceive();
	word pos = ether.packetLoop(len);
	
	if (pos)
	{
		//Serial.println("\nReceived command:");
		//Serial.print((char*)Ethernet::buffer+pos);

		/*
		REST HTTP

		Start
		http://127.0.0.1/Start/<deviceID>/<DelayTimeSec>/<WorkTimeSec>/<CoolTimeSec>

		Http://127.0.0.1/Start/3/180/240/120

		Force Start
		http://127.0.0.1/ForceStart/<deviceID>

		Force Stop
		http://127.0.0.1/ForceStop/<deviceID>

		GetStatus
		http://127.0.0.1/GetStatus/<deviceID>


		��� �� ������ ���� ��� ������� � ����� ��������

		*/

    /*
 * commads:
 * 0 - status 0-free, 1-Working, 2-COOLING, 3-WAITING
 * 1 - start
 * 2 - set pre-time
 * 3 - set cool-time
 * 4 - stop - may be not implemented in some controllers
 * 5 - set main time
 */

		result_1 = 0;
		result_2 = 0;
		status = 1; // Error code

		// Force STOP
		char *  command_stop = strstr((char *)Ethernet::buffer + pos, "GET /ForceStop/");
		if (command_stop != 0)
		{
			//*(command_status + 18) = 0;
			device = atoi(command_stop + 15);
			Serial.println("\nStop Device:");
			Serial.print(device);
      if (device == 16)
      {
        demo_pre_time = 0;
        demo_work_time = 0;
        demo_cool_time = 0;
      }
      else
      {
  			pre_time = 0;
  			work_time = 0;
  			cool_time = 2;
  			retry = 0;
  			SendTime();
  			if (retry == 23)
  			{
  				Serial.println("\nStop success");
  				status = 0;
  			}
  			else
  			{
  				Serial.println("\nStop failed");
  			}
      }     
		}
		// Force START
		char *  command_start_force = strstr((char *)Ethernet::buffer + pos, "GET /ForceStart/");
		if (command_start_force != 0)
		{
			//*(command_status + 18) = 0;
			device = atoi(command_start_force + 15);
			Serial.println("\nDevice:");
			Serial.print(device);
      if (device == 16)
      {
        demo_pre_time = 0;
        prescaler = 60;
        Timer1.restart();
        status = 0;
      }
      else
      {
  			mySerial.write((0x80 | ((device & 0x0f) << 3)) | 1); // 1 == Start command
  			delay(10);
  			mySerial.write(0x55); // validate start		
  			status = 0;
      }
		}

		// STATUS
		char *  command_status = strstr((char *)Ethernet::buffer + pos, "GET /GetStatus/");
		if (command_status != 0)
		{
			//*(command_status + 18) = 0;
			device = atoi(command_status + 15);
			Serial.println("\nDevice:");
			Serial.print(device);
      if (device == 16)
      {
        if (demo_pre_time>0)
        {
          result_2 = demo_pre_time;
          result_1 = 3; // Waiting
        }
        else if (demo_work_time > 0)
        {
          result_2 = demo_work_time;
          result_1 = 1; // Working
        }
        else if (demo_cool_time > 0)
        {
          result_2 = demo_cool_time;
          result_1 = 2; // Cooling
        }
        else
        {
          result_2 = 0;
          result_1 = 0; // Free
        }
        status = 0;
      }
      else
      {
  			mySerial.write((0x80 | ((device & 0x0f) << 3)));
  			delay(100);
  			if (mySerial.available()) 
  			{
  				int sts = mySerial.read();
  				int curr_status = (sts & 0xC0) >> 6;
  				int curr_time = sts & 0x3F;
  				Serial.print("\nStatus: ");
  				Serial.print(curr_status);
  				Serial.print(" Time: ");
  				Serial.print(curr_time);
  				result_2 = curr_time;
  				status = 0;
  				result_1 = curr_status;
  			}
      }
		}
		// START
		demo_mode = 1;
		char * command_start = strstr((char *)Ethernet::buffer + pos, "GET /Start/");
		if (command_start == 0)
		{
			command_start = strstr((char *)Ethernet::buffer + pos, "GET /SStart/");
			demo_mode = 0;
		}
		if (command_start != 0)
		{
			command_start += 10;
			char * tmp = strstr(command_start, "/");
			command_start = tmp+1;
			device = strtol(command_start, &command_start, 10);
			Serial.println("\nDevice:");
			Serial.print(device);	
		
			tmp = strstr(command_start, "/");
			command_start = tmp+1;
			pre_time = strtol(command_start, &command_start, 10);
			Serial.println("\nPreTime:");
			Serial.print(pre_time);		
			
			tmp = strstr(command_start, "/");
			command_start = tmp+1;
			work_time = strtol(command_start, &command_start, 10);
			if (work_time > 8 && demo_mode) work_time = 8;
			Serial.println("\nWorkTime:");
			Serial.print(work_time);		

			tmp = strstr(command_start, "/");
			command_start = tmp+1;
			cool_time = strtol(command_start, &command_start, 10);
			Serial.println("\nCoolTime:");
			Serial.print(cool_time);

			int val1 = atoi(command_start);			
			if (device == 16)
      {
        demo_pre_time = pre_time;
        demo_work_time = work_time;
        demo_cool_time = cool_time;
        prescaler = 60;
        Timer1.restart();
        status = 0;
      }
      else
      {
  			pre_time = pre_time & 0x7F;
  			retry = 0;
  			SendTime();
  			if (retry == 23)
  			{
  				Serial.println("\nStart success");				
  				status = 0;
  			}
  			else
  			{
  				Serial.println("\nStart failed");
  			}
      }			
		}
	}
		
//	if (Serial.available()) {
//		mySerial.write(Serial.read());
//	}
	if (pos)  // check if valid tcp data is received
		ether.httpServerReply(homePage()); // send web page data	
	delay(1);
}
