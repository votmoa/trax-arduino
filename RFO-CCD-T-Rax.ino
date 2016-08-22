// CCD "T-RAX" System
//
// $Id: RFO-CCD-T-Rax.ino,v 1.5 2016/08/12 20:19:17 dlk Exp dlk $
// $Source: /Users/dlk/Documents/Arduino/RFO-CCD-T-Rax/RCS/RFO-CCD-T-Rax.ino,v $

/*
 * Input/Output Pins
 */

#define weatherOK       2
#define securityOK      3
#define bldgPowerIn     4
#define roofPowerIn     5
#define mountPowerIn    6
#define roofOpen        7
#define roofClosed      8
#define mountParked     9
#define roofPowerOut   10
#define mountPowerOut  11
#define fobOutput      12
#define heartLed       13   // onboard LED

/*
 * Globals
 */

int heartBeat = 0;  // toggle to flash onboard LED
unsigned int nextBeat = 0;   // timer for heartBeat

int sysStatus;
int counter;
int initialize;


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

unsigned int ToggleMS[pinCount];		// array to track toggling
int toggle_millis = 1500;	// ms to hold toggled output high

// for userPin
#define myLOW -1
#define myHIGH 1

// User input tracking
int userPin[pinCount];  // -1:LOW 0:notset 1:HIGH
String userBuffer;
String userBufferVersion = "::";
int userBufferFull = 0;

// User commands
#define None        -1
#define Stop         0
#define RPon         1
#define RPoff        2
#define MPon         3
#define MPoff        4
#define Open         5
#define Close        6
#define OverrideOn   7
#define OverrideOff  8
#define DebugOn      9
#define DebugOff    10
#define Status      11
#define CmdCount    12
String Usage;   // Built dynamically during setup()
String Cmds[] = { "Stop", "RPon", "RPoff", "MPon", "MPoff", "Open", "Close", "OverrideOn", "OverrideOff", "DebugOn", "DebugOff", "Status" };
int userCmd = None;  // Takes one of the above #define values:

int EmergencyOverride = 0;
int DebugFlag = 0;


void setup() 
{
  Serial.begin(9600);
  Serial.println("Bootin' Up!");

  pinMode(weatherOK, INPUT);      // Boltwood "Weather OK" signal 
	pinMode(securityOK, INPUT);	    // Signal from the Arduino based CCD security system
  pinMode(bldgPowerIn, INPUT);    // Power detection for the buidling
  pinMode(roofPowerIn, INPUT);    // Power detection for the roof
  pinMode(mountPowerIn, INPUT);   // Power detection for the mount
	pinMode(roofOpen, INPUT);       // Signal from roof open position sensor
	pinMode(roofClosed, INPUT);	    // Signal from roof closed position sensor
  pinMode(mountParked, INPUT);    // Park position sensor for the mount
  pinMode(roofPowerOut, INPUT);   // Roof Power enable
	pinMode(mountPowerOut, OUTPUT);	// Mount Power enable
	pinMode(fobOutput, OUTPUT);	    // Output to physical fob
	pinMode(heartLed, OUTPUT);	    // Heartbeat onboard LED

	//Initialize outputs and variables
	digitalWrite(mountPowerOut, LOW);
  digitalWrite(roofPowerOut, LOW);
	digitalWrite(fobOutput, LOW);
	digitalWrite(heartLed, LOW);

	sysStatus = 0;
	counter = 0;
	initialize = 0;

	// Default all pins to active HIGH; change by hand
	for (int pin = 0; pin < pinCount; pin++) {
		PinActive[pin] = HIGH;
	}
	//PinActive[xxx] = LOW;

	// override some pins; see myDigitalRead()
//	Simulate[weatherOK] = 1;       SimulateType[weatherOK] = HIGH;
//	Simulate[securityOK] = 1;      SimulateType[securityOK]  = HIGH;
//	Simulate[mountPowerIn] = 1;      SimulateType[mountPowerIn]   = HIGH;
//	Simulate[allStopIn] = 1;       SimulateType[allStopIn] = LOW;
//	Simulate[systemOverride] = 1;  SimulateType[systemOverride] = LOW;
//	Simulate[userRoofPowerToggle] = 1;  SimulateType[userRoofPowerToggle] = LOW;
//	Simulate[userMountPowerToggle] = 1;  SimulateType[userMountPowerToggle] = LOW;

	// Define triggers
//	Trigger[openCloseIn] = 1;

  // Dynamically build Usage string
  Usage = "Supported commands:";
  for (int i=0; i<CmdCount; i++) {
     Usage.concat(String(' ') + Cmds[i]);
  }
  Serial.println("INFO: " + Usage);

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
			value = LastValue[pin];  // debounce active
		} else {
			DebounceMS[pin] = 0;  // debounce done
		}
	} else {
		if (value != LastValue[pin]) {
			DebounceMS[pin] = millis() + debounce_millis;  // start debounce timer
		}
	}

	//  A trigger reports exactly once then ignores input until pin value restores
	if (Trigger[pin]) {
		if (TriggerActive[pin]) {
			// Trigger active...
			if (value == LastValue[pin]) {
				// ...but still being ignored; report non-triggered value
				return((PinActive[pin] == HIGH) ? LOW : HIGH);  // return() so we don't update LastValue[]
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


// Just in case I need it
void myDigitalWrite(int pin, int value) {
  digitalWrite(pin, value);
}


/*
 * Briefly toggle the value of the ouput
 */

void toggle(int pin) {
	if (ToggleMS[pin]) {
		// we're already active; just ignore
	} else {
		digitalWrite(pin, HIGH);  // activate pin
		ToggleMS[pin] = millis() + toggle_millis;  // start timer
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
 *  Read inputs but return true/false based on PinActive[] array
 *  Also hook for USB stuff when we figure that out
 */

int sensorInput(int pin) {
	return(myDigitalRead(pin) == PinActive[pin]);
}

// userInput checks both USB and actual input pin
int userInput(int pin) {
	// Put USB input code here...
	if (userPin[pin]) {
		int value = (userPin[pin]==myHIGH) ? HIGH : LOW;
		userPin[pin] = 0;
		return(value);
	}

	// Else check an input pin
	return(sensorInput(pin));
}



/*
 * Pull any data waiting on serial port and stash in userBuffer, then tweak userPin array
 */
void serialSuck() {

	// Suck in all the data available
	while (Serial.available() && !userBufferFull) {
		char c = Serial.read();
		if (c >= 32 && c <= 126) {
			userBuffer += c;
		} else if (c == 10) {  // newline
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

		if (userBuffer.startsWith(userBufferVersion)) {
      if (userCmd != None) {
        Serial.println("WARNING: New command received before last command (" + Cmds[userCmd] + ") was processed!");
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
        Serial.println("ERROR: Unknown command: " + userCmdStr);
        Serial.println("INFO: " + Usage);
      }

			// Now clear the buffer
			userBuffer.remove(0);
			userBufferFull = 0;
		} else {
			Serial.println("ERROR: Prefix mismatch; expecting " + userBufferVersion);
      Serial.println("INFO: " + Usage);
			userBuffer.remove(0);
			userBufferFull = 0;
		}
	}
}

/*
 *  Main processing
 */

void loop() {

	// Flash the onboard LED so we know we're alive
	if (millis() >= nextBeat) {  
		digitalWrite(heartLed, heartBeat ? HIGH : LOW);
		heartBeat = !heartBeat;
		nextBeat = millis() + 250;
	}

	// Reset any toggles that time out
	toggleReset();

  /*
   * Process user commands
   */

	// Fill serial buffer and set userCmd
	serialSuck();

	// Stop: EMERGENCY STOP: turn off roof power
	if (userCmd == Stop) {
    Serial.println("INFO: EMERGENCY STOP; Turning off roof power");
    Serial.println("INFO: You will need to Override to recover from this");
		myDigitalWrite(roofPowerOut, LOW);
    userCmd = None;
	}

	// If we're moving and the scope becomes unparked, turn off roof
	// Except we don't track moving just yet

  // RPon: Turn roof power on
  if (userCmd == RPon) {
    if (EmergencyOverride) {
      Serial.println("OVERRIDE: Turning on roof power");
      myDigitalWrite(roofPowerOut, HIGH);
    } else {
      if (sensorInput(roofPowerIn)) {
        Serial.println("ERROR: Roof is already on!");
      } else {
        Serial.println("INFO: Turning on roof power");
        myDigitalWrite(roofPowerOut, HIGH);
      }
    }
    userCmd = None;
  }

	// RPoff: Turn roof power off
	if (userCmd == RPoff) {
    if (EmergencyOverride) {
      Serial.println("OVERRIDE: Turning off roof power");
      myDigitalWrite(roofPowerOut, LOW);
    } else {
      if (sensorInput(roofPowerIn)) {
  			Serial.println("INFO: Turning off roof power");
  			myDigitalWrite(roofPowerOut, LOW);
  		} else {
        Serial.println("ERROR: Roof is already off!");
  		}
    }
    userCmd = None;
	}

  // MPon: Turn mount power on
  if (userCmd == MPon) {
    if (EmergencyOverride) {
      Serial.println("OVERRIDE: Turning on mount power");
      myDigitalWrite(mountPowerOut, HIGH);
    } else {
      if (sensorInput(mountPowerIn)) {
        Serial.println("ERROR: Mount is already on!");
      } else {
        Serial.println("INFO: Turning on mount power");
        myDigitalWrite(mountPowerOut, HIGH);
      }
    }
    userCmd = None;
  }

  // MPoff: Turn mount power off
  if (userCmd == MPoff) {
    if (EmergencyOverride) {
      Serial.println("OVERRIDE: Turning off mount power");
      myDigitalWrite(mountPowerOut, LOW);
    } else {
      if (sensorInput(mountPowerIn)) {
        Serial.println("INFO: Turning off mount power");
        myDigitalWrite(mountPowerOut, LOW);
      } else {
        Serial.println("ERROR: Mount is already off!");
      }
    }
    userCmd = None;
  }

  // Open: Open the roof
  if (userCmd == Open) {
    if (EmergencyOverride) {
      Serial.println("OVERRIDE: Opening roof");
      toggle(fobOutput);
    } else {
      if (sensorInput(roofClosed)) {
        if (sensorInput(bldgPowerIn)) {
          if (sensorInput(roofPowerIn)) {
            if (sensorInput(mountParked)) {
              if (sensorInput(weatherOK)) {
                if (sensorInput(securityOK)) {
                  Serial.println("INFO: Opening roof");
                  toggle(fobOutput);
                } else {
                  Serial.println("ERROR: Cannot open roof: security not OK");
                }
              } else {
                Serial.println("ERROR: Cannot open roof: weather not OK");
              }
            } else {
              Serial.println("ERROR: Cannot open roof: mount must be parked first");
            }
          } else {
            Serial.println("ERROR: Cannot open roof: roof power is not on");
          }
        } else {
          Serial.println("ERROR: Cannot open roof: building power has failed");
        }
      } else {
        Serial.println("ERROR: Cannot open roof: roof is not closed!");
      }
    }
    userCmd = None;
  }

  // Close: Close the roof
  if (userCmd == Close) {
    if (EmergencyOverride) {
      Serial.println("OVERRIDE: Closing roof");
      toggle(fobOutput);
    } else {
      if (sensorInput(roofOpen)) {
        if (sensorInput(roofPowerIn)) {
          if (sensorInput(mountParked)) {
            Serial.println("INFO: Closing roof");
            toggle(fobOutput);
          } else {
            Serial.println("ERROR: Cannot close roof: mount must be parked first");
          }
        } else {
          Serial.println("ERROR: Cannot close roof: roof power is not on");
        }
      } else {
        Serial.println("ERROR: Cannot close roof: roof is not open");
      }
    }
    userCmd = None;
  }

  // OverrideOn: Enable master override
  if (userCmd == OverrideOn) {
    Serial.println("INFO: Entering emergency override; be vewwy vewwy careful");
    EmergencyOverride = 1;
    userCmd = None;
  }

  // OverrideOn: Enable master override
  if (userCmd == OverrideOff) {
    Serial.println("INFO: Exiting emergency override; *whew*");
    EmergencyOverride = 0;
    userCmd = None;
  }

  // DebugOn: Enable debugging messages
  if (userCmd == DebugOn) {
    Serial.println("INFO: Enabling debug messages");
    DebugFlag = 1;
    userCmd = None;
  }
  
  // DebugOff: Disable debugging messages
  if (userCmd == DebugOff) {
    Serial.println("INFO: Disabling debug messages");
    DebugFlag = 0;
    userCmd = None;
  }

  // Status: Print sensor status
  if (userCmd == Status) {
    Serial.print("INFO:");
    Serial.print(" securityOK:" + String(sensorInput(securityOK), DEC));
    Serial.print(" weatherOK:" + String(sensorInput(weatherOK), DEC));
    Serial.print(" roofOpen:" + String(sensorInput(roofOpen), DEC));
    Serial.print(" roofClosed:" + String(sensorInput(roofClosed), DEC));
    Serial.print(" mountParked:" + String(sensorInput(mountParked), DEC));
    Serial.print(" bldgPowerIn:" + String(sensorInput(bldgPowerIn), DEC));
    Serial.print(" roofPowerIn:" + String(sensorInput(roofPowerIn), DEC));
    Serial.print(" mountPowerIn:" + String(sensorInput(mountPowerIn), DEC));
    Serial.println();
    userCmd = None;
  }

  /*
   * Process sensor inputs
   */

	// Weather changes

	// Building power drops

	// Security changes

}

