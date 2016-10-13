//
// Message queue so we can send slowly
//

#define queueLen 10
#define messageInterval 200

String MessageQueue[queueLen];
int Qhead = 0;		// Index of head of queue
int Qsize = 0;		// Number of messages in queue
int Qdrops = 0;		// Queue full drop counter
int QdropIndex = -1;	// Index of queue full message (if we've dropped)

// Push a message on the tail of the queue
int pushMessage(String msg) {
	// Serial.println(String(millis()) + ": pushMessage: Qhead:" + String(Qhead) + " Qsize:" + String(Qsize));
	if (Qsize == queueLen) {
		// Queue full: overwrite the last slot with "Queue full" message
		int Qtail = (Qhead + Qsize - 1) % queueLen;
		MessageQueue[Qtail] = "ERROR: Queue full: " + String(++Qdrops) + " messages dropped";
		QdropIndex = Qtail;	// Track so we can clear counter
		return(0);
	} else {
		// Queue has space
		int Qtail = (Qhead + Qsize) % queueLen;
		MessageQueue[Qtail] = msg;
		Qsize++;
		return(1);
	}
}

// Pop a message off the head of the queue
String popMessage() {
	// Serial.println(String(millis()) + ": popMessage: Qhead:" + String(Qhead) + " Qsize:" + String(Qsize));
	if (Qsize > 0) {
		// There's a message in the queue
		int thisMsg = Qhead;
		if (Qhead == QdropIndex) {	// Reset drop counter if we pop it
			QdropIndex = -1;
			Qdrops = 0;
		}
		Qhead = (Qhead+1) % queueLen;
		Qsize--;
		return (MessageQueue[thisMsg]);
	}
	return ("");
}

// Send next message in queue if it's time
void sendNextMessage() {
	static unsigned int nextMessage;
	if ((Qsize > 0) && (millis() >= nextMessage)) {
		Serial.println(popMessage());
		nextMessage = millis() + messageInterval;
	}
}
