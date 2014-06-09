/**
 * Reference: http://pulsesensor.myshopify.com/pages/pulse-sensor-amped-arduino-v1dot1
 */

// these variables are volatile because they are used during the interrupt service routine!
volatile int rate[10]; // array to hold last ten IBI values
volatile unsigned long sampleCounter = 0; // used to determine pulse timing
volatile unsigned long lastBeatTime = 0;  // used to find IBI
volatile int thresh = 512; // used to find instant moment of heart beat, seeded
volatile int P = thresh; // used to find peak in pulse wave, seeded
volatile int T = thresh; // used to find trough in pulse wave, seeded
volatile int amp = 100;                   // used to hold amplitude of pulse waveform, seeded
volatile boolean firstBeat = true;        // used to seed rate array so we startup with reasonable BPM
volatile boolean secondBeat = false;      // used to seed rate array so we startup with reasonable BPM

void ADC_init() {
   ADMUX = 0<<REFS1 | 1<<REFS0 | 0<<ADLAR | 0<<MUX2 | 0<<MUX1 | 1<<MUX0; // Voltage reference for ADC=AVCC (5.0V); configurar ADLAR para right adjust for 10 bit resolution, select ADC1 (pino A1),
   ADCSRA = 1<<ADEN | 1<<ADSC | 1<<ADATE | 0<<ADIE | 1<<ADPS2 | 1<<ADPS1 | 1<<ADPS0;  // Enable ADC (ADEN), Start ADC conversion (ADSC), Enable Auto Trigger, disable ADC Interrupts, configure ADPS2-0 for 128 prescale (resulta numa frequência 16MHz/128=125kHz)
   // TODO: Pass to bit shift method
   ADCSRB = 0x00; // free running mode, (leave ACME as 0). 
   DIDR0 = 0x3F; // desliga digital inputs nos pinos ADC0-ADC5 para reduzir consumo
}

/**
 * Initialize timer 2
 *
 * 1. Timer2 throws an interrupt every other millisecond -> sample rate of 500Hz, beat-to-beat timing resolution of 2mS
 * 2. Disable PWM output on pin 3 and 11. 
 * 3. Disable the tone() command.
 */
void TIMER2_init() {  
  TCCR2A = 0<<COM2A1 | 0<<COM2A0 | 0<<COM2B1 | 0<<COM2B0 | 1<<WGM21 | 0<<WGM20; // 0x02 -> Normal port operation (OC2A/OC2B disconnected), CTC
  TCCR2B = 0<<FOC2A | 0<<FOC2B | 0<<WGM22 | 1<<CS22 | 1<<CS21 | 0<<CS20; // 0x06 -> 
  // TODO: Pass to bit shift method
  OCR2A = 0x7C;
  TIMSK2 = 0x02;
}

ISR(TIMER2_COMPA_vect) {
   cli();  // disable interrupts while we do this
   Signal = analogRead(PIN_BPM); //ADC; // analog read
   sampleCounter += 2; // keep track of time +2ms
   int N = sampleCounter - lastBeatTime; // avoid noise later ?!?
   
   // Get highest and lowest values of the PPG wave
   if(Signal < thresh && N > (IBI/5)*3){ // time period of 3/5 IBI that must pass before T
      if (Signal < T){ 
        T = Signal; // lowest
      }
    }
    if(Signal > thresh && Signal > P){  
        P = Signal; // highest           
    }
    
    //  NOW IT'S TIME TO LOOK FOR THE HEART BEAT
    // signal surges up in value every time there is a pulse
    if (N > 250){                                   // avoid high frequency noise
        if ( (Signal > thresh) && (Pulse == false) && (N > (IBI/5)*3) ){        
          Pulse = true;                               // set the Pulse flag when we think there is a pulse
          
          // READ FROM HERE ...
          PORTB = 1<<PORTB1;
          
          IBI = sampleCounter - lastBeatTime;         // measure time between beats in mS
          lastBeatTime = sampleCounter;               // keep track of time for next pulse
    
          if(secondBeat) {                        // if this is the second beat, if secondBeat == TRUE
            secondBeat = false;                  // clear secondBeat flag
            for(int i=0; i<=9; i++){             // seed the running total to get a realisitic BPM at startup
              rate[i] = IBI;                      
            }
          }
    
          if(firstBeat) {                         // if it's the first time we found a beat, if firstBeat == TRUE
            firstBeat = false;                   // clear firstBeat flag
            secondBeat = true;                   // set the second beat flag
            sei();                               // enable interrupts again
            return;                              // IBI value is unreliable so discard it
          }   
    
          // keep a running total of the last 10 IBI values
          word runningTotal = 0;                  // clear the runningTotal variable    
    
          for(int i=0; i<=8; i++){                // shift data in the rate array
            rate[i] = rate[i+1];                  // and drop the oldest IBI value 
            runningTotal += rate[i];              // add up the 9 oldest IBI values
          }
    
          rate[9] = IBI;                          // add the latest IBI to the rate array
          runningTotal += rate[9];                // add the latest IBI to runningTotal
          runningTotal /= 10;                     // average the last 10 IBI values 
          BPM = 60000/runningTotal;               // how many beats can fit into a minute? that's BPM!
          QS = true;                              // set Quantified Self flag           
          // QS FLAG IS NOT CLEARED INSIDE THIS ISR
        }                       
      }
    
      if (Signal < thresh && Pulse == true){   // when the values are going down, the beat is over
        // digitalWrite(blinkPin,LOW);            // turn off pin 13 LED
        PORTB = 1<<PORTB2;
        
        Pulse = false;                         // reset the Pulse flag so we can do it again
        amp = P - T;                           // get amplitude of the pulse wave
        thresh = amp/2 + T;                    // set thresh at 50% of the amplitude
        P = thresh;                            // reset these for next time
        T = thresh;
      }
    
      if (N > 2500){                           // if 2.5 seconds go by without a beat
        thresh = 512;                          // set thresh default
        P = 512;                               // set P default
        T = 512;                               // set T default
        lastBeatTime = sampleCounter;          // bring the lastBeatTime up to date        
        firstBeat = true;                      // set these to avoid noise
        secondBeat = false;                    // when we get the heartbeat back
      }
      sei();                                   // enable interrupts when youre done!
}// end isr
