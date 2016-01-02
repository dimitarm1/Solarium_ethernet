// This is a demo of the RBBB running as webserver with the Ether Card
// 2010-05-28 <jc@wippler.nl> http://opensource.org/licenses/mit-license.php

#include <EtherCard.h>
#include <SoftwareSerial.h>

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

void setup () {
	// define pin modes for tx, rx, led pins:
	pinMode(rxPin, INPUT);
	pinMode(txPin, OUTPUT);
	pinMode(ledPin, OUTPUT);
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
	mySerial.print(someChar);
  //ether.staticSetup(myip);
}

static word homePage() {
  long t = millis() / 1000;
  word h = t / 3600;
  byte m = (t / 60) % 60;
  byte s = t % 60;
  bfill = ether.tcpOffset();
  bfill.emit_p(PSTR(
    "HTTP/1.0 200 OK\r\n"
    "Content-Type: text/html\r\n"
    "Pragma: no-cache\r\n"
    "\r\n"
    "<meta http-equiv='refresh' content='1000'/>"
    "<title>RBBB server</title>" 
    "<h1>$D$D:$D$D:$D$D</h1>"),
      h/10, h%10, m/10, m%10, s/10, s%10);
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

void loop() {
	unsigned char device = 255;
	word len = ether.packetReceive();
	word pos = ether.packetLoop(len);
	
	if (pos)
	{
		//Serial.println("\nReceived command:");
		//Serial.print((char*)Ethernet::buffer+pos);
		char *  command_status = strstr((char *)Ethernet::buffer + pos, "GET /GetStatus/");
		if (command_status != 0)
		{
			//*(command_status + 18) = 0;
			device = atoi(command_status + 15);
			Serial.println("\nDevice:");
			Serial.print(device);
			mySerial.write((0x80 | ((device & 0x0f) << 3)));
			delay(100);
			if (mySerial.available()) {
				int sts = mySerial.read();
				int curr_status = (sts & 0xC0) >> 6;
				int curr_time = sts & 0x3F;
				Serial.print("\nStatus: ");
				Serial.print(curr_status);
				Serial.print(" Time: ");
				Serial.print(curr_time);
			}
		}
		char * command_start = strstr((char *)Ethernet::buffer + pos, "GET /Start/");
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
			int pre_time = strtol(command_start, &command_start, 10);
			Serial.println("\nPreTime:");
			Serial.print(pre_time);		
			
			tmp = strstr(command_start, "/");
			command_start = tmp+1;
			int work_time = strtol(command_start, &command_start, 10);
			Serial.println("\nWorkTime:");
			Serial.print(work_time);		

			tmp = strstr(command_start, "/");
			command_start = tmp+1;
			int cool_time = strtol(command_start, &command_start, 10);
			Serial.println("\nCoolTime:");
			Serial.print(cool_time);

			int val1 = atoi(command_start);
			int retry;
			int time_in_hex = ToBCD(work_time);
			pre_time = pre_time & 0x7F;
			// checksum: CoolTime + Pre-Time - 5 - MainTime
			/*
			while(retry < 20){
				// clear in FIFO
				volatile unsigned char checksum = (preset_pre_time + preset_cool_time  - time_in_hex - 5) & 0x7F;
				volatile unsigned char  remote_check_sum = 220;
				while(USART_GetFlagStatus(USART1,USART_FLAG_RXNE))	USART_ReceiveData(USART1); // Flush input
				while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET); // wAIT UNTIL TX BUFFER IS EMPTY
			
				USART_SendData(USART1,0x80U | ((controller_address & 0x0fU)<<3U) | 2U); //Command 2 == Pre_time_set
				SystickDelay(2);
				USART_SendData(USART1,preset_pre_time);
				while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET); // wAIT UNTIL TX BUFFER IS EMPTY
					
				SystickDelay(2);
				USART_SendData(USART1,0x80U | ((controller_address & 0x0fU)<<3U) | 5U); //Command 5 == Main time set
				while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET); // wAIT UNTIL TX BUFFER IS EMPTY
		
				SystickDelay(2);
				USART_SendData(USART1,time_in_hex);
				while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET); // wAIT UNTIL TX BUFFER IS EMPTY
			
	//			SystickDelay(2);
				retry2 =0;
				while(!USART_GetFlagStatus(USART1,USART_FLAG_RXNE) && retry2++<10){
					SystickDelay(2);
				}
				USART_ReceiveData(USART1); //"Read" old time
	
				SystickDelay(2);
				USART_SendData(USART1,0x80U | ((controller_address & 0x0fU)<<3U) | 3U); //Command 3 == Cool Time set
				while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET); // wAIT UNTIL TX BUFFER IS EMPTY
			
				SystickDelay(2);
				USART_SendData(USART1,preset_cool_time);
				while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET); // wAIT UNTIL TX BUFFER IS EMPTY
			
			
				SystickDelay(4);
				retry2 =0;
				while(!USART_GetFlagStatus(USART1,USART_FLAG_RXNE) && retry2++<10){
					SystickDelay(2);
				}
				if(USART_GetFlagStatus(USART1,USART_FLAG_RXNE)){
					remote_check_sum = USART_ReceiveData(USART1); //"Read" checksum
				}
				SystickDelay(20);
	//			checksum = remote_check_sum;
				if(remote_check_sum == checksum){
					SystickDelay(2);
					USART_SendData(USART1,checksum);
					while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET); // wAIT UNTIL TX BUFFER IS EMPTY
			
				}
				SystickDelay(100);
				USART_SendData(USART1,0x80U | ((controller_address & 0x0f)<<3U) | 0); //Command 0 - Get status
				SystickDelay(20);
				unsigned char data = USART_ReceiveData(USART1); //"Read" status
				if ((data >> 6 ) != 0 ) retry = 22;
				retry ++;
			}
			*/
		}

	}
		
	if (Serial.available()) {
		mySerial.write(Serial.read());
	}
	if (pos)  // check if valid tcp data is received
		ether.httpServerReply(homePage()); // send web page data	
	delay(1);
}
