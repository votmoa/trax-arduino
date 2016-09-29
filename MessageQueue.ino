//
// Message queue so we can send slowly
//

#define queueLen 10
#define messageInterval 200

String MessageQueue[queueLen];
int Qhead = 0;		// Pop messages off head
int Qtail = -1;		// Push messages onto tail (negative means queue empty)
int Qdrops = 0;		// Queue full drop counter
int QdropIndex = -1;	// Index of queue full message (if we've dropped)

// Push a message on the tail of the queue
int pushMessage(String msg) {
	// Prime the pump
	if (Qtail < 0) {
		Qtail = Qhead;
	}
	// Check for queue full
	if ((Qhead == Qtail+1) || (Qhead == 0 && Qtail == queueLen)) {
		// Reserve the last queue slot for Queue full message
		MessageQueue[Qtail] = "ERROR: Queue full: " + String(++Qdrops,DEC) + " messages dropped";
		QdropIndex = Qtail;	// Track so we can clear counter
		return(0);
	} else {
		// Queue has space
		MessageQueue[Qtail] = msg;
		if (++Qtail > queueLen) {	// Increment and check for wrap
			Qtail = 0;
		}
		return(1);
	}
}

// Pop a message off the head of the queue
String popMessage() {
	if (Qtail >= 0) {
		// There's a message in the queue
		int thisMsg = Qhead;
		if (Qhead == QdropIndex) {	// Reset drop counter if we pop it
			QdropIndex = -1;
			Qdrops = 0;
		}
		if (Qhead == Qtail) {
			Qtail = -1;		// We just popped the last entry
		}
		if (++Qhead > queueLen) {	// Increment and check for wrap
			Qhead = 0;
		}
		return (MessageQueue[thisMsg]);
	}
	return ("");
}

// Send next message in queue if it's time
void sendNextMessage() {
	static unsigned int nextMessage;
	if ((Qtail >= 0) && (millis() >= nextMessage)) {
		Serial.println(popMessage());
		nextMessage = millis() + messageInterval;
	}
}
