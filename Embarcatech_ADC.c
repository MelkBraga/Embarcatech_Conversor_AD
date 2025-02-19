#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "inc/ssd1306.h"
#include "inc/font.h"

#define LED_RED 13
#define LED_GREEN 11
#define LED_BLUE 12
#define BUTTON_A 5
#define BUTTON_B 6
#define JOYSTICK_BUTTON 22
#define JOYSTICK_X 26
#define JOYSTICK_Y 27
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define OLED_ADDR 0x3C

ssd1306_t ssd;
bool cor = true;
int espessura_borda = 1;
volatile bool leds_pwm_ativos = true;
volatile bool led_verde_estado = false;
volatile int flag_led = 0;
int cursor_x = 64, cursor_y = 32;

void configurar_pwm(uint gpio) {
    gpio_set_function(gpio, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(gpio);
    pwm_set_wrap(slice, 4095);
    pwm_set_chan_level(slice, pwm_gpio_to_channel(gpio), 0);
    pwm_set_enabled(slice, true);
}

void atualizar_pwm(uint gpio, uint16_t valor) {
    if (leds_pwm_ativos) {
        pwm_set_chan_level(pwm_gpio_to_slice_num(gpio), pwm_gpio_to_channel(gpio), valor);
    }
}

void atualizar_display() {
    ssd1306_fill(&ssd, false);
    ssd1306_rect(&ssd, cursor_y, cursor_x, 8, 8, true, true);
    for (int i = 0; i < espessura_borda; i++) {
        ssd1306_rect(&ssd, 3 + i, 3 + i, 122 - (2 * i), 58 - (2 * i), cor, !cor);
    }
    
    ssd1306_send_data(&ssd);
}

bool debounce(uint gpio) {
    static uint32_t ultimo_tempo_A = 0; 
    static uint32_t ultimo_tempo_B = 0; 
    static uint32_t ultimo_tempo_JOYSTICK_BUTTON = 0;
    
    uint32_t tempo_atual = to_ms_since_boot(get_absolute_time()); 

    if (gpio == BUTTON_A) {
        if (tempo_atual - ultimo_tempo_A > 200) { 
            ultimo_tempo_A = tempo_atual; 
            return true;
        }
    } else if (gpio == JOYSTICK_BUTTON) {
        if (tempo_atual - ultimo_tempo_JOYSTICK_BUTTON > 200) {
            ultimo_tempo_JOYSTICK_BUTTON = tempo_atual;
            return true;
        }
    } else if (gpio == BUTTON_B) {
        if (tempo_atual - ultimo_tempo_B > 200) {
            ultimo_tempo_B = tempo_atual;
            return true;
        }
    }
    return false;
}

void isr_botoes(uint gpio, uint32_t events) {
    if (debounce(gpio)) {
        if (gpio == BUTTON_A) {
            leds_pwm_ativos = !leds_pwm_ativos;
            if (!leds_pwm_ativos) {
                pwm_set_chan_level(pwm_gpio_to_slice_num(LED_RED), pwm_gpio_to_channel(LED_RED), 0);
                pwm_set_chan_level(pwm_gpio_to_slice_num(LED_BLUE), pwm_gpio_to_channel(LED_BLUE), 0);
            }
        } else if (gpio == JOYSTICK_BUTTON) {
            led_verde_estado = !led_verde_estado;
            gpio_put(LED_GREEN, led_verde_estado);
            espessura_borda = (espessura_borda == 1) ? 3 : 1;
        } else if (gpio == BUTTON_B) {
            if (flag_led == 2){
                flag_led = 0;
            } else {
                flag_led += 1;
            }
        }
    }
}

void setup() {
    stdio_init_all();

    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, OLED_ADDR, I2C_PORT);
    ssd1306_config(&ssd);
    ssd1306_send_data(&ssd); 
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);
    ssd1306_fill(&ssd, !cor);
    ssd1306_rect(&ssd, 3, 3, 122, 58, cor, !cor); 
    ssd1306_send_data(&ssd); 
    
    gpio_init(BUTTON_A);
    gpio_set_dir(BUTTON_A, GPIO_IN);
    gpio_pull_up(BUTTON_A);

    gpio_init(BUTTON_B);
    gpio_set_dir(BUTTON_B, GPIO_IN);
    gpio_pull_up(BUTTON_B);

    gpio_init(JOYSTICK_BUTTON);
    gpio_set_dir(JOYSTICK_BUTTON, GPIO_IN);
    gpio_pull_up(JOYSTICK_BUTTON);
    
    gpio_set_irq_enabled_with_callback(BUTTON_A, GPIO_IRQ_EDGE_FALL, true, &isr_botoes);
    gpio_set_irq_enabled(BUTTON_B, GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(JOYSTICK_BUTTON, GPIO_IRQ_EDGE_FALL, true);
    
    gpio_init(LED_GREEN);
    gpio_set_dir(LED_GREEN, GPIO_OUT);

    configurar_pwm(LED_RED);
    configurar_pwm(LED_BLUE);
    
    adc_init();
    adc_gpio_init(JOYSTICK_X);
    adc_gpio_init(JOYSTICK_Y);
}

int main() {
    setup();
    while (true) {
        adc_select_input(0);
        uint16_t eixo_y = adc_read();
        adc_select_input(1);
        uint16_t eixo_x = adc_read();

        if (flag_led == 0){
            atualizar_pwm(LED_RED, (eixo_x >= 2000 && eixo_x <= 2096) ? 0 : eixo_x); 
            atualizar_pwm(LED_BLUE, (eixo_y >= 1900 && eixo_y <= 2096) ? 0 : 4095 - eixo_y);
        } 
        
        if (flag_led == 1){
            atualizar_pwm(LED_RED, (eixo_x >= 2000 && eixo_x <= 2096) ? 0 : eixo_x); 
        }

        if (flag_led == 2){
            atualizar_pwm(LED_BLUE, (eixo_y >= 1900 && eixo_y <= 2096) ? 0 : 4095 - eixo_y);
        }

        cursor_x = eixo_x * 128 / 4095;
        cursor_y = 64 - (eixo_y * 64 / 4095);
        atualizar_display();
        sleep_ms(100);
    }
}