#ifndef ILLUTRONB
#define ILLUTRONB

//////////////////////////////////////////////////////////////////////////////////////////////////////
//
// IllutronB Wave Table Synthesizer Based on the original Illutron synthesiser work of Nikolaj Mobius
//
// Refactored and optimised and documented by Duane B aka RCArduino
// 
// Differences from original Illutron -
//
// The synthesisor is now encapsulated in two classes - CIllutronB and a nested class CIllutronB::CVoice
//
// The voice class contains the parameters of each of the musical channels/tracks/voices - if you are
// not of a musical background you can think of each voice as a separate instrument. The voice class 
// also performs the calculations required to generate the sound for each voice/instrument when it is asked by the containing
// CIllutronB class
//
// The CIllutronB class implements the main synthesisor functions which are - 
//
// 1) Containing the voices that make up the synth sound
// 2) Asking the voices to calculate thier output whenever the synth needs to update the sound - so thats 8,000 times a second !
// 3) Performing additional calculations on the voice outputs to generate the sound - scaling them so that they can share one 8 bit channel
// 4) Generating the output - its incedible, but the entire sound of the synth including the four independent channels is being output through PWM on one 8 bit timer - digital pin 6
// 5) Providing a tempo - the user can update the Beats per minute at any time by calling - setBPM - the pitch will not change, just the speed
//
// Note that a squencer is not currently provided within CIllutronB - it would be a logical development to provide an additional sequencer class
// It would also be logical to provide a class for storing and retreiving voices. But lets get creating first.
//
//////////////////////////////////////////////////////////////////////////////////////////////////////


#include <arduino.h>

// Do not change any of the following defines - if you can think of something you would like to try by doing so, dont.
// Request the function instead and we will find a sustainable way of adding it.

#define CHANNEL_MAX 4
#define SAMPLE_RATE 16000.0
#define UPDATE_RATE 8000
#define TIMER1_MAX 65535
#define TIMER1_FREQUENCY 2000000
#define ENVELOPE_DIVIDER 4           // This is similar to a prescaler, we do not update the envelope every cycle we do it ever cycle/ENVELOPE_DIVIDER
#define MODULATION_PITCH_DIVIDER 800 // This is the same concept as above, we update the modulation pitch every cycle/MODULATION_PITCH_DIVIDER

// using the following you can make your own defines i.e. in your sketch #define BASS CHANNEL_0
#define CHANNEL_0 0
#define CHANNEL_1 1
#define CHANNEL_2 2
#define CHANNEL_3 3



/////////////////////////////////////////////////////////////////////////////////////////////
//
// The main CIllutronB class, contains the definition of the CIllutronB::CVoice class as well
//
// See IllutronB.cpp for function descriptions
//
// Data descriptions below - 
//
/////////////////////////////////////////////////////////////////////////////////////////////
class CIllutronB
{
public:
  // nothing to do in the constructor - all member variables and functions are static
  CIllutronB(){};
  
  // setup and start the timers
  static void initSynth();
  
  // simple counters that can be used outside CIllutronB for sequencing
  // beatComplete will return true if a new beat has been completed
  static void setBPM(uint8_t sBPM);
  static unsigned char beatComplete();

  // Timer interrupt for output compare register A on timer 1
  static void OCR1A_ISR() __attribute__((always_inline)); 

  // Forward declaration of the CIllutronB::CVoice class
  class CVoice;
  // An array holding the 4 CIllutronB::CVoice objects.
  static CVoice m_Voices[CHANNEL_MAX];

protected:
  
  static volatile unsigned int m_unBPMCounterStart;   // m_unBPMCounter counts down from m_unBPMCounterStart
  static volatile unsigned int m_unBPMCounter;        //- on zero sets m_sBeat to indicate a beat at the required BPM has completed and starts the next count down fom m_unBPMCounterStart
  static volatile unsigned char m_sBeatComplete;      //- Flags that a beat is complete, can be ignored or used by user code to trigger a new beat automatically - accessed through beatComplete function
  static volatile unsigned char m_sEnvelopeDivider;   //- We update the envelope every fourth ENVELOPE_DIVIDER, this counts down from ENVELOPE_DIVIDER to 0 and is used to update the envelope at 0 before staring another countdown from ENVELOPE_DIVIDER
  static volatile unsigned int m_unEnvelopePitchModulationDivider; // We update envelope pitch modulations every MODULATION_PITCH_DIVIDER samples, similar to above.
};

// The voices are a bit like individual instruments with thier own sound characteristics
// In the demo you can hear drum, bass, chord and percussion for example.
// Each of these is being generated by one of the four voices.
// 
// The CIllutronB::CVoice class records the voice configuration which determines the sound it makes
// Its actually very powerful, the full range of sounds in the demo are all coming from the same Voice class
// just with four different configurations.
//
// The voice class also contains functions which perform the calculations required to generate sound from an individual voice
// The CIllutronB then mixes the sounds together to produce the output.
class CIllutronB::CVoice
{
public:
  CVoice();
  
  void setWave(unsigned int waveData);
  void setEnvelope(unsigned int envelopeData);
    
  void setup(unsigned int waveform, float pitch, unsigned int envelope, float length, unsigned int mod);
  
  // Three ways to play (trigger) a note - 
  
  // play a midi note using the midi note number - 
  void triggerMidi(unsigned char note);
  
  // play the note using whatever pith it was last played at (probably the one set initially in setup)
  void trigger();
  
  // play a pitch based on the pitches defined in the pitches.h header file supplied with Arduino Tone Examples
  void triggerPitch(uint16_t sPitch);

// These are used by the CIllutronB class and should really be protected ->
// They essentially do the maths required to generate the output for the voice
  signed char getSample(uint8_t,uint8_t) __attribute__((always_inline)); // get the output value for this voice, the systh will mix this with the outputs for the other voices to generate the output sound

// I am not convinced that the maths or even the approach is right to midi pitch generation
// so will confirm and or revise/remove this function
  unsigned int getFrequencyFromMidiNoteNumber(unsigned char note);

  // added to support visualisation
  unsigned char getAmplitude();
  
protected:
  // All wave table synths work in the same way - the synth cycles through an array of values
  // which represent a waveform - the faster the cycle, the higher the frequency
  
  // See - todo link - for plots of the CIllutronB waveform arrays

  // Waveform related parameters - 
  volatile unsigned int m_unWaveTableStart;      // To assign a wavetable to a voice, all we do is point the m_unWaveTableStart member of the voice to the address of the wave table array in memory
  volatile unsigned int m_unWavePhaseAccumulator; // The WaveTablePhaseAccumulator sound complicated because it is based on established wave table terminology - in reality is just an index into the array pointed to by m_unWaveTableStart
  volatile unsigned int m_unWavePhaseIncrement;   // Again we are following established wave table synth terminology - in simpler terms, this is just added to the wave phase accumulator each cycle
                                                  // a low value means we step through the wave table slowly producing a low frequency bass sound, a high value means we step through more quickly producing
                                                  // a high frequency treble sound.

  volatile unsigned int m_unPitch;                // This is the original pitch assigned to the sound - the synth includes some capabilities to bend a note away from
                                                  // its original pitch over the duration of the note - we use this to record the original note pitch
                                                  // Duane B TODO - I dont think pitch is an accurate description of the variables nature and should revisit this.

  // Wave tables on thier own produce a sound which is not that interesting, 
  // in order to produce musical content they are always combined with an envelope.
  //
  // The envelope controls how the sound develops over time, for example - 
  //    A drum hit has an explosive beginning and decays away quickly
  //    A bowed violin string sound begins and ends more slowly.
  // By combining the wavetable with an envelope table we are able to create sounds which are form more
  // engaging and can reproduce the sounds of specific instruments.
  //
  // See TODO Link for an example of wavetables combined with envelopes  
  
  // The Envelope is very similar to the wave table in that its an array stored in memory, we assign an envelope to a voice
  // by pointing the m_unEnvelopeTableStart member of the CVoice class to the start of the envelope in memory.
  volatile unsigned int m_unEnvelopeTableStart;          // The start of the array representing the envelope
  volatile unsigned int m_unEnvelopePhaseAccumulator;    // The current position in the envelope - Note - unlike the wave form, we only cycle through the envelope once, 
                                                         // it describes the life (volume really) of a note from start to finish.
  volatile unsigned int m_unEnvelopePhaseIncrement;      // This controls how fast we move through the envelope table, high values will be fast giving an abrupt note like a drum or percussion
                                                         // lower values will give a prolonged note.
                                                         
  volatile unsigned char m_sAmplitude;                   // This records the most recent value read from the wavetable - its a more efficient than reading and calculating each time.
                                                         // also we do not calculate the amplitude continually, there is a divider - see m_sEnvelopeDivider.
                                                         // m_sAmplitude will always have a cache of the most recent value
                                                         
                                                         // It also have a secondary use, its effectively the current power level of the channel and is ideal for driving
                                                         // a visualised - for example PWM of an LED for each channel
                                                         // or use a shift register to drive a set of LEDs for each channel
                                                         
                                                         // You can build a night club in a box !

  volatile int m_nEnvelopePitchModulation;               // The allows a note to increase or decrease in pitch as its played, for instance a bass sound that drops as it decays
};

#endif
