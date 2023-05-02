#include <mbed.h>
#include <stdio.h>
#include "gyro.h"
#include "file_system.h"
#include "button.h"

#define RECORD_FLAG 1
#define ERASE_FLAG 2
#define DATA_RDY_FLAG 3
#define STOP_FLAG 4

InterruptIn gyro_int(PA_2, PullDown);
EventFlags flags;

// An extended button API
Button button(USER_BUTTON);

// moving average filter will be defined after main()
float movingAverageFilter(float input, float buffer[], size_t N, size_t &index, float &sum);

// double click to start recording
void onDoubleClick()
{
    flags.set(RECORD_FLAG);
}

// long press to erase
void onLongPress()
{
    flags.set(ERASE_FLAG);
}

// click button, stop recording
void onClick()
{
    flags.set(STOP_FLAG);
}

// data ready
void onGyroDataReady()
{
    flags.set(DATA_RDY_FLAG);
}

int main()
{
    // setup gyroscope's initialization parameters
    Gyroscope_Init_Parameters init_parameters;
    init_parameters.conf1 = ODR_200_CUTOFF_50;
    init_parameters.conf3 = INT2_DRDY;
    init_parameters.conf4 = FULL_SCALE_500;

    // setup gyroscope's raw data
    Gyroscope_RawData raw_data;

    // attach ISR for button click
    button.onClick(&onClick);
    // attach ISR for button double click
    button.onDoubleClick(&onDoubleClick);
    // attach ISR for button long press
    button.onLongClick(&onLongPress);

    // gyroscope raw data
    Gyroscope_RawData raw_data;

    float record[3] = {0.0f, 0.0f, 0.0f}; // record data gx, gy, gz

    // open the file system to check if there is a gesture key file
    MountFileSystem(); 
    // TODO: Seek if there is a already stored gesture key file in the file system
    UnmountFileSystem();

    // configure the interrupt pin
    gyro_int.rise(&onGyroDataReady);

    // The gyroscope sensor keeps its configuration between power cycles.
    // This means that the gyroscope will already have it's data-ready interrupt
    // configured when we turn the board on the second time. This can lead to
    // the pin level rising before we have configured our interrupt handler.
    // To account for this, we manually check the signal and set the flag
    // for the first sample.
    if (!(flags.get() & DATA_RDY_FLAG) && (gyro_int.read() == 1))
    {
        flags.set(DATA_RDY_FLAG);
    }

    int index_counter = 0;

    while (1)
    {
        float gx, gy, gz; // temp holder for calcualted gyro data

        flags.wait_all(RECORD_FLAG, osWaitForever, false); // wait for double click to start recording
        InitiateGyroscope(&init_parameters, &raw_data);    // initiate gyroscope
        MountFileSystem();                                 // mount file system

        flags.wait_all(DATA_RDY_FLAG); // wait for data ready
        GetCalibratedRawData();        // get calibrated raw data

        // convert raw data to dps
        gx = ((float)raw_data.x_raw) * (17.5f * 0.017453292519943295769236907684886f / 1000.0f);
        gy = ((float)raw_data.y_raw) * (17.5f * 0.017453292519943295769236907684886f / 1000.0f);
        gz = ((float)raw_data.z_raw) * (17.5f * 0.017453292519943295769236907684886f / 1000.0f);

        // write data to file
        WriteFile(gx, index_counter);
        index_counter++;
        WriteFile(gy, index_counter);
        index_counter++;
        WriteFile(gz, index_counter);
        index_counter++;

        // wait for 50ms
        ThisThread::sleep_for(50ms); 

        flags.wait_all(STOP_FLAG); // wait for click to stop recording
        PowerOff();                // turn off gyroscope
        UnmountFileSystem();       // unmount file system
        flags.clear(RECORD_FLAG);  // clear record flag
    }
}

float movingAverageFilter(float input, float buffer[], size_t N, size_t &index, float &sum) {
    // Remove oldest value from sum
    sum -= buffer[index];

    // Add new value to buffer and sum
    buffer[index] = input;
    sum += input;

    // Increment index, wrap around if needed
    index = (index + 1) % N;

    // Return average
    return sum / static_cast<float>(N);
}