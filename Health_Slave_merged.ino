
/*
  Slave Device - version 1.3
  
  Reads an analog input on pin, convert to temperature using TMP36 sensor and transmit via Xbee
  
 This example code is in the public domain.
 */


//**************************************************************
//CONSTANTS: Statics values defined in the program
//**************************************************************

#define PIN_TEMP 0
#define PIN_BPM 1
#define PIN_MOVE 2
const byte frameStartByte = 0x7E;
const byte frameTypeTXrequest = 0x10;
const byte frameTypeRXpacket = 0x90;
const byte frameTypeATcommand = 0x08;
const byte frameTypeATresponse = 0x88;
const long destAddressHigh = 0x13A200;
const long destAddressLow = 0x40ABB824;    //master device. the slave is 0x40ABB840
char DBcommand[ ] = "DB";
int sensorPin = 0;    // The analog pin the TMP36's Vout (sense) pin is connected to


//**************************************************************
//VARIABLES: Volatile variables changed during runtime 
//**************************************************************

byte ATcounter=0;               // For identifying current AT command frame
byte rssi=0;                    // RSSI value of last received packet
byte routine = 0xFF;            // Routine operation number to be execute during runtime  
boolean onSchedule = true;      // Flag to run Routine 
int payload[8];                 // Payload of Integers (max=8 integers or 16 BYTES. In fact will be used only 8 BYTES (4 integers)) 
byte rxdatacontainer[8];        // Container of received data from Master


volatile int Signal;             // holds the incoming raw data
volatile int IBI = 600;          // holds the time between beats, must be seeded!
volatile int BPM;                // used to hold the pulse rate
volatile boolean Pulse = false;  // true when pulse wave is high, false when it's low
volatile boolean QS = false;     // becomes true when Arduoino finds a beat.

// last 10 BPM values
int lastBPM[10];


//**************************************************************
//Program initialization: SETUP 
//**************************************************************

void setup()
{
  DDRB = 1<<DDB5; // PB5/D13 is an output    MUST BE CHANGED IN ARDUINO ETHERNET!!
  Serial.begin(9600);  //Start the serial connection with the computer
//  analogReference(EXTERNAL); // Set the aref to 3.3v 

  //ADC_init();
  TIMER2_init();
  
  memset(lastBPM,0,sizeof(lastBPM));
  
  DDRB = 1<<PORTB2 | 1<<PORTB1; // PB2/D10 and PB1/D9 as output


}


//**************************************************************
//MAIN LOOP PROGRAM: LOOP
//**************************************************************

void loop(){

  if(QS) {
    saveBPM();
    //Serial.println(BPM);
    QS=false;
  }
  delay(20);
  
   switch (routine) {                                                         // Evaluate the routine operation to be executed
        case 0:                                                               // DEFAULT ROUTINE 
            payload[0] = 0xA2 << 8;
            payload[1] = getADCReading(PIN_TEMP);
            payload[2] = BPM;
            payload[3] = getADCReading(PIN_MOVE);
            transmitRequestAPI(payload);
            //Serial.println(payload[2]);
            routine = 0xFF;            
         break;
    }
  if (Serial.available() >= 10) {                                            // Wait until we have a mouthful of data
      boolean isValid = decodeAPIpacket();                                   // Get API frame bytes and put on rxdatacontainer[] to be evaluated if TRUE
      if(isValid){
        parseRXData();
      }
  }
  delay(250);                                
}

//**************************************************************
//PARSE Received: Parse received data from Master XBee
//**************************************************************
void parseRXData(){

  switch (rxdatacontainer[0]){
    case 0xA2:
      routine = 0;      
    break;
  }
return;
  
}


//**************************************************************
//ADC reading: retrieve voltage reading from a given analog input 
//**************************************************************

int getADCReading(byte Port){
//
//    ADMUX &= 240;   //clear previous selected input ADC
//    ADMUX |= Port;  //set new selected input ADC

     return analogRead(Port);
 }

/*********************************************/
/*                RECEIVE DATA              */
/*********************************************/
boolean decodeAPIpacket(void) {
  
  // Function for decoding the received API frame from XBEE
   byte frametype;

  while (Serial.read() != frameStartByte){
    if (Serial.available()==0)
      return false; // No API frame present. Return 'false'
  }

  // Get the length of bytes in the API frame
  byte msb = Serial.read();            // MSB
  byte lsb = Serial.read();            // LSB
  int frameLength = word(msb,lsb);     // Number of bytes
  
  // Read received frame type
  frametype=Serial.read();

  if (frametype==frameTypeRXpacket){
    // Skip over the bytes in the API frame we don't care about (addresses, receive options)
    for (int i = 0; i < 11; i++) {
      Serial.read();
    }
    // The next bytes are the data sent from Master
    for (int i = 0; i < (frameLength-11); i++){     
      rxdatacontainer[i] = Serial.read();
    }
      Serial.read(); // Read  the last byte (Checksum) but don't store it
//    formatATcommandAPI("DB");  // query the RSSI of the last received packet
  } 
  else if (frametype==frameTypeATresponse){
    // AT Command Response API frame

  }

  return true;
}


/*********************************************/
/*         TRANSMIT PAYLOAD BITS          */
/*********************************************/

void transmitRequestAPI( int dataPayload[]) { 
  // Format and transmit a Transmit Request API frame with 48 bits of data

    long sum = 0; // Accumulate the checksum

  // API frame Start Delimiter
  Serial.write(frameStartByte);

  // Length - High and low parts of the frame length (Number of bytes between the length and the checksum)
  Serial.write(0x00);
  Serial.write(0x16);    //total 22: 14 bytes up to start of “RF Data” fields in TX Request  frame + 8 bytes of payload 

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
  Serial.write((dataPayload[0] >> 8) & 0xFF);       // Function id (high byte)
  Serial.write(dataPayload[0] & 0xFF);              // Function id (low byte)
  Serial.write((dataPayload[1] >> 8) & 0xFF);       // Int1 (high byte)
  Serial.write(dataPayload[1]  & 0xFF);             // Int1 (low byte)
  Serial.write((dataPayload[2] >> 8) & 0xFF);       // Int2 (high byte)
  Serial.write(dataPayload[2]  & 0xFF);             // Int2 (low byte)
  Serial.write((dataPayload[3] >> 8) & 0xFF);       // Int3 (high byte)
  Serial.write(dataPayload[3] & 0xFF);              // Int3 (low byte)
  
  sum += ((dataPayload[0] >> 8) & 0xFF)+(dataPayload[0] & 0xFF)+((dataPayload[1] >> 8) & 0xFF)+(dataPayload[1] & 0xFF) +((dataPayload[2] >> 8) & 0xFF)+(dataPayload[2] & 0xFF) + ((dataPayload[3] >> 8)& 0xFF)+(dataPayload[3] & 0xFF); 
//  Serial.write(rssi); // RSSI reading
//  sum += rssi;

  // Checksum = 0xFF - the 8 bit sum of bytes from offset 3 (Frame Type) to this byte.
  Serial.write( 0xFF - ( sum & 0xFF));

  delay(30); // Pause to let the microcontroller settle down if needed
} 

