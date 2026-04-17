import serial
import struct

# Укажите ваш COM-порт
ser = serial.Serial('COM3', 115200) 

while True:
    # Читаем ровно 4 байта (размер uint32_t)
    data = ser.read(4)
    if len(data) == 4:
        # Распаковываем little-endian uint32 ('<I')
        frame_num = struct.unpack('<I', data)[0]
        print(f"Получен кадр: {frame_num}")
