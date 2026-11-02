#ifndef MORSE_CONFIG_H
#define MORSE_CONFIG_H

/*
 * Settings for the Morse messenger.
 * Everything we might want to tune later lives here, not in main.c.
 */

/* ---- FreeRTOS ---- */
#define DEFAULT_STACK_SIZE 2048
#define TASK_PRIORITY      2
#define TASK_DELAY_MS      20      /* polling period of the tasks */

/* ---- Buttons ---- */
#define DEBOUNCE_MS        50     /* ignore button edges closer than this */

/* ---- IMU thresholds, in g (measured values: see assets/imu_measurements.txt) ---- */
/* Measured on 622 samples (assets/imu_measurements.txt):
 *   flat:   |ax| <= 0.32 g, az >= 0.88 g
 *   tilted: |ax| >= 0.95 g (both directions)
 * The thresholds sit in the middle of the gaps. */
#define FLAT_Z_MIN   0.7f          /* flat:   az above this ...       */
#define FLAT_X_MAX   0.5f          /*         ... and |ax| below this */
#define TILT_X_MIN   0.7f          /* tilted: |ax| above this         */

#endif /* MORSE_CONFIG_H */
 