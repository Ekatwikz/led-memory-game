#include <poll.h>

#include "./katwikOpsys/easyCheck.h"
#include "./katwikOpsys/gpiodHelpers.h"
#include "./katwikOpsys/timerfdHelpers.h"

#include "led-memory-game.h"

const char* usageDescription = "[BUTTON->LED]...\n";
int main(int argc, char** argv) {
	int pair_count = argc - 1;
	usage_(pair_count >= 2);

	// Open the GPIO chip
	struct gpiod_chip* chip = gpiod_chip_open_by_number_(0);

	// Get GPIOs from argv
	int* ledGPIOs = malloc_(pair_count * sizeof(int));
	int* buttonGPIOs = malloc_(pair_count * sizeof(int));
	for (int i = 0; i < pair_count; ++i) {
		sscanf(argv[i + 1], "%d-%d", &buttonGPIOs[i], &ledGPIOs[i]);
		printf_(BUTTON_FORMAT " - " LED_FORMAT "\n", i, buttonGPIOs[i], i, ledGPIOs[i]);
	}

	// Open the GPIO lines for the LEDs and buttons
	struct gpiod_line** led_lines = malloc_(pair_count * sizeof(struct gpiod_line*));
	struct gpiod_line** button_lines = malloc_(pair_count * sizeof(struct gpiod_line*));
	for (int i = 0; i < pair_count; ++i) {
		led_lines[i] = gpiod_chip_get_line_(chip, ledGPIOs[i]);
		button_lines[i] = gpiod_chip_get_line_(chip, buttonGPIOs[i]);
	}

	// Config the LED's lines for output
	int* led_values = calloc_(pair_count, sizeof(int)); // 0 by default
	for (int i = 0; i < pair_count; ++i) {
		char consumer[STATIC_STRING_MAX + 1] = { 0 };
		snprintf_(consumer, STATIC_STRING_MAX - 1, "led%d", ledGPIOs[i]);
		gpiod_line_request_output_(led_lines[i], consumer, led_values[i]);
	}

	// Configure the buttons' GPIO lines for event monitoring
	for (int i = 0; i < pair_count; ++i) {
		char consumer[STATIC_STRING_MAX + 1] = { 0 };
		snprintf_(consumer, STATIC_STRING_MAX, "led%d", buttonGPIOs[i]);
		gpiod_line_request_falling_edge_events_(button_lines[i], consumer);
	}

	// Get the file descriptors for the buttons' event signals
	int* buttonFDs = malloc_(pair_count * sizeof(int));
	for (int i = 0; i < pair_count; ++i)
		buttonFDs[i] = gpiod_line_event_get_fd_(button_lines[i]);

	// Set up debounce timers for each button
	int* debounce_timerFDs = malloc_(pair_count * sizeof(int));
	for (int i = 0; i < pair_count; ++i)
		debounce_timerFDs[i] = timerfd_create_(CLOCK_MONOTONIC, TFD_NONBLOCK);

	// Set up the poll structures for the buttons' file descriptors,
	// and for each of their their debounce timers
	// array has 2n descriptors; even positions are buttons, odd positions are timers
	struct pollfd* pollFDs = malloc_(2 * pair_count * sizeof(struct pollfd*));
	for (int i = 0; i < pair_count; ++i) {
		// button i's poll struct
		pollFDs[2 * i].fd = buttonFDs[i];
		pollFDs[2 * i].events = POLLIN;

		// debounce timer poll struct, for button i
		pollFDs[2 * i + 1].fd = debounce_timerFDs[i];
		pollFDs[2 * i + 1].events = POLLIN;
	}

	printf_("Starting poll loop...\n");
	struct gpiod_line_event last_event = { 0 }; // TODO: thingy on stack?
	while (true) {
		// Wait for events on the button file descriptors
		poll(pollFDs, 2 * pair_count, -1);

		// did we get notified by a button... ?
		for (int i = 0; i < pair_count; ++i) {
			if (pollFDs[2 * i].revents & POLLIN) {
				gpiod_line_event_read_(button_lines[i], &last_event); // to clear event?

				DBGonly(BUTTON_FORMAT " falling edge read...\n", i, buttonGPIOs[i]);

				struct itimerspec curr_timer_val = { 0 };
				struct timespec* curr_timer = &curr_timer_val.it_value;
				timerfd_gettime_(debounce_timerFDs[i], &curr_timer_val);

				if (curr_timer->tv_sec || curr_timer->tv_nsec)
					DBGonly(BUTTON_FORMAT " still bouncing!? No new timer, ignored.\n", i, buttonGPIOs[i]);
				else {
					DBGonly(BUTTON_FORMAT " Starting debounce timer...\n", i, buttonGPIOs[i]);

					struct itimerspec new_timer_val = {
						.it_value = {
							.tv_nsec = DEBOUNCE_SECONDS * GIGA
						}
					};

					timerfd_settime_(debounce_timerFDs[i], 0, &new_timer_val, NULL);
				}
			}
		}

		// ...or are are we getting notified by a timer?
		for (int i = 0; i < pair_count; ++i) {
			if (pollFDs[2 * i + 1].revents & POLLIN) {
				uint64_t expirations; // ignored since we're polling?
				read_(debounce_timerFDs[i], &expirations, sizeof(uint64_t));
				DBGonly(BUTTON_FORMAT " debounce timer complete...\n", i, buttonGPIOs[i]);

				if (!gpiod_line_get_value_(button_lines[i])) {
					DBGonly(BUTTON_FORMAT " value still high! Toggling " LED_FORMAT "\n", i, buttonGPIOs[i], i, ledGPIOs[i]);

					led_values[i] = !led_values[i];
					DBGonly(LED_FORMAT " writing %d\n", i, ledGPIOs[i], led_values[i]);
					gpiod_line_set_value_(led_lines[i], led_values[i]); // ?? WTF
				} else {
					DBGonly(BUTTON_FORMAT " debounce check failed, won't touch LED\n", i, buttonGPIOs[i]);
				}
			}
		}
	}

	// Release the GPIO lines and close the GPIO chip
	for (int i = 0; i < pair_count; ++i) {
		gpiod_line_release(led_lines[i]);
		gpiod_line_release(button_lines[i]);
	}

	gpiod_chip_close(chip);

	// cleanup
	free_(pollFDs);
	free_(buttonFDs);
	free_(led_values);
	free_(button_lines);
	free_(led_lines);
	free_(buttonGPIOs);
	free_(ledGPIOs);
}

