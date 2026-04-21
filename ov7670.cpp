#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/i2c.h"

#include "ov7670.h"

// --- Настройки пинов ---
#define XCLK_PIN        26
#define PCLK_PIN        27
#define HREF_PIN        28
#define VSYNC_PIN       23
#define DATA_BASE_PIN   16

#define TEST0_PIN       29
#define TEST1_PIN       4
#define LED_PIN         25
#define WS2812_PIN      23
#define USERKEY_PIN     24

#define I2C_SDA_PIN     0
#define I2C_SCL_PIN     1

#define OV7670_ADDR     0x21

// --- Буферы и указатели ---
uint8_t buffer1[640];
uint8_t buffer2[640];
uint8_t preamble[4]; // 0 - marker, 1 - packet type, 2 - msb, 3 - lsb

uint8_t* capture_buf = buffer1; // Указатель для накопления
uint8_t* process_buf = buffer2; // Указатель для обработки
uint8_t* temp_buf;              // Временный указатель

// --- Переменные состояния ---
volatile uint16_t x, y, process_y;

// Глобальная переменная для подсчета кадров
static uint8_t frame_counter = 0;

// Вызывается после получения каждой строки
void process_line() {
    for (uint8_t i = 6; i<17; i++) // Цикл по 22 кусочкам строки
    {
        preamble[1] = '0' + i;
        fwrite(&preamble[0], 4, 1, stdout);
        fwrite(&process_buf[i*29], 29, 1, stdout);
        fflush(stdout);
    }
}

// Вызывается после получения всех 480 строк кадра
void process_frame() {
    
    // Прямая запись сырых байт переменной frame_counter в stdout (USB UART)
    // sizeof(frame_counter) для uint32_t равно 4 байтам
    //fwrite(&frame_counter, sizeof(frame_counter), 1, stdout);
    //fwrite(&process_buf[0], 15, 1, stdout);
    //fflush(stdout);

    frame_counter++;

    gpio_put(LED_PIN, true);
    sleep_ms(10);
    gpio_put(LED_PIN, false);
}

// --- I2C для OV7670 ---
void ov7670_write_reg(uint8_t reg, uint8_t val) {
    uint8_t buf[2] = {reg, val};
    i2c_write_blocking(i2c0, OV7670_ADDR, buf, 2, false);
}

void ov7670_init() {
    // Сброс
    ov7670_write_reg(REG_COM7, COM7_RESET); //Reset SCCB
    sleep_ms(100);
    
    ov7670_write_reg(REG_TSLB, TSLB_YLAST);	/* OV */
    ov7670_write_reg(REG_COM7, COM7_FMT_VGA);	/* VGA */
    ov7670_write_reg(REG_CLKRC, 0x1F);

    ov7670_write_reg(REG_HSTART, 0x13);
    ov7670_write_reg(REG_HSTOP, 0x01);
    ov7670_write_reg(REG_HREF, 0x36);
    // (SCALING_XSC[7], SCALING_YSC[7]):
    // 00: no test output
    // 01: shifting 1
    // 10: 8-bar color bar
    // 11: fade to gray color bar
    ov7670_write_reg(REG_SCALING_XSC, 0x3a); // 0x3a
    ov7670_write_reg(REG_SCALING_YSC, 0x35); // 0x35
    ov7670_write_reg(REG_SCALING_DCWCTR, 0x11);
    ov7670_write_reg(REG_SCALING_PCLK_DIV, 0xF0);
}

// --- Генерация XCLK 8 MHz через PWM ---
void init_xclk() {
    gpio_set_function(XCLK_PIN, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(XCLK_PIN);
    
    // Системная частота 125 MHz. 125 MHz / 8 MHz = 15.625
    // Округлим до 16: 125 MHz / 16 = 7.8125 MHz (допустимо для OV7670)
    pwm_set_wrap(slice, 15);
    pwm_set_chan_level(slice, pwm_gpio_to_channel(XCLK_PIN), 8); // Скважность 50%
    pwm_set_enabled(slice, true);
}

int main() {
    stdio_init_all();

    // Инициализация I2C и камеры
    i2c_init(i2c0, 100000);
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    //gpio_pull_up(I2C_SDA_PIN);
    //gpio_pull_up(I2C_SCL_PIN);
    
    init_xclk(); // Запускаем 8 MHz
    ov7670_init();

    // Инициализация пина VSYNC
    gpio_init(VSYNC_PIN);
    gpio_set_dir(VSYNC_PIN, GPIO_IN);
    gpio_init(HREF_PIN);
    gpio_set_dir(HREF_PIN, GPIO_IN);
    gpio_init(PCLK_PIN);
    gpio_set_dir(PCLK_PIN, GPIO_IN);

    // Инициализация TEST
    gpio_init(TEST0_PIN);
    gpio_set_dir(TEST0_PIN, GPIO_OUT);
    gpio_init(TEST1_PIN);
    gpio_set_dir(TEST1_PIN, GPIO_OUT);
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    preamble[0] = 0xfc;

    while (true) {

        while (gpio_get(PCLK_PIN));
        if (gpio_get(VSYNC_PIN))
        {
            if (y>0)
            {
                y = 0;
                process_frame();
            }
        }
        else
        {
            if (gpio_get(HREF_PIN))
            {
                //gpio_put(TEST1_PIN, true);
                if (~x & 1)
                    capture_buf[x>>1] = (
                        gpio_get(DATA_BASE_PIN+5)<<7
                        | gpio_get(DATA_BASE_PIN+4)<<6
                        | gpio_get(DATA_BASE_PIN+3)<<5
                        //| gpio_get(DATA_BASE_PIN+2)<<4
                        //| gpio_get(DATA_BASE_PIN+1)<<3
                        //| gpio_get(DATA_BASE_PIN)<<2
                    );
                x++;
                //gpio_put(TEST1_PIN, false);
            }
            else
            {
                if (x>0)
                {
                    //gpio_put(TEST0_PIN, true);
                    x = 0;
                    // Сохранить номер полученной строки
                    process_y = y;
                    preamble[2] = y>>8;
                    preamble[3] = y;
                    y++;
                    // Поменять указатели на буферы
                    temp_buf = capture_buf;
                    capture_buf = process_buf;
                    process_buf = temp_buf;
                    process_line();
                    //gpio_put(TEST0_PIN, false);
                }
            }
        }
        while (gpio_get(PCLK_PIN) == 0);
    }
}
