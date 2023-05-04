import serial

# Replace 'COMx' with the appropriate serial port name for your system
ser = serial.Serial('COM4', 9600)
data = []

while True:
    line = ser.readline().decode('utf-8').strip()
    if line:
        values = [float(x) for x in line.split(',')]
        data.append(values)

    # Exit the loop after receiving the desired number of data points
    if len(data) >= NUMBER_OF_DATA_POINTS:
        break

ser.close()

# Save the data to a file
with open('output.txt', 'w') as f:
    for entry in data:
        f.write("{}, {}, {}\n".format(entry[0], entry[1], entry[2]))
