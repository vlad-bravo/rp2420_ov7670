# Заданные 8-битные значения
VSTART = 0x03
VSTOP  = 0x7B
VREF   = 0x00
# Заданные 8-битные значения
VSTART = 0x02
VSTOP  = 0x7A
VREF   = 0x0A

# Вычисление start:
# Младшие биты: биты [2:0] из HREF. 
# Для этого накладываем маску 0b00000111 (0x07)
start_low = VREF & 0x03

# Старшие биты: биты из HSTART. 
# Сдвигаем HSTART на 3 позиции влево, чтобы освободить место для младших битов
start_high = VSTART << 2

# Склеиваем старшие и младшие биты
start = start_high | start_low


# Вычисление stop:
# Младшие биты: биты [5:3] из HREF. 
# Сначала сдвигаем HREF на 3 позиции вправо, затем накладываем маску 0x07
stop_low = (VREF >> 2) & 0x03

# Старшие биты: биты из HSTOP.
# Сдвигаем HSTOP на 3 позиции влево
stop_high = VSTOP << 2

# Склеиваем старшие и младшие биты
stop = stop_high | stop_low


# Вывод результатов
print(f"VSTART = 0x{VSTART:02X} (bin: {VSTART:08b})")
print(f"VSTOP  = 0x{VSTOP:02X} (bin: {VSTOP:08b})")
print(f"VREF   = 0x{VREF:02X} (bin: {VREF:08b})")
print("-" * 40)
print(f"start  = 0x{start:X}   (bin: {start:011b})")
print(f"stop   = 0x{stop:X}   (bin: {stop:011b})")
print("=== Десятичные значения ===")
print(f"start      = {start}")
print(f"stop       = {stop}")
print(f"stop - start = {stop - start}")
