#include <FatReader.h>
#include <SdReader.h>
#include <avr/pgmspace.h>
#include <WaveUtil.h>
#include <WaveHC.h>



SdReader card;    // This object holds the information for the card
FatVolume vol;    // This holds the information for the partition on the card
FatReader root;   // This holds the information for the filesystem on the card
FatReader f;      // This holds the information for the file we're playing

uint8_t dirLevel; // indent level for file/dir names    (for prettyprinting)
dir_t dirBuf;     // buffer for directory reads



WaveHC wave;      // This is the only wave (audio) object, since we will only play one at a time



#define DEBOUNCE 100  // button debouncer



// system states

#define DEMO        0  // cycle through the lights
#define STAGING     1  // light top bar, blink second bar, others off
// turn on red, green, yellows for breaking beam of finish, trap, start, prestage

#define LANE1STAGED 2  // lane 1 staged for 3 seconds, stop it's blinking
#define LANE2STAGED 3  // lane 2 staged for 3 seconds, stop it's blinking
#define BOTHSTAGED  4  // both racers staged for 3 seconds, no blinking
#define RUNNING     5  // watching for the first racer to cross the finish line
#define LANE1WON    6  // Lane 1 crossed finish line first
#define LANE2WON    7  // Lane 2 crossed finish line first
#define LANE1FAULT  8  // Lane 1 crossed the start line before green
#define LANE2FAULT  9  // Lane 2 crossed the start line before green
#define BOTHFAULT   10 // Both lanes crossed the start line before green
#define LANE1DISQ   11 // 5 minutes have elapsed and lane 1 is DQ'd
#define LANE2DISQ   12 // 5 minutes have elapsed and lane 2 is DQ'd
#define TIED        13 // Both racers crossed finish line at the same moment as far as we can tell



// Inputs

#define lane1StagingEye    A11  // Arduino analog input pin A11 = Lane 1 staging photo-eye
#define lane1StartingEye   A10  // Arduino analog input pin A10 = Lane 1 starting line photo-eye
#define lane1SpeedTrapEye  A9   // Arduino analog input pin A9  = Lane 1 speed trap photo-eye
#define lane1FinishLineEye A8   // Arduino analog input pin A8  = Lane 1 finish line photo-eye

#define lane2StagingEye    A15  // Arduino analog input pin A15 = Lane 2 staging photo-eye
#define lane2StartingEye   A14  // Arduino analog input pin A14 = Lane 2 starting line photo-eye
#define lane2SpeedTrapEye  A13  // Arduino analog input pin A13 = Lane 2 speed trap photo-eye
#define lane2FinishLineEye A12  // Arduino analog input pin A12 = Lane 2 finish line photo-eye



#define PRINTSTATS Serial.print(raceStartTime); Serial.print(","); Serial.print(lane1StartTime); Serial.print(","); Serial.print(lane1TrapTime); Serial.print(","); Serial.print(lane1FinishTime); Serial.print(","); Serial.print(lane2StartTime); Serial.print(","); Serial.print(lane2TrapTime); Serial.print(","); Serial.println(lane2FinishTime);



const int lane1PreStageLight      = 22;  // Arduino digital output on pin 22 = Lane 1 pre-stage yellow light pair
const int lane1StageLight         = 24;  // Arduino digital output on pin 24 = Lane 1 stage yellow light pair
const int lane1Count3Light        = 26;  // Arduino digital output on pin 26 = Lane 1 count 3 yellow light
const int lane1Count2Light        = 28;  // Arduino digital output on pin 28 = Lane 1 count 2 yellow light
const int lane1Count1Light        = 30;  // Arduino digital output on pin 30 = Lane 1 count 1 yellow light
const int lane1StartGreenLight    = 32;  // Arduino digital output on pin 32 = Lane 1 start  green light
const int lane1FalseStartRedLight = 34;  // Arduino digital output on pin 34 = Lane 1 false-start red light
const int lane1WINdicator     = 36;  // Arduino digital output on pin 36 = Lane 1 WINdicatiion


const int lane2PreStageLight      = 23;  // Arduino digital output on pin 23 = Lane 2 pre-stage yellow light pair
const int lane2StageLight         = 25;  // Arduino digital output on pin 25 = Lane 2 stage yellow light pair
const int lane2Count3Light        = 27;  // Arduino digital output on pin 27 = Lane 2 count 3 yellow light
const int lane2Count2Light        = 29;  // Arduino digital output on pin 29 = Lane 2 count 2 yellow light
const int lane2Count1Light        = 31;  // Arduino digital output on pin 31 = Lane 2 count 1 yellow light
const int lane2StartGreenLight    = 33;  // Arduino digital output on pin 33 = Lane 2 start  green light
const int lane2FalseStartRedLight = 35;  // Arduino digital output on pin 35 = Lane 2 false-start red light
const int lane2WINdicator         = 37;  // Arduino digital output on pin 37 = Lane 2 WINdicatiion

const int startingCamera          = 38;  // Arudino Digital Output on pin 38 = Start camera trigger
const int finishCamera            = 39;  // Arudino Digital Output on pin 39 = Finish line Camera


//const int startButton = 8;             // Arduino digital input on pin 8 = Race Controller's start countdown button

const int startButton = 45;              // Arduino digital input on pin 43 = Race Controller's start countdown button





int state = DEMO;        // master variable for the state machine
boolean lane1StageState = LOW;
boolean lane2StageState = LOW;
boolean blinkState = LOW;
boolean fullResultsNotYetPrinted = HIGH;

boolean lane1Count2State = LOW;
boolean lane2Count2State = LOW;
boolean lane1Count1State = LOW;
boolean lane2Count1State = LOW;
boolean lane1StartGreenState = LOW;
boolean lane2StartGreenState = LOW;
boolean lane1FalseStartRedState = LOW;
boolean lane2FalseStartRedState = LOW;
boolean raceStarted = LOW;

boolean count3State = LOW;
boolean count2State = LOW;
boolean count1State = LOW;

unsigned long lastTime = 0;
unsigned long prevTime = 0;
unsigned long lastReportTime = 0;
unsigned long currentMillis = 0;
unsigned long raceStartTime = 0;
unsigned long lane1StartTime = 0;
unsigned long lane2StartTime = 0;
unsigned long lane1TrapTime = 0;
unsigned long lane2TrapTime = 0;
unsigned long lane1FinishTime = 0;
unsigned long lane2FinishTime = 0;
unsigned long lastTimeLane1Unstaged = 0;
unsigned long lastTimeLane2Unstaged = 0;
unsigned long lastTimeStaged = 0;
unsigned long timeCountdownStarted = 0;
unsigned long timeToStopPowerToLane1WINdicator = 0;
unsigned long timeToStopPowerToLane2WINdicator = 0;

double elapsedTime = 0.0;
double trapTime = 0.0;
double lagTime = 0.0;

boolean valLane1Staged;
boolean valLane2Staged;
boolean valLane1Started;
boolean valLane2Started;
boolean valLane1Trapped;
boolean valLane2Trapped;
boolean valLane1Finished;
boolean valLane2Finished;
boolean valLane1HasFinished;
boolean valLane2HasFinished;
boolean valLane1Faulted;
boolean valLane2Faulted;
boolean valLane1Fault;
boolean valLane2Fault;

boolean valLane1Cleared = LOW;
boolean valLane2Cleared = LOW;

boolean valPressedStartButton;
boolean valStartInitiated = LOW;

boolean readyToPlayFanfare = LOW;

long stageTime1 = 0;
long stageTime2 = 0;
long stopTime1 = 0;
long stopTime2 = 0;

void LightsOut();

boolean sdCardWorking = LOW;

//* SD Card disable

// this handy function will return the number of bytes currently free in RAM, great for debugging!
int freeRam(void)
{
  extern int  __bss_end;
  extern int  *__brkval;
  int free_memory;
  if ((int)__brkval == 0) {
    free_memory = ((int)&free_memory) - ((int)&__bss_end);
  }
  else {
    free_memory = ((int)&free_memory) - ((int)__brkval);
  }
  return free_memory;
}

void sdErrorCheck(void)
{
  if (!card.errorCode()) return;
  putstring("\n\rSD I/O error: ");
  Serial.print(card.errorCode(), HEX);
  putstring(", ");
  Serial.println(card.errorData(), HEX);
  while (1);
}

// Plays a full file from beginning to end with no pause.
void playcomplete(char *name) {
  // call our helper to find and play this name
  Serial.print("Attempting to play ");
  Serial.println(name);
  playfile(name);
  while (wave.isplaying) {
    // do nothing while its playing
  }
  // now its done playing
}

void playfile(char *name) {
  // see if the wave object is currently doing something
  if (wave.isplaying) {// already playing something, so stop it!
    wave.stop(); // stop it
  }
  // look in the root directory and open the file
  if (!f.open(root, name)) {
    putstring("Couldn't open file "); Serial.print(name); return;
  }
  // OK read the file and turn it into a wave object
  if (!wave.create(f)) {
    putstring_nl("Not a valid WAV"); return;
  }

  // ok time to play! start playback
  wave.play();
}

// the setup routine runs once when you press reset:
void setup() {
  currentMillis = millis();
  lastReportTime = currentMillis;
  valStartInitiated = LOW;
  // initialize the digital pins as outputs.
  pinMode(lane1PreStageLight, OUTPUT);
  pinMode(lane1StageLight, OUTPUT);
  pinMode(lane1Count3Light, OUTPUT);
  pinMode(lane1Count2Light, OUTPUT);
  pinMode(lane1Count1Light, OUTPUT);
  pinMode(lane1StartGreenLight, OUTPUT);
  pinMode(lane1FalseStartRedLight, OUTPUT);
  pinMode(lane1WINdicator, OUTPUT);

  pinMode(lane2PreStageLight, OUTPUT);
  pinMode(lane2StageLight, OUTPUT);
  pinMode(lane2Count3Light, OUTPUT);
  pinMode(lane2Count2Light, OUTPUT);
  pinMode(lane2Count1Light, OUTPUT);
  pinMode(lane2StartGreenLight, OUTPUT);
  pinMode(lane2FalseStartRedLight, OUTPUT);
  pinMode(lane2WINdicator, OUTPUT);

  pinMode(   86,    INPUT);   // set pin to input
  pinMode(     lane1StagingEye,    INPUT);   // set pin to input
  digitalWrite(lane1StagingEye,    HIGH);    // turn on pullup resistors
  pinMode(     lane1StartingEye,   INPUT);   // set pin to input
  digitalWrite(lane1StartingEye,   HIGH);    // turn on pullup resistors
  pinMode(     lane1SpeedTrapEye,  INPUT);   // set pin to input lane1SpeedTrapEye
  digitalWrite(lane1SpeedTrapEye,  HIGH);    // turn on pullup resistors
  pinMode(     lane1FinishLineEye, INPUT);   // set pin to input
  digitalWrite(lane1FinishLineEye, HIGH);    // turn on pullup resistors

  pinMode(     lane2StagingEye,    INPUT);   // set pin to input
  digitalWrite(lane2StagingEye,    HIGH);    // turn on pullup resistors
  pinMode(     lane2StartingEye,   INPUT);   // set pin to input
  digitalWrite(lane2StartingEye,   HIGH);    // turn on pullup resistors
  pinMode(     lane2SpeedTrapEye,  INPUT);   // set pin to input
  digitalWrite(lane2SpeedTrapEye,  HIGH);    // turn on pullup resistors
  pinMode(     lane2FinishLineEye, INPUT);   // set pin to input
  digitalWrite(lane2FinishLineEye, HIGH);    // turn on pullup resistors


  pinMode(startButton, INPUT_PULLUP);   // set pin to input
  LightsOut();

  // set up serial port
  Serial.begin(9600);
  Serial1.begin(9600);
  Serial2.begin(9600);
  putstring_nl("WaveHC with 6 buttons");

  putstring("Free RAM: ");       // This can help with debugging, running out of RAM is bad
  Serial.println(freeRam());      // if this is under 150 bytes it may spell trouble!

  // Set the output pins for the DAC control. This pins are defined in the library
  pinMode(2, OUTPUT);
  pinMode(3, OUTPUT);
  pinMode(4, OUTPUT);
  pinMode(5, OUTPUT);

  // pin13 LED
  pinMode(13, OUTPUT);
  initSD();
}

/*
 * list recursively - possible stack overflow if subdirectories too nested
 */
void lsR(FatReader &d)
{
  int8_t r;                     // indicates the level of recursion
  while ((r = d.readDir(dirBuf)) > 0) {     // read the next file in the directory
    // skip subdirs . and ..
    if (dirBuf.name[0] == '.')
      continue;

    for (uint8_t i = 0; i < dirLevel; i++)
      Serial.print(' ');         // this is for prettyprinting, put spaces in front
    printEntryName(dirBuf);        // print the name of the file we just found
    Serial.println();              // and a new line

    if (DIR_IS_SUBDIR(dirBuf)) {   // we will recurse on any direcory
      FatReader s;               // make a new directory object to hold information
      dirLevel += 2;             // indent 2 spaces for future prints
      if (s.open(vol, dirBuf))
        lsR(s);                // list all the files in this directory now!
      dirLevel -= 2;             // remove the extra indentation
    }
  }
  sdErrorCheck();                    // are we doign OK?
}

void initSD()
{
  //  if (!card.init(true)) { //play with 4 MHz spi if 8MHz isn't working for you
  if (!card.init()) {         //play with 8 MHz spi (default faster!)
    putstring_nl("Card init. failed!");  // Something went wrong, lets print out why
    sdCardWorking = LOW;
  }
  else
  {
    sdCardWorking = HIGH;
    // enable optimize read - some cards may timeout. Disable if you're having problems
    card.partialBlockRead(true);

    // Now we will look for a FAT partition!
    uint8_t part;
    for (part = 0; part < 5; part++) { // we have up to 5 slots to look in
      if (vol.init(card, part))
        break;                     // we found one, lets bail
    }
    if (part == 5) {                   // if we ended up not finding one  :(
      putstring_nl("No valid FAT partition!");
      sdErrorCheck();      // Something went wrong, lets print out why
      while (1);                     // then 'halt' - do nothing!
    }

    // Lets tell the user about what we found
    putstring("Using partition ");
    Serial.print(part, DEC);
    putstring(", type is FAT");
    Serial.println(vol.fatType(), DEC);     // FAT16 or FAT32?

    // Try to open the root directory
    if (!root.openRoot(vol)) {
      putstring_nl("Can't open root dir!"); // Something went wrong,
      while (1);                            // then 'halt' - do nothing!
    }

    // Whew! We got past the tough parts.
    putstring_nl("Files found:");
    dirLevel = 0;
    // Print out all of the files in all the directories.
    lsR(root);

    putstring_nl("Playing DO.WAV as a test.");

    if (sdCardWorking) {
      playcomplete("DO.WAV");
    }

    putstring_nl("Ready!");
  }
}

void LightsOut() {
  //reset lights
  digitalWrite(lane1PreStageLight, LOW);
  digitalWrite(lane1StageLight, LOW);
  digitalWrite(lane1Count3Light, LOW);
  digitalWrite(lane1Count2Light, LOW);
  digitalWrite(lane1Count1Light, LOW);
  digitalWrite(lane1StartGreenLight, LOW);
  digitalWrite(lane1FalseStartRedLight, LOW);

  digitalWrite(lane2PreStageLight, LOW);
  digitalWrite(lane2StageLight, LOW);
  digitalWrite(lane2Count3Light, LOW);
  digitalWrite(lane2Count2Light, LOW);
  digitalWrite(lane2Count1Light, LOW);
  digitalWrite(lane2StartGreenLight, LOW);
  digitalWrite(lane2FalseStartRedLight, LOW);
}

void Staging() {
  currentMillis = millis();
  if (currentMillis - lastTime >= 1000)
  {
    lastTime = currentMillis;
    lane1StageState = !lane1StageState;
    lane2StageState = !lane2StageState;
    if (LANE1STAGED == state)
      digitalWrite(lane1PreStageLight, HIGH);
    else
      digitalWrite(lane1PreStageLight, lane1StageState);
    if (LANE2STAGED == state)
      digitalWrite(lane2PreStageLight, HIGH);
    else
      digitalWrite(lane2PreStageLight, lane2StageState);
  }

  valLane1Staged = !digitalRead(lane1StagingEye);       // read the input pin
  valLane2Staged = !digitalRead(lane2StagingEye);       // read the input pin
  valLane1Started = !digitalRead(lane1StartingEye);     // read the input pin
  valLane2Started = !digitalRead(lane2StartingEye);     // read the input pin
  valLane1Trapped = !digitalRead(lane1SpeedTrapEye);   // read the input pin
  valLane2Trapped = !digitalRead(lane2SpeedTrapEye);    // read the input pin
  valLane1Finished = !digitalRead(lane1FinishLineEye); // read the input pin
  valLane2Finished = !digitalRead(lane2FinishLineEye);  // read the input pin
  valPressedStartButton = !digitalRead(startButton);    // read the input pin

  if (valLane1Finished) valLane1HasFinished = HIGH;
  if (valLane2Finished) valLane2HasFinished = HIGH;
  if (valPressedStartButton) valStartInitiated = HIGH;

  //print out the value of the pushbutton
  digitalWrite(13, valPressedStartButton);

  if (!valLane1Staged || valLane1Started) lastTimeLane1Unstaged = millis();
  if (!valLane2Staged || valLane2Started) lastTimeLane2Unstaged = millis();
  if ((!valLane1Staged || valLane1Started) && BOTHSTAGED == state) state = LANE2STAGED;
  if ((!valLane2Staged || valLane2Started) && BOTHSTAGED == state) state = LANE1STAGED;
  if ((!valLane1Staged || valLane1Started) && LANE1STAGED == state) state = STAGING;
  if ((!valLane2Staged || valLane2Started) && LANE2STAGED == state) state = STAGING;
  if (millis() > lastTimeLane1Unstaged + 3000) state = LANE1STAGED;
  if (millis() > lastTimeLane2Unstaged + 3000) state = LANE2STAGED;
  if ((millis() > lastTimeLane1Unstaged + 3000) && (millis() > lastTimeLane2Unstaged + 3000))
        {
    Serial1.println(-10);
    Serial2.println(-10);
                state = BOTHSTAGED;
  }
        if (valStartInitiated) state = BOTHSTAGED;
  digitalWrite(lane1StageLight, valLane1Staged);
  digitalWrite(lane2StageLight, valLane2Staged);
  digitalWrite(lane1Count1Light, valLane1Started);
  digitalWrite(lane2Count1Light, valLane2Started);
  digitalWrite(lane1StartGreenLight, valLane1Trapped);
  digitalWrite(lane2StartGreenLight, valLane2Trapped);
  digitalWrite(lane1FalseStartRedLight, valLane1Finished);
  digitalWrite(lane2FalseStartRedLight, valLane2Finished);
}

void BothStaged() {
  currentMillis = millis();
  if (currentMillis - lastTime >= 1000)
  {
    lastTime = currentMillis;
    Serial1.println(-10);
    Serial2.println(-10);
  }

  digitalWrite(lane1PreStageLight, HIGH);
  digitalWrite(lane2PreStageLight, HIGH);
  digitalWrite(lane1StageLight, HIGH);
  digitalWrite(lane2StageLight, HIGH);
  digitalWrite(lane1Count2Light, LOW);
  digitalWrite(lane2Count2Light, LOW);
  digitalWrite(lane1Count1Light, LOW);
  digitalWrite(lane2Count1Light, LOW);
  digitalWrite(lane1StartGreenLight, LOW);
  digitalWrite(lane2StartGreenLight, LOW);
  digitalWrite(lane1FalseStartRedLight, LOW);
  digitalWrite(lane2FalseStartRedLight, LOW);

  valLane1Staged = !digitalRead(lane1StagingEye);     // read the input pin
  valLane2Staged = !digitalRead(lane2StagingEye);     // read the input pin
  valLane1Started = !digitalRead(lane1StartingEye);   // read the input pin
  valLane2Started = !digitalRead(lane2StartingEye);   // read the input pin
  valLane1Trapped = !digitalRead(lane1SpeedTrapEye);    // read the input pin
  valLane2Trapped = !digitalRead(lane2SpeedTrapEye);    // read the input pin
  valLane1Finished = !digitalRead(lane1FinishLineEye);  // read the input pin
  valLane2Finished = !digitalRead(lane2FinishLineEye);  // read the input pin

  if (!valLane1Staged && BOTHSTAGED == state) state = LANE2STAGED;
  if (!valLane2Staged && BOTHSTAGED == state) state = LANE1STAGED;
  if (!valLane1Staged && LANE1STAGED == state) state = STAGING;
  if (!valLane2Staged && LANE2STAGED == state) state = STAGING;

  valPressedStartButton = !digitalRead(startButton);  // read the input pin

  if (valPressedStartButton) valStartInitiated = HIGH;

  digitalWrite(13, valPressedStartButton);      //display the value of the pushbutton

  // wait for race controller to press the READY button
  // for backup, just count off 60000 seconds with the racers staged behind the start line
  if ((valStartInitiated && (BOTHSTAGED == state) && (!valPressedStartButton)) || ((currentMillis > lastTimeLane1Unstaged + 60000000) && (currentMillis > lastTimeLane2Unstaged + 60000000)))
  {
    if (sdCardWorking) {
      playcomplete("PTDR.WAV");
      readyToPlayFanfare = LOW;
    }
    state = RUNNING;
    currentMillis = millis();
    timeCountdownStarted = currentMillis;
    valLane1Faulted = LOW;
    valLane2Faulted = LOW;
    valStartInitiated = LOW;
    raceStarted = LOW;
    count3State = LOW;
    count2State = LOW;
    count1State = LOW;
  }
}

void CountDownWatchForFinish() {
  valLane2Fault = !digitalRead(lane2StartingEye);   // read the input pin
  valLane1Started = !digitalRead(lane1StartingEye);     // read the input pin
  valLane2Started = !digitalRead(lane2StartingEye);     // read the input pin
  valLane1Trapped = !digitalRead(lane1SpeedTrapEye);   // read the input pin
  valLane2Trapped = !digitalRead(lane2SpeedTrapEye);    // read the input pin
  valLane1Finished = !digitalRead(lane1FinishLineEye); // read the input pin
  valLane2Finished = !digitalRead(lane2FinishLineEye);  // read the input pin
  valPressedStartButton = !digitalRead(startButton);    // read the input pin

  if (valLane1Trapped && (0 == lane1TrapTime))
  {
    lane1TrapTime = millis();
  }
  if (valLane1Trapped && (0 == lane2TrapTime))
  {
    lane2TrapTime = millis();
  }

  if (valLane1Started)
  {
    if (0 == lane1StartTime)
      lane1StartTime = millis();
    if (!raceStarted)
    {
            Serial1.println(-100);
      valLane1Faulted = HIGH;
      digitalWrite(lane1PreStageLight, LOW);
      digitalWrite(lane1StageLight, LOW);
      digitalWrite(lane1Count3Light, LOW);
      digitalWrite(lane1Count2Light, LOW);
      digitalWrite(lane1Count1Light, LOW);
      digitalWrite(lane1FalseStartRedLight, HIGH);
    }
  }
  if (valLane2Started)
  {
    if (0 == lane2StartTime)
      lane2StartTime = millis();
    if (!raceStarted)
    {
            Serial2.println(-100);
      valLane2Faulted = HIGH;
      digitalWrite(lane2PreStageLight, LOW);
      digitalWrite(lane2StageLight, LOW);
      digitalWrite(lane2Count3Light, LOW);
      digitalWrite(lane2Count2Light, LOW);
      digitalWrite(lane2Count1Light, LOW);
      digitalWrite(lane2FalseStartRedLight, HIGH);
    }
  }
  currentMillis = millis();
  if (!count3State && (currentMillis >= timeCountdownStarted + 500))
  {
    if (!valLane1Faulted) digitalWrite(lane1Count3Light, HIGH);
    if (!valLane2Faulted) digitalWrite(lane2Count3Light, HIGH);
//    Serial1.println(3);
//    Serial2.println(3);
    count3State = HIGH;
    Serial.print("\tCounting 3 with currentMillis: ");
    Serial.println(currentMillis);
  }
  if (!count2State && (currentMillis >= timeCountdownStarted + 1000))
  {
    if (!valLane1Faulted) digitalWrite(lane1Count2Light, HIGH);
    if (!valLane2Faulted) digitalWrite(lane2Count2Light, HIGH);
//    Serial1.println(2);
//    Serial2.println(2);
    count2State = HIGH;
    Serial.print("\tCounting 2 with currentMillis: ");
    Serial.println(currentMillis);
  }
  if (!count1State && (currentMillis >= timeCountdownStarted + 1500))
  {
    if (!valLane1Faulted) digitalWrite(lane1Count1Light, HIGH);
    if (!valLane2Faulted) digitalWrite(lane2Count1Light, HIGH);
//    Serial1.println(1);
//    Serial2.println(1);
    count1State = HIGH;
    Serial.print("\tCounting 1 with currentMillis: ");
    Serial.println(currentMillis);
  }
  if (!raceStarted && (currentMillis >= timeCountdownStarted + 2000))
  {
    fullResultsNotYetPrinted = HIGH;
    valLane1HasFinished = LOW;
    valLane2HasFinished = LOW;
    lane1StartTime = 0;
    lane2StartTime = 0;
    lane1TrapTime = 0;
    lane2TrapTime = 0;
    lane1FinishTime = 0;
    lane2FinishTime = 0;

    if (!valLane1Faulted)
      digitalWrite(lane1StartGreenLight, HIGH);
    if (!valLane2Faulted)
      digitalWrite(lane2StartGreenLight, HIGH);
    if (valLane1Faulted && valLane2Faulted)
    {
      state = BOTHFAULT;
      // DEBUG print // Serial.println("Race result: Both lanes faulted.");
      Serial.print("_DualFault: "); PRINTSTATS
        lastTimeStaged = millis();
    }
    // DEBUG print // Serial.println("Countdown complete with timeCountdownStarted: ");
    // DEBUG print // Serial.println(timeCountdownStarted);
    // DEBUG print // Serial.println("Countdown complete with currentMillis: ");
    // DEBUG print // Serial.println(currentMillis);

    raceStarted = HIGH;
    raceStartTime = millis();
    if (!valLane1Faulted)
            Serial1.println(-1);
    if (!valLane2Faulted)
            Serial2.println(-1);
    Serial.print("\tstarting race with raceStartTime: ");
    Serial.println(raceStartTime);

    PRINTSTATS
/*    
    Serial.print("raceStartTime: ");
    Serial.println(raceStartTime);
    Serial.print("lane1StartTime: ");
    Serial.println(lane1StartTime);
    Serial.print("lane1TrapTime: ");
    Serial.println(lane1TrapTime);
    Serial.print("lane1FinishTime: ");
    Serial.println(lane1FinishTime);
    Serial.print("lane2StartTime: ");
    Serial.println(lane2StartTime);
    Serial.print("lane2TrapTime: ");
    Serial.println(lane2TrapTime);
    Serial.print("lane2FinishTime: ");
    Serial.println(lane2FinishTime);
//*/
  }

  valLane1Finished = !digitalRead(lane1FinishLineEye);  // read the input pin
  valLane2Finished = !digitalRead(lane2FinishLineEye);  // read the input pin

  if (valLane1Finished && valLane2Finished && !valLane1Faulted && !valLane2Faulted && raceStarted)
  {
    lane1FinishTime = lane2FinishTime = millis();
    Serial1.println(lane1FinishTime - raceStartTime);
    Serial2.println(lane2FinishTime - raceStartTime);
    state = TIED;
    lastTimeStaged = millis();
    valLane1Cleared = LOW;
    valLane2Cleared = LOW;
    valStartInitiated = LOW;
    Serial.print("_TIED: "); PRINTSTATS

      // DEBUG print // Serial.println("Race result: TIED.");
      // DEBUG print // Serial.print("raceStartTime: ");
      // DEBUG print // Serial.println(raceStartTime);
      // DEBUG print // Serial.print("lane1FinishTime: ");
      // DEBUG print // Serial.println(lane1FinishTime);
      // DEBUG print // Serial.print("lane2FinishTime: ");
      // DEBUG print // Serial.println(lane2FinishTime);
      // DEBUG print // Serial.println("");
      Serial.print("\tLane 1 Elapsed Time A: ");
    elapsedTime = lane1FinishTime - raceStartTime;
    Serial.println(elapsedTime / 1000.0);
    Serial.print("\tLane 1 Trap Time B: ");
    trapTime = (lane1FinishTime - lane1TrapTime);
    Serial.println(trapTime / 1000.0);
    Serial.print("\tLane 2 Elapsed Time C: ");
    elapsedTime = lane2FinishTime - raceStartTime;
    Serial.println(elapsedTime / 1000.0);
    Serial.print("\tLane 2 Trap Time D: ");
    trapTime = (lane2FinishTime - lane2TrapTime);
    Serial.println(trapTime / 1000.0);
  }
  else if (valLane1Finished && !valLane2Finished && !valLane1Faulted && !valLane2Faulted && raceStarted)
  {
    lane1FinishTime = millis();
    Serial1.println(lane1FinishTime - raceStartTime);
    state = LANE1WON;
    digitalWrite(lane1WINdicator, HIGH);
    timeToStopPowerToLane1WINdicator = lane1FinishTime + 750;
    digitalWrite(lane2PreStageLight, LOW);
    digitalWrite(lane2StageLight, LOW);
    digitalWrite(lane2Count3Light, LOW);
    digitalWrite(lane2Count2Light, LOW);
    digitalWrite(lane2Count1Light, LOW);
    digitalWrite(lane2StartGreenLight, LOW);
    digitalWrite(lane2FalseStartRedLight, LOW);
    lastTimeStaged = millis();
    valLane1Cleared = LOW;
    valLane2Cleared = LOW;
    valStartInitiated = LOW;
    Serial.print("_Lane1Won: "); PRINTSTATS
      // DEBUG print // Serial.println("Race result: Lane 1 Won.");
      // DEBUG print // Serial.print("raceStartTime: ");
      // DEBUG print // Serial.println(raceStartTime);
      // DEBUG print // Serial.print("lane1FinishTime: ");
      // DEBUG print // Serial.println(lane1FinishTime);
      // DEBUG print // Serial.println("");
      Serial.print("\tLane 1 Elapsed Time E: ");
    elapsedTime = (lane1FinishTime - raceStartTime);
    Serial.println(elapsedTime / 1000.0);
    Serial.print("\tLane 1 Trap Time F: ");
    trapTime = (lane1FinishTime - lane1TrapTime);
    Serial.println(trapTime / 1000.0);
  }
  else if (!valLane1Finished && valLane2Finished && !valLane1Faulted && !valLane2Faulted && raceStarted)
  {
    lane2FinishTime = millis();
    Serial2.println(lane2FinishTime - raceStartTime);
    state = LANE2WON;
    digitalWrite(lane2WINdicator, HIGH);
    timeToStopPowerToLane2WINdicator = lane2FinishTime + 750;
    digitalWrite(lane1PreStageLight, LOW);
    digitalWrite(lane1StageLight, LOW);
    digitalWrite(lane1Count3Light, LOW);
    digitalWrite(lane1Count2Light, LOW);
    digitalWrite(lane1Count1Light, LOW);
    digitalWrite(lane1StartGreenLight, LOW);
    digitalWrite(lane1FalseStartRedLight, LOW);
    lastTimeStaged = millis();
    valLane1Cleared = LOW;
    valLane2Cleared = LOW;
    valStartInitiated = LOW;
    Serial.print("_Lane2Won: "); PRINTSTATS
      // DEBUG print // Serial.println("Race result: Lane 2 Won.");
      // DEBUG print // Serial.print("raceStartTime: ");
      // DEBUG print // Serial.println(raceStartTime);
      // DEBUG print // Serial.print("lane2TrapTime: ");
      // DEBUG print // Serial.println(lane2TrapTime);
      // DEBUG print // Serial.print("lane2FinishTime: ");
      // DEBUG print // Serial.println(lane2FinishTime);
      Serial.print("\tLane 2 Elapsed Time G: ");
    elapsedTime = (lane2FinishTime - raceStartTime);
    Serial.println(elapsedTime / 1000.0);
    Serial.print("\tLane 2 Trap Time H: ");
    trapTime = (lane2FinishTime - lane2TrapTime);
    Serial.println(trapTime / 1000.0);
  }
  else if (!valLane1Finished && valLane2Finished && valLane1Faulted && !valLane2Faulted && raceStarted)
  {
    lane2FinishTime = millis();
    Serial2.println(lane2FinishTime - raceStartTime);
    state = LANE2WON;
    digitalWrite(lane1PreStageLight, LOW);
    digitalWrite(lane1StageLight, LOW);
    digitalWrite(lane1Count3Light, LOW);
    digitalWrite(lane1Count2Light, LOW);
    digitalWrite(lane1Count1Light, LOW);
    digitalWrite(lane1StartGreenLight, LOW);
    digitalWrite(lane1FalseStartRedLight, LOW);
    lastTimeStaged = millis();
    valLane1Cleared = LOW;
    valLane2Cleared = LOW;
    valStartInitiated = LOW;
    Serial.print("_Lane2WonLane1Faulted: "); PRINTSTATS
      Serial.println("\tRace result: Lane 2 Won because Lane 1 jumped the start.");
    Serial.print("\traceStartTime I: ");
    Serial.println(raceStartTime);
    Serial.print("\tlane2FinishTime J: ");
    Serial.println(lane2FinishTime);
    Serial.println("");
    Serial.print("\tLane 2 Elapsed Time K: ");
    elapsedTime = (lane2FinishTime - raceStartTime);
    Serial.println(elapsedTime / 1000.0);
    Serial.print("\tLane 2 Trap Time L: ");
    trapTime = (lane2FinishTime - lane2TrapTime);
    Serial.println(trapTime / 1000.0);
  }
  else if (valLane1Finished && !valLane2Finished && !valLane1Faulted && valLane2Faulted && raceStarted)
  {
    lane1FinishTime = millis();
    Serial1.println(lane1FinishTime - raceStartTime);
    state = LANE1WON;
    digitalWrite(lane2PreStageLight, LOW);
    digitalWrite(lane2StageLight, LOW);
    digitalWrite(lane2Count3Light, LOW);
    digitalWrite(lane2Count2Light, LOW);
    digitalWrite(lane2Count1Light, LOW);
    digitalWrite(lane2StartGreenLight, LOW);
    digitalWrite(lane2FalseStartRedLight, LOW);
    lastTimeStaged = millis();
    valLane1Cleared = LOW;
    valLane2Cleared = LOW;
    valStartInitiated = LOW;
    Serial.print("_Lane1WonLane2Faulted: "); PRINTSTATS
      Serial.println("\tRace result: Lane 1 Won because Lane 2 jumped the start.");
    Serial.print("\traceStartTime M: ");
    Serial.println(raceStartTime);
    Serial.print("\tlane1FinishTime N: ");
    Serial.println(lane1FinishTime);
    Serial.println("");
    Serial.print("\tLane 1 Elapsed Time O: ");
    elapsedTime = (lane1FinishTime - raceStartTime);
    Serial.println(elapsedTime / 1000.0);
    Serial.print("\tLane 1 Trap Time P: ");
    trapTime = (lane1FinishTime - lane1TrapTime);
    Serial.println(trapTime / 1000.0);
  }
  else if (valLane1Finished && valLane2Finished && raceStarted)
  {
    lane1FinishTime = lane2FinishTime = millis();
    Serial1.println(lane1FinishTime - raceStartTime);
    Serial2.println(lane2FinishTime - raceStartTime);
    state = TIED;
    lastTimeStaged = millis();
    valLane1Cleared = LOW;
    valLane2Cleared = LOW;
    valStartInitiated = LOW;
    Serial.print("_TiedButUnsureHow: "); PRINTSTATS
      Serial.println("\tRace result: TIED for lack of finding another state to finish.");
    Serial.print("\traceStartTime Q: ");
    Serial.println(raceStartTime);
    Serial.print("\tlane1FinishTime R: ");
    Serial.println(lane1FinishTime);
    Serial.print("\tlane2FinishTime S: ");
    Serial.println(lane2FinishTime);
    Serial.println("");
    Serial.print("\tLane 1 Elapsed Time T: ");
    elapsedTime = (lane1FinishTime - raceStartTime);
    Serial.println(elapsedTime / 1000.0);
    Serial.print("\tLane 1 Trap Time U: ");
    trapTime = (lane1FinishTime - lane1TrapTime);
    Serial.println(trapTime / 1000.0);
    Serial.print("\tLane 2 Elapsed Time V: ");
    elapsedTime = (lane2FinishTime - raceStartTime);
    Serial.println(elapsedTime / 1000.0);
    Serial.print("\tLane 2 Trap Time W: ");
    trapTime = (lane2FinishTime - lane2TrapTime);
    Serial.println(trapTime / 1000.0);
  }

  if (!(RUNNING == state)) {
    Serial.print("\tLeaving CountDownWatchForFinish in state: ");
    Serial.println(state);
    Serial.println("");
  }
}


void WatchForStaging() {
  valLane1Staged = !digitalRead(lane1StagingEye);     // read the input pin
  valLane2Staged = !digitalRead(lane2StagingEye);     // read the input pin
  valLane1Finished = !digitalRead(lane1FinishLineEye); // read the input pin
  valLane2Finished = !digitalRead(lane2FinishLineEye);  // read the input pin

  if (valLane1Finished)
  {
    if (0 == lane1FinishTime)
      lane1FinishTime = millis();
    valLane1HasFinished = HIGH;
  }
  if (valLane2Finished)
  {
    if (0 == lane2FinishTime)
      lane2FinishTime = millis();
    valLane2HasFinished = HIGH;
  }

  if (!valLane1Staged) valLane1Cleared = HIGH;
  if (!valLane2Staged) valLane2Cleared = HIGH;

  currentMillis = millis();

  if (currentMillis >= timeToStopPowerToLane1WINdicator)
  {
    digitalWrite(lane1WINdicator, LOW);
  }
  if (currentMillis >= timeToStopPowerToLane2WINdicator)
  {
    digitalWrite(lane2WINdicator, LOW);
  }

  if (currentMillis - lastTime >= 2000)
  {
    lastTime = currentMillis;
    blinkState = !blinkState;
    if (TIED == state)
    {
      digitalWrite(lane1Count3Light, blinkState);
      digitalWrite(lane1Count2Light, blinkState);
      digitalWrite(lane1Count1Light, blinkState);
      digitalWrite(lane2Count3Light, blinkState);
      digitalWrite(lane2Count2Light, blinkState);
      digitalWrite(lane2Count1Light, blinkState);
    }
  }

  if (fullResultsNotYetPrinted && valLane1HasFinished && valLane2HasFinished)
  {
    if (LANE2WON == state)
            Serial1.println(lane1FinishTime - raceStartTime);
    if (LANE1WON == state)
            Serial2.println(lane2FinishTime - raceStartTime);
    Serial.print("_BothFinished: "); PRINTSTATS
    Serial.println("\tBoth Racers finished.  Overall race numbers follow:");
    Serial.print("\traceStartTime X: ");
    Serial.println(raceStartTime);
    Serial.print("\tlane1StartTime Y: ");
    Serial.println(lane1StartTime);
    Serial.print("\tlane2StartTime Z: ");
    Serial.println(lane2StartTime);
    Serial.print("\tlane1TrapTime A2: ");
    Serial.println(lane1TrapTime);
    Serial.print("\tlane2TrapTime B2: ");
    Serial.println(lane2TrapTime);
    Serial.print("\tlane1FinishTime C2: ");
    Serial.println(lane1FinishTime);
    Serial.print("\tlane2FinishTime D2: ");
    Serial.println(lane2FinishTime);
    Serial.println("");
    Serial.print("\tLane 1 Start Lag E2: ");
    lagTime = (lane1StartTime - raceStartTime);
    Serial.println(lagTime / 1000.0);
    Serial.print("\tLane 2 Start Lag F2: ");
    lagTime = (lane2StartTime - raceStartTime);
    Serial.println(lagTime / 1000.0);
    Serial.println("");
    Serial.print("\tLane 1 Elapsed Time G2: ");
    elapsedTime = (lane1FinishTime - raceStartTime);
    Serial.println(elapsedTime / 1000.0);
    Serial.print("\tLane 1 Trap Time H2: ");
    trapTime = (lane1FinishTime - lane1TrapTime);
    Serial.println(trapTime / 1000.0);
    Serial.print("\tLane 2 Elapsed Time I2: ");
    elapsedTime = (lane2FinishTime - raceStartTime);
    Serial.println(elapsedTime / 1000.0);
    Serial.print("\tLane 2 Trap Time J2: ");
    trapTime = (lane2FinishTime - lane2TrapTime);
    Serial.println(trapTime / 1000.0);
    fullResultsNotYetPrinted = LOW;

    Serial.println("");
    Serial.print("\tfinished printing full results leaving in state: ");
    Serial.println(state);

    Serial.println("");
  }

  if (valLane1Cleared && valLane2Cleared && valLane1Staged && valLane2Staged)
  {
    if (currentMillis - lastTimeStaged >= 1000)
    {
      lastTimeLane1Unstaged = millis();
      lastTimeLane2Unstaged = millis();
      LightsOut();
      state = STAGING;
    }
  }
  else
    lastTimeStaged = currentMillis;
}

// the loop routine runs over and over again forever:
void loop() {
  currentMillis = millis();
  // every 10 seconds list time:
  if (currentMillis - lastReportTime >= 1000)
  {
    lastReportTime = currentMillis;

    //if ( !(state = RUNNING) ) {
    //      Serial.print("looping in state : ");
    //      Serial.println(state);
    //}

    /*
      Serial.print("prevTime: ");
      Serial.print(prevTime);
      Serial.print("       currentMillis: ");
      Serial.println(currentMillis);
      //*/
  }
  prevTime = currentMillis;
  valPressedStartButton = !digitalRead(startButton);  // read the input pin

  //print out the value of the pushbutton
  //  Serial.println(valPressedStartButton);
  digitalWrite(13, valPressedStartButton);

  if (currentMillis >= timeToStopPowerToLane1WINdicator)
  {
    digitalWrite(lane1WINdicator, LOW);
  }
  if (currentMillis >= timeToStopPowerToLane2WINdicator)
  {
    digitalWrite(lane2WINdicator, LOW);
  }

  /*
  if (sdCardWorking && readyToPlayFanfare && !(valPressedStartButton)) {

  Serial.println("");
  Serial.print("readyToPlayFanfare: ");
  Serial.println(readyToPlayFanfare);
  playcomplete("PTDR.WAV");
  readyToPlayFanfare = LOW;
  Serial.print("readyToPlayFanfare: ");
  Serial.println(readyToPlayFanfare);
  }
  //*/

  switch (state)
  {
  case DEMO:
    //RunDemo();
    state = STAGING;
    lastTime = millis();
    lane1StageState = HIGH;
    lane2StageState = HIGH;
    lastTimeLane1Unstaged = millis();
    lastTimeLane2Unstaged = millis();
    break;
  case STAGING:
    Staging();
    break;
  case LANE1STAGED:
    Staging();
    break;
  case LANE2STAGED:
    Staging();
    break;
  case BOTHSTAGED:
    BothStaged();
    break;
  case RUNNING:
    CountDownWatchForFinish();
    break;
  case LANE1WON:
    WatchForStaging();
    break;
  case LANE2WON:
    WatchForStaging();
    break;
  case BOTHFAULT:
    WatchForStaging();
    break;
  case TIED:
    WatchForStaging();
    break;
    // TODO Add code to collect speed trap data
    // TODO Add code to detect winner and flash lights for winning lane

    //    if(millis()-RunningTime > RUNNING_TIMEOUT)
    //      {
    //      Serial.println("Running Timeout");
    //      Stop();
    //      }
  default:
    Serial.print("\tUnknown State: ");
    Serial.println(state);
    LightsOut(); // Should never get here
    delay(5000);
    state = STAGING;
    break;
  }
}

