#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#define PWM_BASE  0xFE20C000
#define GPIO_BASE 0xFE200000

#define GPFSEL1   (0x04 / 4)

#define PWM_CTL     (0x00 / 4)
#define PWM_RNG1    (0x10 / 4)
#define PWM_DAT1    (0x14 / 4)

#define BLOCK_SIZE (4*1024)

volatile uint32_t *pwm, *gpio;

int main(void) {
    int mem_fd;
    void *gpio_map, *pwm_map, *clk_map;

    if ((mem_fd = open("/dev/mem", O_RDWR | O_SYNC)) < 0) {
        perror("can't open /dev/mem");
        return -1;
    }

    // map registers to  app virtual space
    gpio_map = mmap(NULL, BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, GPIO_BASE);
    pwm_map  = mmap(NULL, BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, PWM_BASE);

    if (gpio_map == MAP_FAILED || pwm_map == MAP_FAILED || clk_map == MAP_FAILED) {
        perror("mmap error");
        return -1;
    }

    close(mem_fd);
	
    gpio = (volatile uint32_t *)gpio_map;
    pwm  = (volatile uint32_t *)pwm_map;
	
	// init gpio12 as pwm 
    gpio[GPFSEL1] &= ~(7 << 6);  // clear bits
    gpio[GPFSEL1] |=  (4 << 6);  // ALT0 = 100 (PWM0)
	
	// pwm settings
    pwm[PWM_RNG1] = 1000000;       // 50 Hz
    pwm[PWM_DAT1] = 50000;   	   // 1ms
    pwm[PWM_CTL]  = (1 << 0) |	   // PWEN1
	                (1 << 7);	   // MSEN1	  
	
	while (1){
		pwm[PWM_DAT1] = 50000;   	  
		sleep(2);
		pwm[PWM_DAT1] = 100000; 
		sleep(2);
	}
	
	return 0;
}