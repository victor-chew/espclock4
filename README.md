CONFIG_ESP32_ULP_COPROC_RESERVE_MEM=8192
# CONFIG_ESP32_RTC_CLK_SRC_INT_RC is not set
CONFIG_ESP32_RTC_CLK_SRC_INT_8MD256=y

# espclock4
Internet-enabled Analog Clock using ESP32

### Description
Work-in-progress

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

