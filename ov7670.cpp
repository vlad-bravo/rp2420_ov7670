#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/pwm.h"
#include "hardware/i2c.h"

#include "ov7670.h"

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

// Глобальная переменная для подсчета кадров
static uint32_t frame_counter = 0;

// Вызывается после получения каждой строки
void process_line(uint8_t* line_buf) {
    // Обработка строки (пусто)
}

// Вызывается после получения всех 480 строк кадра
void process_frame() {
    frame_counter++;
    
    // Прямая запись сырых байт переменной frame_counter в stdout (USB UART)
    // sizeof(frame_counter) для uint32_t равно 4 байтам
    fwrite(&frame_counter, sizeof(frame_counter), 1, stdout);
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
  /*
    Set the hardware window.  These values from OV don't entirely
    make sense - hstop is less than hstart.  But they work...
  */
    ov7670_write_reg(REG_CLKRC, 0x1F);

    ov7670_write_reg(REG_HSTART, 0x13);
    ov7670_write_reg(REG_HSTOP, 0x01);
    ov7670_write_reg(REG_HREF, 0x36);
    ov7670_write_reg(REG_SCALING_XSC, 0x3a);
    ov7670_write_reg(REG_SCALING_YSC, 0x35);
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

void ov7670_capture_program_init(PIO pio, uint sm, uint offset, uint data_pin_base, uint pclk_pin, uint href_pin) {
    
    // 1. Настройка 6 пинов данных (GP16..GP21) на вход для PIO
    for (uint i = 0; i < 6; i++) {
        pio_gpio_init(pio, data_pin_base + i);
    }
    pio_sm_set_consecutive_pindirs(pio, sm, data_pin_base, 6, false);

    // 2. Настройка пина PCLK (GP27) на вход для PIO
    pio_gpio_init(pio, pclk_pin);
    pio_sm_set_consecutive_pindirs(pio, sm, pclk_pin, 1, false);

    // 3. Настройка пина HREF (GP28) на вход для PIO
    pio_gpio_init(pio, href_pin);
    pio_sm_set_consecutive_pindirs(pio, sm, href_pin, 1, false);

    // 4. Конфигурация State Machine
    pio_sm_config c = pio_get_default_sm_config();
    
    sm_config_set_wrap(&c, offset + 0, offset + 9);
    sm_config_set_in_pins(&c, data_pin_base);  // База для инструкции 'in'
    sm_config_set_jmp_pin(&c, href_pin);       // Пин для инструкции 'jmp pin'
    
    // Настройка сдвига:
    // - Autopush включен: как только ISR заполнится 32 битами, он автоматически отправится в FIFO
    // - Порог Autopush = 32 (4 пикселя * 8 бит)
    // - Сдвиг вправо (ISR сдвигается вправо, младшие биты заполняются последними)
    sm_config_set_in_shift(&c, true, true, 32);
    
    pio_sm_init(pio, sm, offset, &c);
}

#include "hardware/pio.h"
#include "hardware/clocks.h"

// Массив скомпилированных инструкций PIO
// Логика работы:
// 0: wait 1 gpio 28      (Ждем HREF = 1)
// 1: set x, 4            (Счетчик на 4 пикселя для заполнения 32-битного слова: 4 * 8 бит = 32)
// 2: wait 1 gpio 27      (Ждем PCLK = 1, начало байта Y)
// 3: in pins, 6          (Считываем 6 бит данных D0-D5)
// 4: in null, 2          (Добавляем 2 нулевых бита, чтобы сдвинуть 6 бит влево и получить 8-битный байт)
// 5: wait 0 gpio 27      (Ждем спад PCLK)
// 6: wait 1 gpio 27      (Ждем PCLK = 1, начало байта U/V - игнорируем)
// 7: wait 0 gpio 27      (Ждем спад PCLK)
// 8: jmp x--, 2          (Повторяем 4 раза, заполняя 32-битный регистр ISR)
// 9: jmp pin, 1          (Если HREF еще 1, прыгаем на начало. Иначе - строка закончилась)
static const uint16_t ov7670_capture_program_instructions[] = {
    0x20dc, //  0: wait   1 gpio, 28
    0xe424, //  1: set    x, 4
    0x20db, //  2: wait   1 gpio, 27
    0x4006, //  3: in     pins, 6
    0x4062, //  4: in     null, 2
    0x209b, //  5: wait   0 gpio, 27
    0x20db, //  6: wait   1 gpio, 27
    0x209b, //  7: wait   0 gpio, 27
    0x0042, //  8: jmp    x--, 2
    0x00c1  //  9: jmp    pin, 1
};

const struct pio_program ov7670_capture_program = {
    .instructions = ov7670_capture_program_instructions,
    .length = 10,
    .origin = 0,
};

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
