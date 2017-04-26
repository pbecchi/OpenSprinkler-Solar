/* OpenSprinkler Unified (AVR/RPI/BBB/LINUX) Firmware
 * Copyright (C) 2015 by Ray Wang (ray@opensprinkler.com)
 *
 * OpenSprinkler library
 * Feb 2015 @ OpenSprinkler.com
 *
 * This file is part of the OpenSprinkler library
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 */
#include "Defines.h"
#include "OpenSprinkler.h"
#include "Gpio.h"

#ifdef ESP8266
#ifdef SPIFFSDAT
#include <FS.h>
#define sd SPIFFS
#define FIL file
#endif
//#include "PCF8574/PCF8574.h"
#include "PCF8574Mio.h"


#endif

#ifdef PCF8574_M
PCF8574 PCF[10];//_38(0x3F);  // add switches to lines  (used as input)
 //PCF8574 PCF_39(0x3F);  // add leds to lines      (used as output)


 //
 //            pin HACKING FUNCTIONS 
 //            if PIN_NUMBER > MAX MCU PINS (16 for ESP8266) PINs are assigned to PCF 8574 I/O lines 
 //				(16 pins reserved for each IC) (in the order given by ESP8266 address)
 //
#define MAX_MCU_PINS 32
#define pinMode(x,y) if(x<MAX_MCU_PINS) pinMode(x,y);else{DEBUG_PRINT(x); DEBUG_PRINT(" at PCF_addr ");DEBUG_PRINTLN((x - MAX_MCU_PINS) / 16);}//verify pin on Ic??
#define digitalWrite(x,y) (x<MAX_MCU_PINS)? digitalWrite(x,y) :PCF[(x - MAX_MCU_PINS) / 16].write((byte)(x - MAX_MCU_PINS) % 16, y)//;DEBUG_PRINT("<W");DEBUG_PRINT(x);DEBUG_PRINT(">");DEBUG_PRINT(y) //verify pin on Ic??
#define digitalRead(x)   (x<MAX_MCU_PINS)? digitalRead(x): PCF[(x-MAX_MCU_PINS)/16].read((x-MAX_MCU_PINS)%16)
#else
#define pinMode(x, y)  pinMode(x, y)          //verify pin on Ic??
#define digitalWrite(x,y)  digitalWrite(x,y)
#define digitalRead(x)   digitalRead(x)
#endif

extern "C" {
#include "user_interface.h"
	uint16 readvdd33(void);
	bool wifi_set_sleep_type(sleep_type_t);
	sleep_type_t wifi_get_sleep_type(void); 
}
/** Declare static data members */
NVConData OpenSprinkler::nvdata;
ConStatus OpenSprinkler::status;
ConStatus OpenSprinkler::old_status;
byte OpenSprinkler::hw_type;

byte OpenSprinkler::nboards;
byte OpenSprinkler::nstations;
byte OpenSprinkler::station_bits[MAX_EXT_BOARDS+1];
#if defined(__AVR_ATmega1284P__) || defined(__AVR_ATmega1284__)
byte OpenSprinkler::engage_booster;
#endif

ulong OpenSprinkler::sensor_lasttime;
ulong OpenSprinkler::flowcount_log_start;
ulong OpenSprinkler::flowcount_rt;
ulong OpenSprinkler::flowcount_time_ms;
ulong OpenSprinkler::raindelay_start_time;
byte OpenSprinkler::button_timeout;
ulong OpenSprinkler::checkwt_lasttime;
ulong OpenSprinkler::checkwt_success_lasttime;
char tmp_buffer[TMP_BUFFER_SIZE+1];       // scratch buffer

#ifdef OPENSPRINKLER_ARDUINO_DISCRETE
int OpenSprinkler::station_pins[16] =
{
    PIN_STN_S01, PIN_STN_S02, PIN_STN_S03, PIN_STN_S04, PIN_STN_S05, PIN_STN_S06, PIN_STN_S07, PIN_STN_S08
#ifdef PIN_STN_S09
	,PIN_STN_S09, PIN_STN_S10, PIN_STN_S11, PIN_STN_S12, PIN_STN_S13, PIN_STN_S14, PIN_STN_S15, PIN_STN_S16
#endif
};
#endif // OPENSPRINKLER_ARDUINO_DISCRETE

//#ifndef OPENSPRINKLER_ARDUINO			  // Moved to opensprinkler.h
#if PROGM
prog_char wtopts_filename[] PROGMEM = WEATHER_OPTS_FILENAME;
prog_char stns_filename[]   PROGMEM = STATION_ATTR_FILENAME;
#else
char wtopts_filename[] = WEATHER_OPTS_FILENAME;
char stns_filename[]   = STATION_ATTR_FILENAME;
#endif
//#endif

#if defined(ARDUINO)

#ifdef OPENSPRINKLER_ARDUINO_FREETRONICS_LCD
LiquidCrystal OpenSprinkler::lcd ( PIN_LCD_RS, PIN_LCD_EN, PIN_LCD_D4, PIN_LCD_D5, PIN_LCD_D6, PIN_LCD_D7 );
#else
#ifdef LCDI2C

#ifdef LCD_SSD1306

Adafruit_SSD1306 OpenSprinkler::lcd(LCD_RST);


#else

#define BACKLIGHT_PIN PIN_LCD_BACKLIGHT
//LiquidCrystal_I2C	OpenSprinkler::lcd(0X27, 2, 1, 0, 4, 5, 6, 7);
LiquidCrystal_I2C OpenSprinkler::lcd(LCD_ADDR, PIN_LCD_EN, PIN_LCD_RW, PIN_LCD_RS, PIN_LCD_D4, PIN_LCD_D5, PIN_LCD_D6, PIN_LCD_D7);
#endif
#else
LiquidCrystal OpenSprinkler::lcd;
#endif
#endif

#ifdef ESP8266
#ifndef SDFAT
#include <FS.h>
//#include "SPIFFSdFat.h"
#else 
#include <SD.h>
#include <SdFat.h>
extern SdFat sd;
#endif


#endif


#elif defined(OSPI)
// 
: LCD define for OSPi
#endif

#ifdef BATTERY
 static Adafruit_INA219 ina(0x040);
#endif

#if defined(OSPI)
byte OpenSprinkler::pin_sr_data = PIN_SR_DATA;
#endif

/** Option json names (stored in progmem) */
// IMPORTANT: each json name is strictly 5 characters
// with 0 fillings if less
#define OP_JSON_NAME_STEPSIZE 5
//#ifndef OPENSPRINKLER_ARDUINO	// Moved to opensprinkler.h
#if PROGM
prog_char op_json_names[] PROGMEM =
#else
char op_json_names[] =
#endif
"fwv\0\0"
"tz\0\0\0"
"ntp\0\0"
"dhcp\0"
"ip1\0\0"
"ip2\0\0"
"ip3\0\0"
"ip4\0\0"
"gw1\0\0"
"gw2\0\0"
"gw3\0\0"
"gw4\0\0"
"hp0\0\0"
"hp1\0\0"
"hwv\0\0"
"ext\0\0"
"seq\0\0"
"sdt\0\0"
"mas\0\0"
"mton\0"
"mtof\0"
"urs\0\0"
"rso\0\0"
"wl\0\0\0"
"den\0\0"
"ipas\0"
"devid"
"con\0\0"
"lit\0\0"
"dim\0\0"
"bst\0\0"
"uwt\0\0"
"ntp1\0"
"ntp2\0"
"ntp3\0"
"ntp4\0"
"lg\0\0\0"
"mas2\0"
"mton2"
"mtof2"
"fwm\0\0"
"fpr0\0"
"fpr1\0"
"re\0\0\0"
"reset"
#ifdef BATTERY
"DsleD"
"DsleS"
"DsleI"
"DsleV"
"LsleV"
"LsleP"
"batAh"
#endif
;
// OPENSPRINKLER_ARDUINO

/** Option promopts (stored in progmem, for LCD display) */
// Each string is strictly 16 characters
// with SPACE fillings if less
#if PROGM
prog_char op_prompts[] /*PROGMEM*/ =
#else
char op_prompts[] =
#endif
    "Firmware version"
    "Time zone (GMT):"
    "Enable NTP sync?"
    "Enable DHCP?    "
    "Static.ip1:     "
    "Static.ip2:     "
    "Static.ip3:     "
    "Static.ip4:     "
    "Gateway.ip1:    "
    "Gateway.ip2:    "
    "Gateway.ip3:    "
    "Gateway.ip4:    "
    "HTTP Port:      "
    "----------------"
    "Hardware version"
    "# of exp. board:"
    "----------------"
    "Stn. delay (sec)"
    "Master 1 (Mas1):"
    "Mas1  on adjust:"
    "Mas1 off adjust:"
    "Sensor type:    "
    "Normally open?  "
    "Watering level: "
    "Device enabled? "
    "Ignore password?"
    "Device ID:      "
    "LCD contrast:   "
    "LCD brightness: "
    "LCD dimming:    "
    "DC boost time:  "
    "Weather algo.:  "
    "NTP server.ip1: "
    "NTP server.ip2: "
    "NTP server.ip3: "
    "NTP server.ip4: "
    "Enable logging? "
    "Master 2 (Mas2):"
    "Mas2  on adjust:"
    "Mas2 off adjust:"
    "Firmware minor: "
    "Pulse rate:     "
    "----------------"
    "As remote ext.? "
    "Factory reset?  "
#ifdef BATTERY
    "deep sleep dur. "					
	"deep sleep start"					
	"deep sleep inter"					
	"min bat v Dsleep"                 
	"min V L sleep   " 				
	"lightslepp delay"			
	"battery capacity"			
#endif
	;

/** Option maximum values (stored in progmem) */

//#ifndef OPENSPRINKLER_ARDUINO	// Moved to opensprinkler.h
#if PROGM
prog_char op_max[] PROGMEM =
{
#else
char op_max[] =
{
#endif
    0,
    108,
    1,
    1,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    255,
    0,
    MAX_EXT_BOARDS,
    1,
    247,
    MAX_NUM_STATIONS,
    60,
    120,
    255,
    1,
    250,
    1,
    1,
    255,
    255,
    255,
    255,
    250,
    255,
    255,
    255,
    255,
    255,
    1,
    MAX_NUM_STATIONS,
    60,
    120,
    0,
    255,
    255,
    1,
    1
#ifdef BATTERY
	, 255,  //s deep sleep duration						D_SLEEP_DURATION
	255, //min deep sleep start							D_SLEEP_START_TIME
	255,  //min deep sleep interval						D_SLEEP_INTERVAL
	255,  //mV minimum battery voltage start of deep sleeMIN_VOLTS_D_SLEEP
	255,  //mV voltage start of light sleep				MIN_VOLTS_L_SLEEP
	255,    //s light sleep delay time						L_SLEEP_DURATION
	255  //mAh battery capacity							BATTERY_mAH
#endif
};
//#endif // OPENSPRINKLER_ARDUINO

/** Option values (stored in RAM) */
byte OpenSprinkler::options[] =
{
    OS_FW_VERSION, // firmware version
    28, // default time zone: GMT-5
    1,  // 0: disable NTP sync, 1: enable NTP sync
    1,  // 0: use static ip, 1: use dhcp
    0,  // this and next 3 bytes define static ip
    0,
    0,
    0,
    0,  // this and next 3 bytes define static gateway ip
    0,
    0,
    0,
#if defined(ARDUINO)  // on AVR, the default HTTP port is 80
    80, // this and next byte define http port number
    0,
#else // on RPI/BBB/LINUX, the default HTTP port is 8080
    144,// this and next byte define http port number
    31,
#endif
    OS_HW_VERSION,
    0,  // number of 8-station extension board. 0: no extension boards
    1,  // the option 'sequential' is now retired
    128,// station delay time (-59 minutes to 59 minutes).
    0,  // index of master station. 0: no master station
    0,  // master on time [0,60] seconds
    60, // master off time [-60,60] seconds
    0,  // sensor function (see SENSOR_TYPE macro defines)
    0,  // rain sensor type. 0: normally closed; 1: normally open.
    100,// water level (default 100%),
    1,  // device enable
    0,  // 1: ignore password; 0: use password
    0,  // device id
    150,// lcd contrast
    100,// lcd backlight
    15, // lcd dimming
    80, // boost time (only valid to DC and LATCH type)
    0,  // weather algorithm (0 means not using weather algorithm)
    132, // this and the next three bytes define the ntp server ip
    163,
    4,
    101,
    1,  // enable logging: 0: disable; 1: enable.
    0,  // index of master 2. 0: no master2 station
    0,
    60,
    OS_FW_MINOR, // firmware minor version
    100,// this and next byte define flow pulse rate (100x)
    0,
    0,  // set as remote extension
    0	   // reset

		   //
#ifdef BATTERY
	,
// options for battery operated
 60,    //m deep sleep duration						D_SLEEP_DURATION
 22,    //h deep sleep start							D_SLEEP_START_TIME
 8,     //h deep sleep interval						D_SLEEP_INTERVAL
 32,    //V*10 minimum battery voltage start of deep sleeMIN_VOLTS_D_SLEEP
 34,    //V*10 voltage start of light sleep				MIN_VOLTS_L_SLEEP
 30,   //s light sleep delay time						L_SLEEP_DURATION
 48    //10*Ah battery capacity							BATTERY_mAH
//
#endif
};
static float charge=0;
unsigned long chargeTime=0;
/** Weekday strings (stored in progmem, for LCD display) */
static prog_char days_str[]/* PROGMEM*/ =
    "Mon\0"
    "Tue\0"
    "Wed\0"
    "Thu\0"
    "Fri\0"
    "Sat\0"
    "Sun\0";

/** Calculate local time (UTC time plus time zone offset) */
time_t OpenSprinkler::now_tz()
{
    return now()+ ( int32_t ) 3600/4* ( int32_t ) ( options[OPTION_TIMEZONE]-48 );
}

#if defined(ARDUINO)  // AVR network init functions

//static PCF8574 PCF[10];


enum I2Cdev_t
{
	PCF8574_TYPE,
	PCF8574A_TYPE,
	RTC_TYPE,
	EEPROM_TYPE,
	
};
byte ADR[4] = {
	0x20,
	0x38,
	0x64,
	0x50
};
byte ADR_R[4] = {
	8,
	8,
	16,
	8
};
bool ADR_TYP[4] = {
	true,
	true,
	false,
	false
};
String ADR_DEV_NAME[4] = {

	"PCF8574",
	"PCF8574A",
	"DS3231",
	"AT24Cxx"
};
void ScanI2c()
{
	byte address[20];
	byte error;
	int nDevices;

	Serial.println("Scanning..I2C.....");

	nDevices = 0;
	for (byte addres = 1; addres < 127; addres++)
	{
		// The i2c_scanner uses the return value of
		// the Write.endTransmisstion to see if
		// a device did acknowledge to the address.
		Wire.beginTransmission(addres);
		error = Wire.endTransmission();
		bool trov = false;
		byte trova = 255;
		if (error == 0)
		{
			Serial.print("I2C device found at address 0x");
			if (addres < 16)
				Serial.print("0");
			Serial.print(addres, HEX);
			Serial.print("\t");
			byte id = 0;
			while (id < 4) {
				if (addres >= ADR[id] && addres < ADR[id] + ADR_R[id]) {
					Serial.println(ADR_DEV_NAME[id]); trov = true;
					trova = id;

				}
				id++;
			}
			if (!trov) Serial.println(" unknown");
		}
		if (trova != 255&&trov)
			if (ADR_TYP[trova]) {
#ifdef LCD_ADDR                                               //do not count LCD controller if LCD i2c library is used
		        if (addres == LCD_ADDR)Serial.println("found LCD Controller"); 
				else
#endif
				{
					address[nDevices] = addres;
					nDevices++;
#ifdef PCF8574_M
					DEBUG_PRINT("PCF expander "); DEBUG_PRINT(nDevices - 1); DEBUG_PRINT("at"); DEBUG_PRINTLN(addres);
					PCF[nDevices - 1].begin(addres);
#else
					Serial.println("error Pcf_dev set: #define PCF8574_M in Pins.h");
#endif
				}
			}
			else if (error == 4)
			{
				Serial.print("Unknow error at address 0x");
				if (address[nDevices]<16)
					Serial.print("0");
				Serial.println(address[nDevices], HEX);
			}
	}
	if (nDevices == 0)
	{
#ifdef PCF8574_M
		Serial.println("No PCF8574 devices found\n cannot continue!");
		write_message("No PCF devices found!!");
		delay(60000);
#endif
	}
	else
		Serial.println("done\n");

	//	return nDevices;
}

/** read hardware MAC */
#define MAC_CTRL_ID 0x50
bool OpenSprinkler::read_hardware_mac()
{
    uint8_t ret;
    Wire.beginTransmission ( MAC_CTRL_ID );
    Wire.write ( ( uint8_t ) ( 0x00 ) );
    ret = Wire.endTransmission();
    if ( ret )  return false;

    Wire.beginTransmission ( MAC_CTRL_ID );
    Wire.write ( 0xFA ); // The address of the register we want
    Wire.endTransmission(); // Send the data
    Wire.requestFrom ( MAC_CTRL_ID, 6 ); // Request 6 bytes from the EEPROM
    while ( !Wire.available() ); // Wait for the response
    for ( ret=0; ret<6; ret++ )
    {
        tmp_buffer[ret] = Wire.read();
    }
    return true;
}

void ( * resetFunc ) ( void ) = 0; // AVR software reset function

/** Initialize network with the given mac address and http port */
byte OpenSprinkler::start_network( )
{

    lcd_print_line_clear_pgm ( PSTR ( "Connecting..." ), 1 );
    DEBUG_PRINTLN ( F ( "Connecting..." ) );

    // new from 2.2: read hardware MAC
#ifdef OPENSPRINKLER_ARDUINO
    // always use software MAC
    DEBUG_PRINTLN ( F ( "Setting software MAC " ) );
    tmp_buffer[0] = 0x00;
    tmp_buffer[1] = 0x69;
    tmp_buffer[2] = 0x69;
    tmp_buffer[3] = 0x2D;
    tmp_buffer[4] = 0x31;
    tmp_buffer[5] = options[OPTION_DEVICE_ID];
#else
    if ( !read_hardware_mac() )
    {
        // if no hardware MAC exists, use software MAC
        tmp_buffer[0] = 0x00;
        tmp_buffer[1] = 0x69;
        tmp_buffer[2] = 0x69;
        tmp_buffer[3] = 0x2D;
        tmp_buffer[4] = 0x31;
        tmp_buffer[5] = options[OPTION_DEVICE_ID];
    }
    else
    {
        // has hardware MAC chip
        status.has_hwmac = 1;
    }
#endif

    if ( !ether.begin ( ETHER_BUFFER_SIZE, ( uint8_t* ) tmp_buffer, PIN_ETHER_CS ) )
    {
        DEBUG_PRINTLN ( F ( "Ethernet initialisation failed - check wiring" ) );
        return 0;
    }
    else
    {
        DEBUG_PRINTLN ( F ( "Ethernet initialised" ) );
    }

    // calculate http port number
    ether.hisport = ( unsigned int ) ( options[OPTION_HTTPPORT_1]<<8 ) + ( unsigned int ) options[OPTION_HTTPPORT_0];
    DEBUG_PRINT ( F ( "Using http port " ) );
    DEBUG_PRINTLN ( ether.hisport );
//	write_message("connected");
    if ( options[OPTION_USE_DHCP] )
    {
        // set up DHCP
        // register with domain name "OS-xx" where xx is the last byte of the MAC address
		DEBUG_PRINT(F("DHCP IP setup"));
        if ( !ether.dhcpSetup() )
        {
            DEBUG_PRINTLN ( F ( " failed" ) );
			write_message("DHCP conn. failed");
            return 0;
        }

        // once we have valid DHCP IP, we write these into static IP / gateway IP
        byte *ip = ether.myip;
        options[OPTION_STATIC_IP1] = ip[0];
        options[OPTION_STATIC_IP2] = ip[1];
        options[OPTION_STATIC_IP3] = ip[2];
        options[OPTION_STATIC_IP4] = ip[3];

        ip = ether.gwip;
        options[OPTION_GATEWAY_IP1] = ip[0];
        options[OPTION_GATEWAY_IP2] = ip[1];
        options[OPTION_GATEWAY_IP3] = ip[2];
        options[OPTION_GATEWAY_IP4] = ip[3];
        options_save();
        DEBUG_PRINTLN ( F ( "DHCP IP setup successful" ) );
    }
    else
    {
        // set up static IP
        byte staticip[] =
        {
            options[OPTION_STATIC_IP1],
            options[OPTION_STATIC_IP2],
            options[OPTION_STATIC_IP3],
            options[OPTION_STATIC_IP4]
        };

        byte gateway[] =
        {
            options[OPTION_GATEWAY_IP1],
            options[OPTION_GATEWAY_IP2],
            options[OPTION_GATEWAY_IP3],
            options[OPTION_GATEWAY_IP4]
        };

		DEBUG_PRINT(F("Static IP setup "));

        if ( !ether.staticSetup ( staticip, gateway, gateway ) )
        {
            DEBUG_PRINTLN ( F ( "failed" ) );
			write_message("Static conn. failed");
            return 0;
        }
        else
        {
            DEBUG_PRINTLN ( F ( "successful" ) );
        }
    }
    DEBUG_PRINTLN ( F ( "Ethernet setup complete" ) );
    return 1;
}

/** Reboot controller */
void OpenSprinkler::reboot_dev()
{
#ifdef ESP8266
//	eeprom_write_byte((unsigned char *)(NVM_SIZE - 1), byte(charge / 25));
	write_message("restart");
	
	ESP.restart();
#else

    resetFunc();
#endif
}

#else // RPI/BBB/LINUX network init functions

#include "etherport.h"
#include <sys/reboot.h>
#include <stdlib.h>
#include "utils.h"
#include "server.h"

extern EthernetServer *m_server;
extern char ether_buffer[];

/** Initialize network with the given mac address and http port */
byte OpenSprinkler::start_network()
{
    unsigned int port = ( unsigned int ) ( options[OPTION_HTTPPORT_1]<<8 ) + ( unsigned int ) options[OPTION_HTTPPORT_0];
#if defined(DEMO)
    port = 80;
#endif
    if ( m_server )
    {
        delete m_server;
        m_server = 0;
    }

    m_server = new EthernetServer ( port );
    return m_server->begin();
}

/** Reboot controller */
void OpenSprinkler::reboot_dev()
{
#if defined(DEMO)
    // do nothing
#else
    sync(); // add sync to prevent file corruption
    reboot ( RB_AUTOBOOT );
#endif
}

LiquidCrystal_I2C OpenSprinkler::lcd(byte a, byte b, byte c, byte d, byte e, byte f, byte g, byte l)
{
	return LiquidCrystal_I2C();
}

/** Launch update script */
void OpenSprinkler::update_dev()
{
    char cmd[1024];
    sprintf ( cmd, "cd %s & ./updater.sh", get_runtime_path() );
    system ( cmd );
}
#endif // end network init functions

#if defined(ARDUINO)
/** Initialize LCD */
void OpenSprinkler::lcd_start()
{
#ifdef OPENSPRINKLER_ARDUINO_FREETRONICS_LCD
    //set up the LCD number of columns and rows:
    lcd.begin ( 16, 2 );
#else
    // turn on lcd
#ifndef LCDI2C
    lcd.init ( 1, PIN_LCD_RS, 255, PIN_LCD_EN, PIN_LCD_D4, PIN_LCD_D5, PIN_LCD_D6, PIN_LCD_D7, 0,0,0,0 );
    lcd.begin();

    if ( lcd.type() == LCD_STD )
    {
        // this is standard 16x2 LCD
        // set PWM frequency for adjustable LCD backlight and contrast
#if OS_HW_VERSION==(OS_HW_VERSION_BASE+20) || OS_HW_VERSION==(OS_HW_VERSION_BASE+21)  // 8MHz and 12MHz
        TCCR1B = 0x01;
#else // 16MHz
        TCCR1B = 0x02;  // increase division factor for faster clock
#endif
        // turn on LCD backlight and contrast
        lcd_set_brightness();
        lcd_set_contrast();
    }
    else
    {
        // for I2C LCD, we don't need to do anything
    }
#else  //for i2c library have to turn backligth on 

	
#ifndef LCD_SSD1306
	lcd.begin(20, 4);
	lcd.setBacklightPin(BACKLIGHT_PIN, PIN_BACKLIGHT_MODE);
	lcd.setBacklight(HIGH);
	lcd.home();
	lcd.print("start..V.2.1.6");

#else
	lcd.begin(SSD1306_SWITCHCAPVCC, 0x3c);
	lcd.display();
	delay(2000);
	lcd.clearDisplay();
	lcd.setTextColor(WHITE,BLACK); // white char clear background
	lcd.print("start..V.2.1.6");
	lcd.display();
#endif

	
#endif
#endif
}
#endif

extern void flow_isr();
/** Initialize pins, controller variables, LCD */
void OpenSprinkler::begin()
{
	// Init I2C
#ifdef ESP8266
	Wire.begin(SDA_PIN, SCL_PIN);
	DEBUG_PRINTLN("wire begin");
#else
	Wire.begin();
#endif

	ScanI2c();

#ifdef OPENSPRINKLER_ARDUINO_DISCRETE
    // initialize the Digital IO pins as outputs:
    for ( int i = 0; i < ( PIN_EXT_BOARDS * 8 ); i++ )
        pinMode ( station_pins[i], OUTPUT );
#else
    // shift register setup
#ifdef PIN_SSR_OE
    PINMODE ( PIN_SR_OE, OUTPUT );

    // pull shift register OE high to disable output
    digitalWrite ( PIN_SR_OE, HIGH );
#endif
	DEBUG_PRINTLN(PIN_SR_LATCH);
    pinMode ( PIN_SR_LATCH, OUTPUT );
    digitalWrite ( PIN_SR_LATCH, HIGH );
    pinMode ( PIN_SR_CLOCK, OUTPUT );
#endif // OPENSPRINKLER_ARDUINO_DISCRETE

#if defined(OSPI)
    pin_sr_data = PIN_SR_DATA;
    // detect RPi revisionOPENSPRINKLER_ARDUINO_DISCRETE
    unsigned int rev = detect_rpi_rev();
    if ( rev==0x0002 || rev==0x0003 )
        pin_sr_data = PIN_SR_DATA_ALT;
    // if this is revision 1, use PIN_SR_DATA_ALT
    PINMODE ( pin_sr_data, OUTPUT );
#else
#ifndef OPENSPRINKLER_ARDUINO_DISCRETE
    pinMode ( PIN_SR_DATA,  OUTPUT );
#endif
#endif

    // Reset all stations
    clear_all_station_bits();
    apply_all_station_bits();

#ifndef OPENSPRINKLER_ARDUINO_DISCRETE
#ifdef PIN_SR_OE
	// pull shift register OE low to enable output
    digitalWrite ( PIN_SR_OE, LOW );
#endif
#endif // OPENSPRINKLER_ARDUINO_DISCRETE
#ifdef PIN_RAINSENSOR
    // Rain sensor port set up
    pinMode ( PIN_RAINSENSOR, INPUT );

    // Set up sensors
#if defined(ARDUINO)
	digitalWrite ( PIN_RAINSENSOR, HIGH ); // enabled internal pullup on rain sensor
    attachInterrupt ( PIN_FLOWSENSOR_INT, flow_isr, FALLING );
#else
    // OSPI and OSBO use external pullups
    attachInterrupt ( PIN_FLOWSENSOR, "falling", flow_isr );
#endif
#endif
    // Default controller status variables
    // Static variables are assigned 0 by default
    // so only need to initialize non-zero ones
    status.enabled = 1;
    status.safe_reboot = 0;

    old_status = status;

    nvdata.sunrise_time = 360;  // 6:00am default sunrise
    nvdata.sunset_time = 1080;  // 6:00pm default sunset

    nboards = 1;
    nstations = 8;

#ifdef PIN_RF_DATA
    // set rf data pin
    PINMODE ( PIN_RF_DATA, OUTPUT );
    digitalWrite ( PIN_RF_DATA, LOW );
#endif

    hw_type = HW_TYPE_AC;
#if defined(ARDUINO)  // AVR SD and LCD functions
 
	

#if defined(__AVR_ATmega1284P__) || defined(__AVR_ATmega1284__) // OS 2.3 specific detections
    uint8_t ret;

    // detect hardware type
    Wire.beginTransmission ( MAC_CTRL_ID );
    Wire.write ( 0x00 );
    ret = Wire.endTransmission();
    if ( !ret )
    {
        Wire.requestFrom ( MAC_CTRL_ID, 1 );
        while ( !Wire.available() );
        ret = Wire.read();
        if ( ret == HW_TYPE_AC || ret == HW_TYPE_DC || ret == HW_TYPE_LATCH )
        {
            hw_type = ret;
        }
        else
        {
            // hardware type is not assigned
        }
    }

    if ( hw_type == HW_TYPE_DC )
    {
        PINMODE ( PIN_BOOST, OUTPUT );
        digitalWrite ( PIN_BOOST, LOW );

        PINMODE ( PIN_BOOST_EN, OUTPUT );
        digitalWrite ( PIN_BOOST_EN, LOW );
    }

    // detect if current sensing pin is present
    PINMODE ( PIN_CURR_DIGITAL, INPUT );
    digitalWrite ( PIN_CURR_DIGITAL, HIGH ); // enable internal pullup
    status.has_curr_sense = digitalRead ( PIN_CURR_DIGITAL ) ? 0 : 1;
    digitalWrite ( PIN_CURR_DIGITAL, LOW );
#endif

    lcd_start();
#ifndef LCD_SSD1306
    // define lcd custom icons
    byte _icon[8];
    // WiFi icon
    _icon[0] = B00000;
    _icon[1] = B10100;
    _icon[2] = B01000;
    _icon[3] = B10101;
    _icon[4] = B00001;
    _icon[5] = B00101;
    _icon[6] = B00101;
    _icon[7] = B10101;
    lcd.createChar ( 1, _icon );

    _icon[1]=0;
    _icon[2]=0;
    _icon[3]=1;
    lcd.createChar ( 0, _icon );

    // uSD card icon
    _icon[1] = B00000;
    _icon[2] = B11111;
    _icon[3] = B10001;
    _icon[4] = B11111;
    _icon[5] = B10001;
    _icon[6] = B10011;
    _icon[7] = B11110;
    lcd.createChar ( 2, _icon );

    // Rain icon
    _icon[2] = B00110;
    _icon[3] = B01001;
    _icon[4] = B11111;
    _icon[5] = B00000;
    _icon[6] = B10101;
    _icon[7] = B10101;
    lcd.createChar ( 3, _icon );

    // Connect icon
    _icon[2] = B00111;
    _icon[3] = B00011;
    _icon[4] = B00101;
    _icon[5] = B01000;
    _icon[6] = B10000;
    _icon[7] = B00000;
    lcd.createChar ( 4, _icon );

    // Remote extension icon
    _icon[2] = B00000;
    _icon[3] = B10001;
    _icon[4] = B01011;
    _icon[5] = B00101;
    _icon[6] = B01001;
    _icon[7] = B11110;
    lcd.createChar ( 5, _icon );

    // Flow sensor icon
    _icon[2] = B00000;
    _icon[3] = B11010;
    _icon[4] = B10010;
    _icon[5] = B11010;
    _icon[6] = B10011;
    _icon[7] = B00000;
    lcd.createChar ( 6, _icon );

#endif
#ifdef SDFAT 

    // set sd cs pin high to release SD
    pinMode ( PIN_SD_CS, OUTPUT );
    digitalWrite ( PIN_SD_CS, HIGH );
#ifndef ESP8266
    if ( sd.begin(PIN_SD_CS, SPI_HALF_SPEED))
#else
	if(SD.begin(PIN_SD_CS, SPI_HALF_SPEED))
#endif
    {
        status.has_sd = 1;
    }


#elif defined(SPIFFSDFAT)	
	DEBUG_PRINTLN("Start SPIFFS....");
	if (SPIFFS.begin()) {
		Dir dir = SPIFFS.openDir("/");
		while (dir.next())DEBUG_PRINTLN(dir.fileName());
		status.has_sd = 1;
	}
	else DEBUG_PRINTLN("Coundn't start SPIFFS");
#endif


#ifdef BUTTON_ADC_PIN
    pinMode ( BUTTON_ADC_PIN, INPUT );
#else
    // set button pins
    // enable internal pullup
    pinMode ( PIN_BUTTON_1, BUT1_ON == 0 ? INPUT_PULLUP : INPUT);
    pinMode ( PIN_BUTTON_2,  BUT2_ON == 0 ? INPUT_PULLUP : INPUT);
    pinMode ( PIN_BUTTON_3,BUT3_ON==0? INPUT_PULLUP:INPUT );
	DEBUG_PRINT("b1="); DEBUG_PRINTLN(digitalRead(PIN_BUTTON_1));
	DEBUG_PRINT("b2="); DEBUG_PRINTLN(digitalRead(PIN_BUTTON_2));
	DEBUG_PRINT("b3="); DEBUG_PRINTLN(digitalRead(PIN_BUTTON_3));
 //   DIGITALWRITE ( PIN_BUTTON_1, HIGH );
 //   DIGITALWRITE ( PIN_BUTTON_2, HIGH );
 //   DIGITALWRITE ( PIN_BUTTON_3, HIGH );
#endif // OPENSPRINKLER_ARDUINO_FREETRONICS_LCD

    // detect and check RTC type
#ifndef OPENSPRINKLER_ARDUINO
    RTC.detect();
#endif // OPENSPRINKLER_ARDUINO
#else
    DEBUG_PRINTLN ( get_runtime_path() );
#endif
}
////////////////////////////////code for OS bee 2.0///////////////


#ifdef OSBEE
///////////////////////defines for OS Bee////////////////////////////////////////////////////
//#define MAX_NUMBER_ZONES		3	//num.of zones_______________________needed
#define st_pins OpenSprinkler::station_pins		//pins used for attached Zones
//#define PIN_BST_PWR				2	//pin boost power____________________needed for OS Bee 2.0
//#define PIN_BST_EN				3	//bin boost enable___________________needed
//#define OSB_SOT_LATCH			4	//value of option for non latching valves___for OS Bee 2.0
////////////////////////////////////////////////////////////////////////////////////////////////

// open a zone by zone index
/* Set all pins (including COM)
* to a given value
*/
void setallpins(byte value) {
	static byte first = true;
	if (first) {
		first = false;
		pinMode(PIN_COM, OUTPUT);
		pinMode(PIN_BST_EN, OUTPUT);
		pinMode(PIN_BST_PWR, OUTPUT);
		for (byte i = 0; i < MAX_NUMBER_ZONES; i++) {
			//		pinMode(st_pins[i], OUTPUT);
		}
	}
		DEBUG_PRINTLN(value); DEBUG_PRINT("set all pin:");
	digitalWrite(PIN_COM, value);
	for (byte i = 0; i<MAX_NUMBER_ZONES; i++) {
		digitalWrite(st_pins[i], value);
	DEBUG_PRINT(st_pins[i]); DEBUG_PRINT(" ");
		delay(100);
	}
	DEBUG_PRINTLN(" done");
	
	
}
#if OSBEE==0
void boost(){
	// turn on boost converter for 500ms
	digitalWrite(PIN_BST_PWR,HIGH);
	delay(500);
	digitalWrite(PIN_BST_PWR, LOW);
}
void BeeOpen(byte zid) {
	byte pin = st_pins[zid];
	//  all pin set to v+
		// for latching solenoid
		boost();							// boost voltage
		setallpins(STA_HIGH);				// set all switches to HIGH, including COM
		delay(500);
		digitalWrite(pin, STA_LOW);			// set the specified switch to LOW
		digitalWrite(PIN_BST_EN, STA_HIGH);		// dump boosted voltage
		DEBUG_PRINT("Boost..");
		delay(200);		// for 250ms
		digitalWrite(PIN_BST_EN, STA_LOW);  // disable boosted voltage
		DEBUG_PRINTLN("..sent");
		digitalWrite(pin, STA_HIGH);        // set the specified switch back to HIGH
		delay(500);
		setallpins(STA_LOW);			// back to rest
}

// close a zone
void BeeClose(byte zid) {
	byte pin = st_pins[zid];
		// for latching solenoid
		boost();  // boost voltage
		setallpins(STA_LOW);        // set all switches to LOW, including COM
		delay(500);
		digitalWrite(pin, STA_HIGH);// set the specified switch to HIGH
		digitalWrite(PIN_BST_EN, STA_HIGH); // dump boosted voltage
		DEBUG_PRINT("Boost..");
		
		delay(200);                     // for 250ms
		digitalWrite(PIN_BST_EN, STA_LOW);  // disable boosted voltage
		DEBUG_PRINTLN("..sent");

		digitalWrite(pin, STA_LOW);     // set the specified switch back to LOW
	//	delay(500);
	//	setallpins(STA_LOW);               // set all switches back to rest
}
#else
void OSBeeWiFi::boost() {
	// turn on boost converter for 500ms
	digitalWrite(PIN_BST_PWR, HIGH);
	delay(500);
	digitalWrite(PIN_BST_PWR, LOW);
}

// open a zone by zone index
void BeeOpen(byte zid) {
	byte pin = st_pins[zid];
	if (options[OPTION_SOT].ival == OSB_SOT_LATCH) {
		// for latching solenoid
		boost();  // boost voltage
		setallpins(HIGH);       // set all switches to HIGH, including COM
		digitalWrite(pin, LOW); // set the specified switch to LOW
		digitalWrite(PIN_BST_EN, HIGH); // dump boosted voltage
		delay(100);                     // for 250ms
		digitalWrite(pin, HIGH);        // set the specified switch back to HIGH
		digitalWrite(PIN_BST_EN, LOW);  // disable boosted voltage
	}
	else {
		DEBUG_PRINT("open_nl ");
		DEBUG_PRINTLN(zid);
		// for non-latching solenoid
		digitalWrite(PIN_BST_EN, LOW);  // disable output of boosted voltage 
		boost();                        // boost voltage
		digitalWrite(PIN_COM, HIGH);    // set COM to HIGH
		digitalWrite(PIN_BST_EN, HIGH); // dump boosted voltage    
		digitalWrite(pin, LOW);         // set specified switch to LOW
	}
}

// close a zone
void BeeClose(byte zid) {
	byte pin = st_pins[zid];
	if (options[OPTION_SOT].ival == OSB_SOT_LATCH) {
		// for latching solenoid
		boost();  // boost voltage
		setallpins(LOW);        // set all switches to LOW, including COM
		digitalWrite(pin, HIGH);// set the specified switch to HIGH
		digitalWrite(PIN_BST_EN, HIGH); // dump boosted voltage
		delay(100);                     // for 250ms
		digitalWrite(pin, LOW);     // set the specified switch back to LOW
		digitalWrite(PIN_BST_EN, LOW);  // disable boosted voltage
		setallpins(HIGH);               // set all switches back to HIGH
	}
	else {
		DEBUG_PRINT("close_nl ");
		DEBUG_PRINTLN(zid);
		// for non-latching solenoid
		digitalWrite(pin, HIGH);
	}
}
#endif
#endif

//////////////////////////////////////////////////////////////////////////////////////////////////
/** Apply all station bits
 * !!! This will activate/deactivate valves !!!
 */
static byte prev_bits = 0xF;
void OpenSprinkler::apply_all_station_bits()
{

#ifdef OPENSPRINKLER_ARDUINO_DISCRETE //<MOD> ===== Digital Outputs 
    byte bid, s, sbits;

    // Shift out all station bit values from the highest bit to the lowest
    for ( bid = 0; bid <= MAX_EXT_BOARDS; bid++ )
    {

        if ( status.enabled )
            sbits = station_bits[MAX_EXT_BOARDS - bid];
	//	DEBUG_PRINT(int(bid)); DEBUG_PRINT("sb="); DEBUG_PRINTLN(int(sbits));
        else
            sbits = 0;
	//  PROCESS ONLY IF BITS ARE CHANGED !!!!!!!!!!!!!!!!!!!!!!!!
		if (bid == MAX_EXT_BOARDS) { //only for this station not for extension or remotes
			if (sbits == prev_bits) return;
			
		} 
        // Check that we're switching physical discretes within the range defined
		if (MAX_EXT_BOARDS - bid < PIN_EXT_BOARDS)
		{
			
			for (s = 0; s < 8; s++)
			{
#ifdef OSBEE
				if (bid == MAX_EXT_BOARDS) {
					if ((sbits & ((byte)1 << (7 - s))) != (prev_bits & ((byte)1 << (7 - s))))
						if (sbits & ((byte)1 << (7 - s)))
						{
							BeeOpen(s);
							DEBUG_PRINT("============open "); DEBUG_PRINTLN(s);
						//	lcd.setCursor(s * 10 + 20, 30);
						//	lcd.print('*');
						}
						else
						{
							BeeClose(s);
							DEBUG_PRINT("===========closed "); DEBUG_PRINTLN(s);
						//	lcd.setCursor(s * 10 + 20, 30);
						//	lcd.print(' ');
						}
				}
				else
#endif
				{
					//	DEBUG_PRINT(station_pins[(bid * 8) + s]);
					byte sBit = (sbits & ((byte)1 << (7 - s))) ? STA_HIGH : STA_LOW;
					//DEBUG_PRINTF(sBit,DEC);
					digitalWrite(station_pins[((MAX_EXT_BOARDS - bid) * 8) + s], sBit);
			    }

			}
			if (bid == MAX_EXT_BOARDS) {
				DEBUG_PRINT("s "); DEBUG_PRINT(bid); DEBUG_PRINT(" b = "); DEBUG_PRINT(int(prev_bits)); DEBUG_PRINT("  "); DEBUG_PRINTLN(int(sbits));
				prev_bits = sbits;
			}
		}
		
    }
	
#else
    digitalWrite ( PIN_SR_LATCH, LOW );
    byte bid, s, sbits;

    // Shift out all station bit values
    // from the highest bit to the lowest
    for ( bid=0; bid<=MAX_EXT_BOARDS; bid++ )
    {
        if ( status.enabled )
            sbits = station_bits[MAX_EXT_BOARDS-bid];
        else
            sbits = 0;

        for ( s=0; s<8; s++ )
        {
            digitalWrite ( PIN_SR_CLOCK, LOW );
#if defined(OSPI) // if OSPI, use dynamically assigned pin_sr_data
            digitalWrite ( pin_sr_data, ( sbits & ( ( byte ) 1<< ( 7-s ) ) ) ? HIGH : LOW );
#endif
#ifndef OSPI
			byte y = (sbits & ((byte)1 << (7 - s))) ? HIGH : LOW;
			
			digitalWrite ( PIN_SR_DATA, (sbits & ((byte)1 << (7 - s))) ? HIGH : LOW);
#endif
            digitalWrite ( PIN_SR_CLOCK, HIGH );
        }
    }

#if defined(__AVR_ATmega1284P__) || defined(__AVR_ATmega1284__)
    if ( ( hw_type==HW_TYPE_DC ) && engage_booster )
    {
        DEBUG_PRINTLN ( F ( "engage booster" ) );
        // for DC controller: boost voltage
        digitalWrite ( PIN_BOOST_EN, LOW ); // disable output path
        digitalWrite ( PIN_BOOST, HIGH ); // enable boost converter
        delay ( ( int ) options[OPTION_BOOST_TIME]<<2 ); // wait for booster to charge
        digitalWrite ( PIN_BOOST, LOW );  // disable boost converter
        digitalWrite ( PIN_BOOST_EN, HIGH ); // enable output path
        digitalWrite ( PIN_SR_LATCH, HIGH );
        engage_booster = 0;
    }
    else
    {
        digitalWrite ( PIN_SR_LATCH, HIGH );
    }
#else
    digitalWrite ( PIN_SR_LATCH, HIGH );
#endif
#endif // OPENSPRINKLER_ARDUINO_DISCRETE

    // handle refresh of RF and remote stations
    // each time apply_all_station_bits is called
    // we refresh the station whose index is the current time modulo MAX_NUM_STATIONS
    static byte last_sid = 0;
    byte sid = now() % MAX_NUM_STATIONS;
    if ( sid != last_sid )  // avoid refreshing the same station twice in a roll
    {
        last_sid = sid;
        bid=sid>>3;
        s=sid&0x07;
        switch_special_station ( sid, ( station_bits[bid]>>s ) &0x01 );
    }

}

#define SLEEPING_VOLTS		options[MIN_VOLTS_L_SLEEP]*10  //unit 10mV options unit is 100mV  
#define DEEP_SLEEP_VOLTS	options[MIN_VOLTS_D_SLEEP]*10  //unit 10mV options unit is 100mV  
#define DEEP_SLEEP_TIME		options[D_SLEEP_DURATION]*60   //minutes option is in hours
#define SLEEP_START			options[D_SLEEP_START_TIME]*60 //minutes option is in hours
#define SLEEP_DURATION		options[D_SLEEP_DURATION]*60   //minutes option is in hours
#define L_SLEEP_TIME		options[L_SLEEP_DURATION]      //seconds

static int voltAverage, vcount = 0, sleeptime = 30;
/*
#define SLEEPING_VOLTS 330
#define DEEP_SLEEP_VOLTS 320
#define DEEP_SLEEP_TIME 3600UL   // 1 hour
#define SLEEP_START 20*60
#define SLEEP_DURATION 10*60

*/
#define BOTTOM 64-9

#ifdef LCD_SSD1306
void OpenSprinkler::Sleep(int volts) {
	DEBUG_PRINT("V="); DEBUG_PRINTLN(SLEEPING_VOLTS);
	bool sleepFlag = false;
// in the interval for sleeping!!!
	if (hour() > SLEEP_START) { if (hour() - SLEEP_START < SLEEP_DURATION) sleepFlag = true; }
	else if (SLEEP_START + SLEEP_DURATION > 24) if (hour() < SLEEP_START + SLEEP_DURATION - 24)sleepFlag = true;
//////
	if (volts>=100)
		if (volts < DEEP_SLEEP_VOLTS&&sleepFlag)
		{
			lcd.clearDisplay();
			lcd.setCursor(0, BOTTOM);
			lcd.print(volts / 100.); lcd.print(" sleep...");
			unsigned int sleepTime = DEEP_SLEEP_TIME;
			lcd_print_2digit(hour(now_tz() + sleepTime)); lcd.print(":");
			lcd_print_2digit(minute(sleepTime+now_tz()));
			lcd.display();
			ESP.deepSleep(DEEP_SLEEP_TIME*1000000);    // sleep for 1 hour waik up and sleep again if battery is empty      
		}
		else 	if(volts < SLEEPING_VOLTS) {
//		WiFi.setSleepMode(WIFI_LIGHT_SLEEP);
//		wifi_fpm_set_sleep_type(LIGHT_SLEEP_T);
		lcd.setCursor(30, BOTTOM); 
		lcd.print("zzzzzzzz"); 
		lcd.display();
		wifi_set_sleep_type(LIGHT_SLEEP_T);
		delay(L_SLEEP_TIME * 1000L);
		lcd.setCursor(100, BOTTOM);
		lcd.print("z");
		lcd.display();
		
	}
	else {
	//	lcd.setCursor(30, BOTTOM);
	//	lcd.print(volts);
	}
}
void OpenSprinkler::ShowNet() {
	if (second() % 60 == 0) {
		byte level = ether.WiFiLevel(0);
		DEBUG_PRINT("NetLev."); DEBUG_PRINTLN(level);
#define NET_X 19*XFACTOR
#define NET_Y BOTTOM-1
#define NET_S 150
		lcd.setCursor(NET_X-5, NET_Y);
		lcd.print("  ");

		lcd.drawBitmap(NET_X - 5, NET_Y, antenna5x9_bmp, 5, 9, WHITE);
		lcd.drawBitmap(NET_X , NET_Y, blank24x18_bmp, 10, 10, BLACK);

		lcd.drawTriangle(NET_X , NET_Y+YFACTOR,
			NET_X + (level*XFACTOR) / NET_S, NET_Y+YFACTOR,
			NET_X + (level*XFACTOR) / NET_S, NET_Y+YFACTOR - (YFACTOR*level) / NET_S, WHITE);
		lcd.display();
		
	}
}

int OpenSprinkler::BatteryAmps()
{
	return ina.getCurrent_mA();

}
int OpenSprinkler::BatteryCharge()
{
	return charge;

}

int OpenSprinkler::BatteryVoltage() {
	
#define INTERV_BATTERY 5
#define ANALOG_FACTOR (4.9+22)/4.9/1024 //*3.4 if 3.33 v full scale //4.9 & 22 KOhm 3.3 volt scale
	if (chargeTime == 0) {
		ina.begin();
		charge=25*eeprom_read_byte((unsigned char *)(NVM_SIZE - 1));
		if(charge==0||charge > options[BATTERY_mAH] * 100)charge = options[BATTERY_mAH] * 100;
		chargeTime = millis();
	}
	 float current = ina.getCurrent_mA();
	 float voltage = ina.getBusVoltage_V();
	 charge -= current*(millis() - chargeTime) / 3600000;
	 chargeTime = millis();
	 if (charge < 50)charge = 50;
	 if (charge > options[BATTERY_mAH] * 100)charge = options[BATTERY_mAH] * 100;
	 if (minute() == 0 && second() > 58)eeprom_write_byte((unsigned char *)(NVM_SIZE - 1), byte(charge / 25));
	 lcd.setCursor(0, BOTTOM);
	 lcd.print(voltage);
	lcd.setCursor(30, BOTTOM);
	lcd.print("          ");
	lcd.setCursor(30, BOTTOM);
//	char buff[12];
	lcd.printf("%5d %5d",int(current),int(charge));
//	lcd.print(buff);
	//lcd.print(" ");	lcd.print(int(charge));
	if (minute() % INTERV_BATTERY == 0) {
		if (vcount == 0)voltAverage = 0;
		lcd.setCursor(0, BOTTOM);
		//lcd.drawBitmap(0, BOTTOM - 1, battery20x11_bmp, 20, 11, 0);
		float volt = analogRead(A0)*ANALOG_FACTOR;
		lcd.print(volt);
		voltAverage += volt * 100;
		vcount++;
		
			return 0;
	}
	else
		if (vcount>0) {
			lcd.setCursor(0, BOTTOM);
			lcd.print(voltAverage / vcount / 100.);
			lcd.display();
			voltAverage = voltAverage / vcount;
			delay(1000);
			vcount = 0;
		//	volts[ivolts] = voltAverage - 250;
		//	if (button_read(BUTTON_WAIT_NONE)&0x0F == 2)drawdiag(volts, ivolts);
		//	if (ivolts++ >= 100) {
		//		ivolts = 0;
		//	}
			return voltAverage;

		}
	return voltAverage;
		
}
void OpenSprinkler::drawdiag(byte y[], byte n) {
#define YDIAG 60
#define HDIAG 30
	lcd.clearDisplay();
	for (byte i = 0; i < n; i++) {
		lcd.drawFastVLine(2 + i, YDIAG - HDIAG*y[i] / 250, HDIAG*y[i] / 250, WHITE);
	/*	if (i*INTERV_BATTERY % 120 == 0) {
			lcd.setCursor(i, YDIAG);
			lcd.print(((hour() + 24) * 60 + minute() - (n-i) * INTERV_BATTERY) % 24);
		}*/
		
	}
	lcd.display();
}
#endif

/** Read rain sensor status */
void OpenSprinkler::rainsensor_status()
{
    // options[OPTION_RS_TYPE]: 0 if normally closed, 1 if normally open
    if ( options[OPTION_SENSOR_TYPE]!=SENSOR_TYPE_RAIN ) return;
	
     status.rain_sensed = (digitalRead(PIN_RAINSENSOR) == options[OPTION_RAINSENSOR_TYPE] ? 0 : 1 );
}

/** Read current sensing value
 * OpenSprinkler has a 0.2 ohm current sensing resistor.
 * Therefore the conversion from analog reading to milli-amp is:
 * (r/1024)*3.3*1000/0.2
 * Newer AC controller has a 0.2 ohm curent sensing resistor
 * with op-amp to sense the peak current. Therefore the actual
 * current is discounted by 0.707
 */
#if defined(__AVR_ATmega1284P__) || defined(__AVR_ATmega1284__)
uint16_t OpenSprinkler::read_current()
{
    if ( status.has_curr_sense )
    {
        if ( hw_type == HW_TYPE_DC )
        {
            return ( uint16_t ) ( analogRead ( PIN_CURR_SENSE ) * 16.11 );
        }
        else
        {
            return ( uint16_t ) ( analogRead ( PIN_CURR_SENSE ) * 11.39 );
        }
    }
    else
    {
        return 0;
    }
}
#endif

/** Read the number of 8-station expansion boards */
// AVR has capability to detect number of expansion boards
int OpenSprinkler::detect_exp()
{
#if defined(ARDUINO)
    unsigned int v = analogRead ( PIN_EXP_SENSE );
    // OpenSprinkler uses voltage divider to detect expansion boards
    // Master controller has a 1.6K pull-up;
    // each expansion board (8 stations) has 10K pull-down connected in parallel;
    // so the exact ADC value for n expansion boards is:
    //    ADC = 1024 * 10 / (10 + 1.6 * n)
    // For  0,   1,   2,   3,   4,   5,  6 expansion boards, the ADC values are:
    //   1024, 882, 775, 691, 624, 568, 522
    // Actual threshold is taken as the midpoint between, to account for errors
    int n = -1;
    if ( v > 953 ) // 0
    {
        n = 0;
    }
    else if ( v > 828 )   // 1
    {
        n = 1;
    }
    else if ( v > 733 )   // 2
    {
        n = 2;
    }
    else if ( v > 657 )   // 3
    {
        n = 3;
    }
    else if ( v > 596 )   // 4
    {
        n = 4;
    }
    else if ( v > 545 )   // 5
    {
        n = 5;
    }
    else if ( v > 502 )   // 6
    {
        n = 6;
    }
    else      // cannot determine
    {
    }
    return n;
#else
    return -1;
#endif
}

/** Convert hex code to ulong integer */
static ulong hex2ulong ( byte *code, byte len )
{
    char c;
    ulong v = 0;
    for ( byte i=0; i<len; i++ )
    {
        c = code[i];
        v <<= 4;
        if ( c>='0' && c<='9' )
        {
            v += ( c-'0' );
        }
        else if ( c>='A' && c<='F' )
        {
            v += 10 + ( c-'A' );
        }
        else if ( c>='a' && c<='f' )
        {
            v += 10 + ( c-'a' );
        }
        else
        {
            return 0;
        }
    }
    return v;
}

/** Parse RF code into on/off/timeing sections */
uint16_t OpenSprinkler::parse_rfstation_code ( byte *code, ulong* on, ulong *off )
{
    ulong v;
    v = hex2ulong ( code, 6 );
    if ( !v ) return 0;
    if ( on ) *on = v;
    v = hex2ulong ( code+6, 6 );
    if ( !v ) return 0;
    if ( off ) *off = v;
    v = hex2ulong ( code+12, 4 );
    if ( !v ) return 0;
    return v;
}

/** Get station name from NVM */
void OpenSprinkler::get_station_name ( byte sid, char tmp[] )
{
    tmp[STATION_NAME_SIZE]=0;
    nvm_read_block ( tmp, ( void* ) ( ADDR_NVM_STN_NAMES+ ( int ) sid*STATION_NAME_SIZE ), STATION_NAME_SIZE );
}

/** Set station name to NVM */
void OpenSprinkler::set_station_name ( byte sid, char tmp[] )
{
    tmp[STATION_NAME_SIZE]=0;
    nvm_write_block ( tmp, ( void* ) ( ADDR_NVM_STN_NAMES+ ( int ) sid*STATION_NAME_SIZE ), STATION_NAME_SIZE );
}

/** Save station attribute bits to NVM */
void OpenSprinkler::station_attrib_bits_save ( int addr, byte bits[] )
{
    nvm_write_block ( bits, ( void* ) addr, MAX_EXT_BOARDS+1 );
}

/** Load all station attribute bits from NVM */
void OpenSprinkler::station_attrib_bits_load ( int addr, byte bits[] )
{
    nvm_read_block ( bits, ( void* ) addr, MAX_EXT_BOARDS+1 );
}

/** Read one station attribute byte from NVM */
byte OpenSprinkler::station_attrib_bits_read ( int addr )
{
    return nvm_read_byte ( ( byte* ) addr );
}

/** verify if a string matches password */
byte OpenSprinkler::password_verify ( char *pw )
{
    byte *addr = ( byte* ) ADDR_NVM_PASSWORD;
    byte c1, c2;
    while ( 1 )
    {
        if ( addr == ( byte* ) ADDR_NVM_PASSWORD+MAX_USER_PASSWORD )
            c1 = 0;
        else
            c1 = nvm_read_byte ( addr++ );
        c2 = *pw++;
        if ( c1==0 || c2==0 )
            break;
        if ( c1!=c2 )
        {
            return 0;
        }
    }
    return ( c1==c2 ) ? 1 : 0;
}

// ==================
// Schedule Functions
// ==================

/** Index of today's weekday (Monday is 0) */
byte OpenSprinkler::weekday_today()
{
    //return ((byte)weekday()+5)%7; // Time::weekday() assumes Sunday is 1
#if defined(ARDUINO)
    ulong wd = now_tz() / 86400L;
    return ( wd+3 ) % 7; // Jan 1, 1970 is a Thursday
#else
    return 0;
    // todo: is this function needed for RPI/BBB?
#endif
}

/** Switch special station */
void OpenSprinkler::switch_special_station ( byte sid, byte value )
{
    // check station special bit
    if ( station_attrib_bits_read ( ADDR_NVM_STNSPE+ ( sid>>3 ) ) & ( 1<< ( sid&0x07 ) ) )
    {
        // read station special data from sd card
        int stepsize=sizeof ( StationSpecialData );
        read_from_file ( stns_filename, tmp_buffer, stepsize, sid*stepsize );
        StationSpecialData *stn = ( StationSpecialData * ) tmp_buffer;
        // check station type
        if ( stn->type==STN_TYPE_RF )
        {
            // transmit RF signal
#ifdef PORT_RF
            switch_rfstation ( stn->data, value );
#endif

		}
        else if ( stn->type==STN_TYPE_REMOTE )
        {
            // request remote station
            switch_remotestation ( stn->data, value );
        }
    }
}

/** Set station bit
 * This function sets/resets the corresponding station bit variable
 * You have to call apply_all_station_bits next to apply the bits
 * (which results in physical actions of opening/closing valves).
 */
byte OpenSprinkler::set_station_bit ( byte sid, byte value )
{
    byte *data = station_bits+ ( sid>>3 ); // pointer to the station byte
    byte mask = ( byte ) 1<< ( sid&0x07 ); // mask
    if ( value )
    {
        if ( ( *data ) &mask ) return 0; // if bit is already set, return no change
        else
        {
            ( *data ) = ( *data ) | mask;
#if defined(__AVR_ATmega1284P__) || defined(__AVR_ATmega1284__)
            engage_booster = true; // if bit is changing from 0 to 1, set engage_booster
#endif
            switch_special_station ( sid, 1 ); // handle special stations
            return 1;
        }
    }
    else
    {
        if ( ! ( ( *data ) &mask ) ) return 0; // if bit is already reset, return no change
        else
        {
            ( *data ) = ( *data ) & ( ~mask );
            switch_special_station ( sid, 0 ); // handle special stations
            return 255;
        }
    }
    return 0;
}

/** Clear all station bits */
void OpenSprinkler::clear_all_station_bits()
{
    byte sid;
    for ( sid=0; sid<=MAX_NUM_STATIONS; sid++ )
    {
        set_station_bit ( sid, 0 );
    }
}

#if !defined(ARDUINO)
int rf_gpio_fd = -1;
#endif

#ifdef PORT_RF
/** Transmit one RF signal bit */
void transmit_rfbit ( ulong lenH, ulong lenL )
{
#if defined(ARDUINO)
    PORT_RF |= ( 1<<PINX_RF );
    delayMicroseconds ( lenH );
    PORT_RF &=~ ( 1<<PINX_RF );
    delayMicroseconds ( lenL );
#else
    gpio_write ( rf_gpio_fd, 1 );
    delayMicrosecondsHard ( lenH );
    gpio_write ( rf_gpio_fd, 0 );
    delayMicrosecondsHard ( lenL );
#endif
}

/** Transmit RF signal */
void send_rfsignal ( ulong code, ulong len )
{
    ulong len3 = len * 3;
    ulong len31 = len * 31;
    for ( byte n=0; n<15; n++ )
    {
        int i=23;
        // send code
        while ( i>=0 )
        {
            if ( ( code>>i ) & 1 )
            {
                transmit_rfbit ( len3, len );
            }
            else
            {
                transmit_rfbit ( len, len3 );
            }
            i--;
        };
        // send sync
        transmit_rfbit ( len, len31 );
    }
}


/** Switch RF station
 * This function takes a RF code,
 * parses it into signals and timing,
 * and sends it out through RF transmitter.
 */
void OpenSprinkler::switch_rfstation ( byte *code, bool turnon )
{
    ulong on, off;
    uint16_t length = parse_rfstation_code ( code, &on, &off );
#if defined(ARDUINO)
    send_rfsignal ( turnon ? on : off, length );
#else
    // pre-open gpio file to minimize overhead
    rf_gpio_fd = gpio_fd_open ( PIN_RF_DATA );
    send_rfsignal ( turnon ? on : off, length );
    gpio_fd_close ( rf_gpio_fd );
    rf_gpio_fd = -1;
#endif

}
#endif
/** Callback function for remote station calls */
static void switchremote_callback ( byte status, uint16_t off, uint16_t len )
{
    /* do nothing */
}

/** Switch remote station
 * This function takes a remote station code,
 * parses it into remote IP, port, station index,
 * and makes a HTTP GET request.
 * The remote controller is assumed to have the same
 * password as the main controller
 */
void OpenSprinkler::switch_remotestation ( byte *code, bool turnon )
{
#if defined(ARDUINO)
    // construct string
    ulong ip = hex2ulong ( code, 8 );
    ether.hisip[0] = ip>>24;
    ether.hisip[1] = ( ip>>16 ) &0xff;
    ether.hisip[2] = ( ip>>8 ) &0xff;
    ether.hisip[3] = ip&0xff;

    uint16_t _port = ether.hisport; // save current port number
    ether.hisport = hex2ulong ( code+8, 4 );

    char *p = tmp_buffer + sizeof ( StationSpecialData );
    BufferFiller bf = ( byte* ) p;
    bf.emit_p ( PSTR ( "?pw=$E&sid=$D&en=$D&t=$D" ),
                ADDR_NVM_PASSWORD,
                ( int ) hex2ulong ( code+12,2 ),
                turnon, 2*MAX_NUM_STATIONS ); // MAX_NUM_STATIONS is the refresh cycle
    DEBUG_PRINTLN ( p );
    ether.browseUrl ( PSTR ( "/cm" ), p, PSTR ( "*" ), switchremote_callback );
    for ( int l=0; l<100; l++ )
    {
        ether.packetLoop ( ether.packetReceive() );
    }
    ether.hisport = _port;
#else
    EthernetClient client;

    uint8_t hisip[4];
    uint16_t hisport;
    ulong ip = hex2ulong ( code, 8 );
    hisip[0] = ip>>24;
    hisip[1] = ( ip>>16 ) &0xff;
    hisip[2] = ( ip>>8 ) &0xff;
    hisip[3] = ip&0xff;
    hisport = hex2ulong ( code+8, 4 );

    if ( !client.connect ( hisip, hisport ) )
    {
        client.stop();
        return;
    }

    char *p = tmp_buffer + sizeof ( StationSpecialData );
    BufferFiller bf = p;
    bf.emit_p ( PSTR ( "GET /cm?pw=$E&sid=$D&en=$D&t=$D" ),
                ADDR_NVM_PASSWORD,
                ( int ) hex2ulong ( code+12,2 ),
                turnon, 2*MAX_NUM_STATIONS ); // MAX_NUM_STATIONS is the refresh cycle
    bf.emit_p ( PSTR ( " HTTP/1.0\r\nHOST: *\r\n\r\n" ) );

    client.write ( ( uint8_t * ) p, strlen ( p ) );

    bzero ( ether_buffer, ETHER_BUFFER_SIZE );

    time_t timeout = now() + 5; // 5 seconds timeout
    while ( now() < timeout )
    {
        int len=client.read ( ( uint8_t * ) ether_buffer, ETHER_BUFFER_SIZE );
        if ( len<=0 )
        {
            if ( !client.connected() )
                break;
            else
                continue;
        }
        switchremote_callback ( 0, 0, ETHER_BUFFER_SIZE );
    }
    client.stop();
#endif
}

/** Setup function for options */
void OpenSprinkler::options_setup()
{

    // add 0.25 second delay to allow nvm to stablize
    delay ( 250 );
	
	int i = 0; char d;
	DEBUG_PRINTLN(DEFAULT_WEATHER_URL);
	DEBUG_PRINTLN(strlen(DEFAULT_WEATHER_URL));
	//while (d != 0) { d = eeprom_read_byte((byte*)ADDR_NVM_WEATHERURL + i++); DEBUG_PRINT(d); }
    byte curr_ver = nvm_read_byte ( ( byte* ) ( ADDR_NVM_OPTIONS+OPTION_FW_VERSION ) );
	DEBUG_PRINTLN((int)curr_ver);
    // check reset condition: either firmware version has changed, or reset flag is up
    // if so, trigger a factory reset
    if ( curr_ver != OS_FW_VERSION || nvm_read_byte ( ( byte* ) ( ADDR_NVM_OPTIONS+OPTION_RESET ) ) ==0xAA )
    {
#if defined(ARDUINO)
        lcd_print_line_clear_pgm ( PSTR ( "Resetting..." ), 0 );
        lcd_print_line_clear_pgm ( PSTR ( "Please Wait..." ), 1 );
		DEBUG_PRINTLN("Reset EEPROM");
		write_message("Resetting to factory default!");
#else
        DEBUG_PRINT ( "Resetting Options..." );
#endif
        // ======== Reset NVM data ========
        int i, sn;
        // 0. wipe out nvm
        for ( i=0; i<TMP_BUFFER_SIZE; i++ ) tmp_buffer[i]=0;
        for ( i=0; i<NVM_SIZE; i+=TMP_BUFFER_SIZE )
        {
			DEBUG_PRINT(i); 
			DEBUG_PRINT(' ');
			delay(100);
            int nbytes = ( ( NVM_SIZE-i ) >TMP_BUFFER_SIZE ) ?TMP_BUFFER_SIZE: ( NVM_SIZE-i );
            nvm_write_block ( tmp_buffer, ( void* ) i, nbytes );
        }
		DEBUG_PRINTLN("wiped");
        // 1. write non-volatile controller status
        nvdata_save();
		DEBUG_PRINTLN("nvdata");
        // 2. write string parameters
        nvm_write_block ( DEFAULT_PASSWORD, ( void* ) ADDR_NVM_PASSWORD, strlen ( DEFAULT_PASSWORD )+1 );
        nvm_write_block ( DEFAULT_LOCATION, ( void* ) ADDR_NVM_LOCATION, strlen ( DEFAULT_LOCATION )+1 );
        nvm_write_block ( DEFAULT_JAVASCRIPT_URL, ( void* ) ADDR_NVM_JAVASCRIPTURL, strlen ( DEFAULT_JAVASCRIPT_URL )+1 );
		DEBUG_PRINT(char(eeprom_read_byte((byte*)ADDR_NVM_JAVASCRIPTURL + strlen(DEFAULT_JAVASCRIPT_URL) + 1)&&0x0F));
		DEBUG_PRINTLN(char(eeprom_read_byte((byte*)ADDR_NVM_JAVASCRIPTURL + strlen(DEFAULT_JAVASCRIPT_URL) + 2)&&0x0F));
		nvm_write_block ( DEFAULT_WEATHER_URL, ( void* ) ADDR_NVM_WEATHERURL, strlen ( DEFAULT_WEATHER_URL )+1 );
        nvm_write_block ( DEFAULT_WEATHER_KEY, ( void* ) ADDR_NVM_WEATHER_KEY, strlen ( DEFAULT_WEATHER_KEY )+1 );

        // 3. reset station names and special attributes, default Sxx
        tmp_buffer[0]='S';
        tmp_buffer[3]=0;
        for ( i=ADDR_NVM_STN_NAMES, sn=1; i<ADDR_NVM_MAS_OP; i+=STATION_NAME_SIZE, sn++ )
        {
            tmp_buffer[1]='0'+ ( sn/10 );
            tmp_buffer[2]='0'+ ( sn%10 );
            nvm_write_block ( tmp_buffer, ( void* ) i, strlen ( tmp_buffer )+1 );
        }

        tmp_buffer[0]=STN_TYPE_STANDARD;
        tmp_buffer[1]='0';
        tmp_buffer[2]=0;
        int stepsize=sizeof ( StationSpecialData );
		DEBUG_PRINTLN("Preparing files");
		delay(1000);
        for ( i=0; i<MAX_NUM_STATIONS; i++ )
        {
            write_to_file ( stns_filename, tmp_buffer, stepsize, i*stepsize, false );
        }
		DEBUG_PRINTLN("file written");
        // 4. reset station attribute bits
        // since we wiped out nvm, only non-zero attributes need to be initialized
        for ( i=0; i<MAX_EXT_BOARDS+1; i++ )
        {
            tmp_buffer[i]=0xff;
        }
        nvm_write_block ( tmp_buffer, ( void* ) ADDR_NVM_MAS_OP, MAX_EXT_BOARDS+1 );
        nvm_write_block ( tmp_buffer, ( void* ) ADDR_NVM_STNSEQ, MAX_EXT_BOARDS+1 );

        // 5. delete sd file
        remove_file ( wtopts_filename );

        // 6. write options
        options_save(); // write default option values
		DEBUG_PRINTLN("done");
        //======== END OF NVM RESET CODE ========

        // restart after resetting NVM.
        delay ( 500 );
#if defined(ARDUINO)
        reboot_dev();
#endif
    }

    {
        // load ram parameters from nvm
        // load options
        options_load();

        // load non-volatile controller data
        nvdata_load();
    }

#if defined(ARDUINO)  // handle AVR buttons
    byte button = button_read ( BUTTON_WAIT_NONE );
	DEBUG_PRINT(button);
//	button = 2;
	byte ii = 1;

	switch (button & BUTTON_MASK)
	{

	case BUTTON_1:
		// if BUTTON_1 is pressed during startup, go to 'reset all options'
		ui_set_options(OPTION_RESET);
		if (options[OPTION_RESET])
		{
			reboot_dev();
		}
		break;

	case BUTTON_2:  // button 2 is used to enter bootloader
		ether.netflag = true;
		lcd_print_line_clear_pgm(PSTR("==Change Network=="), 0);
		delay(DISPLAY_MSG_MS);
#ifdef MESSAGE
		print_message(-(ii++ * 20));
		delay(2000);
		while (button_read(BUTTON_WAIT_NONE)&BUTTON_MASK == 2)
		{
			print_message(-(ii++ * 20));
			delay(2000);
		}
#endif
		break;

    case BUTTON_3:
        // if BUTTON_3 is pressed during startup, enter Setup option mode
        lcd_print_line_clear_pgm ( PSTR ( "==Set Options==" ), 0 );
        delay ( DISPLAY_MSG_MS );
        lcd_print_line_clear_pgm ( PSTR ( "B1/B2:+/-, B3:->" ), 0 );
        lcd_print_line_clear_pgm ( PSTR ( "Hold B3 to save" ), 1 );
        do
        {
            button = button_read ( BUTTON_WAIT_NONE );
        }
        while ( ! ( button & BUTTON_FLAG_DOWN ) );
#ifndef LCD_SSD1306
		lcd.clear();
#else
		lcd.clearDisplay();
#endif
        ui_set_options ( 0 );
        if ( options[OPTION_RESET] )
        {
            reboot_dev();
        }
        break;
    }

    // turn on LCD backlight and contrast
    lcd_set_brightness();
    lcd_set_contrast();

    if ( !button )
	{
		DEBUG_PRINT("no but");
        // flash screen
        lcd_print_line_clear_pgm ( PSTR ( " OpenSprinkler" ),0 );
        lcd.setCursor ( 2*XFACTOR, 1*YFACTOR );
        lcd_print_pgm ( PSTR ( "HW v" ) );
        byte hwv = options[OPTION_HW_VERSION];
        lcd.print ( ( char ) ( '0'+ ( hwv/10 ) ) );
        lcd.print ( '.' );
        lcd.print ( ( char ) ( '0'+ ( hwv%10 ) ) );
        switch ( hw_type )
        {
        case HW_TYPE_DC:
            lcd_print_pgm ( PSTR ( " DC" ) );
            break;
        case HW_TYPE_LATCH:
            lcd_print_pgm ( PSTR ( " LA" ) );
            break;
        default:
            lcd_print_pgm ( PSTR ( " AC" ) );
        }

#ifdef LCD_SSD1306
		lcd.display();
#endif
        delay ( 1000 );
    }
#endif
}

/** Load non-volatile controller status data from internal NVM */
void OpenSprinkler::nvdata_load()
{
    nvm_read_block ( &nvdata, ( void* ) ADDR_NVM_NVCONDATA, sizeof ( NVConData ) );
    old_status = status;
}

/** Save non-volatile controller status data to internal NVM */
void OpenSprinkler::nvdata_save()
{
    nvm_write_block ( &nvdata, ( void* ) ADDR_NVM_NVCONDATA, sizeof ( NVConData ) );

}

/** Load options from internal NVM */
void OpenSprinkler::options_load()
{
    nvm_read_block ( tmp_buffer, ( void* ) ADDR_NVM_OPTIONS, NUM_OPTIONS );
    for ( int i=0; i<NUM_OPTIONS; i++ )
    {
        options[i] = tmp_buffer[i];
    }
    nboards = options[OPTION_EXT_BOARDS]+1;
    nstations = nboards * 8;
    status.enabled = options[OPTION_DEVICE_ENABLE];
    options[OPTION_FW_MINOR] = OS_FW_MINOR;
}

/** Save options to internal NVM */
void OpenSprinkler::options_save()
{
    // save options in reverse order so version number is written the last
	for (int i = 0; i<NUM_OPTIONS ; i++)
    {
        tmp_buffer[i] = options[i];
    }
	DEBUG_PRINT("OptionsWrite OS:"); DEBUG_PRINTLN((int)options[0]);
    nvm_write_block ( tmp_buffer, ( void* ) ADDR_NVM_OPTIONS, NUM_OPTIONS );
	DEBUG_PRINTLN((int)eeprom_read_byte((byte *)ADDR_NVM_OPTIONS));
    nboards = options[OPTION_EXT_BOARDS]+1;
    nstations = nboards * 8;
    status.enabled = options[OPTION_DEVICE_ENABLE];
}

// ==============================
// Controller Operation Functions
// ==============================

/** Enable controller operation */
void OpenSprinkler::enable()
{
    status.enabled = 1;
    options[OPTION_DEVICE_ENABLE] = 1;
    options_save();
}

/** Disable controller operation */
void OpenSprinkler::disable()
{
    status.enabled = 0;
    options[OPTION_DEVICE_ENABLE] = 0;
    options_save();
}

/** Start rain delay */
void OpenSprinkler::raindelay_start()
{
    status.rain_delayed = 1;
    nvdata_save();
}

/** Stop rain delay */
void OpenSprinkler::raindelay_stop()
{
    status.rain_delayed = 0;
    nvdata.rd_stop_time = 0;
    nvdata_save();
}

/** LCD and button functions */
void OpenSprinkler::lcd_clear() {
#ifndef LCD_SSD1306
	lcd.clear();
#else
	lcd.clearDisplay();
#endif
}
#if defined(ARDUINO)    // AVR LCD and button functions
/** print a program memory string */
void OpenSprinkler::lcd_print_pgm ( PGM_P /*PROGMEM*/ str )
{
    uint8_t c;
    while ( ( c=pgm_read_byte ( str++ ) ) != '\0' )
    {
        lcd.print ( ( char ) c );
    }
#ifdef LCD_SSD1306
	lcd.display();
#endif
}

/** print a program memory string to a given line with clearing */
void OpenSprinkler::lcd_print_line_clear_pgm ( PGM_P /*PROGMEM*/ str, byte line )
{
    lcd.setCursor ( 0, line*YFACTOR );
    uint8_t c;
    int8_t cnt = 0;
    while ( ( c=pgm_read_byte ( str++ ) ) != '\0' )
    {
        lcd.print ( ( char ) c );
        cnt++;
    }
    for ( ; ( 16-cnt ) >= 0; cnt ++ ) lcd_print_pgm ( PSTR ( " " ) );

#ifdef LCD_SSD1306
	lcd.display();
#endif
}

void OpenSprinkler::lcd_print_2digit ( int v )
{
    lcd.print ( ( int ) ( v/10 ) );
    lcd.print ( ( int ) ( v%10 ) );
}

/** print time to a given line */
void OpenSprinkler::lcd_print_time ( time_t t )
{
    lcd.setCursor ( 0, 0 );
    lcd_print_2digit ( hour ( t ) );

#ifdef OPENSPRINKLER_ARDUINO_HEARTBEAT
    lcd_print_pgm ( ( t % 2 == 0 ? PSTR ( ":" ) : PSTR ( " " ) ) );
#else
    lcd_print_pgm ( PSTR ( ":" ) );
#endif // OPENSPRINKLER_ARDUINO_HEARTBEAT

    lcd_print_2digit ( minute ( t ) );
    lcd_print_pgm ( PSTR ( "  " ) );
    // each weekday string has 3 characters + ending 0
    lcd_print_pgm ( days_str+4*weekday_today() );
    lcd_print_pgm ( PSTR ( " " ) );
    lcd_print_2digit ( month ( t ) );
    lcd_print_pgm ( PSTR ( "-" ) );
    lcd_print_2digit ( day ( t ) );

#ifdef LCD_SSD1306
	lcd.display();
#endif
}

/** print ip address */
void OpenSprinkler::lcd_print_ip ( const byte *ip, byte endian )
{

    lcd_clear();

    lcd.setCursor ( 0, 0 );
    for ( byte i=0; i<4; i++ )
    {
        lcd.print ( endian ? ( int ) ip[3-i] : ( int ) ip[i] );
        if ( i<3 ) lcd_print_pgm ( PSTR ( "." ) );
    }

#ifdef LCD_SSD1306
	lcd.display();
#endif
}

/** print mac address */
void OpenSprinkler::lcd_print_mac ( const byte *mac )
{
    lcd.setCursor ( 0, 0 );
    for ( byte i=0; i<6; i++ )
    {
        if ( i )  lcd_print_pgm ( PSTR ( "-" ) );
        lcd.print ( ( mac[i]>>4 ), HEX );
        lcd.print ( ( mac[i]&0x0F ), HEX );
        if ( i==4 ) lcd.setCursor ( 0, YFACTOR );
    }
    lcd_print_pgm ( PSTR ( " (MAC)" ) );

#ifdef LCD_SSD1306
	lcd.display();
#endif
}

/** print station bits */
void OpenSprinkler::lcd_print_station(byte line, char c)
{
#ifdef LCD_SSN1306
	lcd.setTextSize(2);
#endif
	lcd.setCursor(0, line*YFACTOR);
	if (status.display_board == 0)
	{
		lcd_print_pgm(PSTR("MC")); // Master controller is display as 'MC'
	}
	else
	{
		lcd_print_pgm(PSTR("E"));
		lcd.print((int)status.display_board);
		lcd_print_pgm(PSTR("")); // extension boards are displayed as E1, E2...
	}

	if (!status.enabled)
	{
		lcd_print_line_clear_pgm(PSTR("-Disabled!-"), 1);
	}
	else
	{
		byte xchar = 2;
		byte bitvalue = station_bits[status.display_board];
		for (byte s = 0; s < 8; s++)
		{
			byte sid = (byte)status.display_board << 3;
			sid += (s + 1);
			if (sid == options[OPTION_MASTER_STATION])
			{
#ifndef LCD_SSD1306x
				lcd.print((bitvalue & 1) ? (char)c : 'M'); // print master station
#else
				lcd.drawBitmap(2 * XFACTOR*xchar++, YFACTOR, (bitvalue & 1) ? valveOpen10x16_bmp : valveClosed10x16_bmp, 10, 16, WHITE);
#endif
			}
			else if (sid == options[OPTION_MASTER_STATION_2])
			{
				lcd.print((bitvalue & 1) ? (char)c : 'N'); // print master2 station
			}
			else
			{

				lcd.print((bitvalue & 1) ? (char)c : '_'); // print master station

//				lcd.drawBitmap(2 * XFACTOR*xchar++, YFACTOR, (bitvalue & 1) ? valveOpen10x16_bmp : valveClosed10x16_bmp, 10, 16, WHITE);

			}
			bitvalue >>= 1;
		}
	}
#ifndef LCD_SSD1306
	lcd_print_pgm(PSTR("     "));
	lcd.setCursor(12, 1);

	if (options[OPTION_REMOTE_EXT_MODE])
	{
		lcd.write(5);
	}
	lcd.setCursor(13 , 1);
#endif

	if (status.rain_delayed || (status.rain_sensed && options[OPTION_SENSOR_TYPE] == SENSOR_TYPE_RAIN))
	{
#ifndef LCD_SSD1306
		
		lcd.write(3);
#else
		lcd.drawBitmap(117, 0, rain10x10_bmp, 10, 10, WHITE);
#endif

	}
	if (options[OPTION_SENSOR_TYPE] == SENSOR_TYPE_FLOW)
	{
		lcd.write(6);
	}

	if (status.has_sd)
	{

#ifndef LCD_SSD1306
		lcd.setCursor(14, 1);
		lcd.write(2);
#else
		lcd.drawBitmap(16*XFACTOR, 0, sdisk10x10_bmp, 10, 10, WHITE);
#endif
	}

#ifndef LCD_SSD1306
	lcd.setCursor(15, 1);
	lcd.write(status.network_fails>2 ? 1 : 0); // if network failure detection is more than 2, display disconnect icon
#else
	lcd.drawBitmap(18*XFACTOR, 0, antenna5x9_bmp, 5, 9, WHITE);
#endif
    
#ifdef LCD_SSD1306
	lcd.display();
	lcd.setTextSize(1);
#endif
}

/** print a version number */
void OpenSprinkler::lcd_print_version ( byte v )
{
    if ( v > 99 )
    {
        lcd.print ( v/100 );
        lcd.print ( "." );
    }
    if ( v>9 )
    {
        lcd.print ( ( v/10 ) %10 );
        lcd.print ( "." );
    }
    lcd.print ( v%10 );

#ifdef LCD_SSD1306
	lcd.display();
#endif
}

/** print an option value */
void OpenSprinkler::lcd_print_option ( int i )
{
    // each prompt string takes 16 characters
    strncpy_P0 ( tmp_buffer, op_prompts+16*i, 16 );
    lcd.setCursor ( 0, 0 );
    lcd.print ( tmp_buffer );
    lcd_print_line_clear_pgm ( PSTR ( "" ), 1 );
    lcd.setCursor ( 0, YFACTOR );
    int tz;
    switch ( i )
    {
    case OPTION_HW_VERSION:
        lcd.print ( "v" );
    case OPTION_FW_VERSION:
        lcd_print_version ( options[i] );
        break;
    case OPTION_TIMEZONE: // if this is the time zone option, do some conversion
        tz = ( int ) options[i]-48;
        if ( tz>=0 ) lcd_print_pgm ( PSTR ( "+" ) );
        else
        {
            lcd_print_pgm ( PSTR ( "-" ) );
            tz=-tz;
        }
        lcd.print ( tz/4 ); // print integer portion
        lcd_print_pgm ( PSTR ( ":" ) );
        tz = ( tz%4 ) *15;
        if ( tz==0 )  lcd_print_pgm ( PSTR ( "00" ) );
        else
        {
            lcd.print ( tz ); // print fractional portion
        }
        break;
    case OPTION_MASTER_ON_ADJ:
    case OPTION_MASTER_ON_ADJ_2:
        lcd_print_pgm ( PSTR ( "+" ) );
        lcd.print ( ( int ) options[i] );
        break;
    case OPTION_MASTER_OFF_ADJ:
    case OPTION_MASTER_OFF_ADJ_2:
        if ( options[i]>=60 )  lcd_print_pgm ( PSTR ( "+" ) );
        lcd.print ( ( int ) options[i]-60 );
        break;
    case OPTION_HTTPPORT_0:
        lcd.print ( ( unsigned int ) ( options[i+1]<<8 )+options[i] );
        break;
    case OPTION_PULSE_RATE_0:
    {
        uint16_t fpr = ( unsigned int ) ( options[i+1]<<8 )+options[i];
        lcd.print ( fpr/100 );
        lcd_print_pgm ( PSTR ( "." ) );
        lcd.print ( ( fpr/10 ) %10 );
        lcd.print ( fpr%10 );
    }
    break;
    case OPTION_STATION_DELAY_TIME:
        lcd.print ( water_time_decode_signed ( options[i] ) );
        break;
    case OPTION_LCD_CONTRAST:
        lcd_set_contrast();
        lcd.print ( ( int ) options[i] );
        break;
    case OPTION_LCD_BACKLIGHT:
        lcd_set_brightness();
        lcd.print ( ( int ) options[i] );
        break;
    case OPTION_BOOST_TIME:
#if defined(__AVR_ATmega1284P__) || defined(__AVR_ATmega1284__)
        if ( hw_type==HW_TYPE_AC )
        {
            lcd.print ( '-' );
        }
        else
        {
            lcd.print ( ( int ) options[i]*4 );
            lcd_print_pgm ( PSTR ( " ms" ) );
        }
#else
        lcd.print ( '-' );
#endif
        break;
    default:
        // if this is a boolean option
        if ( pgm_read_byte ( op_max+i ) ==1 )
            lcd_print_pgm ( options[i] ? PSTR ( "Yes" ) : PSTR ( "No" ) );
        else
            lcd.print ( ( int ) options[i] );
        break;
    }
    if ( i==OPTION_WATER_PERCENTAGE )  lcd_print_pgm ( PSTR ( "%" ) );
    else if ( i==OPTION_MASTER_ON_ADJ || i==OPTION_MASTER_OFF_ADJ || i==OPTION_MASTER_ON_ADJ_2 || i==OPTION_MASTER_OFF_ADJ_2 )
        lcd_print_pgm ( PSTR ( " sec" ) );

#ifdef LCD_SSD1306
	lcd.display();
#endif
}

/** Button functions */

#ifdef BUTTON_ADC_PIN
// Wait for button
byte OpenSprinkler::button_read_busy ( byte pin_butt, byte waitmode, byte butt, byte is_holding )
{

    int hold_time = 0;

    if ( waitmode == BUTTON_WAIT_NONE || ( waitmode == BUTTON_WAIT_HOLD && is_holding ) )
    {
        if ( button_sample() == BUTTON_NONE ) return BUTTON_NONE;
        return butt | ( is_holding ? BUTTON_FLAG_HOLD : 0 );
    }

    while ( button_sample() != BUTTON_NONE &&
            ( waitmode == BUTTON_WAIT_RELEASE || ( waitmode == BUTTON_WAIT_HOLD && hold_time<BUTTON_HOLD_MS ) ) )
    {
        delay ( BUTTON_DELAY_MS );
        hold_time += BUTTON_DELAY_MS;
    };
    if ( is_holding || hold_time >= BUTTON_HOLD_MS )
        butt |= BUTTON_FLAG_HOLD;
    return butt;

}

// Read button and returns button value 'OR'ed with flag bits
byte OpenSprinkler::button_read ( byte waitmode )
{
    static byte old = BUTTON_NONE;
    byte curr = BUTTON_NONE;
    byte is_holding = ( old&BUTTON_FLAG_HOLD );

    delay ( BUTTON_DELAY_MS );

    curr = button_sample();
	//DEBUG_PRINTLN(curr);
    // A normal opensprinkler is only 3 buttons - map them to the five buttons of the LCD shield
    // Button 1 = Increase  = Up and Right
    // Button 2 = Decrease = Down and Left
    // Button 3 = Select = Select
    switch ( curr )
    {
    case BUTTON_UP:
    case BUTTON_RIGHT:
        curr = button_read_busy ( BUTTON_ADC_PIN, waitmode, BUTTON_1, is_holding );
        ;
        break;

    case BUTTON_DOWN:
    case BUTTON_LEFT:
        curr = button_read_busy ( BUTTON_ADC_PIN, waitmode, BUTTON_2, is_holding );
        break;

    case BUTTON_SELECT:
        curr = button_read_busy ( BUTTON_ADC_PIN, waitmode, BUTTON_3, is_holding );
        break;

    default:
        curr = BUTTON_NONE;
    }
	//DEBUG_PRINTLN(curr);
    /* set flags in return value */
    byte ret = curr;
    if ( ! ( old&BUTTON_MASK ) && ( curr&BUTTON_MASK ) )
        ret |= BUTTON_FLAG_DOWN;
    if ( ( old&BUTTON_MASK ) && ! ( curr&BUTTON_MASK ) )
        ret |= BUTTON_FLAG_UP;

    old = curr;
    return ret;
}

byte OpenSprinkler::button_sample()
{
    unsigned int buttonVoltage;

    //read the button ADC pin voltage
    buttonVoltage = analogRead ( BUTTON_ADC_PIN );
	//DEBUG_PRINTLN(buttonVoltage);
    //sense if the voltage falls within valid voltage windows
	//avoid to pick 0 volt as Button right ...
    if ( buttonVoltage < ( RIGHT_10BIT_ADC + BUTTONHYSTERESIS )&& buttonVoltage > (RIGHT_10BIT_ADC - BUTTONHYSTERESIS))
    {
        return BUTTON_RIGHT;
    }
    else if ( buttonVoltage >= ( UP_10BIT_ADC - BUTTONHYSTERESIS )
              && buttonVoltage <= ( UP_10BIT_ADC + BUTTONHYSTERESIS ) )
    {
        return BUTTON_UP;
    }
    else if ( buttonVoltage >= ( DOWN_10BIT_ADC - BUTTONHYSTERESIS )
              && buttonVoltage <= ( DOWN_10BIT_ADC + BUTTONHYSTERESIS ) )
    {
        return BUTTON_DOWN;
    }
    else if ( buttonVoltage >= ( LEFT_10BIT_ADC - BUTTONHYSTERESIS )
              && buttonVoltage <= ( LEFT_10BIT_ADC + BUTTONHYSTERESIS ) )
    {
        return BUTTON_LEFT;
    }
    else if ( buttonVoltage >= ( SELECT_10BIT_ADC - BUTTONHYSTERESIS )
              && buttonVoltage <= ( SELECT_10BIT_ADC + BUTTONHYSTERESIS ) )
    {
        return BUTTON_SELECT;
    }
    else
        return BUTTON_NONE;
}
#else
/** wait for button */
byte OpenSprinkler::button_read_busy ( byte pin_butt, byte waitmode, byte butt, byte is_holding )
{
	byte but_on = 0 ;
	if (pin_butt == PIN_BUTTON_1)but_on = BUT1_ON;
	else if (pin_butt == PIN_BUTTON_2)but_on = BUT2_ON;
	else but_on = BUT3_ON;

    int hold_time = 0;

    if ( waitmode==BUTTON_WAIT_NONE || ( waitmode == BUTTON_WAIT_HOLD && is_holding ) )
    {
        if (( digitalRead ( pin_butt ) )!= but_on ) return BUTTON_NONE;
        return butt | ( is_holding ? BUTTON_FLAG_HOLD : 0 );
    }

    while ( (digitalRead ( pin_butt ) )== but_on &&
            ( waitmode == BUTTON_WAIT_RELEASE || ( waitmode == BUTTON_WAIT_HOLD && hold_time<BUTTON_HOLD_MS ) ) )
    {
        delay ( BUTTON_DELAY_MS );
        hold_time += BUTTON_DELAY_MS;
    };
    if ( is_holding || hold_time >= BUTTON_HOLD_MS )
        butt |= BUTTON_FLAG_HOLD;
    return butt;

}

/** read button and returns button value 'OR'ed with flag bits */
byte OpenSprinkler::button_read ( byte waitmode )
{
    static byte old = BUTTON_NONE;
    byte curr = BUTTON_NONE;
    byte is_holding = ( old&BUTTON_FLAG_HOLD );

    delay ( BUTTON_DELAY_MS );
	byte but1 = digitalRead(PIN_BUTTON_1);
	byte but2 = digitalRead(PIN_BUTTON_2);
	byte but3 = digitalRead(PIN_BUTTON_3);
    if (  but1== BUT1_ON )
    {
		DEBUG_PRINT("b1="); DEBUG_PRINT(but1);
        curr = button_read_busy ( PIN_BUTTON_1, waitmode, BUTTON_1, is_holding );
    }
    else if ( but2 == BUT2_ON )
    {   
		DEBUG_PRINT("b2="); DEBUG_PRINT(but2);
        curr = button_read_busy ( PIN_BUTTON_2, waitmode, BUTTON_2, is_holding );
    }
    else if ( but3 == BUT3_ON )
    {
		DEBUG_PRINT("b3="); DEBUG_PRINT(but3);
        curr = button_read_busy ( PIN_BUTTON_3, waitmode, BUTTON_3, is_holding );
    }
	//DEBUG_PRINTLN('r');
    // set flags in return value
    byte ret = curr;
    if ( ! ( old&BUTTON_MASK ) && ( curr&BUTTON_MASK ) )
        ret |= BUTTON_FLAG_DOWN;
    if ( ( old&BUTTON_MASK ) && ! ( curr&BUTTON_MASK ) )
        ret |= BUTTON_FLAG_UP;

    old = curr;
    return ret;
}
#endif //BUTTON_ADC_PIN .... OPENSPRINKLER_ARDUINO_FREETRONICS_LCD

/** user interface for setting options during startup */
void OpenSprinkler::ui_set_options ( int oid )
{
    boolean finished = false;
    byte button;
    int i=oid;

    while ( !finished )
    {
        button = button_read ( BUTTON_WAIT_HOLD );

        switch ( button & BUTTON_MASK )
        {
        case BUTTON_1:
            if ( i==OPTION_FW_VERSION || i==OPTION_HW_VERSION || i==OPTION_FW_MINOR ||
                    i==OPTION_HTTPPORT_0 || i==OPTION_HTTPPORT_1 ||
                    i==OPTION_PULSE_RATE_0 || i==OPTION_PULSE_RATE_1 ) break; // ignore non-editable options
            if ( pgm_read_byte ( op_max+i ) != options[i] ) options[i] ++;
            break;

        case BUTTON_2:
            if ( i==OPTION_FW_VERSION || i==OPTION_HW_VERSION || i==OPTION_FW_MINOR ||
                    i==OPTION_HTTPPORT_0 || i==OPTION_HTTPPORT_1 ||
                    i==OPTION_PULSE_RATE_0 || i==OPTION_PULSE_RATE_1 ) break; // ignore non-editable options
            if ( options[i] != 0 ) options[i] --;
            break;

        case BUTTON_3:
            if ( ! ( button & BUTTON_FLAG_DOWN ) ) break;
            if ( button & BUTTON_FLAG_HOLD )
            {
                // if OPTION_RESET is set to nonzero, change it to reset condition value
                if ( options[OPTION_RESET] )
                {
                    options[OPTION_RESET] = 0xAA;
                }
                // long press, save options
                options_save();
                finished = true;
            }
            else
            {
                // click, move to the next option
                if ( i==OPTION_USE_DHCP && options[i] ) i += 9; // if use DHCP, skip static ip set
                else if ( i==OPTION_HTTPPORT_0 ) i+=2; // skip OPTION_HTTPPORT_1
                else if ( i==OPTION_PULSE_RATE_0 ) i+=2; // skip OPTION_PULSE_RATE_1
                else if ( i==OPTION_SENSOR_TYPE && options[i]!=SENSOR_TYPE_RAIN ) i+=2; // if not using rain sensor, skip rain sensor type
                else if ( i==OPTION_MASTER_STATION && options[i]==0 ) i+=3; // if not using master station, skip master on/off adjust
                else if ( i==OPTION_MASTER_STATION_2&& options[i]==0 ) i+=3; // if not using master2, skip master2 on/off adjust
                else
                {
                    i = ( i+1 ) % NUM_OPTIONS;
                }
                if ( i==OPTION_SEQUENTIAL_RETIRED ) i++;
            }
            break;
        }

        if ( button != BUTTON_NONE )
        {
            lcd_print_option ( i );
        }
    }
#ifndef LCD_SSD1306
	lcd.noBlink();
#else

#endif
    
}

/** Set LCD contrast (using PWM) */
void OpenSprinkler::lcd_set_contrast()
{
#ifndef OPENSPRINKLER_ARDUINO_FREETRONICS_LCD
#ifndef LCDI2C
    // set contrast is only valid for standard LCD
    if ( lcd.type() ==LCD_STD )
    {
        pinMode ( PIN_LCD_CONTRAST, OUTPUT );
        analogWrite ( PIN_LCD_CONTRAST, options[OPTION_LCD_CONTRAST] );
    }
#endif
#endif

}

/** Set LCD brightness (using PWM) */
void OpenSprinkler::lcd_set_brightness ( byte value )
{
#ifdef OPENSPRINKLER_ARDUINO_FREETRONICS_LCD
	// value of 1 = on bright / 0 = dim
    pinMode ( PIN_LCD_BACKLIGHT, OUTPUT );
	if (value == 1)
		LCD_BACKLIGHT_ON();
	else
		LCD_BACKLIGHT_OFF();
#else
#if defined(__AVR_ATmega1284P__) || defined(__AVR_ATmega1284__)
    if ( lcd.type() ==LCD_I2C )
    {
        if ( value ) lcd.backlight();
        else lcd.noBacklight();
    }

#endif
#ifndef LCDI2C
    if ( lcd.type() ==LCD_STD )
    {
        pinMode ( PIN_LCD_BACKLIGHT, OUTPUT );
        if ( value )
        {
            analogWrite ( PIN_LCD_BACKLIGHT, 255-options[OPTION_LCD_BACKLIGHT] );
        }
        else
        {
            analogWrite ( PIN_LCD_BACKLIGHT, 255-options[OPTION_LCD_DIMMING] );
        }
    }
#else
	if (value == 1)
		digitalWrite(PIN_LCD_BACKLIGHT, LOW);
	else
		digitalWrite(PIN_LCD_BACKLIGHT, HIGH);
#endif
#endif // OPENSPRINKLER_ARDUINO_FREETRONICS_LCD
}
#endif  // end of LCD and button functions

#ifdef OPENSPRINKLER_ARDUINO_FREEMEM
// ==================
// Print free memory for debugging
// ==================
void OpenSprinkler::lcd_print_memory ( byte line )
{
    lcd.setCursor ( 0, line );
    lcd_print_pgm ( PSTR ( "Free RAM:        " ) );

    lcd.setCursor ( 10, line );
    lcd.print ( freeMemory() );

    lcd.setCursor ( 15, line );
    lcd.write ( status.network_fails>0 ? 1 : 0 );
}
#endif

