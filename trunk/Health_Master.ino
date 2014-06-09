/*
 Health Monitor - Master controller
 
 This sketch runs on master device wich is responsible for
 gathering data from slaves devices through a Zigbee network.
 The gathered data is sent over internet issuing a HTTP POST 
 to a webserver. The server also provides an interface HTTP
 to an Android application which is responsible for displaying
 information in near-real-time. 
 
 Circuit:
 * Arduino Ethernet Board (pins 10, 11, 12, 13 are reserved)
 * XBee device mounted on Arduino Shield
 * RJ-45 cable to connect LAN interface
 
 created 01 May 2014
 by ISCTE: METI-PLA1 Group
 Julio Ribeiro
 João Guiomar
 Rui Pereira
*/

#include <SPI.h>
#include <Ethernet.h>


//**************************************************************
//STATIC VARIABLES
//**************************************************************

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
const char server[] = "www.clevermobile.dx.am";    // Webserver 
const int SAMPLES = 1;
const String SECOND_PARAMETER = "deviceId";
const String deviceId = "arduino1";

IPAddress ip(192,168,0,177);                      //Static IP address to use if the DHCP fails to assign

const byte frameStartByte = 0x7E;
const byte frameTypeTXrequest = 0x10;
const byte frameTypeRXpacket = 0x90;
const byte frameTypeATcommand = 0x08;
const byte frameTypeATresponse = 0x88;
const long destAddressHigh = 0x13A200;
const long destAddressLow = 0x40ABB840;          //the master device is 0x40ABB824. 
char DBcommand[ ] = "DB";


//**************************************************************
//DYNAMIC VARIABLES
//**************************************************************
boolean started = false;                   //Flag to indicate initialization setup complete
int postinterval = 5;                      //Interval between two consecutives HTTP POSTs (in seconds)
int xbeeinterval =2;
boolean isComplete = false;
int value = 0;

volatile boolean onSchedule = false;      //Flag to run XBee query
volatile boolean isPost = false;          //Flag to run HTTP POST 
volatile int elapsedtime = 0;

String jsonParam = "";
char param [9];
EthernetClient client;

byte ATcounter=0;               // For identifying current AT command frame
byte rssi=0;                    // RSSI value of last received packet
byte routine = 0;               // Routine operation number to be execute during runtime  
byte payload[4];                // Payload of bytes: 1 function, 2 parameters, 1 RSSI optional. Master sends few bytes, only for requesting data
byte rxdatacontainer[8];        // Container of received bytes from Slave 

String postData;                           // String containing the Function ID and values to be POSTED
String lastPostData = "null";              // String containing the last POST (to prevent fill the webserver with duplicated data and save outbound bandwidth)
//**************************************************************
//Program initialization: SETUP 
//**************************************************************
void setup() {

  DDRB = 1<< DDB0;               // PB0/D8 is an output to indicate POST Status
  DDRD = 1<< DDD7;               // PD7/D7 is an output to indicate Xbee trx
  DDRD = 1<< DDD6;               // PD6/D6 is an output to indicate Webserver connection Status
  
  Serial.begin(9600);
  TIMER_init();
  asm("SEI");
  
  if (Ethernet.begin(mac) == 0) {
    //Serial.println("Failed to configure Ethernet using DHCP");
    Ethernet.begin(mac, ip);
  }
  // give the Ethernet shield a second to initialize:
  delay(1000);
  //Serial.println("SETUP: Trying to reach webserver...");

  // if you get a connection, report back via serial:
  if (client.connect(server, 80)) {
    //Serial.println("SETUP: Server is on!");
    started = true;
    PORTD = 1 << PORTD6;
    client.stop();
  } 
  else {
    //Serial.println("SETUP: Server NOT responding!");
    started = false;
    }
}


//**************************************************************
//Timer initialization
//**************************************************************
void TIMER_init(){
  TCCR1A = 0<<COM1A1 | 0<< COM1A0 | 0<< COM1B1 | 0<< COM1B0 | 0<<WGM11 | 0<<WGM10; // Normal port operation ( OC1A/OC1B disconnected); CTC mode with ICR1 (mode 12),
  TCCR1B = 1<<WGM13 | 1<<WGM12 | 1<<CS12 | 0<<CS11 | 1<<CS10; // CTC mode with ICR1 (mode 12) and divide clock by 1024 (16,000,000 ÷ 1024 = 15,625Hz) 
  ICR1 = 15625; // used as compare value for counter
  TIMSK1=1<<ICIE1; // enable input capture interrupt
}


//**************************************************************
//INTERRUP HANDLER: Timer Interrupt 
//**************************************************************
ISR(TIMER1_CAPT_vect) {

  elapsedtime++;
  
  if(elapsedtime >= xbeeinterval){    
   PORTD = 1 << PORTD7;
   onSchedule = true;
     if(!started) elapsedtime=0;
  }
  if(elapsedtime >= postinterval && started==true){
    PORTB = 1 << PORTB0;
    isPost = true;
  }
}

//**************************************************************
//LOOP: MAIN LOOP
//**************************************************************

void loop()
{
  //Is time to run routines?
  if(onSchedule){               
      getSlaveHealthMonitor(0xFF);
      transmitRequestAPI(payload);
      onSchedule = false;
      PORTD = 0 << PORTD7;
     }
    
  //Evaluate if is time to POST
  if(isPost){
      if (lastPostData != postData){                                       // Avoid duplicate post to save DB space and outbound bw
        postHttp();
        isPost = false;
        lastPostData = postData;
        PORTB = 0 << PORTB0;
      }
   }

  //Evaluate XBEE received bytes
  if (Serial.available() >= 10) {                                            // Wait until we have a mouthful of data
      boolean isValid = decodeAPIpacket();                                   // Get API frame bytes and put on rxdatacontainer[] to be evaluated if TRUE
      if(isValid){
        parseSlave();
      }
      
    }

  delay(250); 
  
  //Evaluate ETHERNET incomming bytes
  if (client.available()){
    char c = client.read();
    //Serial.print(c);
  }

  //Evaluate ETHERNET link status
  if (!client.connected()){
    client.stop();
  }

/*

  //Evaluate USART incomming bytes (for manual testing only)
  if(Serial.available())
  {
    char cmd = Serial.read();
    Serial.print(cmd);
      if (cmd == 'g'){
        Serial.println("Send GET command...");
        getHttp();
      }
      if (cmd == 'p'){
        Serial.println("Send POST command...");
        postHttp("bpm",0);  
      }
  }

 */
 
}


//**************************************************************
//PARSE Received: Parse received data from Slave XBee
//**************************************************************
void parseSlave(){


  switch (rxdatacontainer[0]){
    case 0xA2:   //Function: getSlaveHealthMonitor
      int temp = word(rxdatacontainer[2],rxdatacontainer[3]);
      int bpm = word(rxdatacontainer[4],rxdatacontainer[5]);
      int mov = word(rxdatacontainer[6],rxdatacontainer[7]);
      //postData="fid=A2&val=#"+String(temp)+"#"+String(bpm)+"#"+String(mov);
   
      //Creat POST string with received values in rxdatacontainer
      postData="deviceId="+deviceId+"&param1=temp&val1="+temp;  
      
     break;
  }
return;
  
}




//**************************************************************
//HTTP Get: Retrieve data over internet
//**************************************************************
void getHttp(){

  //Serial.println("GET: Connecting...");
 if (client.connect(server, 80)) {
    //Serial.println("GET: Connected!");
    client.println("GET /mobile2.php?key=bpm&deviceId=arduino1 HTTP/1.1");
    client.println("Host: www.clevermobile.dx.am");
    client.println("Connection: close");
    client.println();
  } 
  else {
    //Serial.println("GET: Connection failed");
  }
  
}

//**************************************************************
//HTTP Post: Send data over internet
//**************************************************************
void postHttp(){

 
//    postData="deviceId="+deviceId+"&param1=temp&val1=99";  

  if (client.connect(server,80)){
    //Serial.println("POST: Connection established");
    client.println("POST /post.php HTTP/1.1");
    client.println("Host:  www.clevermobile.dx.am");
    client.println("User-Agent: Arduino/1.0");
    client.println("Connection: close");
    client.println("Content-Type: application/x-www-form-urlencoded;");
    client.print("Content-Length: ");
    client.println(postData.length());
    client.println();
    client.println(postData);
    }
  client.stop();
  elapsedtime = 0;
  
}


/*********************************************/
/*                RECEIVE DATA              */
/*********************************************/

boolean decodeAPIpacket(void) {

  // Function for decoding the received API frame from XBEE
  byte frametype;
  int packetlenght;

  while (Serial.read() != frameStartByte){
    if (Serial.available()==0)
      return false; // No API frame present. Return FALSE
  }

   // Get the length of bytes in the API frame
  byte msb = Serial.read();            // MSB
  byte lsb = Serial.read();            // LSB
  int frameLength = word(msb,lsb);     // Number of bytes

  // Read received frame type
  frametype=Serial.read();

  if (frametype==frameTypeRXpacket){
    // Zigbee Receive Packet API FRame

    // Skip over the bytes in the API frame we don't care about (addresses, receive options)
    for (int i = 0; i < 11; i++) {
      Serial.read();
    }
    
    
    // The next bytes are the data sent from Slave
    for (byte i = 0; i < (frameLength-12); i++){     
      rxdatacontainer[i] = Serial.read();
      //value = frameLength-12;
    }
      
    Serial.read(); // Read  the last byte (Checksum) but don't store it
//  formatATcommandAPI("DB");  // query the RSSI of the last received packet
  } 
  else if (frametype==frameTypeATresponse){
    // AT Command Response API frame

  }

  return true;
}


/*****************************************************************************************************/
/*  TRANSMIT PAYLOAD BITS: Format and transmit a "Transmit Request API frame" with 56 bits of data   */
/*****************************************************************************************************/
void transmitRequestAPI(byte dataPayload[]) { 
  
  // Accumulate the checksum
  long sum = 0; 

  // API frame Start Delimiter
  Serial.write(frameStartByte);

  // Length - High and low parts of the frame length (Number of bytes between the length and the checksum)
  Serial.write(0x00);
  Serial.write(0x12);    //total 18: 14 bytes up to start of “RF Data” fields in TX Request  frame + 4 bytes of payload 

  // Frame Type - Indicate this frame contains a Transmit Request
  Serial.write(frameTypeTXrequest); 
  sum += frameTypeTXrequest;

  // Frame ID - set to zero for no reply
  Serial.write(0x00); 
  sum += 0x00;

  // 64-bit Destination Address - The following 8 bytes indicate the 64 bit address of the recipient (high and low parts).
  // Use 0xFFFF to broadcast to all nodes.
  Serial.write((destAddressHigh >> 24) & 0xFF);
  Serial.write((destAddressHigh >> 16) & 0xFF);
  Serial.write((destAddressHigh >> 8) & 0xFF);
  Serial.write((destAddressHigh) & 0xFF);
  sum += ((destAddressHigh >> 24) & 0xFF);
  sum += ((destAddressHigh >> 16) & 0xFF);
  sum += ((destAddressHigh >> 8) & 0xFF);
  sum += (destAddressHigh & 0xFF);
  Serial.write((destAddressLow >> 24) & 0xFF);
  Serial.write((destAddressLow >> 16) & 0xFF);
  Serial.write((destAddressLow >> 8) & 0xFF);
  Serial.write(destAddressLow & 0xFF);
  sum += ((destAddressLow >> 24) & 0xFF);
  sum += ((destAddressLow >> 16) & 0xFF);
  sum += ((destAddressLow >> 8) & 0xFF);
  sum += (destAddressLow & 0xFF);

  // 16-bit Destination Network Address - The following 2 bytes indicate the 16-bit address of the recipient.
  // Use 0xFFFE if the address is unknown.
  Serial.write(0xFF);
  Serial.write(0xFE);
  sum += 0xFF+0xFE;

  // Broadcast Radius - when set to 0, the broadcast radius will be set to the maximum hops value
  Serial.write(0x00);
  sum += 0x00;

  // Options
  // 0x01 - Disable retries and route repair
  // 0x20 - Enable APS encryption (if EE=1)
  // 0x40 - Use the extended transmission timeout
  Serial.write(0x00);
  sum += 0x00;

  // RF Data
  for (int i = 0; i<4; i++){
    Serial.write(dataPayload[i]);   
  }
  sum += ( dataPayload[0] + dataPayload[1] + dataPayload[2] + dataPayload[3]) ; 

//  Serial.write(rssi); // RSSI reading
//  sum += rssi;

  // Checksum = 0xFF - the 8 bit sum of bytes from offset 3 (Frame Type) to this byte.
  Serial.write( 0xFF - ( sum & 0xFF));
  delay(30); // Pause to let the microcontroller settle down if needed
  
  return;
} 

//**************************************************************
//getSlaveHealthMonitor: Get HealthMonitor Variables remoteley
//**************************************************************
void getSlaveHealthMonitor(byte Param){
      payload[0] = 0xA2;    //Function Id
      payload[1] = Param;   //0x01 = Temp;  0x02 = BPM; 0x03 = Move; 0xFF=ALL
      payload[2] = 0x00;    //This function has not aditional parameter
  return;        
}


