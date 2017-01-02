/*
Name:		ESP_Spotify_Remote_Display.ino
Created:	1/2/2017 10:12:38 PM
Author:		Rojait00
*/

/*######################### Include #############################################*/
#include <Key.h>			//Keypad 2x3
#include <Keypad.h>			//Keypad 2x3
#include "SSD1306.h"		//Display
#include "OLEDDisplayUi.h"	// Include the UI lib
#include "images.h"			// Include custom images
#include <ESP8266WiFi.h>	//WiFi


/*######################### Debug Options #######################################*/
//#define DEBUG

#ifdef DEBUG
	#define Sprintln(a) (Serial.println(a)) // Use Serial
	#define Sprint(a) (Serial.print(a)) // Use Serial
	#define getInput() (Serialread()) // Use Serial
#else	
	#define Sprintln(a) // Don't print
	#define Sprint(a) // Don't print
	#define getInput() (Serialread()) // (kpd.getKey()) // use keypad if your ESP has got enough pins free
#endif // DEBUG




/*######################### WiFi Parameter #######################################*/
const char* ssid = "B5C4";
const char* password = "0622527231993956";


/*######################### Keypad Mapping #######################################*/
byte Port_rowPins[] = { 3,1};
byte Port_colPins[] = { 5,13,4 };
byte ROWS = 2; // Two rows
byte COLS = 3; // Three columns
char keys[2][3] = {
	{ '-','X','+' },  //volume down, (un)mute, volume up
	{ 'B','P','N'} }; //back, Play/Pause, next
Keypad kpd = Keypad(makeKeymap(keys), Port_rowPins, Port_colPins, ROWS, COLS);


/*######################### OLED Display #######################################*/
SSD1306  display(0x3c, 2, 0);
OLEDDisplayUi ui(&display);

/*######################### Player Status #######################################*/
String artist="12345678901234567890";
bool newArtist = true;
String song="ABCDEFGHIJKLMNOPQRSTUVW";
bool newSong = true;
int16_t volume = 50;
bool newVolume = true;
bool playing = false;
bool newPlayState = true;

int oldVolume=0;
long lastDataMills =-20000;
const long maxRefreshTime =  20000;
String metaValue = "";
String statusValue = "";
String lastAction = "";
int lastActionMillis = 0;
bool newDataNeeded = false;

/*######################### User-Interface #######################################*/
FrameCallback frames[] = { drawState, drawArtist, drawSong, drawVolume };
int frameCount = 4;
// Overlays are statically drawn on top of a frame eg. a clock
OverlayCallback overlays[] = { topBar };
int overlaysCount = 1;
int offset_y = 5;

/*######################### Webserver Specs #######################################*/
const char* host = "mischpult";
const int port = 4000;
struct URLs {
	const char* STATUS = "/api/info/status";
	const char* META = "/api/info/metadata";
	const char* PLAY = "/api/playback/play";
	const char* PAUSE = "/api/playback/pause";
	const char* NEXT = "/api/playback/next";
	const char* PREV = "/api/playback/prev";
	const char* VOL = "/api/playback/volume";
} urls;


/*##################################### Handle-Input ###########################################*/
///Return char from Serial if available. Otherwise returns 0.
char Serialread()
{
	if (Serial.available())
		return Serial.read();
	else
		return 0;
}

///gets Input an handles commands.
void handleInput()
{
	char key = getInput();

	switch (key)
	{
	case '+':
		volume += 3;
		if (volume > 100)
			volume = 100;
		map(getValue(metaValue, "volume").toInt(), 0, 65535, 1, 103) > 65535 ? 98 : volume;
		setVolume();
		lastAction = "volume up";
		break;
	case '-':
		volume -= 3;
		if (volume < 0)
			volume = 0;
		setVolume();
		lastAction = "volume down";
		break;
	case 'X':
		if (volume == 1)
			volume = oldVolume;
		else
		{
			oldVolume = volume;
			volume = 1;
		}
		setVolume();
		lastAction = "(un)mute";
		break;
	case 'N':
		next();
		break;
	case 'P':
		playPause();
		break;
	case 'B':
		prev();
		break;
	default:

		return;
	}
}


/*##################################### Commands ###########################################*/
///sends "next song"-command
void next()
{
	newArtist = false;
	newSong = false;
	lastAction = "next";
	httpRequestGET(urls.NEXT);
}

///sends "play"-command
void play()
{
	httpRequestGET(urls.PLAY);
	lastAction = "play";
}

///sends "pause"-command
void pause()
{
	httpRequestGET(urls.PAUSE);
	lastAction = "pause";
}

///sends "prev. song"-command
void prev()
{
	httpRequestGET(urls.PREV);
	newArtist = false;
	newSong = false;
	lastAction = "back";
}

///calls play/pause depending on current state.
void playPause()
{
	if (needNewData())
		getNewData();
	delay(1);
	if (!newPlayState)
	{
		String val = getValue(statusValue, "playing");
		if (val == "true")
		{
			playing = true;
			Sprintln("new State: true");
		}
		else
		{
			playing = false;
			Sprintln("new State: false");
		}
		newPlayState = true;

	}

	if (playing)
		pause();
	else
		play();
	playing = !playing;
	newPlayState = true;

}

/*##################################### HTTP Area ###########################################*/
///sends the new volume to the server.
void setVolume() {
	Sprintln(String("POST") + urls.VOL);
	
	if (!newVolume)
	{

		volume = map(getValue(metaValue, "volume").toInt(), 0, 65535, 1, 103);
		newVolume = true;
		Sprintln(volume);
		if (volume != 0)
			oldVolume = volume;
	}

	WiFiClient client;
	String msg = "";

	if (client.connect(host, 4000))
	{
		Serial.println("Connected to server");
		int outval = map(volume, 0, 100, 2, 65532);
		outval = outval > 65534 ? 65534 : outval;
		outval = outval < 0 ? 1 : outval;
		String PostData = "value=" + String(outval)+ "&";
		Sprintln(PostData);
		client.println("POST /api/playback/volume HTTP/1.1");
		client.println("Connection: close");
		client.println("Content-Type: application/x-www-form-urlencoded; charset=UTF-8");
		client.println("Host: mischpult:4000");
		client.println("Accept-Encoding: gzip");
		client.println(String("Content-Length: ") + String(PostData.length()));
		client.println();
		client.println(PostData);

		long interval = 2000;
		unsigned long currentMillis = millis(), previousMillis = millis();

		while (!client.available()) {

			if ((currentMillis - previousMillis) > interval) {

				client.stop();
				return;
			}
			currentMillis = millis();
		}

		while (client.connected())
		{
			if (client.available())
			{
				char str = client.read();
				Sprint(str);
			}
		}
		client.stop();
	}
}

///checks if the local data is outdated.
bool needNewData(bool loopRefresh = false)
{
	bool ret = false;
	//Sprintln("NeedNewData()");
	long timeDiff = millis();
	timeDiff -= lastDataMills;
	long maxTime = loopRefresh ? maxRefreshTime : maxRefreshTime / 2;

	if (timeDiff>maxTime)
	{
		lastDataMills = millis();
		ret = true;
		newDataNeeded = true;
		Sprint("needsNewData! Timediff: " + String(timeDiff)); Sprintln(" maxTime: " + String(maxTime));
	}
	else
	{
		ret = false;
		newDataNeeded = false;
	}
	return ret;
}

///requests new data and stores it only as raw data.
void getNewData()
{
	if (newDataNeeded)
	{
		newDataNeeded = false;
		lastDataMills = millis();
		//Sprintln(lastDataMills+" lastDataMills in getNewData");
		//String status = "";
		statusValue = httpRequestGET(urls.STATUS);
		delay(1);
		if (statusValue != "")
		{
			lastDataMills = millis();
			newPlayState = false;
			//statusValue = status;
			//String meta = "";
			metaValue = httpRequestGET(urls.META);
			delay(1);
			if (metaValue != "")
			{
				metaValue = metaValue;
				newArtist = false;
				newSong = false;
				newVolume = false;
			}
		}
	}
}

///performs a httpRequest an returns the server response.
String httpRequestGET(String url) {
	Sprintln("GET" + url);
	WiFiClient client;
	String msg = "";
	if (!client.connect(host, port)) {
		return "";
	}
	// This will send the request to the server
	client.println(String("GET ") + url + " HTTP/1.1");
	client.println(String("Host: ") + host);
	client.println(String("Connection: close"));
	client.println();
	unsigned long timeout = millis();
	while (client.available() == 0 && millis() - timeout < 5000) {}
	while (client.available() != 0)
	{
		msg += client.readString();
		delay(2);
	}
	client.stop();
	Sprintln(msg);
	return msg;
}

/*##################################### Draw Screens ###########################################*/
///draws a topBar containing the last command.
void topBar(OLEDDisplay *display, OLEDDisplayUiState* state) {
	display->setTextAlignment(TEXT_ALIGN_RIGHT);
	display->setFont(ArialMT_Plain_16);
	display->drawString(128, 0, lastAction);
	
	if ((millis() - lastActionMillis) > 4000)
		lastAction = "";
}

///gets value out of raw data if necessary and displays it
void drawState(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
	if (!newPlayState)
	{
		String val = getValue(statusValue, "playing");
		if (val == "true")
		{
			playing = true;
			Sprintln("new State: true");
		}
		else
		{
			playing = false;
			Sprintln("new State: false");
		}
		newPlayState = true;
		
	}

	display->setTextAlignment(TEXT_ALIGN_LEFT);
	display->setFont(ArialMT_Plain_24);
	String playMsg = "Playing" + String(playing ? " <-" : " ");
	//Sprintln(playMsg);
	display->drawString(0 + x, offset_y + y, playMsg);
	String pauseMsg = "Paused" + String(!playing ? " <-" : " ");
	//Sprintln(pauseMsg);
	display->drawString(0 + x, (offset_y + 22) + y, pauseMsg);

}

///gets value out of raw data if necessary and displays it
void drawArtist(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
	if (!newArtist)
	{
		artist = getValue(metaValue, "artist_name");
		newArtist = true;
	}
	// Demonstrates the 3 included default sizes. The fonts come from SSD1306Fonts.h file
	// Besides the default fonts there will be a program to convert TrueType fonts into this format
	display->setTextAlignment(TEXT_ALIGN_LEFT);
	display->setFont(ArialMT_Plain_24);
	drawString(artist, display, x, y);
}

///gets value out of raw data if necessary and displays it
void drawSong(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
	if (!newSong)
	{
		song = getValue(metaValue, "track_name");
		newSong = true;
	}
	// Demonstrates the 3 included default sizes. The fonts come from SSD1306Fonts.h file
	// Besides the default fonts there will be a program to convert TrueType fonts into this format
	display->setTextAlignment(TEXT_ALIGN_LEFT);
	display->setFont(ArialMT_Plain_24);
	
	drawString(song, display, x, y);
}

///gets value out of raw data if necessary and displays it
void drawVolume(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
	if (!newVolume)
	{
		
		volume = map(getValue(metaValue, "volume").toInt(),0,65535,0,100);
		newVolume = true;
		Sprintln(volume);
		if (volume != 0)
			oldVolume = volume;
	}
	display->setTextAlignment(TEXT_ALIGN_LEFT);
	display->setFont(ArialMT_Plain_24);
	// draw the progress bar
	display->drawProgressBar(0, offset_y+25, 120, 10, volume);

	// draw the percentage as String
	display->setTextAlignment(TEXT_ALIGN_CENTER);
	display->drawString(64, offset_y, "Volume:"+String(volume));
}

///handles long strings an prints them on the display
void drawString(String str, OLEDDisplay *display, int16_t x, int16_t y)
{
	if (str.length()>10)
	{
		if (str.length() > 20)
			str = str.substring(0, 20);

		if (str.indexOf(' ', str.length() - 10) != -1 && str.indexOf(' ', str.length() - 10)-1 < 9)
		{
			display->drawString(0 + x, offset_y + y, str.substring(0, str.indexOf(' ', str.length() - 10)));
			display->drawString(0 + x, (offset_y + 22) + y, str.substring(str.indexOf(' ', str.length() - 10)+1, str.length()));
		}
		else
		{
			display->drawString(0 + x, offset_y + y, str.substring(0, 9) + '-');
			display->drawString(0 + x, (offset_y + 22) + y, str.substring(9, str.length()));
		}
	}
	else
	{
		display->drawString(0 + x, (offset_y + 11) + y, str);
	}
}


/*##################################### String Tools ###########################################*/
///returs value of "search" out of v
///Example: 
///search = "artist"
///v = "{ "artist": "ACDC" }
///return = ACDC
String getValue(String v, String search) {
	Sprint("getValue: " + search);
	if (v != "")
	{
		int i = getValueIndex(v, search);
		int e = v.indexOf(',', i);
		if (v.charAt(i) == '"')
			i++;
		if (v.charAt(e - 1) == '"')
			e--;
		//Sprint("-> i: "); Sprint(i); Sprint(" - e: "); Sprintln(e);
		Sprintln(" -> Result: " + v.substring(i, e));
		return v.substring(i, e);
	}
	else
	{
		Sprintln(" -> v was empty");
		return "";
	}
	
}

///returs the end-index of the String "search" in v
int getValueIndex(String v, String search) {
	int searchlength = search.length();
	int max = v.length() - searchlength;
	int i = 0;
	for (i = 0; i <= max; i++) {
		if (v.substring(i, i+searchlength) == search)
		{
			//Sprint(" Org I: "+i);
			return i + searchlength + 3; // 3 = (": )
		}
	}
	Sprint("failed with: " + i);
	return -1;
}


/*##################################### Setup Loop ###########################################*/
///prepares everything
void setup() {
	Serial.begin(9600);

	//Display
	pinMode(0, INPUT);
	pinMode(2, INPUT);

	// We start by connecting to a WiFi network

	Sprintln();
	Sprintln();
	Sprint("Connecting to ");
	Sprintln(ssid);

	/* Explicitly set the ESP8266 to be a WiFi-client, otherwise, it by default,
	would try to act as both a client and an access-point and could cause
	network-issues with your other WiFi-devices on your WiFi-network. */

	WiFi.mode(WIFI_STA);
	WiFi.begin(ssid, password);

	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		Sprint(".");
	}

	Sprintln("");
	Sprintln("WiFi connected");
	Sprintln("IP address: ");
	Sprintln(WiFi.localIP());

	
	Sprintln();
	Sprintln();

	// The ESP is capable of rendering 60fps in 80Mhz mode
	// but that won't give you much time for anything else
	// run it in 160Mhz mode or just set it to 30 fps
	ui.setTargetFPS(25);

	// Customize the active and inactive symbol
	ui.setActiveSymbol(activeSymbol);
	ui.setInactiveSymbol(inactiveSymbol);

	// You can change this to
	// TOP, LEFT, BOTTOM, RIGHT
	ui.setIndicatorPosition(BOTTOM);

	// Defines where the first frame is located in the bar.
	ui.setIndicatorDirection(LEFT_RIGHT);

	// You can change the transition that is used
	// SLIDE_LEFT, SLIDE_RIGHT, SLIDE_UP, SLIDE_DOWN
	ui.setFrameAnimation(SLIDE_LEFT);

	// Add frames
	ui.setFrames(frames, frameCount);

	// Add overlays
	ui.setOverlays(overlays, overlaysCount);

	// Initialising the UI will init the display too.
	ui.init();

	display.flipScreenVertically();

	getNewData();
	delay(1);
}

///gets repeated every frame
void loop() {
	int remainingTimeBudget = ui.update();

	//if (remainingTimeBudget > 0)  //If uncommented the UI has got a higher prio than the Code.
		loopingCode();
	delay(1);
}

///any code that should be called frequently
void loopingCode()
{
	if (needNewData(true))
	{
		getNewData();
	}
	handleInput();
}

//Sample Server response
/*
http://mischpult:4000/api/info/status
{
"active": true,
"logged_in": true,
"playing": false,
"repeat": false,
"shuffle": true
}




http://mischpult:4000/api/info/metadata
{
"album_name": "Bills",
"album_uri": "spotify:album:6ItEjseW8RWuVTDLd1vcnp",
"artist_name": "Lunchmoney Lewis",
"artist_uri": "spotify:artist:2iUbk5KhZYZt4CRvWbwb7S",
"context_uri": "spotify:user:spotify:playlist:37i9dQZF1Cz35eQTh2fHR0",
"cover_uri": "spotify:image:6530aa2cb1109c3b1dfd3a7cf98c39dd71495351",
"data0": "Deine Top Tracks aus 2016",
"duration": 204600,
"track_name": "Bills",
"track_uri": "spotify:track:7rlCeTnjn0plPZkVAcctPZ",
"volume": 65535
}
*/