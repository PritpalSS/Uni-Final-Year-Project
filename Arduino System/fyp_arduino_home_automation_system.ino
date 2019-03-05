#include <Process.h>
#include <FileIO.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "DHT.h"

/*Digital Pins*/
//LED's -> Digital Pins 4,5,6
int RedLed = 4;
int GreenLed = 5;
int YellowLed = 6;
//Relay -> Digital Pin 7 --> Heating System
int IN1=7;
//Relay -> Digital Pin 13 --> Cooling System
int IN2=13;

//Humidity Sensor Definitions
/*Defining the Pin for Humidity Sensor*/
#define DHTPIN 8 // what pin we're connected to

//#define DHTTYPE DHT22 Sensor 
#define DHTTYPE DHT22

// Initialize DHT sensor for Arduino Yun 
DHT dht(DHTPIN, DHTTYPE);

//PIR Motion Sensor Definitions
int inputPirPin = 12; // Input pin (for PIR sensor)
int pirVal = 0; // variable for reading the PIR Sensor pin status

//Gas sensor Definitions
const int analogInPin = A0; // Analog input pin that the potentiometer (Gas Sensor) is attached to
int sensorValueGas = 0; // value read from the sensor

//Water Detection Definitions
const int inputWaterPin = 2; // sets the digital pin as input for Water Detection
int sensorValueWater = 0; //value read from the water detection circuit

// Light (LED) Definition
int led = 10; //Defining pin value connected to LED light

/*DS18B20 Temperature Sensor Definitions*/
// Data wire is plugged into pin 3 on the Arduino
#define ONE_WIRE_BUS 3
// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(ONE_WIRE_BUS);
// Pass our oneWire reference to Dallas Temperature.
DallasTemperature temp(&oneWire);
// Assign the addresses of your 1-Wire temp sensors.
DeviceAddress Thermometer = { 0x28, 0x0E, 0xF1, 0xCF, 0x05, 0x00, 0x00, 0xF0 };

//Server path, login and password Definitions
String FTP_SERVER_PATH="192.168.1.22/FTP/";
String login="Gugz";
String pass="gugz";

void setup() {
	/*Configure the Digital Pins as Inputs and Outputs*/ 
	pinMode(RedLed, OUTPUT);
	pinMode(GreenLed, OUTPUT);
	pinMode(YellowLed, OUTPUT);
	pinMode(IN1, OUTPUT);
	pinMode(IN2, OUTPUT);
	pinMode(led, INPUT);
	pinMode(inputWaterPin, INPUT);
	pinMode(inputPirPin, INPUT);

	// Initialize the Bridge and the Serial
	Bridge.begin();
	Serial.begin(9600);

	//Initializes the SD card and FileIO class
	FileSystem.begin();

	//Begin operation of Humidity Sensor
	dht.begin();

	// Start up the Temperature sensor library
	temp.begin();
	// set the resolution to 10 bit
	temp.setResolution(Thermometer, 10);

	//Setting the correct date and time from the Linux processor via Bridge
	Process time; //Created a process time that will Use Bridge library's Process class to run Linux process
	time.runShellCommand("ntpd -qn -p 0.ie.pool.ntp.org");
	time.close();

	while (!Serial); // wait for Serial port to connect.
	Serial.println("Arduino Yun Project Setting Up...\n");
}

void loop () {

	//Download Controlling.txt from the FTP Server and Upload MonitorFile.txt to the FTP Server
	Process p;
	p.runShellCommand("curl ftp://"+FTP_SERVER_PATH+"Controlling.txt --user "+login+":"+pass+" -o /mnt/sda1/Controlling.txt");
	p.runShellCommand("curl -T /mnt/sda1/MonitorFile.txt ftp://"+FTP_SERVER_PATH+"Monitoring.txt --user "+login+":"+pass);
	p.runShellCommand("curl -T /mmt/sda1/MonitorFileHistory.txt ftp://"+FTP_SERVER_PATH+"MonitoringHistory.txt --user "+login+":"+pass);
	
	// Read the commands in the Commands.txt file located in the SD Card
	File Commands = FileSystem.open("/mnt/sda1/Controlling.txt", FILE_READ);

	// Positions of the values in the Commands.txt file in these string variables String Temp, Hys, RI,LI,Brightness;
	int init=0;

	// If the file is available, read from it and parse integers from it and store in corresponding variables
	int first=0;
	if (Commands) {
		while (Commands.available()>0) { 
			char c = Commands.read();
			if (c=='\'')
				init++;
			if (init==1) {
				if (first==1) { 
					Temp+=c;
				} else 
					first=1;
			}
			if (init==3) {
				if (first==0) { 
					Hys+=c;
				} 
				else 
					first=0;
			}
			if (init==5) {
				if (first==1) { 
					RI+=c;
				} 
				else
					first=1; 
			}
			if (init==7) { 
				if (first==0) {
					LI+=c; 
				} else
					first=0;
			}
			if (init==9) {
				if (first==1) {
					Brightness+=c;
				} 
				else
					first=1;
			}
		}
	
		Serial.println("Controlling.txt read!");
		Serial.println(""); //blank line after the previous print output 
		Commands.close();
	}

	// If the file isn't open, pop up an error: 
	else {
		Serial.println("Error opening Controlling.txt"); 
	}


	//Define a type conversion buffer and default value for hysteresis
	char floatbuf[10];
	float Default=0.3;

	//Convert String Commands to float and int
	Temp.toCharArray(floatbuf, sizeof(floatbuf)); 
	float DesireTemp = atof(floatbuf); 
	Hys.toCharArray(floatbuf, sizeof(floatbuf));
	float Hysteresis = atof(floatbuf);
	int ReadingInterval = RI.toInt();
	int LED_Status = LI.toInt();
	int BrightnessValuePercent = Brightness.toInt();

	//If the user doesn't introduce the hysteresis value, the hysteresis takes a default value
	if (!Hysteresis)
		Hysteresis=Default;

	//If value parsed from the controlling.txt file is "1" then turn the LED light ON 
	//Adjust the brightness of LED light from specified value
	if (LED_Status == 1) {
		digitalWrite(led,HIGH);

		//convert the percentage of brighness value to value between 0 - 255
		int BrightnessValue = (255/100)*(BrightnessValuePercent);

		// Set the brightness of LED speceficed by user
		analogWrite(led, BrightnessValue);
	}
	else {
		//Other wise if the value is not "1" then LED light is turned OFF
		digitalWrite(led,LOW);
	}

	// Read Temperature sensor, Gas Sensor, Humidity Sensor, Motion Sensor, Water Detection and save the data in the MonitorFile in the SD Card
	// Strings are created for assembling the data to log MonitorFile
	String TimeStamp; //Stores Time Stamp values in this format: mm/dd/yy-hh:mm:ss String System_Info;
	String TempString;
	String GasString;
	String WaterString;
	String HumidityString;
	String MotionString;

	TimeStamp += getTimeStamp(); //Writes value returned from getTimeStamp() function to TimeStamp
	System_Info += "System = ";
	TempString += "Current Temperature = ";
	GasString += Gas(); //Writes value returned from Gas() function to GasString
	WaterString += Water();
	HumidityString += Humidity();
	MotionString += "Motion = ";
	MotionString += Motion();

	//Read the Temperature sensor through 1-Wire Protocol
	temp.requestTemperatures();
	float sensorTemp = temp.getTempC(Thermometer);
	if (sensorTemp == -126.00) {
		Serial.print("Error getting temperature"); 
	}
	TempString += sensorTemp; //add Temperature value to TempString String

	//Implement the Temperature Control
	// Heating Part of the Control
	if (sensorTemp < (DesireTemp - Hysteresis)) {
		// When Sensor Temp. is less then (Desired Temp. - Hysteresis) OR less then Desired Temp. -- > Heating Relay turned ON (Heating ON) (Red LED ON)
		digitalWrite(RedLed, HIGH);
		digitalWrite(IN1, HIGH);
		digitalWrite(IN2, LOW);
		digitalWrite(GreenLed, LOW);
		digitalWrite(YellowLed, LOW);
		System_Info += "Heating ON";
	}

	// Cooling Part of the Control
	if (sensorTemp > (DesireTemp + Hysteresis)) {
		// When Sensor Temp. reached above (Desired Temp. + Hysteresis) OR reached above Desired Temp. --> Cooling Relay turned ON (Cooler ON) (Green LED ON)
		digitalWrite(GreenLed, HIGH);
		digitalWrite(IN1, LOW);
		digitalWrite(IN2, HIGH);
		digitalWrite(RedLed, LOW);
		digitalWrite(YellowLed, LOW);
		System_Info += "Cooler ON";
	}
	
	//Ideal State
	if (sensorTemp > (DesireTemp - Hysteresis) && sensorTemp < (DesireTemp + Hysteresis)) {
		//If Sensor Temp. is between these values --> System is in Ideal state (Sensor Temp. ⁓ Desired Temp.) (Yellow LED ON)
		digitalWrite(YellowLed, HIGH);
		digitalWrite(RedLed, LOW);
		digitalWrite(IN1, LOW);
		digitalWrite(IN2, LOW);
		digitalWrite(GreenLed, LOW);
		System_Info += "Ideal State"; 
	}

	// Open the file. Note that only one file can be open at a time,
	// The FileSystem card is mounted at the following "/mnt/FileSystem"
	File MonitorFileHistory = FileSystem.open("/mnt/sda1/MonitorFileHistory.txt", FILE_APPEND); //Monitoring File where updated data is appended to the File
	File MonitorFile = FileSystem.open("/mnt/sda1/MonitorFile.txt", FILE_WRITE); //Monitoring File where it only writes the updated data
	
	// If the file is available, write to it: 
	if (MonitorFile) {
		MonitorFile.println(TimeStamp); 
		MonitorFile.println(System_Info);
		MonitorFile.println(TempString);
		MonitorFile.println(GasString);
		MonitorFile.println(WaterString);
		MonitorFile.println(HumidityString);
		MonitorFile.println(MotionString);
		MonitorFile.close();
	}
	// If the file isn't open, pop up an error
	else {
		Serial.println("Error opening MonitorFile.txt");
	}

	// If the file is available, write to it:
	if (MonitorFileHistory) {
		MonitorFileHistory.println(TimeStamp);
		MonitorFileHistory.println(System_Info);
		MonitorFileHistory.println(TempString);
		MonitorFileHistory.println(GasString);
		MonitorFileHistory.println(WaterString);
		MonitorFileHistory.println(HumidityString);
		MonitorFileHistory.println(MotionString);
		MonitorFileHistory.println(" ");
		MonitorFileHistory.close();
	}
	// If the file isn't open, pop up an error
	else {
		Serial.println("Error opening MonitorFileHistory.txt");
	}

	// Print to the serial port too:
	Serial.println("Storing Data in Monitoring File...");
	Serial.println(TimeStamp);
	Serial.println(System_Info);
	Serial.println(TempString);
	Serial.println(GasString);
	Serial.println(WaterString);
	Serial.println(HumidityString);
	Serial.println(MotionString);
	Serial.println(""); //blank line after the previous print output

	delay(ReadingInterval*10); //Regular Interval Time
}

// This function returns a string with the time stamp
String getTimeStamp() {
	String result;
	Process time;
	// Date is a command line utility to get the date and the time
	// in different formats depending on the additional parameter
	time.begin("date");
	time.addParameter("+%D-%T"); // parameters: D for the complete date mm/dd/yy and T for the time hh:mm:ss
	time.run(); // run the command

	// Read the output of the command
	while (time.available() > 0) {
		char c = time.read();
		if (c != '\n')
			result += c; 
	}

	return result;
}

String Gas(){
	String GasValue; //Defining String to store the value of int variable sensorValueGas to String String HarmFulGas;
	String Gas; //String that get's returend from the function at the end

	// Read the analog value from Gas sensor Pin
	sensorValueGas = analogRead(analogInPin);
	// Determine status of analoge value from the sensor
	if (sensorValueGas >= 750)
	{
		HarmFulGas += "YES"; //stores this string indicating Harmful Gas is detected
	}
	else {
		HarmFulGas += "NO"; //stores this string indicating Harmful Gas is not detected
	}

	//Convert the int value of sensorValueGas to String and store all the data gathered in one string
	GasValue += String(sensorValueGas);
	Gas += "Gas Sensor Value = "+GasValue+'\n'+"Harmful Gas Detected = "+HarmFulGas;

	// Wait 10 milliseconds before the next loop
	// for the analog-to-digital converter to settle after the last reading 
	delay(10);

	//return the Gas string from this function
	return Gas;
}

String Water(){
	String WaterDetect; //String that get's returend from the function, which tells whether Water was detected or not
	
	// Read value from inputWaterPin on arduino
	sensorValueWater = digitalRead(inputWaterPin);
	
	//If the water was detected
	if (sensorValueWater == 1) {
		WaterDetect += "Water Detected = YES"; //write to WaterDetect string
	}
	// If the water was not detected 
	else {
		WaterDetect += "Water Detected = NO"; //write to WaterDetect string
	}

	delay(10); // Delay before next digitalRead

	//return WaterDetect string from this function
	return WaterDetect;
}

String Humidity(){
	String HumidityValue; //Defining String to store the int value from the humidity sensor as String
	String Humidity; //String that get's returend from the function at the end
	HumidityValue += String(dht.readHumidity()); //Store int value from humidty sensor to String
	
	// Check if any reads failed and exit early (to try again).
	if (isnan(dht.readHumidity())) {
		Serial.println("Failed to read from DHT sensor!");
		Humidity += "Humidity = ";
	}

	Humidity += "Humidity = "+HumidityValue;
	delay(10); // Delay before next digitalRead
	
	//return Humidity string from this function
	return Humidity;
}

String Motion(){
	String Motion;
	pirVal = digitalRead(inputPirPin); // read input value from motion sensor and store value in this variable

	if (pirVal == HIGH) { // check if the input is HIGH 
		Motion += "Detected";
	} else {
		Motion += "Ended"; 
	}

	delay(10); // Delay before next digitalRead

	//return Motion string from this function
	return Motion;
}