#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

const static int led_gpio = 18;
const char gpiodir[] = "/sys/class/gpio";

#define GPIO_WRITE(val) do { \
	nw = write(fd_val, val, 1); \
	if (nw != 1) { \
		perror("write to gpio value file failed"); \
		exit(1); \
	} \
} while (0)

int main()
{
	char gpiofile[128], gpiodir_export[128], led_gpio_str[3];
	char gpio_led_direction[128], gpio_led_value[128];
	int fd, fd_export, fd_dir, fd_val;
	size_t nw = 0;

	memset(gpiofile, 0, 128);
	snprintf(gpiofile, 127, "%s/gpio%d", gpiodir, led_gpio);
	printf("gpio (pseudo)file = %s\n", gpiofile);

	memset(led_gpio_str, 0, 3);
	snprintf(led_gpio_str, 3, "%d", led_gpio);
	memset(gpiodir_export, 0, 128);
	snprintf(gpiodir_export, 127, "%s/export", gpiodir);

	fd = open(gpiofile, O_RDWR);
	if (fd < 0) {
		printf("Exporting GPIO #%d now...\n", led_gpio);
		fd_export = open(gpiodir_export, O_WRONLY);
		if (fd_export < 0) {
			perror("open of gpio export file failed (belong to group 'gpio'?)");
			exit(1);
		}
		nw = write(fd_export, led_gpio_str, strlen(led_gpio_str));
		if (nw < 0) {
			perror("write to gpio export file failed");
			exit(1);
		}
	}

	/* By now the pseudofile /sys/class/gpio/gpio<led_gpio> should exist
	 * Set direction to 'out'
	 */
	memset(gpio_led_direction, 0, 128);
	snprintf(gpio_led_direction, 127, "%s/gpio%d/direction", gpiodir, led_gpio);
	fd_dir = open(gpio_led_direction, O_WRONLY);
	if (fd_dir < 0) {
		perror("open of gpio direction file failed (belong to group 'gpio'?)");
		exit(1);
	}
	nw = write(fd_dir, "out", 3);
	if (nw < 0) {
		perror("write to gpio export file failed (belong to group 'gpio'?)");
		exit(1);
	}

	// Prep the 'value' pseudofile for write
	memset(gpio_led_value, 0, 128);
	snprintf(gpio_led_value, 127, "%s/gpio%d/value", gpiodir, led_gpio);
	fd_val = open(gpio_led_value, O_WRONLY);
	if (fd_val < 0) {
		perror("open of gpio value file failed (belong to group 'gpio'?)");
		exit(1);
	}

	// Ok, now (finally!) let's toggle the LED!
	while (1) {
		GPIO_WRITE("1");
		sleep(1);
		GPIO_WRITE("0");
		sleep(1);
	}
		


	exit(0);
}
