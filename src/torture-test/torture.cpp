#include <Arduino.h>

int tickpin = 25;

void forward_tick() {
  for (int i=0; i<32; i++) {
    digitalWrite(tickpin, HIGH); delayMicroseconds(900);
    digitalWrite(tickpin, LOW); delayMicroseconds(100);
  }
	tickpin = (tickpin == 25 ? 27 : 25);
  delay(125-32);
}

void reverse_tick() {
	digitalWrite(tickpin, HIGH); delay(12);
	digitalWrite(tickpin, LOW); delay(12);
	tickpin = (tickpin == 25 ? 27 : 25);
	digitalWrite(tickpin, HIGH); delay(28);
  delay(250-12-12-28);
}

void setup() {
  pinMode(25, OUTPUT);
  pinMode(27, OUTPUT);

  while(true) {
    // 60 ticks forward
    for (int i=0; i<60; i++) forward_tick();
    delay(5000);

    // 60 ticks backwards
    for (int i=0; i<60; i++) reverse_tick();
    delay(5000);
  }
}

void loop() {
}