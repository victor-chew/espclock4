#define SUPPLY_VLOW             4200                              // 4xAA = 4200; 18650 = 3100
#define NORM_TICK_MS            31                                // Length of forward tick pulse in msecs
#define NORM_TICK_ON_US         60                                // Duty cycle of forward tick pulse (out of 100us)
#define NORM_COUNT_MASK         7                                 // 0 = 8 ticks/sec, 1 = 4 ticks/sec, 3 = 2 ticks/sec, 7 = 1 tick /sec
#define FWD_TICK_MS             31                                // Length of forward tick pulse in msecs
#define FWD_TICK_ON_US          65                                // Duty cycle of forward tick pulse (out of 100us)
#define FWD_COUNT_MASK          0                                 // 0 = 8 ticks/sec, 1 = 4 ticks/sec, 3 = 2 ticks/sec, 7 = 1 tick /sec
#define REV_TICKA_LO            35                                // REV_TICKA_LO <= second hand < REV_TICKA_HI will use REV_TICKA_* parameters
#define REV_TICKA_HI            55                                //   Otherwise, REV_TICKA_* parameters will be used
#define REV_TICKA_T1_MS         9                                 // Length of reverse tick short pulse in msecs
#define REV_TICKA_T2_MS         5                                 // Length of delay before reverse tick long pulse in msecs
#define REV_TICKA_T3_MS         23                                // Length of reverse tick long pulse in msecs
#define REV_TICKA_ON_US         85                                // Duty cycle of reverse tick pulse in usec (out of 100usec)
#define REV_TICKB_T1_MS         9                                 // Length of reverse tick short pulse in msecs
#define REV_TICKB_T2_MS         5                                 // Length of delay before reverse tick long pulse in msecs
#define REV_TICKB_T3_MS         23                                // Length of reverse tick long pulse in msecs
#define REV_TICKB_ON_US         85                                // Duty cycle of reverse tick pulse in usec (out of 100usec)
#define REV_COUNT_MASK          1                                 // 0 = 8 ticks/sec, 1 = 4 ticks/sec, 3 = 2 ticks/sec, 7 = 1 tick /sec
#define DIFF_THRESHOLD_HH       7                                 // If diff(clock time, network time) < threshold, fastforward; else reverse
#define DIFF_THRESHOLD_MM       0
#define DIFF_THRESHOLD_SS       2
