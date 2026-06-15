# Значения start и stop, полученные в предыдущем скрипте
start = 12
stop  = start + 480
start = 12 + 260
stop  = start + 50

# --- Вычисление HSTART и HSTOP ---
# При прямом преобразовании HSTART и HSTOP были сдвинуты на 3 бита влево.
# Чтобы вернуть их обратно, сдвигаем start и stop на 3 бита вправо.
VSTART = start >> 2
VSTOP  = stop >> 2

# --- Вычисление HREF ---
# Младшие биты HREF ([2:0]) хранятся в младших битах start
href_bits_2_0 = start & 0x03

# Биты HREF [5:3] хранятся в младших битах stop.
# Выделяем их маской 0x07 и сдвигаем на 3 позиции влево на их законное место.
href_bits_5_3 = (stop & 0x03) << 2

# Склеиваем части HREF
VREF = href_bits_5_3 | href_bits_2_0

# Примечание: биты [7:6] регистра HREF невозможно восстановить из start и stop,
# так как они не были в них сохранены. Если в вашей системе бит 7 всегда равен 1
# (как в 0x80), вы можете принудительно установить его: HREF |= 0x80

# --- Печать результатов ---
print("=== Десятичные значения ===")
print(f"start      = {start}")
print(f"stop       = {stop}")
print(f"stop - start = {stop - start}")

print("\n=== Восстановленные значения ===")
print(f"VSTART = 0x{VSTART:02X} (bin: {VSTART:08b})")
print(f"VSTOP  = 0x{VSTOP:02X} (bin: {VSTOP:08b})")
print(f"VREF   = 0x{VREF:02X} (bin: {VREF:08b})")
