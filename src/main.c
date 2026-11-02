/*
 * Secret Messages - Morse messenger (Tier 1)
 * Team JGLS: Aleksi Huttunen, Matias Raudasoja
 * TODO: Point distribution: Aleksi X, Matias Y
 *
 * Flat = dot '.', tilted = dash '-'. Button A sends the symbol,
 * button B adds spaces (3rd press sends '\n').
 *
 * STATUS: structure only. The TODO parts are filled in step by step in the coming weeks .
 */

#include <stdio.h>
#include <math.h>

#include <pico/stdlib.h>

#include <FreeRTOS.h>
#include <task.h>

#include "tkjhat/sdk.h"
#include "morse_config.h"

/* ---------- State machine and shared data ---------- */
/* Design: the program is a small state machine. Only two tasks use it:
 *   sensor_task - handles button A, reads the IMU (the "input" side)
 *   comm_task   - handles button B, writes to the PC (the "output" side)
 * Splitting input and output means a slow USB write never delays an IMU
 * reading, and each task has one clear job. The tasks talk only through
 * the shared variables below. */
/* WAITING -(A)-> READ_POSITION -> SEND_SYMBOL -> WAITING
 * WAITING -(B)-> SEND_SEPARATOR -> WAITING                      */
enum state { WAITING = 1, READ_POSITION, SEND_SYMBOL, SEND_SEPARATOR };

static volatile enum state programState = WAITING;
/* symbolToSend: '.' or '-' chosen by sensor_task, printed by comm_task. */
static volatile char       symbolToSend = 0;
/* buttonAEvent / buttonBEvent: set by the button interrupt, cleared by the
 * task that handles the button. One flag per button, so A and B can't be
 * mixed up. volatile because the interrupt changes them behind the tasks' back. */
static volatile bool       buttonAEvent = false;  /* set in interrupt */
static volatile bool       buttonBEvent = false;  /* set in interrupt */
/* spaceCount: how many times B was pressed in a row (0..2). Decides whether
 * the next press sends a space (end of letter, end of word) or '\n'.
 * Reset to 0 whenever a symbol is sent, because then a new letter starts. */
static uint8_t             spaceCount   = 0;

    /* ---------- Button interrupt ----------
 
        Called by the Pico SDK every time a GPIO edge interrupt fires (we enabled
    rising edges for both buttons in main()). The Pico has ONE shared callback
    for all pins, so the parameter "gpio" tells us which button was pressed.*
    Rule for interrupt handlers: keep them as short as possible. This one
    only sets a flag. The real work (reading the IMU, printing) is done later
    by the tasks, which look at the flag.*/
static void btn_fxn(uint gpio, uint32_t eventMask) {
    (void)eventMask;                 /* not used, avoids the "unused" warning /

    / "static" keeps these values between calls. They remember when each
     button last counted as a press, so that we can ignore switch bounce. */
  static uint32_t lastA = 0, lastB = 0;

    /* Milliseconds since the Pico booted. */
    uint32_t now = to_ms_since_boot(get_absolute_time());

    if (gpio == BUTTON1 && now - lastA > DEBOUNCE_MS) {
        /* Button A (SW1): accept the press only if the last one is more
        than DEBOUNCE_MS ago. A real button "bounces" and can create
        several edges for one press; this removes the extra ones. */
    lastA = now;
    buttonAEvent = true;         /* sensor_task will see this and clear it */
} else if (gpio == BUTTON2 && now - lastB > DEBOUNCE_MS) {
    /* Button B (SW2): same thing, with its own timer. */
    lastB = now;
    buttonBEvent = true;         /* comm_task will see this and clear it */
}
}
/* Decide dot or dash from the accelerometer, using the thresholds in
 * morse_config.h (measured values: assets/imu_measurements.txt).
 * ax, az are in g. Returns '.', '-' or 0 when the board is in neither
 * position (for example, halfway through a tilt). */
static char classify_position(float ax, float az) {
    /* Flat: gravity points mostly along z, and x is small. */
    if (az > FLAT_Z_MIN && fabsf(ax) < FLAT_X_MAX) return '.';
    /* Tilted onto either long edge: gravity points along x, so |ax| is about 1 g. */
    if (fabsf(ax) > TILT_X_MIN) return '-';
    return 0;   /* the zone between the two thresholds is ignored */
}

/* ---------- Tasks ---------- */
/* sensor_task: waits for button A, then reads the IMU and decides the symbol.
 * State flow: WAITING -> READ_POSITION -> SEND_SYMBOL (or back to WAITING). */
static void sensor_task(void *arg) {
    (void)arg;
    float ax, ay, az, gx, gy, gz, t;   /* the sensor gives 6 values + temperature */

    /* Set up the IMU once, before the loop. Same calls as the hat_imu_ex example. */
    init_ICM42670();
    ICM42670_start_with_default_values();

    for (;;) {
        /* Button A pressed? Only legal from WAITING: then start reading. */
        if (buttonAEvent) {
            buttonAEvent = false;            /* clear the flag: one press counts once */
            if (programState == WAITING) {
                programState = READ_POSITION;
            }
        }

        if (programState == READ_POSITION) {
            char s = 0;
            /* The read function returns 0 when it worked. */
            if (ICM42670_read_sensor_data(&ax, &ay, &az, &gx, &gy, &gz, &t) == 0) {
                s = classify_position(ax, az);
            }
            if (s != 0) {
                symbolToSend = s;
                programState = SEND_SYMBOL;  /* hand over to comm_task */
            } else {
                programState = WAITING;      /* no clear position or read failed: ignore */
            }
        }
        vTaskDelay(pdMS_TO_TICKS(TASK_DELAY_MS));
    }
}

static void comm_task(void *arg) {
    (void)arg;
    for (;;) {
        if (programState == SEND_SYMBOL) {
            printf("%c", symbolToSend);   /* send the '.' or '-' to the PC */
            fflush(stdout);               /* push it out now, don't wait for a newline */
            spaceCount = 0;               /* a new symbol restarts the B-press counter */
            programState = WAITING;
        }
        /* TEMPORARY step-2 test for button B, replaced in step 4 */
        if (buttonBEvent) { buttonBEvent = false; printf("B\n"); }
        vTaskDelay(pdMS_TO_TICKS(TASK_DELAY_MS));
    }
}

/* ---------- main ---------- */
int main() {
    stdio_init_all();
    init_hat_sdk();
    sleep_ms(300);

    init_sw1();
    init_sw2();
    /* The first call registers btn_fxn as the callback for ALL GPIO interrupts
     * (the Pico has only one shared callback) and enables button A (BUTTON1). */
    gpio_set_irq_enabled_with_callback(BUTTON1, GPIO_IRQ_EDGE_RISE, true, btn_fxn);
    /* Button B: the lab used only one button. The callback is already
     * registered above, so button B only needs its own interrupt enabled
     * (no second callback). btn_fxn tells the buttons apart by pin number. */
    gpio_set_irq_enabled(BUTTON2, GPIO_IRQ_EDGE_RISE, true);

    /* Task creation check adapted from the course's template.c (there for one
     * task, here for two): xTaskCreate returns pdPASS only if the task was made. */
    BaseType_t sensorOk = xTaskCreate(sensor_task, "sensor", DEFAULT_STACK_SIZE, NULL, TASK_PRIORITY, NULL);
    BaseType_t commOk   = xTaskCreate(comm_task,   "comm",   DEFAULT_STACK_SIZE, NULL, TASK_PRIORITY, NULL);
    if (sensorOk != pdPASS || commOk != pdPASS) {
        printf("Task creation failed\n");
        return 0;
    }

    /* No separate usb_task / tud_task() here, unlike the plan's third task.
     * The output uses the standard printf over USB (stdio, enabled by
     * pico_enable_stdio_usb in CMakeLists.txt), and the Pico SDK already runs
     * the USB stack for it. A second task calling tud_task() could conflict
     * with that. We still have two tasks (sensor_task, comm_task). */
    vTaskStartScheduler();
    return 0;
}
