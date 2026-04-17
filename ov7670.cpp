#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/pwm.h"
#include "hardware/i2c.h"

// --- Настройки пинов ---
#define XCLK_PIN        26
#define PCLK_PIN        27
#define HREF_PIN        28
#define VSYNC_PIN       23
#define DATA_BASE_PIN   16 // D0-D7 подключены к GP2-GP9

#define I2C_SDA_PIN     14
#define I2C_SCL_PIN     15

#define OV7670_ADDR     0x21

// --- Буферы и указатели ---
uint8_t buffer1[640];
uint8_t buffer2[640];

uint8_t* capture_buf = buffer1; // Указатель для накопления
uint8_t* process_buf = buffer2; // Указатель для обработки

// --- Переменные состояния ---
volatile int line_counter = 0;
volatile bool line_ready = false;
volatile bool frame_done = false;

// Вызовы пользователя
extern void process_line(uint8_t* line_buf);
extern void process_frame();

// --- I2C для OV7670 ---
void ov7670_write_reg(uint8_t reg, uint8_t val) {
    uint8_t buf[2] = {reg, val};
    i2c_write_blocking(i2c0, OV7670_ADDR, buf, 2, false);
}

void ov7670_init() {
    // Сброс
    ov7670_write_reg(0x12, 0x80);
    sleep_ms(100);
    
    // Настройка VGA и формата YUV422
    ov7670_write_reg(0x12, 0x00); // QVGA=0, YUV422
    ov7670_write_reg(0x3D, 0x08); // COM13 - Включить YUYV формат (Y первый)
    ov7670_write_reg(0x40, 0xD0); // COM15 - Полный диапазон выхода (0-255)
    // При необходимости добавьте другие регистры для частоты и баланса белого
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

// --- Настройка PIO ---
void init_pio_capture(PIO pio, uint *sm) {
    *sm = pio_claim_unused_sm(pio, true);
    uint offset = pio_add_program(pio, &ov7670_capture_program);

    ov7670_capture_program_init(pio, *sm, offset, DATA_BASE_PIN, PCLK_PIN, HREF_PIN);

    // Настраиваем Autopush: PIO будет аппаратно скидывать 32 бита в FIFO 
    // после того как соберет 4 байта Y (4 * 8 = 32 бита)
    pio_sm_set_enabled(pio, *sm, false);
    pio->sm[*sm].shiftctrl = 
        (4 << PIO_SM0_SHIFTCTRL_PULL_THRESH_LSB) | // Pull 4 слова (не используется тут)
        (4 << PIO_SM0_SHIFTCTRL_PUSH_THRESH_LSB) | // Push thresh 4 (32 бита)
        PIO_SM0_SHIFTCTRL_AUTOPUSH_BITS |           // Включить Autopush
        PIO_SM0_SHIFTCTRL_FJOIN_RX_BITS;   // Сдвиг вправо (ISR)
    pio_sm_set_enabled(pio, *sm, true);
}

// --- Настройка DMA ---
int dma_chan;
dma_channel_config dma_cfg;

void dma_handler() {
    dma_hw->ints0 = 1u << dma_chan; // Сброс флага прерывания

    // После получения строки меняем указатели местами
    uint8_t* temp = capture_buf;
    capture_buf = process_buf;
    process_buf = temp;

    line_counter++;
    line_ready = true; // Флаг для основного цикла

    if (line_counter < 480) {
        // Перезапускаем DMA для следующей строки на новый capture_buf
        dma_channel_configure(dma_chan, &dma_cfg,
            capture_buf,          //Dst
            &pio0->rxf[0],       //Src (FIFO PIO)
            160,                  //160 32-битных слов = 640 байт
            true                  //Start immediately
        );
    } else {
        frame_done = true; // Кадр полностью получен
    }
}

void init_dma(PIO pio, uint sm) {
    dma_chan = dma_claim_unused_channel(true);
    dma_cfg = dma_channel_get_default_config(dma_chan);

    channel_config_set_transfer_data_size(&dma_cfg, DMA_SIZE_32); // Пересылаем по 32 бита
    channel_config_set_read_increment(&dma_cfg, false);           // Читаем из одного регистра FIFO
    channel_config_set_write_increment(&dma_cfg, true);           // Пишем в память последовательно
    channel_config_set_dreq(&dma_cfg, pio_get_dreq(pio, sm, false)); // Синхронизация от PIO RX FIFO

    dma_channel_set_irq0_enabled(dma_chan, true);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);
}

int main() {
    stdio_init_all();

    // Инициализация I2C и камеры
    i2c_init(i2c0, 100000);
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);
    
    init_xclk(); // Запускаем 8 MHz
    ov7670_init();

    // Инициализация пина VSYNC
    gpio_init(VSYNC_PIN);
    gpio_set_dir(VSYNC_PIN, GPIO_IN);

    // Инициализация PIO и DMA
    PIO pio = pio0;
    uint sm;
    init_pio_capture(pio, &sm);
    init_dma(pio, sm);

    while (true) {
        // Ждем начала нового кадра (VSYNC: низкий уровень -> высокий уровень)
        while (gpio_get(VSYNC_PIN) == 1);
        while (gpio_get(VSYNC_PIN) == 0);

        line_counter = 0;
        frame_done = false;
        line_ready = false;

        // Стартуем DMA для первой строки
        dma_channel_configure(dma_chan, &dma_cfg,
            capture_buf,
            &pio0->rxf[sm],
            160, // 640 байт / 4 = 160 32-битных пересылок
            true
        );

        // Обработка данных в основном цикле
        while (!frame_done) {
            if (line_ready) {
                line_ready = false;
                
                // Вызываем обработку строки (process_buf сейчас указывает на нее)
                process_line(process_buf);
                
                // Если это была последняя строка, обработка кадра
                if (line_counter == 480) {
                    frame_done = true;
                }
            }
            // Здесь можно выполнять другие фоновые задачи
            tight_loop_contents();
        }

        // Все 480 строк получены
        process_frame();
    }
}
