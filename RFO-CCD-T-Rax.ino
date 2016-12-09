// CCD "T-RAX" System
//
// $Id: RFO-CCD-T-Rax.ino,v 1.5 2016/08/12 20:19:17 dlk Exp dlk $
// $Source: /Users/dlk/Documents/Arduino/RFO-CCD-T-Rax/RCS/RFO-CCD-T-Rax.ino,v $


//ADDED BY JCF libraries to run RGB lcd and creation of lcd object
#include <Wire.h>
#include <rgb_lcd.h>

rgb_lcd lcd;


/*
 * Input/Output Pins
 */

#define weatherOK	 2
#define securityOK	 3
#define bldgPowerIn	 4
#define roofPowerIn	 5
#define mountPowerIn	 6
#define roofOpen	 7
#define roofClosed	 8
#define mountParked	 9
#define roofPowerOut	10
#define mountPowerOut	11
#define fobOutput	12
#define heartLed	13	 // onboard LED

/*
 * Globals
 */

int heartBeatInterval = 1000;	// ms between heart beat flashes

// Array and count of which inputs to watch for status changes
int statusWatchers[] = { weatherOK, securityOK, bldgPowerIn, roofPowerIn, mountPowerIn, roofOpen, roofClosed, mountParked };
int statusWatchersCount = 8;
int statusInterval = 30 * 1000;	// force a status update at least every 30 seconds

// Roof-close mode setup
unsigned int roofCloseNotifyInterval = 5000;	// ms between roof-close mode notifications
int lastWeatherOK = HIGH;	// Track previous weather status
int lastBldgPower = HIGH;	// Track previous power status
int wxCloseRoof = 0;	// Roof-close mode flag for weather
int pwrCloseRoof = 0;	// Roof-close mode flag for power
#define FORCE 1
#define MAYBE 2
#define LAST  3


/*
 * Below is a set of arrays used for pin management
 * Set the count at to the highest input pin PLUS ONE (zero based index)
 * Initialize values in setup(); they will be read by myDigitalRead()
 */

#define pinCount 20		// Index of highest input pin; make sure there are enough!
int LastValue[pinCount];	// used to track previous value of pin
int PinActive[pinCount];	// Array of HIGH/LOW values that make a pin active

// Array of pins to simulate indexed by pin number; set to 1 to simulate pin
int Simulate[pinCount];		// whether a pin is simulated
int SimulateType[pinCount];	// value to simulate (HIGH or LOW)


// Array used to debounce an input (all inputs are debounced for now)
unsigned int DebounceMS[pinCount];	// array to track debounce
int debounce_millis = 100;	// ms to debounce

// Array used to track triggers
int Trigger[pinCount];		// whether pin is a trigger or not
int TriggerActive[pinCount];	// whether trigger is actively triggered

unsigned int ToggleMS[pinCount]; // array to track toggling
int toggle_millis = 1500;	// ms to hold toggled output high

// for userPin
#define myLOW -1
#define myHIGH 1

// User input tracking
int userPin[pinCount];	// -1:LOW 0:notset 1:HIGH
String userBuffer;
String userBufferVersion = "::";
int userBufferFull = 0;

// User commands
#define None		-1
#define Stop		 0
#define RPon		 1
#define RPoff		 2
#define MPon		 3
#define MPoff		 4
#define Open		 5
#define Close		 6
#define OverrideOn	 7
#define OverrideOff	 8
#define DebugOn		 9
#define DebugOff	10
#define Status		11
#define ToggleFob	12
#define CmdCount	13
String Usage;	 // Built dynamically during setup()
String Cmds[] = { "Stop", "RPon", "RPoff", "MPon", "MPoff", "Open", "Close", "OverrideOn", "OverrideOff", "DebugOn", "DebugOff", "Status", "ToggleFob" };
int userCmd = None;	// Takes one of the above #define values:

int EmergencyOverride = 0;
int DebugFlag = 0;

// Setup ------------------------------------------------------------

#define C_DEFAULT    0
#define C_CLOSEMODE  1
#define C_OVERRIDE   2

static int Color[3][3] = {
	{ 255, 0, 0 },    // C_DEFAULT: red
	{ 0, 0, 255 },    // C_CLOSEMODE: blue
	{ 0, 255, 255 },  // C_OVERRIDE: cyan
};
int lastColor = -1;


void setup() 
{
	// ADDED BY JCF set up the LCD's number of columns and rows, set color, and write initial message:
	lcd.begin(16, 2);	 
	lcd.setRGB(Color[C_DEFAULT][0], Color[C_DEFAULT][1], Color[C_DEFAULT][2]);
	lcd.write("RFO Roof Control");
	lcd.setCursor(0,1);
	lcd.write("Let's Roll! v0.1");

	Serial.begin(57600);
	Serial.println("Bootin' Up!");

	pinMode(weatherOK, INPUT);		// Boltwood "Weather OK" signal 
	pinMode(securityOK, INPUT);		// Signal from the Arduino based CCD security system
	pinMode(bldgPowerIn, INPUT);		// Power detection for the buidling
	pinMode(roofPowerIn, INPUT);		// Power detection for the roof
	pinMode(mountPowerIn, INPUT);		// Power detection for the mount
	pinMode(roofOpen, INPUT);		// Signal from roof open position sensor
	pinMode(roofClosed, INPUT);		// Signal from roof closed position sensor
	pinMode(mountParked, INPUT);		// Park position sensor for the mount
	pinMode(roofPowerOut, OUTPUT);		// Roof Power enable
	pinMode(mountPowerOut, OUTPUT);		// Mount Power enable
	pinMode(fobOutput, OUTPUT);		// Output to physical fob
	pinMode(heartLed, OUTPUT);		// Heartbeat onboard LED

	// Initialize outputs and variables
	digitalWrite(mountPowerOut, HIGH);
	digitalWrite(roofPowerOut, HIGH);
	digitalWrite(fobOutput, LOW);
	digitalWrite(heartLed, LOW);

	// Default all pins to active HIGH; change by hand
	for (int pin = 0; pin < pinCount; pin++) {
		PinActive[pin] = HIGH;
	}
	//PinActive[xxx] = LOW;
	PinActive[weatherOK] = LOW;
	PinActive[roofPowerOut] = LOW;
	PinActive[mountPowerOut] = LOW;

	// override some pins; see myDigitalRead()
	//Simulate[xxx] = 1; SimulateType[xxx] = HIGH|LOW;
	Simulate[bldgPowerIn] = 1; SimulateType[bldgPowerIn] = HIGH;
	Simulate[securityOK] = 1; SimulateType[securityOK] = HIGH;

	// Define triggers
	// Trigger[openCloseIn] = 1;

	// Dynamically build Usage string
	Usage = "Supported commands:";
	for (int i=0; i<CmdCount; i++) {
		 Usage.concat(String(' ') + Cmds[i]);
	}
	pushMessage("INFO: " + Usage);

}



// Flash the onboard LED so we know we're alive
void flashHeartBeat() {
	static int heartBeat = 0;	// toggle to flash onboard LED
	static unsigned int nextBeat = 0;
	if (millis() >= nextBeat) {	
		digitalWrite(heartLed, heartBeat ? HIGH : LOW);
		heartBeat = !heartBeat;
		nextBeat = millis() + heartBeatInterval;
	}
}


extern unsigned int __bss_end;
extern unsigned int __heap_start;
//extern void *__brkval;

int freeMemory() {
	int free_memory;
	free_memory = ((int)&free_memory) - ((int)&__bss_end);
	return free_memory;
}


// Tally and periodically report stats
void reportStatsMaybe() {
	static int statsMillis = 60000;  // Report stats every 60 seconds
	static unsigned int nextStats = 0;
	static int loopCount = 0;

	if (millis() > nextStats) {
		if (nextStats == 0) {
			// First time through
			nextStats = millis() + statsMillis;
		} else {
			// Time to report
			String msg;
			msg = "INFO:";
			msg.concat(" loopCount:" + String(loopCount/(statsMillis/1000)) + "/sec");
			//msg.concat(" mem:" + String(freeMemory()));
			pushMessage(msg);
				loopCount = 0;  //reset
				nextStats = millis() + statsMillis;
		}
	} else {
		loopCount++;
	}
}

// Send update if input status changes or a timer expires
void statusUpdateMaybe() {
	static unsigned int nextStatus;
	static unsigned int lastStatus;
	unsigned int thisStatus = 0;
	for (int i=0; i<statusWatchersCount; i++) {
		thisStatus += myDigitalRead(statusWatchers[i]) * 10^i;
	}
	if ((millis() >= nextStatus) || (thisStatus != lastStatus)) {
		printStatus();
		nextStatus = millis() + statusInterval;
		lastStatus = thisStatus;
	}
}


void SerialDebug(String msg) {
	if (DebugFlag) {
		Serial.println("DEBUG: " + msg);
	}
}


/*
 * Return value of pin, but allow us to simulate, debounce, trigger, etc
 */

int myDigitalRead(int pin) {

	// First let simulations override
	if (Simulate[pin]) {
		return(SimulateType[pin]);
	}

	// Then get the actual pin value
	int value = digitalRead(pin);

	// Debounce ignores actual value for 100ms after a change
	// All inputs debounce but might want to change that
	if (DebounceMS[pin]) {
		if (millis() < DebounceMS[pin]) {
			SerialDebug("Debounce " + String(pin) + " active: old:" + String(LastValue[pin]) + " new:" + String(value));
			value = LastValue[pin];	// debounce active
		} else {
			SerialDebug("Debounce " + String(pin) + " done: old:" + String(LastValue[pin]) + " new:" + String(value));
			DebounceMS[pin] = 0;	// debounce done
		}
	} else {
		if (value != LastValue[pin]) {
			SerialDebug("Debounce " + String(pin) + " starting: old:" + String(LastValue[pin]) + " new:" + String(value));
			DebounceMS[pin] = millis() + debounce_millis;	// start debounce timer
		}
	}

	//	A trigger reports exactly once then ignores input until pin value restores
	if (Trigger[pin]) {
		if (TriggerActive[pin]) {
			// Trigger active...
			if (value == LastValue[pin]) {
				// ...but still being ignored; report non-triggered value
				return((PinActive[pin] == HIGH) ? LOW : HIGH);	// return() so we don't update LastValue[]
			} else {
				// ...trigger has reset; start listening to input again
				TriggerActive[pin] = 0;
			}
		} else {
			// Trigger not active...
			if (value == PinActive[pin]) {
				// ...but it is now! Report value once but suppress next query
				TriggerActive[pin] = 1;
			}
		}
	}

	// We're done! Report whatever we decided above
	LastValue[pin] = value;
	return(value);
}


// Support active LOW
void myDigitalWrite(int pin, int value) {
	// Reverse value if pin is active LOW
	if (PinActive[pin] == LOW) {
		value = (value == LOW) ? HIGH : LOW;
	}
	digitalWrite(pin, value);
}


/*
 * Briefly toggle the value of the ouput
 */

void toggle(int pin) {
	if (ToggleMS[pin]) {
		// we're already active; just ignore
	} else {
		digitalWrite(pin, HIGH);	// activate pin
		ToggleMS[pin] = millis() + toggle_millis;	// start timer
	}
}

void toggleReset() {
	for (int pin = 0; pin < pinCount; pin++) {
		if (ToggleMS[pin] && ToggleMS[pin] < millis()) {
			// toggle has timed out
			digitalWrite(pin, LOW);
			ToggleMS[pin] = 0;
		}
	}
}


/*
 *	Read inputs but return true/false based on PinActive[] array
 */

int sensorInput(int pin) {
	return(myDigitalRead(pin) == PinActive[pin]);
}



/*
 * Pull any data waiting on serial port and stash in userBuffer, then set userCmd
 */
void serialSuck() {

	// Suck in all the data available
	while (Serial.available() && !userBufferFull) {
		char c = Serial.read();

		if (c >= 32 && c <= 126) {
			userBuffer += c;
		} else if (c == 10) {	// newline
			userBufferFull = 1;
		} // skip anything else
	}

	// Then scan the line and update userPin[]
	if (userBufferFull) {

		// Skip blank lines
		if (userBuffer.length() == 0) {
			userBufferFull = 0;
			return;
		}
		SerialDebug("userBuffer: " + userBuffer);

		// Overwrite first line with new command
		lcd.setCursor(0, 0);
		lcd.write("                ");
		lcd.setCursor(0, 0);
		lcd.print(userBuffer.substring(2));

		if (userBuffer.startsWith(userBufferVersion)) {
			if (userCmd != None) {
				pushMessage("WARNING: New command received before last command (" + Cmds[userCmd] + ") was processed!");
			}

			String userCmdStr = userBuffer.substring(userBufferVersion.length());

			int found = 0;
			for (int i=0; i<CmdCount; i++) {
				if (userCmdStr == Cmds[i]) {
					userCmd = i;
					found = 1;
					SerialDebug("userCmd: " + String(i));
				}
			}

			if (!found) {
				pushMessage("ERROR: Unknown command: " + userCmdStr);
				pushMessage("INFO: " + Usage);
			}

			// Now clear the buffer
			userBuffer.remove(0);
			userBufferFull = 0;

		} else {
			pushMessage("ERROR: Prefix mismatch; expecting " + userBufferVersion);
			pushMessage("INFO: " + Usage);
			userBuffer.remove(0);
			userBufferFull = 0;
		}
	}
}


void printStatus() {
	String msg = "INFO:";
	msg.concat(" securityOK:" + String(sensorInput(securityOK), DEC));
	msg.concat(" weatherOK:" + String(sensorInput(weatherOK), DEC));
	msg.concat(" roofOpen:" + String(sensorInput(roofOpen), DEC));
	msg.concat(" roofClosed:" + String(sensorInput(roofClosed), DEC));
	msg.concat(" mountParked:" + String(sensorInput(mountParked), DEC));
	msg.concat(" bldgPowerIn:" + String(sensorInput(bldgPowerIn), DEC));
	msg.concat(" roofPowerIn:" + String(sensorInput(roofPowerIn), DEC));
	msg.concat(" mountPowerIn:" + String(sensorInput(mountPowerIn), DEC));
	pushMessage(msg);
}


/*
 * Print message if timer has expired
 *	 MAYBE prints if a new message or timer has expired
 *	 FORCE always prints and resets timer
 *	 LAST always prints and clears timer
 */
void roofCloseNotify(int how, String msg) {
	static unsigned int nextRoofCloseNotify = 0;
	static String lastRoofMsg;
	// Force printing if we have a new message
	if ((how == MAYBE) && (lastRoofMsg != msg)) {
		how = FORCE;
	}
	// Has timer expired?
	if ((how == MAYBE) && (nextRoofCloseNotify > 0) && (millis() < nextRoofCloseNotify)) {
		return;
	}
	// Notify and update timer
	pushMessage(msg);
	if (how == LAST) {
		nextRoofCloseNotify = 0;
		lastRoofMsg = String("");
	} else {
		nextRoofCloseNotify = millis() + roofCloseNotifyInterval;
		lastRoofMsg = msg;
	}
}


/*
 *	Main processing ------------------------------------------------
 */

void loop() {

	flashHeartBeat();

	reportStatsMaybe();

	// Send update if status changes or timer expires
	statusUpdateMaybe();

	// Send the next message in the queue if it's time
	sendNextMessage();

	// Reset any toggles that time out
	toggleReset();

	/*
	 * Process user commands
	 */

	// Fill serial buffer and set userCmd
	serialSuck();

	// Stop: EMERGENCY STOP: turn off roof power
	if (userCmd == Stop) {
		pushMessage("INFO: EMERGENCY STOP; Turning off roof power");
		pushMessage("INFO: You will need to Override to recover from this");
		myDigitalWrite(roofPowerOut, LOW);
		userCmd = None;
	}

	// If we're moving and the scope becomes unparked, turn off roof
	// Except we don't track moving just yet

	// RPon: Turn roof power on
	if (userCmd == RPon) {
		if (EmergencyOverride) {
			pushMessage("OVERRIDE: Turning on roof power");
			myDigitalWrite(roofPowerOut, HIGH);
		} else {
			if (sensorInput(roofPowerIn)) {
				pushMessage("ERROR: Roof is already on!");
			} else {
				pushMessage("INFO: Turning on roof power");
				myDigitalWrite(roofPowerOut, HIGH);
			}
		}
		userCmd = None;
	}

	// RPoff: Turn roof power off
	if (userCmd == RPoff) {
		if (EmergencyOverride) {
			pushMessage("OVERRIDE: Turning off roof power");
			myDigitalWrite(roofPowerOut, LOW);
		} else {
			if (sensorInput(roofPowerIn)) {
				pushMessage("INFO: Turning off roof power");
				myDigitalWrite(roofPowerOut, LOW);
			} else {
				pushMessage("ERROR: Roof is already off!");
			}
		}
		userCmd = None;
	}

	// MPon: Turn mount power on
	if (userCmd == MPon) {
		if (EmergencyOverride) {
			pushMessage("OVERRIDE: Turning on mount power");
			myDigitalWrite(mountPowerOut, HIGH);
		} else {
			if (sensorInput(mountPowerIn)) {
				pushMessage("ERROR: Mount is already on!");
			} else {
				if (!sensorInput(roofOpen)) {
					pushMessage("ERROR: Cannot turn on mount: roof is not open");
				} else {
					pushMessage("INFO: Turning on mount power");
					myDigitalWrite(mountPowerOut, HIGH);
				}
			}
		}
		userCmd = None;
	}

	// MPoff: Turn mount power off
	if (userCmd == MPoff) {
		if (EmergencyOverride) {
			pushMessage("OVERRIDE: Turning off mount power");
			myDigitalWrite(mountPowerOut, LOW);
		} else {
			if (sensorInput(mountPowerIn)) {
				pushMessage("INFO: Turning off mount power");
				myDigitalWrite(mountPowerOut, LOW);
			} else {
				pushMessage("ERROR: Mount is already off!");
			}
		}
		userCmd = None;
	}

	// ToggleFob: Open or close the roof
	if (userCmd == ToggleFob) {
		if (EmergencyOverride) {
			pushMessage("OVERRIDE: Toggling fob");
			toggle(fobOutput);
		} else {
			if (sensorInput(roofClosed)) {
				if (sensorInput(bldgPowerIn)) {
					if (sensorInput(roofPowerIn)) {
						if (sensorInput(mountParked)) {
							if (!sensorInput(mountPowerIn)) {
								if (sensorInput(weatherOK)) {
									if (sensorInput(securityOK)) {
										pushMessage("INFO: Toggling fob: opening roof");
										toggle(fobOutput);
									} else {
										pushMessage("ERROR: Cannot open roof: security not OK");
									}
								} else {
									pushMessage("ERROR: Cannot open roof: weather not OK");
								}
							} else {
								pushMessage("ERROR: Cannot open roof: mount power is on");
							}
						} else {
							pushMessage("ERROR: Cannot open roof: mount must be parked first");
						}
					} else {
						pushMessage("ERROR: Cannot open roof: roof power is not on");
					}
				} else {
					pushMessage("ERROR: Cannot open roof: building power has failed");
				}

			} else if (sensorInput(roofOpen)) {
				if (sensorInput(roofPowerIn)) {
					if (sensorInput(mountParked)) {
						if (!sensorInput(mountPowerIn)) {
							pushMessage("INFO: Toggling fob: closing roof");
							toggle(fobOutput);
						} else {
							pushMessage("ERROR: Cannot close roof: mount power is on");
						}
					} else {
						pushMessage("ERROR: Cannot close roof: mount must be parked first");
					}
				} else {
					pushMessage("ERROR: Cannot close roof: roof power is not on");
				}
			} else {
				pushMessage("INFO: Toggling fob");
				toggle(fobOutput);
			}
		}
		userCmd = None;
	}

	// Open: Open the roof
	if (userCmd == Open) {
		if (EmergencyOverride) {
			pushMessage("OVERRIDE: Opening roof");
			toggle(fobOutput);
		} else {
			if (sensorInput(roofClosed)) {
				if (sensorInput(bldgPowerIn)) {
					if (sensorInput(roofPowerIn)) {
						if (sensorInput(mountParked)) {
							if (!sensorInput(mountPowerIn)) {
								if (sensorInput(weatherOK)) {
									if (sensorInput(securityOK)) {
										pushMessage("INFO: Opening roof");
										toggle(fobOutput);
									} else {
										pushMessage("ERROR: Cannot open roof: security not OK");
									}
								} else {
									pushMessage("ERROR: Cannot open roof: weather not OK");
								}
							} else {
								pushMessage("ERROR: Cannot open roof: mount power is on");
							}
						} else {
							pushMessage("ERROR: Cannot open roof: mount must be parked first");
						}
					} else {
						pushMessage("ERROR: Cannot open roof: roof power is not on");
					}
				} else {
					pushMessage("ERROR: Cannot open roof: building power has failed");
				}
			} else {
				pushMessage("ERROR: Cannot open roof: roof is not closed!");
			}
		}
		userCmd = None;
	}

	// Close: Close the roof
	if (userCmd == Close) {
		if (EmergencyOverride) {
			pushMessage("OVERRIDE: Closing roof");
			toggle(fobOutput);
		} else {
			if (sensorInput(roofOpen)) {
				if (sensorInput(roofPowerIn)) {
					if (sensorInput(mountParked)) {
						if (!sensorInput(mountPowerIn)) {
							pushMessage("INFO: Closing roof");
							toggle(fobOutput);
						} else {
							pushMessage("ERROR: Cannot close roof: mount power is on");
						}
					} else {
						pushMessage("ERROR: Cannot close roof: mount must be parked first");
					}
				} else {
					pushMessage("ERROR: Cannot close roof: roof power is not on");
				}
			} else {
				pushMessage("ERROR: Cannot close roof: roof is not open");
			}
		}
		userCmd = None;
	}

	// OverrideOn: Enable master override
	if (userCmd == OverrideOn) {
		pushMessage("INFO: Entering emergency override; BE VERY CAREFUL");
		//lcd.setRGB(Cyan[0], Cyan[1], Cyan[2]);
		EmergencyOverride = 1;
		userCmd = None;
	}

	// OverrideOn: Enable master override
	if (userCmd == OverrideOff) {
		pushMessage("INFO: Exiting emergency override; *whew*");
		//lcd.setRGB(Red[0], Red[1], Red[2]);
		EmergencyOverride = 0;
		userCmd = None;
	}

	// DebugOn: Enable debugging messages
	if (userCmd == DebugOn) {
		pushMessage("INFO: Enabling debug messages");
		DebugFlag = 1;
		userCmd = None;
	}
	
	// DebugOff: Disable debugging messages
	if (userCmd == DebugOff) {
		pushMessage("INFO: Disabling debug messages");
		DebugFlag = 0;
		userCmd = None;
	}

	// Status: Print sensor status
	if (userCmd == Status) {
		printStatus();
		userCmd = None;
	}


	/*
	 * Process sensor inputs
	 */

	// Weather changes
	int currWeatherOK = sensorInput(weatherOK);
	if (currWeatherOK != lastWeatherOK) {
		if (currWeatherOK) {
			// Weather was not OK but has recovered
			pushMessage("INFO: Weather sensor recovery; resuming normal operation");
			wxCloseRoof = 0;
			if (EmergencyOverride) {
				//lcd.setRGB(Cyan[0], Cyan[1], Cyan[2]);
			} else {
				//lcd.setRGB(Red[0], Red[1], Red[2]);
			}
		} else {
			// Weather was OK but is no longer
			if (EmergencyOverride) {
				pushMessage("OVERRIDE: Weather sensor indicates inclemency; ignoring");
			} else {
				pushMessage("WARNING: Weather sensor indicates inclemency; entering roof-close mode");
				wxCloseRoof = 1;
				//lcd.setRGB(Blue[0], Blue[1], Blue[2]);
			}
		}
		// Track weather status
		lastWeatherOK = currWeatherOK;
	}

	// Building power drops
	int currBldgPower = sensorInput(bldgPowerIn);
	if (currBldgPower != lastBldgPower) {
		if (currBldgPower) {
			// Building power was not OK but has recovered
			pushMessage("WARNING: Building power recovery; resuming normal operation");
			pwrCloseRoof = 0;
			if (EmergencyOverride) {
				//lcd.setRGB(Cyan[0], Cyan[1], Cyan[2]);
			} else {
				//lcd.setRGB(Red[0], Red[1], Red[2]);
			}
		} else {
			// Weather was OK but is no longer
			if (EmergencyOverride) {
				pushMessage("OVERRIDE: Building power failure; ignoring");
			} else {
				roofCloseNotify(FORCE, "WARNING: Building power failure; entering roof-close mode");
				pwrCloseRoof = 1;
				//lcd.setRGB(Blue[0], Blue[1], Blue[2]);
			}
		}
		// Track weather status
		lastBldgPower = currBldgPower;
	}

	// Security changes -- what do we do here???

	// Are we in WX or PWR roof-close mode?
	if (wxCloseRoof || pwrCloseRoof) {
		static int roofClosing = 0;
		if (EmergencyOverride) {
			roofCloseNotify(LAST, "OVERRIDE: Cancelling roof-close mode");
			wxCloseRoof = 0;
			pwrCloseRoof = 0;
			roofClosing = 0;
		} else {
			if (sensorInput(roofClosed)) {
				roofCloseNotify(LAST, "INFO: Roof-close mode: mount power off; exiting roof-close mode");
				wxCloseRoof = 0;
				pwrCloseRoof = 0;
				roofClosing = 0;
			} else if (sensorInput(roofOpen)) {
				if (sensorInput(roofPowerIn)) {
					if (sensorInput(mountParked)) {
						if (sensorInput(mountPowerIn)) {
							roofCloseNotify(MAYBE, "INFO: Roof-close mode: mount parked; turning off mount power");
							myDigitalWrite(mountPowerOut, LOW);
						} else {
							if (roofClosing) {
								roofCloseNotify(MAYBE, "INFO: Roof-close mode: waiting for roof to start closing");
							} else {
								roofCloseNotify(FORCE, "INFO: Roof-close mode: closing roof");
								toggle(fobOutput);
								roofClosing = 1;
							}
						}
					} else {
						if (sensorInput(mountPowerIn)) {
							roofCloseNotify(MAYBE, "INFO: Roof-close mode: waiting for mount to park");
						} else {
							roofCloseNotify(MAYBE, "INFO: Roof-close mode: Turning on mount power");
							myDigitalWrite(mountPowerOut, HIGH);
						}
					}
				} else {
					roofCloseNotify(MAYBE, "INFO: Roof-close mode: Turning on roof power");
					myDigitalWrite(roofPowerOut, HIGH);
				}
			} else {
				roofCloseNotify(MAYBE, "INFO: Roof-close mode: waiting for roof to close");
			}
		}
	}

	// Update display color
	int currColor;
	if (EmergencyOverride) {
		currColor = C_OVERRIDE;
	} else {
		if (!currWeatherOK || !currBldgPower) {
			currColor = C_CLOSEMODE;
		} else {
			currColor = C_DEFAULT;
		}
	}
	if (currColor != lastColor) {
		lcd.setRGB(Color[currColor][0], Color[currColor][1], Color[currColor][2]);
		lastColor = currColor;
	}

}

