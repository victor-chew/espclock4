# ESPCLOCK4
Internet-enabled Analog Clock using ESP32.

### Description
This is version 4 of the ESPCLOCK project.

The clock gets accurate time information from the Internet and makes sure the physical clock time is up-to-date. It also automatically detects your current timezone via browser geolocation and deals with daylight saving adjustments with no user intervention.

V1 uses the ESP-12E development board to drive a cheap $2 Ikea analog clock.

V2 uses the WeMOS D1 Mini and ATtiny85 to reduce power draw. That design yields a runtime of ~1 month.

V3 uses the ESP-07, ATtiny85 and PCF8563 RTC to reduce the power draw even further. This design yields at least 4 months of runtime on a set of readily-available 4 x 1.2V AA NiMH rechargables.

V4 uses a [ESP32 D1 Mini dev board](https://www.aliexpress.com/item/32816073234.html) with the power LED and UART module removed to save power. The low power ULP coprocessor is used to drive the clock and monitor the voltage for low power, while the more power hungry ESP32 is only woken up every 2 hours to get the current time from the Internet.

The ULP timer (RTC_SLOW_CLK) is used to keep time. Since this clock is not very accurate (5% drift), the number of cycles between ticks is dynamically tuned using the actual time retrieved from the Internet. It is found that with this method, the actual drift every 2 hours can be contained to within 30 seconds.

The advantages of this approach are as follows:

- Very easy to implement, simple enough for a prototype board. You only need the following components:
	- 1 x ESP32 D1 Mini dev board
	- 1 x 0.47F supercap
	- 1 x pushbutton
	- 2 x resistors to form a divider to measure the input voltage

- Similar or better power efficiency as V3. A set of 4 x 1.2V AA NiMH rechargables will drive the clock for at least 4 months.

- More stable than V2/V3, where the ESP8266 and ATtiny85 do not share a common ground (but are connected through the I2C bus). This leads to ESP8266 sometimes not restarting cleanly when power is removed and reconnected.

### Stress test
Uncomment `STRESS_TEST` to activate the stress test code.

	// Uncomment to perform stress test of fastforward/reverse clock movement
	#define STRESS_TEST

By default, the code will cycle through normal ticking, fastforward and reverse ticking in 15s blocks.

To stress test only one particular movement eg. reverse ticking, change the following code:

	case ESP_SLEEP_WAKEUP_ULP: {
		delay(5000);
		_set(VAR_TICK_ACTION, TICK_REV); // Add this and comment out the next line
		//_set(VAR_TICK_ACTION, (_get(VAR_TICK_ACTION) % 3) + 1); // Alternate between 1 to 3 (TICK_NORMAL, TICK_FWD and TICK_REV)

### Disabling reverse ticking

The timing for reverse ticking is tricky to tune correctly. You can disable by change the following definitions in `ulpfuncs.h`:

	#define DIFF_THRESHOLD_HH       7
	#define DIFF_THRESHOLD_MM       0
	#define DIFF_THRESHOLD_SS       2

to

	#define DIFF_THRESHOLD_HH       12
	#define DIFF_THRESHOLD_MM       0
	#define DIFF_THRESHOLD_SS       0

Then clock time adjustment will only be done through forward ticking.

