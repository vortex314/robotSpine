//_______________________________________________________________________________________________________________
//
#include <LedBlinker.h> 

LedBlinker::LedBlinker(Thread& thr,uint32_t pin, uint32_t delay)
	: Actor(thr),_pin(pin),blinkTimer(thr,delay,true) {

	blinkTimer >> ([&](const TimerMsg tm) {
    digitalWrite(_pin, _on);
		_on = _on ? 0 : 1 ;
	});

	_pin = pin;
	blinkSlow  >> [&](bool blink_slow) {
		if ( blink_slow ) blinkTimer.interval(500);
		else blinkTimer.interval(100);
	};
}
void LedBlinker::init() {
    pinMode(_pin, OUTPUT);
    digitalWrite(_pin, 1);
}

void LedBlinker::delay(uint32_t d) {
	blinkTimer.interval(d);
}