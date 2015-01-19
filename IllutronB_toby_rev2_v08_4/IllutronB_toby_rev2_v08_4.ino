//**************************************************************************
//*   Simple AVR wavetable synthesizer V1.0                                *
//*                                                                        *
//*   Implements 4 voices using selectable waveform and envelope tables    *
//*   Uses 8-bit PWM @ 62.5 kHz for audio output                           *
//*                                                                        *
//*   (C) DZL 2008                                                         *
//**************************************************************************

// Refactoring pass 1. Subsequent passes will address the interface to accessing CVoice members of the CIllutronB class

// The Illutron is a wave table synthesiser, which generates complex sound by combining simpler wave forms
// Each of the voices is formed by combining one waveforms - noise,ramp,saw,sin,square,triangle - included with the library
// With an envelope - the envelope controls how the sound evolves over time - the initial attach, the sustain and then the decay of the note
// The synth provides additional parameters from controlling the pitch, duration and pitch over time of the sound
//
// The synth is the original work of Nikolaj Mobius and has been refactored, optimised and documented by Duane B (rcarduino)
//
// The synth is contained in the files IllutronB.h and IllutronB.cpp
//
// This sketch presents a demonstration of the synth sound using a simple sequencer


#include <math.h>

#include "avr/interrupt.h"
#include "avr/pgmspace.h"

#include "IllutronB.h"

// Include the wavetables, you could add your own as well
#include "sin256.h"
#include "ramp256.h"
#include "saw256.h"
#include "square256.h"
#include "noise256.h"
#include "tria256.h"

// include the envelopes, again you could add more 
#include "env0.h"
#include "env1.h"
#include "env2.h"
#include "env3.h"

// include the pitches as defined by the Arduino tone function
// while these work - it would be useful to check that the tones are correct against a tuned instrument
// the midi note calculations and tone library pitches do not agree with each other.
//#include "pitches.h"

#include "AmenBreak.h"

#define PLAY_BACK_BPM_PIN 7 // analog pin rev2 7  rev1 1
#define PITCH_PIN 6         // analog pin rev2 6  rev1 2

#define CHANNEL0_GATE 11  //Button1 Rev2 11 rev1 7
#define CHANNEL1_GATE 10  //Button2 rev2 10  rev1 4
#define CHANNEL2_GATE 9   //Button3 rev2 9 rev1 2
#define CHANNEL3_GATE 4   //Button4 rev2 4  rev1 3
#define BUTTON_5 3        //Button5 rev2 3  rev1 8

byte button1, button2, button3, button4, prevbutton1, prevbutton2, prevbutton3, prevbutton4, button1_latch, button2_latch, button3_latch, button4_latch;
byte gate0, gate1, gate2, gate3;
// Pins and other definitions for the LED Visualiser
#define REFRESH_DIVIDER 8

// These pins are the least interesting digital pins, no interrupts, no SPI, no PWM
// so lets use them for the LEDs
#define CHANNEL0_LED 13     //LED 1  rev2 13  rev1 10
#define CHANNEL1_LED 12     //LED 2  rev2 12  rev1 12
// digital pin 5 might be used for output if we are splitting channels.
// digital pin 6 is our output
#define CHANNEL2_LED 8      //LED 3  rev2 8  rev1 9
#define CHANNEL3_LED 7      //LED 4  rev2 7  rev1 11

#define LED_5 2             //LED 5  rev2 2  rev1 13

byte mode,prevmode,mode_latch;
byte play_track_now;
int pitch2, pitch1, pitch0;
int song_select;
int ramp_out = 512;
long previousMillis = 0; // will store last time LED was updated
long previousMillis_bpm = 0; // will store last time LED was updated
int pot1; 
int bpm_pitch = 500;
byte bpm = 120;
byte bpm_latch;
int cycle_pitch = 0;
int cycle_man = 0;
// the follow variables is a long because the time, measured in miliseconds,
// will quickly become a bigger number than can be stored in an int.
int intervalll = 50;   


void setup()
{
  pitch1=0;
  pitch2=0;
  
  Serial.begin(9600);
  play_track_now = 1;

/* PROJECT SPECIFIC SETUP */
  pinMode(CHANNEL0_LED,OUTPUT);
  pinMode(CHANNEL1_LED,OUTPUT);
  pinMode(CHANNEL2_LED,OUTPUT);
  pinMode(CHANNEL3_LED,OUTPUT);
  pinMode(LED_5,OUTPUT);

  pinMode(CHANNEL0_GATE,INPUT_PULLUP); 
  pinMode(CHANNEL1_GATE,INPUT_PULLUP);
  pinMode(CHANNEL2_GATE,INPUT_PULLUP);
  pinMode(CHANNEL3_GATE,INPUT_PULLUP);
  pinMode(BUTTON_5,INPUT_PULLUP);
  
  
  CIllutronB::setBPM(120);  
  CIllutronB::initSynth();

  CIllutronB::m_Voices[0].setup((unsigned int)SinTable,200.0,(unsigned int)Env0,0.4,300);
  CIllutronB::m_Voices[1].setup((unsigned int)RampTable,100.0,(unsigned int)Env1,1.0,512);
  CIllutronB::m_Voices[2].setup((unsigned int)TriangleTable,100.0,(unsigned int)Env2,.5,1000);
  CIllutronB::m_Voices[3].setup((unsigned int)NoiseTable,1200.0,(unsigned int)Env3,.04,500);
}

uint8_t nCycle = 0;
uint8_t nBeat = 0;



void loop()
{
    // The synth works in the background using a timer interrupt
    // Ask the IllutronB if the current beat has completed, if so lets add the next one
    if(CIllutronB::beatComplete()) 
    {
      // This is just for fun - allow the user to change the play back speed at anytime using
      // a potentiometer on analogue pin A1 - Map the potentiometer to a range of 80 to 240 BPM
      // Use this for user control of BPM
      CIllutronB::setBPM(map (bpm_pitch ,0,1024,10,160));
      //CIllutronB::setBPM(map(analogRead(PLAY_BACK_BPM_PIN),0,1024,10,170));        
      // use this to hear the original sequence at the original play back speed
//      CIllutronB::setBPM(140);        
      
      // If there is a new beat/note for this channel, tell the Illutron B to play it
      // repeat simple repeats the note using whatever configuration it was previously given
      // its good for drum sounds where you just want to repeat without changing the tone
      unsigned char sNote;
      Serial.print("  manual Cycle: ");     
      Serial.print(cycle_man); 
      Serial.print("  nCycle: ");     
      Serial.print(nCycle);  
      Serial.print("  Beat ");     
      Serial.print(nBeat, DEC);
      
      switch(play_track_now)
        {
          case 1:
           sNote = pCurrentSequence1->getTrigger(0,nBeat);
             break;
          case 2:
            sNote = pCurrentSequence2->getTrigger(0,nBeat);
             break;
          case 3:
            sNote = pCurrentSequence3->getTrigger(0,nBeat);
             break;
          case 4:
            sNote = pCurrentSequence4->getTrigger(0,nBeat);
             break;
        }
        
        Serial.print("  Channel 0: ");
        
      if(sNote && (gate0==0))
      {
        CIllutronB::m_Voices[CHANNEL_0].trigger();
       // CIllutronB::m_Voices[CHANNEL_3].trigger();
       Serial.print(sNote  );
      }
      
      // triggerMidi allows you to trigger the channel to play back a sound at a particular note
      // it uses the midi note number, there is a look up table here - http://www.phys.unsw.edu.au/jw/notes.html
      // Notice that this is also connected to an analog input, this adjusts the base play back note.
      switch(play_track_now)
        {
          case 1:
           sNote = pCurrentSequence1->getTrigger(1,nBeat);
             break;
          case 2:
            sNote = pCurrentSequence2->getTrigger(1,nBeat);
             break;
          case 3:
            sNote = pCurrentSequence3->getTrigger(1,nBeat);
             break;
          case 4:
            sNote = pCurrentSequence4->getTrigger(1,nBeat);
             break;
        }
        Serial.print("  Channel 1: ");
        
      if(sNote && (gate1==0))
      {
        // Use this to add user control of the pitch other wise the default will play the pitch defined in the sequence
        sNote=sNote+pitch1;
        CIllutronB::m_Voices[CHANNEL_1].triggerMidi((sNote));
        Serial.print(sNote, OCT);
        // To hear the original sequence played as intended, use the following - 
     //  CIllutronB::m_Voices[CHANNEL_1].triggerMidi(sNote);
      }
      
      // This is also a neat trick, it looks at the pattern in channel 2 to trigger the percussion sound
      // but uses the notes in parttern 1 to add some variety to the sound by playing it a different pitches
      switch(play_track_now)
        {
          case 1:
           sNote = pCurrentSequence1->getTrigger(2,nBeat);
             break;
          case 2:
            sNote = pCurrentSequence2->getTrigger(2,nBeat);
             break;
          case 3:
            sNote = pCurrentSequence3->getTrigger(2,nBeat);
             break;
          case 4:
            sNote = pCurrentSequence4->getTrigger(2,nBeat);
             break;
        }
        Serial.print("  Channel 2: ");
        
        
      if(sNote && (gate2==0))
      {
        sNote=sNote+pitch2;
        CIllutronB::m_Voices[CHANNEL_2].triggerMidi(sNote);
         Serial.print(sNote, OCT);
      }
      
      // another example of simply repeating a drum sound
      switch(play_track_now)
        {
          case 1:
           sNote = pCurrentSequence1->getTrigger(3,nBeat);
             break;
          case 2:
            sNote = pCurrentSequence2->getTrigger(3,nBeat);
             break;
          case 3:
            sNote = pCurrentSequence3->getTrigger(3,nBeat);
             break;
          case 4:
            sNote = pCurrentSequence4->getTrigger(3,nBeat);
             break;
        }
        Serial.print("  Channel 3: ");
        
      if(sNote  && (gate3==0))
      {
        // double up for a bang and then sustain using two voices, one for the bang and one for the sustain
      //  CIllutronB::m_Voices[CHANNEL_0].trigger();
        CIllutronB::m_Voices[CHANNEL_3].trigger();
        Serial.print(sNote);
      }
 
      Serial.println(" ... ");

      nBeat++;      // update the beat counter
      bpm_latch++;
      bpm_latch%=4;
      
     switch(cycle_man){ 
     case 0:  
      switch(nCycle)
      {
        case 4:
          CIllutronB::m_Voices[3].setup((unsigned int)TriangleTable,1500.0,(unsigned int)Env3,.03,100);
          CIllutronB::m_Voices[1].setup((unsigned int)RampTable,100.0,(unsigned int)Env1,1.0,512);
          break; 
       case 6:
          CIllutronB::m_Voices[3].setup((unsigned int)TriangleTable,1500.0,(unsigned int)Env3,.03,100);
          break;     
        case 8:
          CIllutronB::m_Voices[3].setup((unsigned int)NoiseTable,1500.0,(unsigned int)Env3,.03,300);
          break;  
        case 10:
          CIllutronB::m_Voices[1].setup((unsigned int)TriangleTable,100.0,(unsigned int)Env1,0.6,512);
          break;
        case 11:
          CIllutronB::m_Voices[1].setup((unsigned int)TriangleTable,100.0,(unsigned int)Env1,0.5,512);
          break;    
        case 12:
          CIllutronB::m_Voices[1].setup((unsigned int)TriangleTable,100.0,(unsigned int)Env1,0.4,512);
          break;
        case 14:
          CIllutronB::m_Voices[1].setup((unsigned int)RampTable,100.0,(unsigned int)Env1,0.5,512); 
          break;      
        case 15:
          CIllutronB::m_Voices[1].setup((unsigned int)RampTable,100.0,(unsigned int)Env1,1.0,512); 
          break;  
        case 16:
          CIllutronB::m_Voices[1].setup((unsigned int)RampTable,100.0,(unsigned int)Env1,0.5,512); 
          break;    
      }
     case 2:
          CIllutronB::m_Voices[1].setup((unsigned int)TriangleTable,100.0,(unsigned int)Env1,0.5,512);
          break;      
     case 3:
          CIllutronB::m_Voices[1].setup((unsigned int)NoiseTable,100.0,(unsigned int)Env1,0.1,512);
          break;
     case 4:
          CIllutronB::m_Voices[1].setup((unsigned int)RampTable,100.0,(unsigned int)Env1,0.5,512); 
          break;
     case 5:
          CIllutronB::m_Voices[1].setup((unsigned int)SinTable,100.0,(unsigned int)Env1,1.0,512); 
          break;          
     case 6:
          CIllutronB::m_Voices[2].setup((unsigned int)TriangleTable,100.0,(unsigned int)Env1,0.5,512); 
          break;          
     case 7:
          CIllutronB::m_Voices[2].setup((unsigned int)NoiseTable,100.0,(unsigned int)Env1,0.8,512); 
          break;
     case 8:
          CIllutronB::m_Voices[2].setup((unsigned int)RampTable,100.0,(unsigned int)Env1,0.8,512); 
          break;          
     case 9:
          CIllutronB::m_Voices[2].setup((unsigned int)SinTable,100.0,(unsigned int)Env1,0.8,512); 
          break;       
     case 10:
          CIllutronB::m_Voices[1].setup((unsigned int)TriangleTable,100.0,(unsigned int)Env1,0.6,512);
          break;
     case 11:
          CIllutronB::m_Voices[1].setup((unsigned int)RampTable,100.0,(unsigned int)Env1,0.5,512); 
          break;
     case 12:
          CIllutronB::m_Voices[1].setup((unsigned int)RampTable,100.0,(unsigned int)Env1,0.5,512); 
          break;          
     case 13:
          CIllutronB::m_Voices[1].setup((unsigned int)RampTable,100.0,(unsigned int)Env1,0.5,512); 
          break;          
     case 14:
          CIllutronB::m_Voices[1].setup((unsigned int)RampTable,100.0,(unsigned int)Env1,0.5,512); 
          break;
     case 15:
          CIllutronB::m_Voices[1].setup((unsigned int)RampTable,100.0,(unsigned int)Env1,0.5,512); 
          break;          
     case 16:
          CIllutronB::m_Voices[1].setup((unsigned int)RampTable,100.0,(unsigned int)Env1,0.5,512); 
          break; 
     } 
     
      
      // if it gets to the end of our sequence, reset it and update the cycle counter
      // the cycle counter is used below to change some of the voices
       switch(play_track_now)
        {
          case 1:
         if(nBeat == pCurrentSequence1->getLength())
      {
        nBeat=0;
        nCycle++;
        Serial.println(nCycle);
        
        if(nCycle >= 16)
        {
          nCycle = 0;
        } 
      }
             break;
          case 2:
           if(nBeat == pCurrentSequence2->getLength())
      {
        nBeat=0;
        nCycle++;
        if(nCycle >= 16)
        {
          nCycle = 0;
        } 
      }
             break;
          case 3:
           if(nBeat == pCurrentSequence3->getLength())
      {
        nBeat=0;
        nCycle++;
        if(nCycle >= 16)
        {
          nCycle = 0;
        } 
      }
             break;
          case 4:
         if(nBeat == pCurrentSequence4->getLength())
      {
        nBeat=0;
        nCycle++;
        if(nCycle >= 16)
        {
          nCycle = 0;
        }
      }
             break;
        }
  /*   
     
      if(nBeat == pCurrentSequence1->getLength())
      {
        nBeat=0;
        nCycle++;
        if(nCycle >= 16)
        {
          nCycle = 0;
        }
      }
      
 */     
      
      
    }
    
    // Thats it, now make some music and if its good feel free to post it here - 
    // http://rcarduino.blogspot.com/2012/08/the-must-build-arduino-project-illutron.html
    // If its really good I will add it as an option in the source code guaranteeing your future fame and fortune.
   
    // Duane B rcarduino.blogspot.com 
 // if (mode_latch==0){ 
   //  updateVisualiser();
//  }

  
  unsigned long currentMillis = millis();
 
  if(currentMillis - previousMillis > intervalll) {
    // save the last time you blinked the LED 
    previousMillis = currentMillis;   
    LED();
    BUTTONS();
    if (mode_latch==0){ 
     updateVisualiser();
    }
    }
}

void updateVisualiser()
{
  // for each channel we have an 8-bit power level - its the amplitude
  // lets divide by 4 to give 0-15
  // lets decrement each pass, if high byte set, set top led, keep the bottom led on as long as the value is > 1
  static unsigned char sRefreshDivider = REFRESH_DIVIDER;
  static unsigned char sChannelPower[CHANNEL_MAX];
  
  if(0 == sRefreshDivider)
  {
    for(unsigned char sIndex = 0;sIndex < CHANNEL_MAX;sIndex++)
    {
      sChannelPower[sIndex] = (CIllutronB::m_Voices[sIndex].getAmplitude() >> 4);
    }
    sRefreshDivider = REFRESH_DIVIDER;
  }
  
  // this is ok for a demo, but in anything more sophisticated we would want faster updates using direct port access.
  digitalWrite(CHANNEL0_LED,sChannelPower[0]);
  digitalWrite(CHANNEL1_LED,sChannelPower[1]);
  digitalWrite(CHANNEL2_LED,sChannelPower[2]);
  digitalWrite(CHANNEL3_LED,sChannelPower[3]);
  
  for(unsigned char sIndex = 0;sIndex < CHANNEL_MAX;sIndex++)
  {
    if(sChannelPower[sIndex]>0)
      sChannelPower[sIndex]--;
  }
  
  sRefreshDivider--;
  
}


void BUTTONS() {



  mode=digitalRead(BUTTON_5);   // mode toggle
  pitch0=(map(analogRead(PITCH_PIN),0,1024,-77,77));
  pot1=(analogRead(PLAY_BACK_BPM_PIN));
  
/* if (mode==0){
    if (difference(pitch2, pitch0) < 64){
    pitch2=pitch0;
    }
    
    if (difference(cycle_pitch, pot1) < 128){
       cycle_pitch=pot1;
       cycle_man=(map(cycle_pitch ,0,1024,0,17));
       //nCycle=(map(pot1 ,0,1024,0,17));
       //Serial.print(cycle_pitch);
       //Serial.println(nCycle); 
    }
  }
  if (mode==1){
  if (difference(pitch1, pitch0) < 16){
    pitch1=pitch0;
    } 
  if (difference(bpm_pitch, pot1) < 64){
    bpm_pitch=pot1;
    }
  }
  */
  
  if (mode==0 && prevmode==1){
    mode_latch++;
    mode_latch%=2;
  }
  prevmode=mode;
  
  
  
   button1=digitalRead(CHANNEL0_GATE);   //gate0 toggle
   button2=digitalRead(CHANNEL1_GATE);   //gate1 toggle
   button3=digitalRead(CHANNEL2_GATE);   //gate2 toggle
   button4=digitalRead(CHANNEL3_GATE);   //gate3 toggle


 if (mode_latch==0){            // cannel mute, pitch CANNEL_1
  if (button1==0 && prevbutton1==1){
    button1_latch++;
    button1_latch%=2;
  }
   if (button2==0 && prevbutton2==1){
    button2_latch++;
    button2_latch%=2;
  }
   if (button3==0 && prevbutton3==1){
    button3_latch++;
    button3_latch%=2;
  }
   if (button4==0 && prevbutton4==1){
    button4_latch++;
    button4_latch%=2;
  }
  if (difference(cycle_pitch, pot1) < 256){              // sound synth
     cycle_pitch=pot1;
     cycle_man=(map(cycle_pitch ,0,1024,0,17));
    }  
/*  if (difference(bpm_pitch, pot1) < 128){          // BPM pitch
    bpm_pitch=pot1;
    bpm=(map(bpm_pitch ,0,1024,5,162));
    } */
  if (difference(pitch1, pitch0) < 16){       //   CHANNEL_1 pitch
    pitch1=pitch0;
    }


  prevbutton1=button1;
  prevbutton2=button2;
  prevbutton3=button3;
  prevbutton4=button4;
 
  gate0=button1_latch;
  gate1=button2_latch;
  gate2=button3_latch;
  gate3=button4_latch;
 }
 
 if (mode_latch==1){  // Song select, pitch CANNEL_2:, pitch BPM  mode 
   if (button1==0){
     play_track_now=1;
     nBeat=0;
     pitch1=0;
     pitch2=0;
   }
   if (button2==0){
     play_track_now=2;
     nBeat=0;
     pitch1=0;
     pitch2=0;
   }
   if (button3==0){
     play_track_now=3;
     nBeat=0;
     pitch1=0;
     pitch2=0;
   }
   if (button4==0){
     play_track_now=4;
     nBeat=0;
     pitch1=0;
     pitch2=0;
   }
    if (difference(bpm_pitch, pot1) < 128){          // BPM pitch
    bpm_pitch=pot1;
    bpm=(map(bpm_pitch ,0,1024,1,180));
    }
 /* if (difference(cycle_pitch, pot1) < 128){              // sound synth
     cycle_pitch=pot1;
     cycle_man=(map(cycle_pitch ,0,1024,0,17));
    }  */
   if (difference(pitch2, pitch0) < 16){        //   CHANNEL_2 pitch
    pitch2=pitch0;
    }  
   
 }    
  
}

void LED() {
 if (mode_latch==0){
   digitalWrite(LED_5,LOW);
 }
 
 if (mode_latch==1){
  
   if (bpm_latch==3){
     digitalWrite(LED_5,HIGH);
   }
   else{
     digitalWrite(LED_5,LOW);
   }
      switch(play_track_now)
      {
        case 1:
          digitalWrite(CHANNEL0_LED,HIGH);
          digitalWrite(CHANNEL1_LED,LOW);
          digitalWrite(CHANNEL2_LED,LOW);
          digitalWrite(CHANNEL3_LED,LOW);
          break;      
        case 2:
          digitalWrite(CHANNEL0_LED,LOW);
          digitalWrite(CHANNEL1_LED,HIGH);
          digitalWrite(CHANNEL2_LED,LOW);
          digitalWrite(CHANNEL3_LED,LOW);
          break;      
        case 3:
          digitalWrite(CHANNEL0_LED,LOW);
          digitalWrite(CHANNEL1_LED,LOW);
          digitalWrite(CHANNEL2_LED,HIGH);
          digitalWrite(CHANNEL3_LED,LOW);
          break;      
        case 4:
          digitalWrite(CHANNEL0_LED,LOW);
          digitalWrite(CHANNEL1_LED,LOW);
          digitalWrite(CHANNEL2_LED,LOW);
          digitalWrite(CHANNEL3_LED,HIGH);
          break;
      }   
 }
}    


// *************************************
//              UTILITY
// *************************************

// a handy DIFFERENCE FUNCTION
int difference(int i, int j)
{
  int k = i - j;
  if (k < 0)
    k = j - i;
  return k;
}
