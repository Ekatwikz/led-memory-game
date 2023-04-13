#include <poll.h>

#include "./katwikOpsys/easyCheck.h"
#include "./katwikOpsys/gpiodHelpers.h"
#include "./katwikOpsys/timerfdHelpers.h"

#include "led-memory-game.h"

static void game_data_setup(game_data_t* game_data, int pair_count, char** argv);
static void game_sequence_clear(game_data_t* game_data);
static void game_data_cleanup(game_data_t* game_data);

const char* usageDescription = "[BUTTON->LED]...\n";
int main(int argc, char** argv) {
	int pair_count = argc - 1;
	usage_(pair_count >= 2);

	game_data_t game_data = { 0 };
	game_data_setup(&game_data, pair_count, argv);

	printf_("Starting poll loop...\n");
	struct gpiod_line_event last_event = { 0 }; // TODO: thingy on stack?
	while (true) {
		// Wait for events on the button file descriptors
		poll(game_data.pollFDs, 2 * pair_count, -1);

		// did we get notified by a button... ?
		for (int i = 0; i < pair_count; ++i) {
			if (game_data.pollFDs[2 * i].revents & POLLIN) {
				gpiod_line_event_read_(game_data.button_lines[i], &last_event); // to clear event?

				DBGonly(BUTTON_FORMAT " falling edge read...\n", i, game_data.buttonGPIOs[i]);

				struct itimerspec curr_timer_val = { 0 };
				struct timespec* curr_timer = &curr_timer_val.it_value;
				timerfd_gettime_(game_data.debounce_timerFDs[i], &curr_timer_val);

				if (curr_timer->tv_sec || curr_timer->tv_nsec)
					DBGonly(BUTTON_FORMAT " still bouncing!? No new timer, ignored.\n", i, game_data.buttonGPIOs[i]);
				else {
					DBGonly(BUTTON_FORMAT " Starting debounce timer...\n", i, game_data.buttonGPIOs[i]);

					struct itimerspec new_timer_val = {
						.it_value = {
							.tv_nsec = DEBOUNCE_SECONDS * GIGA
						}
					};

					timerfd_settime_(game_data.debounce_timerFDs[i], 0, &new_timer_val, NULL);
				}
			}
		}

		// ...or are are we getting notified by a timer?
		for (int i = 0; i < pair_count; ++i) {
			if (game_data.pollFDs[2 * i + 1].revents & POLLIN) {
				uint64_t expirations; // ignored since we're polling?
				read_(game_data.debounce_timerFDs[i], &expirations, sizeof(uint64_t));
				DBGonly(BUTTON_FORMAT " debounce timer complete...\n", i, game_data.buttonGPIOs[i]);

				if (!gpiod_line_get_value_(game_data.button_lines[i])) {
					DBGonly(BUTTON_FORMAT " value still high! Toggling " LED_FORMAT "\n", i, game_data.buttonGPIOs[i], i, game_data.ledGPIOs[i]);

					game_data.led_values[i] = !game_data.led_values[i];
					DBGonly(LED_FORMAT " writing %d\n", i, game_data.ledGPIOs[i], game_data.led_values[i]);
					gpiod_line_set_value_(game_data.led_lines[i], game_data.led_values[i]); // ?? WTF
				} else {
					DBGonly(BUTTON_FORMAT " debounce check failed, won't touch LED\n", i, game_data.buttonGPIOs[i]);
				}
			}
		}
	}

	game_data_cleanup(&game_data); // uncreachable for now... ?
}

static void game_data_setup(game_data_t* game_data, int pair_count, char** argv) {
	game_data->pair_count = pair_count;

	// Init game sequence
	game_data->led_sequence = malloc(GAME_MAX_LEVEL * sizeof(int));
	game_sequence_clear(game_data);

	// Open the GPIO chip
	game_data->chip = gpiod_chip_open_by_number_(0);

	// Get GPIOs from argv
	game_data->ledGPIOs = malloc_(pair_count * sizeof(int));
	game_data->buttonGPIOs = malloc_(pair_count * sizeof(int));
	for (int i = 0; i < pair_count; ++i) {
		sscanf(argv[i + 1], "%d-%d", &game_data->buttonGPIOs[i], &game_data->ledGPIOs[i]);
		printf_(BUTTON_FORMAT " - " LED_FORMAT "\n", i, game_data->buttonGPIOs[i], i, game_data->ledGPIOs[i]);
	}

	// Open the GPIO lines for the LEDs and buttons
	game_data->led_lines = malloc_(pair_count * sizeof(struct gpiod_line*));
	game_data->button_lines = malloc_(pair_count * sizeof(struct gpiod_line*));
	for (int i = 0; i < pair_count; ++i) {
		game_data->led_lines[i] = gpiod_chip_get_line_(game_data->chip, game_data->ledGPIOs[i]);
		game_data->button_lines[i] = gpiod_chip_get_line_(game_data->chip, game_data->buttonGPIOs[i]);
	}

	// Config the LED's lines for output
	game_data->led_values = calloc_(pair_count, sizeof(int)); // 0 by default
	for (int i = 0; i < pair_count; ++i) {
		char consumer[STATIC_STRING_MAX + 1] = { 0 };
		snprintf_(consumer, STATIC_STRING_MAX - 1, "led%d", game_data->ledGPIOs[i]);
		gpiod_line_request_output_(game_data->led_lines[i], consumer, game_data->led_values[i]);
	}

	// Configure the buttons' GPIO lines for event monitoring
	for (int i = 0; i < pair_count; ++i) {
		char consumer[STATIC_STRING_MAX + 1] = { 0 };
		snprintf_(consumer, STATIC_STRING_MAX, "led%d", game_data->buttonGPIOs[i]);
		gpiod_line_request_falling_edge_events_(game_data->button_lines[i], consumer);
	}

	// Get the file descriptors for the buttons' event signals
	game_data->buttonFDs = malloc_(pair_count * sizeof(int));
	for (int i = 0; i < pair_count; ++i)
		game_data->buttonFDs[i] = gpiod_line_event_get_fd_(game_data->button_lines[i]);

	// Set up debounce timers for each button
	game_data->debounce_timerFDs = malloc_(pair_count * sizeof(int));
	for (int i = 0; i < pair_count; ++i)
		game_data->debounce_timerFDs[i] = timerfd_create_(CLOCK_MONOTONIC, TFD_NONBLOCK);

	// Set up the poll structures for the buttons' file descriptors,
	// and for each of their their debounce timers
	// array has 2n descriptors
	game_data->pollFDs = malloc_(2 * pair_count * sizeof(struct pollfd*));
	for (int i = 0; i < pair_count; ++i) {
		// button i's poll struct
		game_data->pollFDs[2 * i].fd = game_data->buttonFDs[i];
		game_data->pollFDs[2 * i].events = POLLIN;

		// debounce timer poll struct, for button i
		game_data->pollFDs[2 * i + 1].fd = game_data->debounce_timerFDs[i];
		game_data->pollFDs[2 * i + 1].events = POLLIN;
	}
}

static void game_sequence_clear(game_data_t* game_data) {
	memset(game_data->led_sequence, 0, game_data->pair_count * sizeof(int));
}

static void game_data_cleanup(game_data_t* game_data) {
	// Release the GPIO lines
	for (int i = 0; i < game_data->pair_count; ++i) {
		gpiod_line_release(game_data->led_lines[i]);
		gpiod_line_release(game_data->button_lines[i]);
	}

	// close the GPIO chip
	gpiod_chip_close(game_data->chip);

	// cleanup
	free_(game_data->pollFDs);
	free_(game_data->buttonFDs);
	free_(game_data->led_values);
	free_(game_data->button_lines);
	free_(game_data->led_lines);
	free_(game_data->buttonGPIOs);
	free_(game_data->ledGPIOs);
}

