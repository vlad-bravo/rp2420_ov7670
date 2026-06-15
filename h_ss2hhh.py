# Значения start и stop, полученные в предыдущем скрипте
start = 0x88   # В десятичной: 136
stop  = 0x308  # В десятичной: 776
# Значения start и stop, полученные в предыдущем скрипте
start = 158
stop  = start + 640
start = 158 + 80
stop  = start + 120

# --- Вычисление HSTART и HSTOP ---
# При прямом преобразовании HSTART и HSTOP были сдвинуты на 3 бита влево.
# Чтобы вернуть их обратно, сдвигаем start и stop на 3 бита вправо.
HSTART = start >> 3
HSTOP  = stop >> 3

# --- Вычисление HREF ---
# Младшие биты HREF ([2:0]) хранятся в младших битах start
href_bits_2_0 = start & 0x07

# Биты HREF [5:3] хранятся в младших битах stop.
# Выделяем их маской 0x07 и сдвигаем на 3 позиции влево на их законное место.
href_bits_5_3 = (stop & 0x07) << 3

# Склеиваем части HREF
HREF = href_bits_5_3 | href_bits_2_0

# Примечание: биты [7:6] регистра HREF невозможно восстановить из start и stop,
# так как они не были в них сохранены. Если в вашей системе бит 7 всегда равен 1
# (как в 0x80), вы можете принудительно установить его: HREF |= 0x80

# --- Печать результатов ---
print("=== Десятичные значения ===")
print(f"start      = {start}")
print(f"stop       = {stop}")
print(f"stop - start = {stop - start}")

print("\n=== Восстановленные значения ===")
print(f"HSTART = 0x{HSTART:02X} (bin: {HSTART:08b})")
print(f"HSTOP  = 0x{HSTOP:02X} (bin: {HSTOP:08b})")
print(f"HREF   = 0x{HREF:02X} (bin: {HREF:08b})")
