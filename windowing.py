
def mode_values(mode):
    if mode == 'H':
        bits_count = 3
        mask = 7
    elif mode == 'V':
        bits_count = 2
        mask = 3
    else:
        raise
    return bits_count, mask

def xxx_to_ss(xstart, xstop, xref, mode):

    bits_count, mask = mode_values(mode)

    # Вычисление start:
    # Младшие биты: биты [2:0] из HREF. 
    # Для этого накладываем маску 0b00000111 (0x07)
    start_low = xref & mask

    # Старшие биты: биты из HSTART. 
    # Сдвигаем HSTART на 3 позиции влево, чтобы освободить место для младших битов
    start_high = xstart << bits_count

    # Склеиваем старшие и младшие биты
    start = start_high | start_low


    # Вычисление stop:
    # Младшие биты: биты [5:3] из HREF. 
    # Сначала сдвигаем HREF на 3 позиции вправо, затем накладываем маску 0x07
    stop_low = (xref >> bits_count) & mask

    # Старшие биты: биты из HSTOP.
    # Сдвигаем HSTOP на 3 позиции влево
    stop_high = xstop << bits_count

    # Склеиваем старшие и младшие биты
    stop = stop_high | stop_low

    return start, stop

def ss_to_xxx(start, stop, mode):

    bits_count, mask = mode_values(mode)

    # --- Вычисление HSTART и HSTOP ---
    # При прямом преобразовании HSTART и HSTOP были сдвинуты на 3 бита влево.
    # Чтобы вернуть их обратно, сдвигаем start и stop на 3 бита вправо.
    xstart = start >> bits_count
    xstop  = stop >> bits_count

    # --- Вычисление HREF ---
    # Младшие биты HREF ([2:0]) хранятся в младших битах start
    xref_bits_lo = start & mask

    # Биты HREF [5:3] хранятся в младших битах stop.
    # Выделяем их маской 0x07 и сдвигаем на 3 позиции влево на их законное место.
    xref_bits_hi = (stop & mask) << bits_count

    # Склеиваем части HREF
    xref = xref_bits_hi | xref_bits_lo

    return xstart, xstop, xref

if __name__ == "__main__":
    VSTART, VSTOP, VREF = 0x02, 0x7A, 0x0A
    start, stop = xxx_to_ss(VSTART, VSTOP, VREF, 'V')
    print(f"VSTART = 0x{VSTART:02X} (bin: {VSTART:08b})")
    print(f"VSTOP  = 0x{VSTOP:02X} (bin: {VSTOP:08b})")
    print(f"VREF   = 0x{VREF:02X} (bin: {VREF:08b})")
    print("=== Вычисленные значения ===")
    print(f"start  = 0x{start:03X} (bin: {start:011b}) = {start}")
    print(f"stop   = 0x{stop:03X} (bin: {stop:011b}) = {stop}")
    print(f"stop - start = {stop - start}")

    print("-" * 40)

    start, stop = 238, 358
    HSTART, HSTOP, HREF = ss_to_xxx(start, stop, 'H')
    print(f"start      = {start}")
    print(f"stop       = {stop}")
    print(f"stop - start = {stop - start}")
    print("=== Восстановленные значения ===")
    print(f"HSTART = 0x{HSTART:02X} (bin: {HSTART:08b})")
    print(f"HSTOP  = 0x{HSTOP:02X} (bin: {HSTOP:08b})")
    print(f"HREF   = 0x{HREF:02X} (bin: {HREF:08b})")
