#ifndef LED_MEMORY_GAME_H
#define LED_MEMORY_GAME_H

#define STATIC_STRING_MAX 50
#define DEBOUNCE_SECONDS 0.02

#define GPIO_FORMAT "(GPIO:%d)"
#define BUTTON_FORMAT "[Button[%d] " GPIO_FORMAT "]"
#define LED_FORMAT "[LED[%d] " GPIO_FORMAT "]"

#define GAME_MAX_LEVEL 50
typedef struct game_data_t {
	struct gpiod_chip *chip;
	int pair_count,
        *ledGPIOs, *buttonGPIOs,
        *led_values,
        *buttonFDs, *debounce_timerFDs,
        *led_sequence, current_level; // TODO: implement these
	struct gpiod_line **led_lines, **button_lines;
	struct pollfd *pollFDs;
} game_data_t;

#endif // LED_MEMORY_GAME_H

