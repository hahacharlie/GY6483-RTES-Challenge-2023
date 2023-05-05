#include <mbed.h>
#include <vector>
#include "gyro.h"
#include "drivers/LCD_DISCO_F429ZI.h"
#include "drivers/TS_DISCO_F429ZI.h"

// Event flags
#define KEY_FLAG 1
#define DATA_READY_FLAG 2
#define UNLOCK_FLAG 3
#define ERASE_FLAG 4
// Timeout values in milliseconds for button events
#define SINGLE_CLICK_TIMEOUT 200
#define DOUBLE_CLICK_TIMEOUT 500
#define LONG_PRESS_TIMEOUT 1000
// Debounce timeout in milliseconds
#define DEBOUNCE_TIMEOUT 20

InterruptIn gyro_int2(PA_2, PullDown);
InterruptIn user_button(USER_BUTTON, PullDown);

LCD_DISCO_F429ZI lcd;
TS_DISCO_F429ZI ts;

EventFlags button_flags;

Timer timer;

void draw_button(int x, int y, int width, int height, const char *label);
bool is_touch_inside_button(int touch_x, int touch_y, int button_x, int button_y, int button_width, int button_height);

// /*******************************************************************************
//  *
//  * @brief Button state machine states
//  *
//  * ****************************************************************************/
// typedef enum
// {
//     BUTTON_STATE_IDLE,
//     BUTTON_STATE_WAIT_DEBOUNCE,
//     BUTTON_STATE_PRESSED,
//     BUTTON_STATE_WAIT_RELEASE
// } button_state_t;

// Button state variable
// volatile button_state_t button_state = BUTTON_STATE_IDLE;

/*******************************************************************************
 * Function Prototypes of Threads
 * ****************************************************************************/
void gyroscope_thread();
void touch_screen_thread();

/*******************************************************************************
 * Function Prototypes of Flash
 * ****************************************************************************/
bool storeGyroDataToFlash(vector<array<float, 3>> &gesture_key, uint32_t flash_address);
vector<array<float, 3>> readGyroDataFromFlash(uint32_t flash_address, size_t data_size);

/*******************************************************************************
 * Function Prototypes of filters
 * ****************************************************************************/
// moving average filter will be defined after main()
float movingAverageFilter(float input, float buffer[], size_t N, size_t &index, float &sum);

/*******************************************************************************
 * ISR Callback Functions
 * ****************************************************************************/
// Callback function for button press
void button_press()
{
    button_flags.set(KEY_FLAG);
}
// Gyrscope data ready ISR
void onGyroDataReady()
{
    button_flags.set(DATA_READY_FLAG);
}

/*******************************************************************************
 * @brief Global Variables
 * ****************************************************************************/
// Create a vector to store gyroscope data
vector<array<float, 3>> gesture_key;
vector<array<float, 3>> unlocking_record;
const int button1_x = 60;
const int button1_y = 80;
const int button1_width = 120;
const int button1_height = 50;
const char *button1_label = "RECORD";
const int button2_x = 60;
const int button2_y = 200;
const int button2_width = 120;
const int button2_height = 50;
const char *button2_label = "UNLOCK";

/*******************************************************************************
 * @brief main function
 * ****************************************************************************/
int main()
{
    lcd.Clear(LCD_COLOR_BLACK);

    // Draw button 1
    draw_button(button1_x, button1_y, button1_width, button1_height, button1_label);

    // Draw button 2
    draw_button(button2_x, button2_y, button2_width, button2_height, button2_label);

    // initialize all interrupts
    user_button.rise(&button_press);
    gyro_int2.rise(&onGyroDataReady);

    ThisThread::sleep_for(100ms);

    // Create the gyroscope thread
    Thread key_saving;
    key_saving.start(callback(gyroscope_thread));
    printf("Gesture Key recording started\r\n");
    // Create the touch screen thread
    Thread touch_thread;
    touch_thread.start(callback(touch_screen_thread));
    printf("Touch screen thread started\r\n");

    while (1)
    {
        ThisThread::sleep_for(100ms);
    }
}

/*******************************************************************************
 *
 * @brief gyroscope gesture key saving thread
 *
 * ****************************************************************************/
// Function definition for the gyroscope thread task
void gyroscope_thread()
{
    // Add your gyroscope initialization parameters here
    Gyroscope_Init_Parameters init_parameters;
    init_parameters.conf1 = ODR_200_CUTOFF_50;
    init_parameters.conf3 = INT2_DRDY;
    init_parameters.conf4 = FULL_SCALE_500;

    // Set up gyroscope's raw data
    Gyroscope_RawData raw_data;

    // The gyroscope sensor keeps its configuration between power cycles.
    // This means that the gyroscope will already have it's data-ready interrupt
    // configured when we turn the board on the second time. This can lead to
    // the pin level rising before we have configured our interrupt handler.
    // To account for this, we manually check the signal and set the flag
    // for the first sample.
    if (!(button_flags.get() & DATA_READY_FLAG) && (gyro_int2.read() == 1))
    {
        button_flags.set(DATA_READY_FLAG);
    }

    while (1)
    {
        button_flags.wait_all(KEY_FLAG);
        // Initiate gyroscope
        InitiateGyroscope(&init_parameters, &raw_data);

        printf("========[Recording initializing...]========\r\n");

        timer.start();
        while (timer.elapsed_time() < 5s)
        {
            // Wait for the gyroscope data to be ready
            button_flags.wait_all(DATA_READY_FLAG);
            // Read the data from the gyroscope
            GetCalibratedRawData();
            // Add the converted data to the gesture_key vector
            gesture_key.push_back({ConvertToDPS(raw_data.x_raw), ConvertToDPS(raw_data.y_raw), ConvertToDPS(raw_data.z_raw)});
            ThisThread::sleep_for(100ms); // 10Hz
        }

        timer.stop();  // Stop timer
        timer.reset(); // Reset timer

        printf("========[Recording finish.]========\r\n");

        ThisThread::sleep_for(1s);

        // Print the data
        printf("========[Printing data from guesture key...]========\r\n");
        printf("There are %d data in the vector.\r\n", gesture_key.size());
        for (size_t i = 0; i < gesture_key.size(); i++)
        {
            printf("x: %f, y: %f, z: %f\r\n", gesture_key[i][0], gesture_key[i][1], gesture_key[i][2]);
        }
        printf("========[Printing finish.]========\r\n");

        gesture_key.clear(); // Clear the vector

        ThisThread::sleep_for(100ms);
    }
}

/*******************************************************************************
 *
 * @brief touch screen thread
 *
 * ****************************************************************************/
void touch_screen_thread()
{
    // Add your touch screen initialization and handling code here
    TS_StateTypeDef ts_state;

    if (ts.Init(lcd.GetXSize(), lcd.GetYSize()) != TS_OK)
    {
        printf("Failed to initialize the touch screen!\r\n");
        return;
    }

    while (1)
    {
        ts.GetState(&ts_state);
        if (ts_state.TouchDetected)
        {
            int touch_x = ts_state.X;
            int touch_y = ts_state.Y;

            // Check if the touch is inside button 1
            if (is_touch_inside_button(touch_x, touch_y, button2_x, button2_y - 30, button1_width, button1_height))
            {
                printf("========[Recording....]========\r\n");
            }

            // Check if the touch is inside button 2
            if (is_touch_inside_button(touch_x, touch_y, button1_x - 30, button1_y - 30, button2_width, button2_height))
            {
                printf("========[Unlocking....]========\r\n");
            }
        }
        ThisThread::sleep_for(10ms);
    }
}

/*******************************************************************************
 *
 * @brief store data to flash
 * @param gesture_key: the data to store
 * @param flash_address: the address of the flash to store
 * @return true if the data is stored successfully, false otherwise
 *
 * ****************************************************************************/
bool storeGyroDataToFlash(vector<array<float, 3>> &gesture_key, uint32_t flash_address)
{
    FlashIAP flash;
    flash.init();

    // Calculate the total size of the data to be stored in bytes
    uint32_t data_size = gesture_key.size() * sizeof(array<float, 3>);

    // Erase the flash sector
    flash.erase(flash_address, data_size);

    // Write the data to flash
    int write_result = flash.program(gesture_key.data(), flash_address, data_size);

    flash.deinit();

    return write_result == 0;
}

/*******************************************************************************
 *
 * @brief read data from flash
 * @param flash_address: the address of the flash to read
 * @param data_size: the size of the data to read in bytes
 * @return a vector of array<float, 3> containing the data
 *
 * ****************************************************************************/
vector<array<float, 3>> readGyroDataFromFlash(uint32_t flash_address, size_t data_size)
{
    vector<array<float, 3>> gesture_key(data_size);

    FlashIAP flash;
    flash.init();

    // Read the data from flash
    flash.read(gesture_key.data(), flash_address, data_size * sizeof(array<float, 3>));

    flash.deinit();

    return gesture_key;
}

// /*******************************************************************************
//  *
//  * @brief button press ISR
//  *
//  * ****************************************************************************/
// void button_press()
// {
//     static Timer timer;
//     static unsigned int press_count = 0;

//     switch (button_state)
//     {
//     case BUTTON_STATE_IDLE:
//         button_state = BUTTON_STATE_WAIT_DEBOUNCE;
//         timer.start();
//         break;
//     case BUTTON_STATE_WAIT_DEBOUNCE:
//         if (timer.elapsed_time().count() >= DEBOUNCE_TIMEOUT)
//         {
//             button_state = BUTTON_STATE_PRESSED;
//             timer.reset();
//         }
//         break;
//     case BUTTON_STATE_PRESSED:
//         if (timer.elapsed_time().count() >= LONG_PRESS_TIMEOUT)
//         {
//             button_flags.set(ERASE_FLAG);
//             press_count = 0;
//             timer.stop();
//             timer.reset();
//             button_state = BUTTON_STATE_WAIT_RELEASE;
//         }
//         else if (user_button)
//         {
//             press_count++;
//             button_state = BUTTON_STATE_WAIT_RELEASE;
//         }
//         break;
//     case BUTTON_STATE_WAIT_RELEASE:
//         if (user_button)
//         {
//             if (press_count == 2)
//             {
//                 button_flags.set(KEY_FLAG);
//                 press_count = 0;
//                 timer.stop();
//                 timer.reset();
//             }
//             else if (press_count == 1)
//             {
//                 ThisThread::sleep_for(SINGLE_CLICK_TIMEOUT);
//                 button_flags.set(UNLOCK_FLAG);
//                 press_count = 0;
//                 timer.stop();
//                 timer.reset();
//             }
//             button_state = BUTTON_STATE_IDLE;
//         }
//         break;
//     }
// }

// /*******************************************************************************
//  *
//  * @brief draw button
//  * @param x: x coordinate of the button
//  * @param y: y coordinate of the button
//  * @param width: width of the button
//  * @param height: height of the button
//  * @param label: label of the button
//  *
//  * ****************************************************************************/
void draw_button(int x, int y, int width, int height, const char *label)
{
    lcd.SetTextColor(LCD_COLOR_BLUE);
    lcd.FillRect(x, y, width, height);
    // lcd.SetTextColor(LCD_COLOR_WHITE);
    lcd.DisplayStringAt(x + width / 2 - strlen(label) * 19, y + height / 2 - 8, (uint8_t *)label, CENTER_MODE);
}

// /*******************************************************************************
//  *
//  * @brief Check if the touch point is inside the button
//  * @param touch_x: x coordinate of the touch point
//  * @param touch_y: y coordinate of the touch point
//  * @param button_x: x coordinate of the button
//  * @param button_y: y coordinate of the button
//  * @param button_width: width of the button
//  * @param button_height: height of the button
//  *
//  * ****************************************************************************/
bool is_touch_inside_button(int touch_x, int touch_y, int button_x, int button_y, int button_width, int button_height)
{
    return (touch_x >= button_x && touch_x <= button_x + button_width &&
            touch_y >= button_y && touch_y <= button_y + button_height);
}