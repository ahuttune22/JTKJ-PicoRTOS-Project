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

/* ---------- Button interrupt: only sets flags ---------- */
static void btn_fxn(uint gpio, uint32_t eventMask) {
    (void)gpio;
    (void)eventMask;
    /* TODO step 2: debounce, set buttonAEvent / buttonBEvent */
}

/* ---------- Tasks ---------- */
static void sensor_task(void *arg) {
    (void)arg;
    /* TODO step 3: init IMU. WAITING -> READ_POSITION on button A, read and classify,
       then SEND_SYMBOL (dot/dash found) or back to WAITING */
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(TASK_DELAY_MS));
    }
}

static void comm_task(void *arg) {
    (void)arg;
    /* TODO step 4: SEND_SYMBOL: print symbol. Button B: WAITING -> SEND_SEPARATOR,
       print space, space, newline. Always return to WAITING */
    for (;;) {
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
