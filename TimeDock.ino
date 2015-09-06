/*
* TIMEDOCK
* Author: Jeremy Wolfe
* With code from Arduino & Pebble
*/

#include <WaveHC.h>
#include <WaveUtil.h>
#include <ArduinoPebbleSerial.h>

// Pebble Serial protocol consts
static const uint16_t SERVICE_ID = 0x1001;

static const uint16_t SOUND_ATTRIBUTE_ID = 0x0001;
static const size_t SOUND_ATTRIBUTE_LENGTH = 1;

static const uint16_t UPTIME_ATTRIBUTE_ID = 0x0002;
static const size_t UPTIME_ATTRIBUTE_LENGTH = 4;

//  Say time command: two bytes, first is hour in 24hrs., second is minutes
static const uint16_t SAY_TIME_ATTRIBUTE_ID = 0x0003;
static const size_t SAY_TIME_ATTRIBUTE_ID_LENGTH = 2;

// Renotify command : Alert over serial when a notification occurs, and then goes away.
// Passes a bool, true is a new notification, false is the notification window closes.
static const uint16_t RENOTIFY_ATTRIBUTE_ID = 0x0004;
static const size_t RENOTIFY_ATTRIBUTE_ID_LENGTH = 1;

//Ultrasound fired
static const uint16_t ULTRASOUND_ATTRIBUTE_ID = 0x0005;
static const size_t ULTRASOUND_ATTRIBUTE_ID_LENGTH = 1;

static const uint16_t SERVICES[] = { SERVICE_ID };
static const uint8_t NUM_SERVICES = 1;

static const uint8_t PEBBLE_DATA_PIN = 8;
static uint8_t buffer[GET_PAYLOAD_BUFFER_SIZE(4)];

const int ULTRASOUND_THRESHOLD = 18;  // values below this mean to trigger.


// SD Card globals
SdReader card;    // This object holds the information for the card
FatVolume vol;    // This holds the information for the partition on the card
FatReader root;   // This holds the information for the volumes root directory
FatReader file;   // This object represent the WAV file for a pi digit or period
WaveHC wave;      // This is the only wave (audio) object, since we will only play one at a time
unsigned digit = 0;
char filename[13];
bool isConnected;

/*
* Define macro to put error messages in flash memory
*/
#define error(msg) error_P(PSTR(msg))

void setup() {

	// set up Serial library at 9600 bps
	Serial.begin(9600);

	PgmPrintln("Pi speaker");

	if (!card.init()) {
		error("Card init. failed!");
	}
	if (!vol.init(card)) {
		error("No partition!");
	}
	if (!root.openRoot(vol)) {
		error("Couldn't open dir");
	}

	PgmPrintln("Files found:");
	root.ls();

	isConnected = false;
	ArduinoPebbleSerial::begin_software(PEBBLE_DATA_PIN, buffer, sizeof(buffer), Baud57600, SERVICES, NUM_SERVICES);
  pinMode(13, OUTPUT);// This pin has an on-board LED connected.
}

void loop() {
	const uint32_t current_time = millis() / 1000; //Time in integer seconds
	static uint32_t last_LED_time = 0;
	static bool LED_on = false;

	checkUltraSound();
	checkDockStatus();

	if (ArduinoPebbleSerial::is_connected()) {
		static uint32_t last_notify_time = 0;
		if (current_time > last_notify_time) {
			ArduinoPebbleSerial::notify(SERVICE_ID, UPTIME_ATTRIBUTE_ID);
			last_notify_time = current_time;
		}
	}
	
  /*  Blink the LED on pin 13
    if (current_time > last_LED_time) {
		LED_on = !LED_on;
		digitalWrite(13, HIGH);
		last_LED_time = current_time;
	}*/ 

	uint16_t service_id;
	uint16_t attribute_id;
	size_t length;
	RequestType type;
	if (ArduinoPebbleSerial::feed(&service_id, &attribute_id, &length, &type)) {
		// process the request
		if (service_id == SERVICE_ID) {
			switch (attribute_id) {
			case UPTIME_ATTRIBUTE_ID:
				handle_uptime_request(type, length);
				break;
			case SOUND_ATTRIBUTE_ID:
				handle_sound_request(type, length);
				break;
			case SAY_TIME_ATTRIBUTE_ID:
				handle_say_hour_request(type, length);
				break;
			case RENOTIFY_ATTRIBUTE_ID:
				handle_renotify_request(type, length);
				break;
			default:
				break;
			}
		}
	}
}
void checkUltraSound() {
	// for ultrasound sensor
	static uint32_t lastFired = 0;
	const uint32_t current_time_tenths = millis()/100; //Time in tenths of seconds
	static uint32_t last_Analog_time = 0;
	int analogValue;
	static int analogOldValue1 = 1023;
	static int analogOldValue2 = 1023;

	// Sample analog input 0 every tenth of a second
	if ((current_time_tenths > last_Analog_time) && ((millis() - lastFired) > 5000 )) {
		analogValue = analogRead(0); // read analog pin 0
		// check value
		if ((analogValue < ULTRASOUND_THRESHOLD) && (analogOldValue1 < ULTRASOUND_THRESHOLD) && (analogOldValue2 < ULTRASOUND_THRESHOLD))
		{
			ArduinoPebbleSerial::notify(SERVICE_ID, ULTRASOUND_ATTRIBUTE_ID);
			lastFired = millis();
			analogOldValue1 = 1023;
			analogOldValue2 = 1023;
		}
		// Update statics for next cycle
		analogOldValue2 = analogOldValue1;
		analogOldValue1 = analogValue;
		last_Analog_time = current_time_tenths;
	}
}

void checkDockStatus() {
	if (ArduinoPebbleSerial::is_connected() != isConnected) {
		isConnected = ArduinoPebbleSerial::is_connected();
		if (isConnected) {
			playDockSound();
		}
		else {
			playUndockSound();
		}
	}
}

void playDockSound() {
	strcpy_P(filename, PSTR("DOCK.WAV"));
	playcomplete(filename);
}

void playUndockSound() {
	strcpy_P(filename, PSTR("UNDOCK.WAV"));
	playcomplete(filename);
}

void sayTime(int hour, int minute) {
	char hourFilename[7];
	char minuteFilename[7];
	char ampmFilename[7];
	char greetingFilename[10];
	char ohFilename[7];
	int pauseLength = 15; //length of pause between words, in ms

	strcpy_P(greetingFilename, PSTR("hello.WAV"));

	if (hour == 0 && minute == 0) {
		strcpy_P(filename, PSTR("MIDNIGHT.WAV"));
		playcomplete(greetingFilename);
		//delay(pauseLength);
		playcomplete(filename);
		return;
	}
	else if (hour == 12 && minute == 0) {
		playcomplete(greetingFilename);
		//delay(pauseLength);
		strcpy_P(filename, PSTR("NOON.WAV"));
		playcomplete(filename);
		return;
	}
	else {
		if (hour == 0) {
			hour = 12;
		}

		bool pm = (hour >= 12);
		strcpy_P(ampmFilename, PSTR("AM.WAV"));
		if (pm && hour != 12) {
			hour -= 12;
			ampmFilename[0] = 'P';
		}

		strcpy_P(ohFilename, PSTR("00.WAV"));
		if (minute > 0 && minute < 10) {
			ohFilename[0] = 'O';
			ohFilename[1] = 'H';
		}

		char parsebuffer[3];
		strcpy_P(hourFilename, PSTR("00.WAV"));
		sprintf(parsebuffer, "%02d", hour);
		hourFilename[0] = parsebuffer[0];
		hourFilename[1] = parsebuffer[1];

		strcpy_P(minuteFilename, PSTR("00.WAV"));
		sprintf(parsebuffer, "%02d", minute);
		minuteFilename[0] = parsebuffer[0];
		minuteFilename[1] = parsebuffer[1];
	}

	playcomplete(greetingFilename);
	//delay(pauseLength);
	playcomplete(hourFilename);
	//delay(pauseLength);
	keepAlive();
	playcomplete(ohFilename);
	if (minute > 0)
		playcomplete(minuteFilename);
	//delay(pauseLength);
	playcomplete(ampmFilename);
}

/*
* print error message and halt
*/
void error_P(const char *str) {
	PgmPrint("Error: ");
	SerialPrint_P(str);
	sdErrorCheck();
	while (1);
}
/*
* print error message and halt if SD I/O error
*/
void sdErrorCheck(void) {
	if (!card.errorCode()) return;
	PgmPrint("\r\nSD I/O error: ");
	Serial.print(card.errorCode(), HEX);
	PgmPrint(", ");
	Serial.println(card.errorData(), HEX);
	while (1);
}
/*
* Play a file and wait for it to complete
*/
void playcomplete(char *name) {
	playfile(name);
	while (wave.isplaying);

	// see if an error occurred while playing
	sdErrorCheck();
}
/*
* Open and start playing a WAV file
*/
void playfile(char *name) {
	if (wave.isplaying) {// already playing something, so stop it!
		wave.stop(); // stop it
	}
	if (!file.open(root, name)) {
		PgmPrint("Couldn't open file ");
		Serial.print(name);
		return;
	}
	if (!wave.create(file)) {
		PgmPrintln("Not a valid WAV");
		return;
	}
	// ok time to play!
	wave.play();
}

//Routines for requests from Pebble
void handle_uptime_request(RequestType type, size_t length) {
	if (type != RequestTypeRead) {
		// unexpected request type
		return;
	}
	// write back the current uptime
	//const uint32_t uptime = millis() / 1000;
	const uint32_t uptime = analogRead(0);
	ArduinoPebbleSerial::write(true, (uint8_t *)&uptime, sizeof(uptime));
}

void handle_sound_request(RequestType type, size_t length) {
	if (type != RequestTypeWrite) {
		// unexpected request type
		return;
	}
	else if (length != SOUND_ATTRIBUTE_LENGTH) {
		// unexpected request length
		return;
	}

	// ACK that the write request was received
	ArduinoPebbleSerial::write(true, NULL, 0);

	//delay(10);


}

void handle_say_hour_request(RequestType type, size_t length) {
	uint8_t	hour;
	uint8_t	min;

	if (type != RequestTypeWrite) {
		// unexpected request type
		return;
	}
	else if (length != SAY_TIME_ATTRIBUTE_ID_LENGTH) {
		// unexpected request length
		return;
	}

	// Get the hour and minute
	hour = buffer[0];
	min = buffer[1];

	//return that we got the message before we act on it
	ArduinoPebbleSerial::write(true, NULL, 0);

	sayTime(hour, min);

}

void handle_renotify_request(RequestType type, size_t length) {
	uint8_t start_not_end;

	if (type != RequestTypeWrite) {
		// unexpected request type
		return;
	}
	else if (length != RENOTIFY_ATTRIBUTE_ID_LENGTH) {
		// unexpected request length
		return;
	}

	// Get the value that tells us if the notification window just appeared (true) or just closed (false)
	start_not_end = buffer[0];

	//return that we got the message before we act on it
	ArduinoPebbleSerial::write(true, NULL, 0);

	//Sount the notification
	if (start_not_end == 0)
	{
		strcpy_P(filename, PSTR("ALERT.WAV"));
		playcomplete(filename);
	}
	else
	{
		strcpy_P(filename, PSTR("ALERT2.WAV"));
		playcomplete(filename);
	}

}

void keepAlive()
{
	uint16_t service_id;
	uint16_t attribute_id;
	size_t length;
	RequestType type;
	ArduinoPebbleSerial::feed(&service_id, &attribute_id, &length, &type);
}
