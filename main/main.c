#include "driver/gpio.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "nvs_flash.h"
#include <gde.h>
#include <gdeh029a1.h>
#include <pictures.h>

esp_err_t event_handler(void *ctx, system_event_t *event) { return ESP_OK; }

uint32_t
get_buttons(void)
{
	uint32_t bits = 0;
	bits |= gpio_get_level(0) << 0; // A
	bits |= gpio_get_level(27) << 1; // B
	bits |= gpio_get_level(25) << 2; // MID
	bits |= gpio_get_level(26) << 3; // UP
	bits |= gpio_get_level(32) << 4; // DOWN
	bits |= gpio_get_level(33) << 5; // LEFT
	bits |= gpio_get_level(35) << 6; // RIGHT
	bits |= gpio_get_level(21) << 7; // GDE BUSY
	return bits;
}

xQueueHandle evt_queue = NULL;

uint32_t buttons_state = -1;

void gpio_intr_test(void *arg)
{
	// read status to get interrupt status for GPIO 0-31
	uint32_t gpio_intr_status_lo = READ_PERI_REG(GPIO_STATUS_REG);
	// read status to get interrupt status for GPIO 32-39
	uint32_t gpio_intr_status_hi = READ_PERI_REG(GPIO_STATUS1_REG);
	// clear intr for GPIO 0-31
	SET_PERI_REG_MASK(GPIO_STATUS_W1TC_REG, gpio_intr_status_lo);
	// clear intr for GPIO 32-39
	SET_PERI_REG_MASK(GPIO_STATUS1_W1TC_REG, gpio_intr_status_hi);

	uint32_t buttons_new  = get_buttons();
	uint32_t buttons_down = (~buttons_new) & buttons_state;
	uint32_t buttons_up   = buttons_new & (~buttons_state);
	buttons_state = buttons_new;

	if (buttons_down != 0)
		xQueueSendFromISR(evt_queue, &buttons_down, NULL);

	if (buttons_down & (1 << 0))
		ets_printf("Button A\n");
	if (buttons_down & (1 << 1))
		ets_printf("Button B\n");
	if (buttons_down & (1 << 2))
		ets_printf("Button MID\n");
	if (buttons_down & (1 << 3))
		ets_printf("Button UP\n");
	if (buttons_down & (1 << 4))
		ets_printf("Button DOWN\n");
	if (buttons_down & (1 << 5))
		ets_printf("Button LEFT\n");
	if (buttons_down & (1 << 6))
		ets_printf("Button RIGHT\n");
	if (buttons_down & (1 << 7))
		ets_printf("GDE-Busy down\n");
	if (buttons_up & (1 << 7))
		ets_printf("GDE-Busy up\n");

	// pass on BUSY signal to LED.
	gpio_set_level(22, 1-gpio_get_level(21));
}

void app_main(void) {
	nvs_flash_init();

	/* create event queue */
	evt_queue = xQueueCreate(10, sizeof(uint32_t));

	/** configure input **/
	gpio_isr_register(gpio_intr_test, NULL, 0, NULL);

	gpio_config_t io_conf;
	io_conf.intr_type = GPIO_INTR_ANYEDGE;
	io_conf.mode = GPIO_MODE_INPUT;
	io_conf.pin_bit_mask =
		GPIO_SEL_0 |
		GPIO_SEL_21 | // GDE BUSY pin
		GPIO_SEL_25 |
		GPIO_SEL_26 |
		GPIO_SEL_27 |
		GPIO_SEL_32 |
		GPIO_SEL_33 |
		GPIO_SEL_35;
	io_conf.pull_down_en = 0;
	io_conf.pull_up_en = 1;
	gpio_config(&io_conf);

	/** configure output **/
    gpio_pad_select_gpio(22);
	gpio_set_direction(22, GPIO_MODE_OUTPUT);

	tcpip_adapter_init();
	ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));
//  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
//  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
//  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
//  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
//  wifi_config_t sta_config = {.sta = {.ssid = "access_point_name",
//                                      .password = "password",
//                                      .bssid_set = false}};
//  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
//  ESP_ERROR_CHECK(esp_wifi_start());
//  ESP_ERROR_CHECK(esp_wifi_connect());

	gdeInit();
	initDisplay(false); // configure slow LUT

	int picture_id = 0;
	drawImage(pictures[picture_id]);
	updateDisplay();
	gdeBusyWait();

	bool enable_fast_waveform = true;
	writeLUT(enable_fast_waveform); // configure fast LUT

	while (1) {
		uint32_t buttons_down;
		if (xQueueReceive(evt_queue, &buttons_down, portMAX_DELAY)) {
			if (buttons_down & (1 << 0)) {
				ets_printf("Button A handling\n");
				if (picture_id + 1 < NUM_PICTURES) {
					picture_id++;
					drawImage(pictures[picture_id]);
					updateDisplay();
					gdeBusyWait();
				}
			}
			if (buttons_down & (1 << 1)) {
				ets_printf("Button B handling\n");
				if (picture_id > 0) {
					picture_id--;
					drawImage(pictures[picture_id]);
					updateDisplay();
					gdeBusyWait();
				}
			}
			if (buttons_down & (1 << 2)) {
				ets_printf("Button MID handling\n");
				drawImage(pictures[picture_id]);
				const char *line_1 = "esp-idf supports compiling multiple files in parallel, so all of the above commands can be run as `make -jN` where `N` is the number of parallel make processes to run (generally N should be equal to or one more than the number of CPU cores in your system.)";

				int pos = 0;
				int row = 14;
				while (line_1[pos]) {
					int num = drawText(row, 16, -16, &line_1[pos], FONT_16PX|FONT_INVERT|FONT_FULL_WIDTH);
					if (num == 0)
						break;
					pos += num;
					row -= 2;
				}
				drawText(row, 16, -16, "", FONT_16PX|FONT_INVERT|FONT_FULL_WIDTH);
				row -= 2;
/*
				const char *line_2 = "Multiple make functions can be combined into one. For example: to build the app & bootloader using 5 jobs in parallel, then flash everything, and then display serial output from the ESP32 run:";
				pos = 0;
				while (line_2[pos]) {
					int num = drawText(row, 16, -16, &line_2[pos], FONT_16PX|FONT_INVERT|FONT_FULL_WIDTH);
					if (num == 0)
						break;
					pos += num;
					row -= 2;
				}
*/

				drawText(0,0,0," Just a status line. Wifi status: not connected",FONT_FULL_WIDTH|FONT_MONOSPACE);

				updateDisplay();
				gdeBusyWait();
			}
			if (buttons_down & (1 << 3)) {
				ets_printf("Button UP handling\n");
				writeLUT(false);
				drawImage(pictures[picture_id]);
				updateDisplay();
				gdeBusyWait();
				writeLUT(enable_fast_waveform);
			}
			if (buttons_down & (1 << 4)) {
				ets_printf("Button DOWN handling\n");
				picture_id = (picture_id + 1) % NUM_PICTURES;

				int i;
				for (i=0; i<8; i++) {
					int j = ((i << 1) | (i >> 2)) & 7;
					drawImage(pictures[picture_id]);
					updateDisplayPartial(37*j, 37*j+36);
					gdeBusyWait();
					vTaskDelay(1000 / portTICK_PERIOD_MS);
				}
			}
			if (buttons_down & (1 << 5)) {
				ets_printf("Button LEFT handling\n");
				enable_fast_waveform = !enable_fast_waveform;
				writeLUT(enable_fast_waveform);
				drawImage(pictures[picture_id]);
				updateDisplay();
				gdeBusyWait();
			}
		}
	}
}
