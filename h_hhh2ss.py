# Заданные 8-битные значения
HSTART = 0x11
HSTOP  = 0x61
HREF   = 0x80
# Заданные 8-битные значения
HSTART = 0x13
HSTOP  = 0x01
HREF   = 0x36

# Вычисление start:
# Младшие биты: биты [2:0] из HREF. 
# Для этого накладываем маску 0b00000111 (0x07)
start_low = HREF & 0x07

# Старшие биты: биты из HSTART. 
# Сдвигаем HSTART на 3 позиции влево, чтобы освободить место для младших битов
start_high = HSTART << 3

# Склеиваем старшие и младшие биты
start = start_high | start_low


# Вычисление stop:
# Младшие биты: биты [5:3] из HREF. 
# Сначала сдвигаем HREF на 3 позиции вправо, затем накладываем маску 0x07
stop_low = (HREF >> 3) & 0x07

# Старшие биты: биты из HSTOP.
# Сдвигаем HSTOP на 3 позиции влево
stop_high = HSTOP << 3

# Склеиваем старшие и младшие биты
stop = stop_high | stop_low


# Вывод результатов
print(f"HSTART = 0x{HSTART:02X} (bin: {HSTART:08b})")
print(f"HSTOP  = 0x{HSTOP:02X} (bin: {HSTOP:08b})")
print(f"HREF   = 0x{HREF:02X} (bin: {HREF:08b})")
print("-" * 40)
print(f"start  = 0x{start:X}   (bin: {start:011b})")
print(f"stop   = 0x{stop:X}   (bin: {stop:011b})")
print("=== Десятичные значения ===")
print(f"start      = {start}")
print(f"stop       = {stop}")
print(f"stop - start = {stop - start}")
