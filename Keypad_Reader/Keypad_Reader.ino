/*
 Name:		Keypad_Reader.ino
 Created:	1/2/2017 10:12:38 PM
 Author:	Rojait00
*/

#include <Keypad.h>

const byte ROWS = 2; //four rows
const byte COLS = 3; //three columns
char keys[ROWS][COLS] = {
	{ '-','X','+' },
	{ 'B','P','N' }
};
byte rowPins[ROWS] = { 6,10 }; //connect to the row pinouts of the keypad
byte colPins[COLS] = { 9,8,7 }; //connect to the column pinouts of the keypad

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

void setup() {
	Serial.begin(9600);
	//Serial1.begin(9600);
}

void loop() {
	char key = keypad.getKey();

	if (key) {
		Serial.println(key);
		//    Serial1.println(key);
	}
}
