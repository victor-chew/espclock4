# ESPCLOCK4
Internet-enabled Analog Clock using ESP32.

### Description
This is version 4 of the ESPCLOCK project.

The clock gets accurate time information from the Internet and makes sure the physical clock time is up-to-date. It also automatically detects your current timezone via browser geolocation and deals with daylight saving adjustments with no user intervention.

V4 uses a [ESP32 D1 Mini dev board](https://www.aliexpress.com/item/32816073234.html) with the power LED and UART module removed to save power. The low power ULP coprocessor is used to drive the clock and monitor the voltage for low power, while the more power hungry ESP32 is only woken up every 2 hours to get the current time from the Internet.

The ULP timer (RTC_SLOW_CLK) is used to keep time. Since this clock is not very accurate (5% drift), the number of cycles between ticks is dynamically tuned using the actual time retrieved from the Internet. It is found that with this method, the actual drift every 2 hours can be contained to within 30 seconds.

The advantages of this approach are as follows:

- Very easy to implement, simple enough for a prototype board. You only need the following components:
	- 1 x ESP32 D1 Mini dev board
	- 1 x 0.47F supercap
	- 1 x pushbutton
	- 2 x resistors to form a divider to measure the input voltage

- Similar or better power efficiency as V3. A set of 4 x 1.2V AA NiMH rechargables will drive the clock for at least 4 months.

- More stable than V2/V3, where the ESP8266 and ATtiny85 do not share a common ground (but are connected through the I2C bus). This leads to ESP8266 sometimes not restarting cleanly when power is removed/reconnected.

- Adds driving the second hand in reverse (anti-clockwise) so that synchronization with network time can be achieved more quickly.

### Circuit Diagram
![ESPClock4 circuit diagram](https://github.com/victor-chew/espclock4/raw/main/images/circuit.png)

### Makes
Two clocks have been made so far.

- **20cm clock ($2)**:

![20cm clock front](https://github.com/victor-chew/espclock4/raw/main/images/clock-20cm-front.jpg)
![20cm clock back](https://github.com/victor-chew/espclock4/raw/main/images/clock-20cm-back.jpg)

- **30cm clock ($2)**:

![30cm clock front](https://github.com/victor-chew/espclock4/raw/main/images/clock-30cm-front.jpg)
![30cm clock back](https://github.com/victor-chew/espclock4/raw/main/images/clock-30cm-back.jpg)

### Configuration
The user interface has not changed much from previous versions. 

When the clock is first started, the built-in LED turns on to indicate that it is ready to be configured. Configuration is done via a captive WiFi portal spun up by the ESP32. Connecting to the captive portal brings up the web browser with the following configuration page:

![ESPClock4 configuration page](https://github.com/victor-chew/espclock4/raw/main/images/configuration.png)

Select your WiFi router and enter the router password. 

The time on the clock face should be entered in `HHMMSS` format eg. `120000`. If SS is not entered, 00 is assumed eg. `1200`.

The timezone is prefilled with information obtained from your web browser. However if the prefill is wrong, you can always enter the correct value by consulting [this list](https://en.wikipedia.org/wiki/List_of_tz_database_time_zones).

The last field lets you enter the URL from which network time is obtained. By default, it is [http://espclock.randseq.org/now.php](http://espclock.randseq.org/now.php), though you can change that to point to another URL hosted by your own server.

Once configuration is done, the clock will start ticking. If necessary, it will also start fast ticking clockwise or anticlockwise to catch up with the network time. After that, it simply behaves like a normal clock but will adjust to daylight saving automatically.

### ULP Timer Calibration
As mentioned previously, the ULP timer is not very accurate and has a 5% drift. Hence the clock calibrates the timer every 2 hours based on the difference between current clock and network time.

However, when it first starts running, it will perform this calibration after 5, 15, 30 and 60 minutes. This is to quickly arrive at a suitable value for the timer instead of waiting for the full 2 hours.

### Factory Reset
A click of the pushbutton will pause the clock, and another click will restart it. Pausing the clock will also save the current clock time to flash storage.

A long press of the pushbutton after about 10 seconds will factory reset the clock and bring up the captive portal again for configuration.

*Note: That is a minor deviation from previous versions where a click of the pushbutton will reboot the MCU, and a long press will perform a factory reset.*

### Battery change
When the battery runs low, or it is removed altogether, the clock will pause (but not save the current clock time to flash storage, because there might not be enough power to do so). The ULP code is still running, but it is not doing much else other than waiting for the supply voltage to be restored to working level.

If the battery is simply low (4.2V for 4xAA battery, 3.1V for 18650 battery), it will have ample reserve to keep the ULP running for many days, so clock time will not be lost.

When the battery is removed to change to a fresh set, the supercapacitor will have enough juice to power the ULP for about 5 to 6 minutes before clock time is lost. So any change of batteries have to be performed within that time interval.

### Router Offline
If the router is down, or the ESP32 is unable to connect to the router for whatever reason, it will wait for the next opportunity to do so i.e. wait for another 2 hours.

Note that if the ESP32 is unable to connect to the Internet for an extended period of time, the clock will drift noticeably due to the 5% RTC clock error. However, this will be fixed automatically once the ESP32 is able to get online again.

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

