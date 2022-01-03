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

- Similar or better power efficiency compared to V3. A set of 4 x 1.2V AA NiMH rechargables will drive the clock for at least 4 months.

- More stable than V2/V3, where the ESP8266 and ATtiny85 do not share a common ground. This leads to ESP8266 sometimes not restarting cleanly when power is removed/reconnected.

- Adds driving the second hand in reverse (anti-clockwise) so that synchronization with network time can be achieved more quickly.

### Circuit Diagram
![ESPClock4 circuit diagram](https://github.com/victor-chew/espclock4/raw/main/images/circuit.png)

### Makes
Two clocks have been made so far.

- **20cm clock ($2)**:

![20cm clock front](https://github.com/victor-chew/espclock4/raw/main/images/clock-20cm-front.jpg)
![20cm clock back](https://github.com/victor-chew/espclock4/raw/main/images/clock-20cm-back.jpg)

- **30cm clock ($10)**:

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

### Calibration
To drive the clock clockwise and anti-clockwise, the following pulse patterns are used:

![ESPClock4 clock pulse patterns](https://github.com/victor-chew/espclock4/raw/main/images/forward-and-reverse-pulses.png)

Because the clock is designed to work with ~1.5V, and ESP32 outputs 3.3V, we use pulse width modulation (PWN) to reduce the effective voltage applied to the clock's Lavet motor pins. 

In `ulpdefs.h`, a file `clockXXX.h` is included eg. `clock20cm.h`. This file contains all the operational parameters specific to the clock.

	#define SUPPLY_VLOW             3100    // 4xAA = 4200; 18650 = 3100
	#define NORM_TICK_MS            31      // Length of forward tick pulse in msecs
	#define NORM_TICK_ON_US         60      // Duty cycle of forward tick pulse (out of 100us)
	#define NORM_COUNT_MASK         7       // 0 = 8 ticks/sec, 1 = 4 ticks/sec, 3 = 2 ticks/sec, 7 = 1 tick /sec
	#define FWD_TICK_MS             32      // Length of forward tick pulse in msecs
	#define FWD_TICK_ON_US          60      // Duty cycle of forward tick pulse (out of 100us)
	#define FWD_COUNT_MASK          1       // 0 = 8 ticks/sec, 1 = 4 ticks/sec, 3 = 2 ticks/sec, 7 = 1 tick /sec
	#define REV_TICKA_LO            35      // REV_TICKA_LO <= second hand < REV_TICKA_HI will use REV_TICKA_* parameters
	#define REV_TICKA_HI            55      //   Otherwise, REV_TICKA_* parameters will be used
	#define REV_TICKA_T1_MS         10      // Length of reverse tick short pulse in msecs
	#define REV_TICKA_T2_MS         7       // Length of delay before reverse tick long pulse in msecs
	#define REV_TICKA_T3_MS         28      // Length of reverse tick long pulse in msecs
	#define REV_TICKA_ON_US         90      // Duty cycle of reverse tick pulse in usec (out of 100usec)
	#define REV_TICKB_T1_MS         10      // Length of reverse tick short pulse in msecs
	#define REV_TICKB_T2_MS         7       // Length of delay before reverse tick long pulse in msecs
	#define REV_TICKB_T3_MS         28      // Length of reverse tick long pulse in msecs
	#define REV_TICKB_ON_US         82      // Duty cycle of reverse tick pulse in usec (out of 100usec)
	#define REV_COUNT_MASK          3       // 0 = 8 ticks/sec, 1 = 4 ticks/sec, 3 = 2 ticks/sec, 7 = 1 tick /sec
	#define DIFF_THRESHOLD_HH       6       // If diff(clock time, network time) < threshold, fastforward; else reverse
	#define DIFF_THRESHOLD_MM       0
	#define DIFF_THRESHOLD_SS       2

In theory, this is how things work. The ULP is called every 125ms, 8x per sec. But the ULP timer does not work like a timer interrupt. A timer value is set via `ulp_set_wakeup_period()`. When the timer counts down to 0, ULP code is executed. When ULP code finishes execution via `I_HAIT()`, the timer value counts down again from the original set value. Hence the timer does not include the ULP execution time. If the timer value is 125ms, and ULP code takes 10ms to execution, the ULP code will actually execute at 135ms interval.

So what I did was to allocate 60ms for the ULP code (`MAX_PULSE_MS`), leaving 65ms for the ULP timer (`DEF_ULP_TIMER`). If the ULP code takes less than 60ms to execute, it will call the `X_DELAY_MS()` macro to wait out the remaining time before calling `I_HAIT()`. So for example, if the forward tick cycle takes 32ms to execute, then it will wait out 60ms - 32ms = 28ms before calling `I_HALT()`. This value is precalculated in `FWD_TICK_FILLER_MS` for the forward tick example.

Note that this is still an approximation of the amount of time taken by the code, because I am not doing cycle counting for the code path. During each call of the ULP code, besides certain mandatory tasks (eg. check supply voltage, check reset button etc.), it decides on 1 of 3 clock actions to take: normal tick (1 tick per sec), fast-forward (up to 8 ticks per sec, decided by `FWD_COUNT_MASK`), fast-reverse (up to 4 ticks per sec, decided by `REV_COUNT_MASK`). Based on the action performed, the corresponding padding time will be used for the wait-out (`NORM_TICK_FILLER_MS`, `FWD_TICK_FILLER_MS`, `REV_TICKA_FILLER_MS`, `REV_TICKB_FILLER_MS`).

The 65ms ULP timer will be calibrated every 2 hours based on the difference between the clock and network time. This makes the 5% timer drift more bearable.

The reason why there are 2 fast-reverse filler types is because on my 20cm clock, I found that between the 7 to 11 region, a little more power (90% duty cycle versus 82% duty cycle) is required to get the second hand to reverse reliably. However, the same 90% duty cycle applied to the region between 1 and 4 region will cause some skipping). Hence, I split the reverse cycle in 2 regions. If the second hand is between `REV_TICKA_LO (35)` and `REV_TICKA_HI (55)`, `REV_TICKA` values will be used. Otherwise `REV_TICKB` values will be used.

Note that on my 30cm clock, this issue is not present. Hence, `REV_TICKA` and `REV_TICKB` values are the same.

To ensure the set values works reliably for the clock, a stress test must be performed. This is done by uncommenting `STRESS_TEST` in `espclock4.h`.

The stress test basically performs a 12-hour tick test of the second hand in a particular direction. 

First, if you are performing a fast-reverse test, the second hand position must be set in the code:

	case ESP_SLEEP_WAKEUP_UNDEFINED:
		init_vars(); 
		init_gpio();
		_set(VAR_CLK_SS, 4); // Set second hand position here (0-59)
		_set(VAR_SLEEP_COUNT, 12*60*60);
		_set(VAR_TICK_ACTION, TICK_NORMAL);
		_set(VAR_TICK_DELAY, 5);
		while(digitalRead(RESETBTN_PIN_GPIO) == HIGH);
		break;

Then, specifying the direction of test (`TICK_NORMAL`, `TICK_FWD`, `TICK_REV`):

	case ESP_SLEEP_WAKEUP_ULP:
		delay(5000);
		int randnum = random(1,6)*5;
		int count = _get(VAR_SLEEP_COUNT);
		while(count == 0) delay(60000);
		if (randnum >= count) randnum = count;
		_set(VAR_SLEEP_COUNT, count - randnum);
		_set(VAR_TICK_ACTION, TICK_REV); // Set direction here (TICK_NORMAL, TICK_FWD, TICK_REV)
		_set(VAR_TICK_DELAY, randnum); 
		_set(VAR_PAUSE_CLOCK, 0);
		break;

Finally, compile and run. The clock will wait for you to press the reset button, then it will tick normally 5 times. This is to make sure the pin polarity are sorted out before the test is started (i.e. because we always start with pulsing pin 1, there is a likelihood we will miss the first tick if pin 2 is supposed to be pulsed first. However, this will possibly lead to a major slippage if we start fast-reverse on the wrong pin). Then the clock will pause for 5s (note the second hand position now) and start the 12-hour test. It will tick in the chosen direction for random number of ticks (between 5s to 25s) each time, pause for 5s and continue until exactly 12*60*60 ticks are made. The random number of ticks each time will test reliability issues when the second hand starts from different positions, which it is extremely sensitive for fast-reverse. If all goes well, the second hand should return to the original position.

Normal ticking should be easy to tune, since the default value 31ms should be standard for all analog clocks, and the only thing that you might like to tune is the duty cycle `NORM_TICK_ON_US`. The lower the value, the less power will be used for normal ticking (which is what the clock will be doing most of the time). 

	#define NORM_TICK_MS            31      // Length of forward tick pulse in msecs
	#define NORM_TICK_ON_US         60      // Duty cycle of forward tick pulse (out of 100us)

Fast-forward ticking should also be quite easy to tune. Typically, a little more power is required (either by increasing the pulse width `FWD_TICK_MS`, or by increasing the PWM duty cycle `FWD_TICK_ON_US`. In the case of my 20cm clock, I find that I am unable to get reliable fast-forward operation at 8 ticks/sec, so I have to lower it to 4 ticks/sec. This did not happen for my 30cm clock). 

	#define FWD_TICK_MS             32      // Length of forward tick pulse in msecs
	#define FWD_TICK_ON_US          60      // Duty cycle of forward tick pulse (out of 100us)
	#define FWD_COUNT_MASK          1       // 0 = 8 ticks/sec, 1 = 4 ticks/sec, 3 = 2 ticks/sec, 7 = 1 tick /sec

Fast-reverse is the most finicky to tune. Not only are there more parameters, different regions (typically at the 3 and 9 regions) may require different treatment, so it is not for the impatient!

	#define REV_TICKA_LO            35      // REV_TICKA_LO <= second hand < REV_TICKA_HI will use REV_TICKA_* parameters
	#define REV_TICKA_HI            55      //   Otherwise, REV_TICKA_* parameters will be used
	#define REV_TICKA_T1_MS         10      // Length of reverse tick short pulse in msecs
	#define REV_TICKA_T2_MS         7       // Length of delay before reverse tick long pulse in msecs
	#define REV_TICKA_T3_MS         28      // Length of reverse tick long pulse in msecs
	#define REV_TICKA_ON_US         90      // Duty cycle of reverse tick pulse in usec (out of 100usec)
	#define REV_TICKB_T1_MS         10      // Length of reverse tick short pulse in msecs
	#define REV_TICKB_T2_MS         7       // Length of delay before reverse tick long pulse in msecs
	#define REV_TICKB_T3_MS         28      // Length of reverse tick long pulse in msecs
	#define REV_TICKB_ON_US         82      // Duty cycle of reverse tick pulse in usec (out of 100usec)
	#define REV_COUNT_MASK          3       // 0 = 8 ticks/sec, 1 = 4 ticks/sec, 3 = 2 ticks/sec, 7 = 1 tick /sec

You may wish to disable reverse ticking altogether by changing the following definitions in `ulpdefs.h`:

	#define DIFF_THRESHOLD_HH       6
	#define DIFF_THRESHOLD_MM       0
	#define DIFF_THRESHOLD_SS       2

to

	#define DIFF_THRESHOLD_HH       12
	#define DIFF_THRESHOLD_MM       0
	#define DIFF_THRESHOLD_SS       0

Then the clock will only use fast-forwarding for synchronization.

The `DIFF_THRESHOLD` parameters can be determined by running the `threshold.py` Python code. The following values need to be set:

	fwd_speedup = 4
	rev_speedup = 2

The program will then generate a threshold value that will help the ULP to decide whether to use fast-forward or fast-reverse for clock sync for any particular clock-network-time-pair.

### Future Work
The RTC_SLOW_CLK drift can be dramatically improved by [connecting an external 32K crystal](https://www.esp32.com/viewtopic.php?t=1175&start=10) to the 32K_XP and 32K_XN pins of the ESP32. However, the [ESP32 Arduino Core](https://github.com/espressif/arduino-esp32) will need to be recompiled in order to use the external crystal, as there is currently no way to achieve this programmtically.

Programming the ULP coprocessor using assembly is tedious and error-prone. In the future, I might like to try using the ULP-RISC-V coprocessor on the ESP32-S2, which should support using the C language. However, I would have to wait for support to drop on the Arduino framework before I can proceed.

