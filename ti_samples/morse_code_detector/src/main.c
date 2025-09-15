#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/util_macro.h>
#include <string.h>

/* Config */
#define STACK_SIZE 1024
#define THREAD_PRIORITY 5

/* GPIO: Button */
static const struct device *gpio_dev = DEVICE_DT_GET(DT_GPIO_CTLR(DT_NODELABEL(btn1), gpios));
static const struct gpio_dt_spec button1 = GPIO_DT_SPEC_GET(DT_NODELABEL(btn1), gpios);
static struct gpio_callback button_cb_data;

/* GPIO: LED */
#define LED0_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

static const struct device *uart = DEVICE_DT_GET(DT_NODELABEL(uart0));

/* Morse timing */
#define DOT_THRESHOLD    1000   /* ms */
#define LETTER_PAUSE     2000   /* ms */
#define WORD_PAUSE       3000   /* ms */

/* Message queue for Morse symbols */
#define MSGQ_MAX_MSG 16
#define MSG_SIZE sizeof(char)
K_MSGQ_DEFINE(morse_msgq, MSG_SIZE, MSGQ_MAX_MSG, 4);

/* Timing */
static int64_t press_time = 0;

/* Morse lookup table */
struct morse_entry {
    const char *code;
    char letter;
};

static const struct morse_entry morse_table[] = {
    {".-", 'A'},   {"-...", 'B'}, {"-.-.", 'C'}, {"-..", 'D'},
    {".", 'E'},    {"..-.", 'F'}, {"--.", 'G'},  {"....", 'H'},
    {"..", 'I'},   {".---", 'J'}, {"-.-", 'K'},  {".-..", 'L'},
    {"--", 'M'},   {"-.", 'N'},   {"---", 'O'},  {".--.", 'P'},
    {"--.-", 'Q'}, {".-.", 'R'},  {"...", 'S'},  {"-", 'T'},
    {"..-", 'U'},  {"...-", 'V'}, {".--", 'W'},  {"-..-", 'X'},
    {"-.--", 'Y'}, {"--..", 'Z'},
    {"-----", '0'}, {".----", '1'}, {"..---", '2'}, {"...--", '3'},
    {"....-", '4'}, {".....", '5'}, {"-....", '6'}, {"--...", '7'},
    {"---..", '8'}, {"----.", '9'},
    {NULL, 0}
};

/* --- GPIO interrupt handler --- */
void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    if (gpio_pin_get(gpio_dev, button1.pin)) {
        /* Button down */
        press_time = k_uptime_get();
        gpio_pin_set(gpio_dev, led.pin, 1);   // Turn LED ON

    } else {
		/* Button released */
        int64_t release_time = k_uptime_get();
        int64_t duration = release_time - press_time;

        char symbol = (duration < DOT_THRESHOLD) ? '.' : '-';
        k_msgq_put(&morse_msgq, &symbol, K_NO_WAIT);

        gpio_pin_set(gpio_dev, led.pin, 0);   // Turn LED OFF
    }
}

/* --- Decoder thread --- */
void decoder_thread(void *p1, void *p2, void *p3)
{
    char buffer[8] = {0};
    int idx = 0;
    int64_t last_time = k_uptime_get();

    while (1) {
        char symbol;
        if (k_msgq_get(&morse_msgq, &symbol, K_MSEC(50)) == 0) {
            buffer[idx++] = symbol;
            buffer[idx] = '\0';
            last_time = k_uptime_get();
        } else {
            /* Check for pauses */
            int64_t now = k_uptime_get();
            if (idx > 0 && (now - last_time > LETTER_PAUSE)) {
                /* End of letter */
                char decoded = '?';
                for (int i = 0; morse_table[i].code; i++) {
                    if (strcmp(morse_table[i].code, buffer) == 0) {
                        decoded = morse_table[i].letter;
                        break;
                    }
                }
                uart_poll_out(uart, decoded);
                idx = 0;
                buffer[0] = '\0';
                last_time = now;
            } else if (now - last_time > WORD_PAUSE) {
                /* End of word */
                //uart_poll_out(uart, '\n');
                last_time = now;
            }
        }
    }
}

/* --- Main --- */
K_THREAD_STACK_DEFINE(decoder_stack, STACK_SIZE);
static struct k_thread decoder_data;

int main(void)
{
    //printf("Zephyr Morse Code Button + LED Example\n");

    gpio_pin_configure_dt(&button1, GPIO_INPUT | GPIO_PULL_UP);
    gpio_pin_interrupt_configure_dt(&button1, GPIO_INT_EDGE_BOTH);
	gpio_init_callback(&button_cb_data, button_pressed, BIT(button1.pin));
    gpio_add_callback(gpio_dev, &button_cb_data);

    gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
	gpio_pin_set(gpio_dev, led.pin, 0);   // Turn LED OFF

    /* Start decoder thread */
    k_thread_create(&decoder_data, decoder_stack, STACK_SIZE,
                    decoder_thread, NULL, NULL, NULL,
                    THREAD_PRIORITY, 0, K_NO_WAIT);

    return 0;
}
