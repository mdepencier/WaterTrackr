
 * * * * * * * * * * * * * * * * * * * * * * * *
 * Water Trackr:
 *
 * Table of Contents:
 * 29-159: Variable, Struct, File declarations
 * 161-168: Main set-up function. Commences state machine and opens error log.
 * 171-211: processState function
 * 211-end: Methods
 * 319: Functions for calculating statistics.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <fstream>
#include <math.h>
#include <time.h>
#include <ugpio/ugpio.h>
#include <iostream>
#include <string>
#include <future>

using namespace std;

//Set-Up and GPIO functions
int setup();
bool exportGPIO (int& RV, int pin);
bool setDirection (int& RV, int direction, int pin);
bool notExported (int& RQ, int pin);
void configRates();
void configQuota();
void printHelp();

/* WaterTracker Software Explanation
The first time a user clicks a switch, it will begin recording the amount of water used every 12 seconds in a waterUsage struct.
At the same time it will add it to the first interval of the histogram.
Then it will add the histogram to a statstics struct which will be used to calculate stats that will finally be logged onto a USB.
When the user closes the switch it would do one final log with data over the entire session.
Then the program adds the statistics and waterUsage structs in that one session to the array in the lifeTime struct.
*/

// Struct to track amount of water used in one session total
struct WaterUsage {
  float tapWater;
  float showerWater;
  float currentWater;
  float quota; // in L
};

// Histogram struct tracks amount of water used in set time interval, default is 6 intervals, logs data once per 12 seconds in a minute
struct Histogram {
  int intervalSize;
  int intervalCounter;
  float* histogram;
};

// Struct to track statistics
struct Statistics {
  float average;
  float min;
  float max;
  Histogram waterData;
};

// Struct to track lifetime totals of water used
struct Lifetime {
  int sessionNumber;
  float totalTapWater;
  float totalShowerWater;
  Statistics* stats;
  WaterUsage* waterData;
};

// GPIO READ VARIABLES
int rq1, rv1, rq2, rv2, rq3, rv3, rqConfig, rvConfig, rqRed, rvRed, rqYel, rvYel, rqGre, rvGre;

// Water flow pins
const int gpio1 = 1;
const int gpio2 = 9;
const int gpio3 = 7;

//LED PINS
const int ledRed = 0;
const int ledYellow = 3;
const int ledGreen = 11;
int value1, value2, value3;

// Configuration mode pin
const int configPin = 2;
int configValue;

// Default water flow values (L/s)
float rateTapLow = 0.051;
float rateTapHigh = 0.090;
float rateShower = 0.100;

//Default total water limits
float greenLimit = 25;
float yellowLimit = 50;
float redLimit = 100;

// Program timing variables
int sleepTime = 1; // delay between pin readings (s)
int histogramEveryNseconds = 60;
int secondsCounter = 0;
int minutesCounter = 0;
int intervalCounter = 0;
const int intervalSize = 2; //size in minutes for how often you want a stat file to be made. normally, this would be 24 minutes, but for demonstrations we set it to 1 or 2
int statFileCounter = 0;

// Track lifetime amount of water
Lifetime life = {
  .sessionNumber = 1
};

//Track session water usage
WaterUsage waterUsage = {
  .tapWater = 0,
  .showerWater = 0,
  .currentWater = 0,
  .quota = 5
};

//Track session histogram water usage
Histogram waterHistogram = {
  .intervalSize = 1000,
  .intervalCounter = 0
};

// State machine states
enum STATE { ONSTART, CONFIG, RUN, COMPUTE, END };
STATE nextState = ONSTART;

// main Water loop Functions
int waterLoop(STATE& state);
int processState(STATE& state);
int config(STATE& state);
void getValues(int& value1, int& value2, int& value3);
bool stringToFloat(string input, float& value);
int writeData(Statistics stat, WaterUsage water);
int writeData(Statistics stat, WaterUsage water, Lifetime life);

// Functions for Statistics
void average(Statistics& stats);
void minimum(Statistics& stats);
void maximum(Statistics& stats);

//Functions for drawing graphs
void horizontal(int graphSize, ofstream& ofile);
float findMax(float array[], int size);
void scale(float array[], int scaleSize, int arraySize, float scaledArray[]);
void horizontalBarGraph(float array[], string axis, int arraySize, ofstream& ofile);

// Output files
ofstream errorfile;
ofstream statfile;

int main(const int argc, char** argv){
  cout << " > Starting program... " << endl;
  errorfile.open("errors.log");
  setup();
  processState(nextState);
  errorfile.close();
  return 0;
}

// Recursive State Machine
int processState(STATE& state){
  switch (state) {
    case ONSTART:
      cout << " IN ONSTART CASE" << endl;
      state = CONFIG;
      break;
    case CONFIG:
      cout << " IN CONFIG CASE" << endl;
      config(state);
      break;
    case RUN:
      cout << " IN RUN CASE" << endl;
      waterLoop(state);
      break;
    case COMPUTE:
      Statistics stats;
      stats.waterData = waterHistogram;
      average(stats);
      minimum(stats);
      maximum(stats);
      cout << "Total Tap: " << waterUsage.tapWater << endl;
      cout << "Total Shower: " << waterUsage.showerWater << endl;
      cout << "Minimum water used: " << stats.min << endl;
      cout << "Maximum water used: " << stats.max << endl;
      cout << "Average water used: " << stats.average << endl;
      cout << " > Computing statistics..." << endl;
      life.stats[life.sessionNumber] = stats;
      life.waterData[life.sessionNumber] = waterUsage;
      writeData(stats, waterUsage, life);
      life.sessionNumber++;
      waterUsage.tapWater = 0;
      waterUsage.showerWater = 0;
      state = CONFIG;
      break;
    case END:
      cout << " > ending program" << endl;
      return 0;
      break;
  }
  return processState(state);
}

bool checkNumber(char c){
  if(c == '0' || c == '1' || c == '2' || c == '3' || c == '4' || c == '5') {
    return true;
  } else if (c == '6' || c == '7' || c == '8' || c == '9') {
    return true;
  }
  return false;
}


// Recursive State Machine
int processState(STATE& state){
  switch (state) {
    case ONSTART:
      cout << " IN ONSTART CASE" << endl;
      state = CONFIG;
      break;
    case CONFIG:
      cout << " IN CONFIG CASE" << endl;
      config(state);
      break;
    case RUN:
      cout << " IN RUN CASE" << endl;
      waterLoop(state);
      break;
    case COMPUTE:
      Statistics stats;
      stats.waterData = waterHistogram;
      average(stats);
      minimum(stats);
      maximum(stats);
      cout << "Total Tap: " << waterUsage.tapWater << endl;
      cout << "Total Shower: " << waterUsage.showerWater << endl;
      cout << "Minimum water used: " << stats.min << endl;
      cout << "Maximum water used: " << stats.max << endl;
      cout << "Average water used: " << stats.average << endl;
      cout << " > Computing statistics..." << endl;
      life.stats[life.sessionNumber] = stats;
      life.waterData[life.sessionNumber] = waterUsage;
      writeData(stats, waterUsage, life);
      life.sessionNumber++;
      waterUsage.tapWater = 0;
      waterUsage.showerWater = 0;
      state = CONFIG;
      break;
    case END:
      cout << " > ending program" << endl;
      return 0;
      break;
  }
  return processState(state);
}

bool checkNumber(char c){
  if(c == '0' || c == '1' || c == '2' || c == '3' || c == '4' || c == '5') {
    return true;
  } else if (c == '6' || c == '7' || c == '8' || c == '9') {
    return true;
  }
  return false;
}

// Set-up function to check if the GPIO inputs are working
int setup(){
  life.stats = new Statistics[100];
  life.waterData = new WaterUsage[100];
  waterHistogram.histogram = new float[1000];
  ifstream infile;
  infile.open("LifetimeWater.txt");
  char line[50];
  infile.getline(line, 50);
  float tapWater = 0;
  float decimalTap = 0;
  bool decimal = false;
  for(int i = 0; i < 50; i++){
    if(line[i] == '\0') break;
    if(decimal && checkNumber(line[i])){
      decimalTap /= 10;
      decimalTap += (line[i] - '0');
    }else if(checkNumber(line[i])){
      tapWater *= 10;
      tapWater += (line[i] - '0');
    }
    if(line[i] == '.'){
      decimal = true;
    }
  }
  decimal = false;
  life.totalTapWater = tapWater + decimalTap;
  float showerWater = 0;
  float decimalShower = 0;
  infile.getline(line, 50);
  for(int i = 0; i < 50; i++){
    if(line[i] == '\0') break;
    if(decimal && checkNumber(line[i])){
      decimalShower /= 10;
      decimalShower += (line[i] - '0');
    }else if(checkNumber(line[i])){
      showerWater *= 10;
      showerWater += line[i] - '0';
    }
    if(line[i] == '.'){
      decimal = true;
    }
  }
  life.totalShowerWater = showerWater + decimalShower;
  if(!notExported(rq1, gpio1) || !notExported(rq2, gpio2) || !notExported(rq3, gpio3) || !notExported(rqConfig, configPin)
   || !notExported(rqRed, ledRed) || !notExported(rqYel, ledYellow) || !notExported(rqGre, ledGreen)){
    errorfile << "ERROR: A gpio pin was NOT exported" << endl;
    return EXIT_FAILURE;
  }

  cout << " > exporting all gpio pins" << endl;
  // export the gpio
  if(!rq1) {
    if(!exportGPIO(rv1,gpio1)){
      errorfile << "ERROR: " << gpio1 << " was NOT exported (gpio)" << endl;
      return EXIT_FAILURE;
    }
  }
  if(!rq2) {
    if(!exportGPIO(rv2,gpio2)){
      errorfile << "ERROR: pin " << gpio2 << " was NOT exported (gpio)" << endl;
      return EXIT_FAILURE;
    }
  }
  if(!rq3) {
    if(!exportGPIO(rv3,gpio3)){
      errorfile << "ERROR: pin " << gpio3 << " was NOT exported (gpio)" << endl;
      return EXIT_FAILURE;
    }
  }
  if(!rqConfig){
    if(!exportGPIO(rvConfig, configPin)){
      errorfile << "ERROR: pin " << configPin << " was NOT exported (config pin)" << endl;
      return EXIT_FAILURE;
    }
  }
  if(!rqRed){
    if(!exportGPIO(rvRed, ledRed)){
      errorfile << "ERROR: pin " << ledRed << " was NOT exported (led pin)" << endl;
      return EXIT_FAILURE;
    }
  }
  if(!rqGre){
    if(!exportGPIO(rvGre, ledGreen)){
      errorfile << "ERROR: pin " << ledGreen << " was NOT exported (led pin)" << endl;
      return EXIT_FAILURE;
    }
  }
  if(!rqYel){
    if(!exportGPIO(rvYel, ledYellow)){
      errorfile << "ERROR: pin " << ledYellow << " was NOT exported (led pin)" << endl;
      return EXIT_FAILURE;
    }
  }

  // set to input direction (1 = INPUT, 0 = OUTPUT)
  cout << " > setting pins " << gpio1 << ", " << gpio2 << ", " << gpio3 << ", " << configPin << endl;

  if(!setDirection(rv1, 1, gpio1)){
    errorfile << "Set direction of pin " << gpio1 << " failed (gpio pin)" << endl;
    return EXIT_FAILURE;
  }
  if(!setDirection(rv2, 1, gpio2)){
    errorfile << "Set direction of pin " << gpio2 << " failed (gpio pin)" << endl;
    return EXIT_FAILURE;
  }
  if(!setDirection(rv3, 1, gpio3)){
    errorfile << "Set direction of pin " << gpio3 << " failed (gpio pin)" << endl;
    return EXIT_FAILURE;
  }
  if(!setDirection(rvConfig, 1, configPin)){
    errorfile << "Set direction of pin " << configPin << " failed (config pin)" << endl;
    return EXIT_FAILURE;
  }
  if(!setDirection(rvRed, 0, ledRed)){
    errorfile << "Set direction of pin " << ledRed << " failed (led pin)" << endl;
    return EXIT_FAILURE;
  }
  if(!setDirection(rvGre, 0, ledGreen)){
    errorfile << "Set direction of pin " << ledGreen << " failed (led pin)" << endl;
    return EXIT_FAILURE;
  }
  if(!setDirection(rvYel, 0, ledYellow)){
    errorfile << "Set direction of pin " << ledYellow << " failed (led pin)" << endl;
    return EXIT_FAILURE;
  }

  cout << "Setup successful" << endl;
  return 0;
}

//Main loop for tracking water usage data
int waterLoop(STATE& state){
  Statistics stats;
  secondsCounter = 0;
  Histogram histo;
  histo.intervalSize = 5;
  histo.histogram = new float[5];
  histo.intervalCounter = 0;
  WaterUsage local;
  local.tapWater = 0;
  local.showerWater = 0;
  while(gpio_get_value(configPin) == 1){
    getValues(value1, value2, value3);
    if(value1) {
      cout << " > Tap running at low..." << endl;
      waterUsage.tapWater += rateTapLow;
      local.tapWater += rateTapLow;
    }
    if(value2) {
      cout << " > Tap running at high..." << endl;
      waterUsage.tapWater += rateTapHigh;
      local.tapWater += rateTapHigh;
    }
    if(value3) {
      cout << " > Shower running at high..." << endl;
      waterUsage.showerWater += rateShower;
      local.showerWater += rateShower;
    }
    cout << " > Current Total Tap water: " << waterUsage.tapWater << endl;
    cout << " > Current Total Shower water: " << waterUsage.showerWater << endl;
    secondsCounter++;
    cout << "second: " << secondsCounter << endl;
    if(secondsCounter % 12 == 0){
      histo.histogram[histo.intervalCounter] = local.showerWater + local.tapWater;
      histo.intervalCounter++;
      waterHistogram.histogram[waterHistogram.intervalCounter] = local.showerWater + local.tapWater;
      waterHistogram.intervalCounter++;
      if(secondsCounter % 60 == 0){
        cout << "60 seconds passed" << endl;
        stats.waterData = histo;
        average(stats);
        minimum(stats);
        maximum(stats);
        writeData(stats, waterUsage);
        histo.intervalCounter = 0;
      }
      waterUsage.currentWater += local.tapWater;
      waterUsage.currentWater += local.showerWater;
      local.tapWater = 0;
      local.showerWater = 0;
    }
    float totalWater = local.tapWater + life.waterData[life.sessionNumber].tapWater + local.showerWater + life.waterData[life.sessionNumber].showerWater;

    if(waterUsage.currentWater > waterUsage.quota){
      gpio_set_value(ledRed, 1);
      gpio_set_value(ledYellow, 0);
      gpio_set_value(ledGreen, 0);
    }else if(waterUsage.currentWater > waterUsage.quota/2){
      gpio_set_value(ledRed, 0);
      gpio_set_value(ledYellow, 1);
      gpio_set_value(ledGreen, 0);
    }else if(waterUsage.currentWater < waterUsage.quota/2){
      gpio_set_value(ledRed, 0);
      gpio_set_value(ledYellow, 0);
      gpio_set_value(ledGreen, 1);
    }
    sleep(1);
  }
  cout << " >> PIN 3 IS OFF" << endl;
  // SET STATE TO COMPUTE
  // AND COMPUTE SESSION STATS
  state = COMPUTE;
  sleep(1);
}

//Writes statistical and water usage data to a file per one minute interval
int writeData(Statistics stats, WaterUsage water){
  char newFileName [12] = {0};
  newFileName[0] = 's';
  newFileName[1] = 't';
  newFileName[2] = 'a';
  newFileName[3] = 't';
  newFileName[4] = 's';
  newFileName[5] = '0' + ++statFileCounter;
  newFileName[6] = '.';
  newFileName[7] = 's';
  newFileName[8] = 't';
  newFileName[9] = 'a';
  newFileName[10] = 't';
  newFileName[11] = 0;
  ofstream outfile;
  outfile.open(newFileName);
  if(!outfile.is_open()){
    errorfile << "File " << newFileName << " could not be created. writeData() failed" << endl;
    return -1;
  }
  outfile << "Tap water used: " << water.tapWater << "\n";
  outfile << "Shower water used: " << water.showerWater << "\n";
  outfile << "Total water used: " << water.tapWater + water.showerWater << "\n";
  outfile << "Minimum water used: " << stats.min << "\n";
	outfile << "Maximum water used: " << stats.max << "\n";
  outfile << "Average water used: " << stats.average << "\n";
  outfile << "Intervals passed: " << stats.waterData.intervalCounter << "\n";
  horizontalBarGraph (stats.waterData.histogram, "Interval", stats.waterData.intervalSize, outfile);
  outfile.close();
}

//Writes statistical and water usage data to a file at the end of the session, overloaded function
int writeData(Statistics stats, WaterUsage water, Lifetime life){
  life.totalTapWater += life.waterData[life.sessionNumber].tapWater;
  life.totalShowerWater += life.waterData[life.sessionNumber].showerWater;
  ofstream outfile;
  outfile.open("WaterTrackerDataSession");
  if(!outfile.is_open()) return -1;
  outfile << "Tap water used: " << water.tapWater << "\n";
  outfile << "Shower water used: " << water.showerWater << "\n";
  outfile << "Total water used: " << water.tapWater + water.showerWater << "\n";
  outfile << "Minimum water used: " << stats.min << "\n";
	outfile << "Maximum water used: " << stats.max << "\n";
  outfile << "Average water used: " << stats.average << "\n";
  outfile << "Intervals passed: " << stats.waterData.intervalCounter << "\n";
  outfile << "Lifetime Tap water used: " << life.totalTapWater << "\n";
  outfile << "Lifetime Shower water used: " << life.totalShowerWater << "\n";
  outfile << "Lifetime water used: " << life.totalTapWater + life.totalShowerWater << "\n";
  horizontalBarGraph (waterHistogram.histogram, "Interval", waterHistogram.intervalCounter, outfile);
  ofstream outfile2;
  outfile2.open("LifetimeWater.txt");
  outfile2 << life.totalTapWater << "\n";
  outfile2 << life.totalShowerWater;
  outfile.close();
  outfile2.close();
}

void getValues(int& value1, int& value2, int& value3){
  value1 = gpio_get_value(gpio1);
  value2 = gpio_get_value(gpio2);
  value3 = gpio_get_value(gpio3);
  return;
}

// Config function that allows the user to edit options
int config(STATE& state){
  cout << " > Entered configuration state" << endl;
  // future<bool> fut = async(checkConfigPin);
  printHelp();
  string input;
  while(true){
    cout << " cmd>> ";
    cin >> input;
    if(input.compare("help") == 0) {
      cout << "  ----- Configuration helper -----" << endl;
      printHelp();
    } else if(input.compare("rates") == 0) {
      cout << " > entered rates case" << endl;
      configRates();
    } else if(input.compare("run") == 0) {
      cout << " > entered run case" << endl;
      cout << " > switching state to RUN" << endl;
      state = RUN;
      break;
    } else if(input.compare("end") == 0) {
      cout << " > entered end case" << endl;
      state = END;
      break;
    } else if(input.compare("quota") == 0) {
      cout << " > entered quota case" << endl;
      configQuota();
    }
  }
}

bool checkConfigPin(){
  while(true){
    if(gpio_get_value(configPin)){
      return true;
    }
  }
}

void printHelp(){
  cout << " > 'rates' to configure water flow rates" << endl;
  cout << " > 'quota' to configure the quota" << endl;
  cout << " > 'run' to start reading pins" << endl;
  cout << " > 'end' to end the program" << endl;
  cout << " > 'help' to bring up this menu again" << endl;
}

void configRates(){
  cout << "entered configRates" << endl;
  string input;
  float value = 0;
  bool validInput = true;
  cout << " --- Current Rates ---" << endl;
  cout << "|  TapLow: " << rateTapLow << " L/s" << endl;
  cout << "| TapHigh: " << rateTapHigh << " L/s" << endl;
  cout << "|  Shower: " << rateShower << " L/s" << endl;
  cout << " ---------------------" << endl;
  cout << "Enter values for new rates (0 for no change): " << endl;

  do {
    cout << " > TapLow: ";
    cin >> input;
    validInput = stringToFloat(input, value);
  } while(!validInput);
  if(value != 0)
    rateTapLow = value;
  cout << "RATE LOW: " << rateTapLow << endl;

  do {
    cout << " > TapHigh: ";
    cin >> input;
    validInput = stringToFloat(input, value);
  } while(!validInput);
  if(value != 0)
    rateTapHigh = value;
  cout << "RATE HIGH: " << rateTapHigh << endl;

  do {
    cout << " > Shower: ";
    cin >> input;
    validInput = stringToFloat(input, value);
  } while(!validInput);
  if(value != 0)
    rateShower = value;
  cout << "RATE SHOWER: " << rateShower << endl;
}

void configQuota(){
  cout << "entered configQuota" << endl;
  string input;
  float value = 0;
  float percent = 100*(waterUsage.currentWater)/waterUsage.quota;
  bool validInput = true;
  cout << " --- Current Quota and usage ---" << endl;
  cout << "|  Quota: " << waterUsage.quota << " L" << endl;
  cout << "|  Usage: " << waterUsage.currentWater << " L/" << waterUsage.quota << " L, (" << percent << "%)" << endl;
  cout << " ---------------------" << endl;
  cout << "Enter value for new quota (0 for no change): " << endl;

  do {
    cout << " > Quota: ";
    cin >> input;
    validInput = stringToFloat(input, value);
  } while(!validInput);
  if(value != 0)
    waterUsage.quota = value;
  cout << "NEW QUOTA: " << waterUsage.quota << endl;
}

float pow(float value, int n){
  float x = value;
  for(int i = 0; i < n; i++)
    x *= value;
  return x;
}

bool stringToFloat(string input, float& value) {
  enum State {start, gotSign, gotDigit, gotDecimal, gotExponent, null};
  State state = start;
  double x = 0;
  bool negVal = false;
  bool dec = false;
  double decimal = 0;
  int decimalPlaces = 0;
  int position = 0;
  while(state != null){
    switch(input[position]){
      case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9': {
        state = gotDigit;
        break;
      }
      case '-': case '+': {
        state = gotSign;
        break;
      }
      case '.': {
        state = gotDecimal;
        break;
      }
      case 0: {
        state = null;
        break;
      }
      default: {
        cout << " > Please enter a valid float" << endl;
        return false;
      };
    } // input switch
    switch(state) {
      case gotDigit: {
        if(!dec){
          x *= 10;
          x += input[position] - '0';
        }
        if(dec) {
          decimal *= 10;
          decimal += input[position] - '0';
          decimalPlaces++;
        }
        break;
      }
      case gotDecimal: {
        dec = true;
        break;
      }
      case gotSign: {
        if(input[position] == '-')
          negVal = true;
        break;
      }
    }
    position++;
  } // while
  decimal = decimal/pow(10, decimalPlaces-1);
  x += decimal;
  if(x > 99999 || x < 0)
    return false;
  if(negVal)
    value *= -1;
  value = x;
  return true;
}

//Sets the pin to either input or output mode
bool setDirection(int& RV, int direction, int pin){
	if (direction == 1){
		if ((RV = gpio_direction_input(pin)) < 0){
      errorfile << "Error: pin " << pin << " could not be set to output. Error in setDirection()" << endl;
			perror("gpio_direction_input");
			return false;
	  }
		return true;
	}else if (direction == 0){
		if ((RV = gpio_direction_output(pin,1)) < 0){
      errorfile << "Error: pin " << pin << " could not be set to input. Error in setDirection()" << endl;
			perror("gpio_direction_input");
			return false;
		}
		return true;
	}
}

bool exportGPIO(int& RV, int pin){
	if ((RV = gpio_request(pin, NULL)) < 0)
	{
    errorfile << "Error: pin " << pin << " could not be exported. Error in exportGPIO()" << endl;
		perror("gpio_request");
		return EXIT_FAILURE;
	}
	return true;
}

bool notExported(int& RQ, int pin){
	if ((RQ = gpio_is_requested(pin)) < 0)	{
    errorfile << "Error: pin " << pin << " could not be exported. Error in notExported()" << endl;
		perror("gpio_is_requested");
		return EXIT_FAILURE;
	}
		return true;
}

//Finds the average of all elements in the historgram
void average(Statistics& stats){
  float sum = 0;
  int intervalSize = stats.waterData.intervalSize;
  for (int i = 0; i < intervalSize; i++){
    sum += stats.waterData.histogram[i];
  }
  stats.average = sum/intervalSize;
}

//Finds the minimum element in the histogram
void minimum(Statistics& stats){
  float min = stats.waterData.histogram[0];
  int intervalSize = stats.waterData.intervalSize;
  for(int i = 0; i < intervalSize; i++){
    if(stats.waterData.histogram[i] < min){
      min = stats.waterData.histogram[i];
    }
  }
  stats.min = min;
}

//Finds the maximum element in the historgram
void maximum(Statistics& stats){
  float max = stats.waterData.histogram[0];
  int intervalSize = stats.waterData.intervalSize;
  for(int i = 0; i < intervalSize; i++){
    if(stats.waterData.histogram[i] > max){
      max = stats.waterData.histogram[i];
    }
  }
  stats.max = max;
}

//Generates the display for the bar graph in the outfile
void horizontal(int graphSize, float interval, ofstream& ofile){
  int times = graphSize / interval;
	for(int i = 0; i < times; i++){
	  ofile << "■";
	}
  ofile << endl;
}

//Finds the max of the data collected, important for generating a scaled bar graph
float findMax(float array[], int size){
	float max = array[0];
	for(int i = 0; i < size; i++){
		float current = array[i];
		if(current > max){
			max = current;
		}
	}
	return max;
}

//Scales bargraph to a size that fits nicely in output file, based off of data size collected
void scale(float array[], int scaleSize, int arraySize, float scaledArray[]){
	for(int i = 0; i < arraySize; i++){
		scaledArray[i] = (array[i]/scaleSize);
	}
}

//Generates a bargraph using helper functions
void horizontalBarGraph(float array[], string axis, int arraySize, ofstream& ofile){
	float arrayMax = findMax(array, arraySize);
	float scaledArray[arraySize];
	ofile << "-----------------------------------------" << endl;
	ofile << "      Water used over each interval  " << endl;
	ofile << "-----------------------------------------" << endl;
  float arrayInterval = arrayMax / 10;
	for(int j = 0; j < arraySize; j++){
		ofile << axis << " " << j+1 << " :";
		horizontal(array[j], arrayInterval, ofile);
  }
	ofile << "---------------------------------------" << endl;
	ofile << "Legend: ■ = " << arrayInterval << "L" << endl;
	// }
	ofile << "---------------------------------------" << endl;
}
