#include <mbed.h>
#include <vector>
#include <array>
#include <limits>
#include <cmath>
#include "gyro.h"
#include "drivers/LCD_DISCO_F429ZI.h"
#include "drivers/TS_DISCO_F429ZI.h"

// Event flags
#define KEY_FLAG 1
#define UNLOCK_FLAG 2
#define ERASE_FLAG 4

#define DATA_READY_FLAG 8

InterruptIn gyro_int2(PA_2, PullDown);
InterruptIn user_button(USER_BUTTON, PullDown);

LCD_DISCO_F429ZI lcd;
TS_DISCO_F429ZI ts;

EventFlags flags;

Timer timer;

void draw_button(int x, int y, int width, int height, const char *label);
bool is_touch_inside_button(int touch_x, int touch_y, int button_x, int button_y, int button_width, int button_height);
float euclidean_distance(const std::array<float, 3> &a, const std::array<float, 3> &b);
float dtw(const std::vector<std::array<float, 3>> &s, const std::vector<std::array<float, 3>> &t);

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
    flags.set(ERASE_FLAG);
}
// Gyrscope data ready ISR
void onGyroDataReady()
{
    flags.set(DATA_READY_FLAG);
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

    // keep main thread alive
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
    if (!(flags.get() & DATA_READY_FLAG) && (gyro_int2.read() == 1))
    {
        flags.set(DATA_READY_FLAG);
    }

    while (1)
    {
        vector<array<float, 3>> temp_key;

        auto flag_check = flags.wait_any(KEY_FLAG | UNLOCK_FLAG | ERASE_FLAG);

        if (flag_check & ERASE_FLAG)
        {
            printf("========[Erasing....]========\r\n");
            gesture_key.clear();
            printf("========[Key Erasing finish.]========\r\n");
            unlocking_record.clear();
            printf("========[All Erasing finish.]========\r\n");
        }

        if (flag_check & (KEY_FLAG | UNLOCK_FLAG))
        {
            // Initiate gyroscope
            InitiateGyroscope(&init_parameters, &raw_data);

            printf("========[Recording initializing...]========\r\n");
            timer.start();
            while (timer.elapsed_time() < 5s)
            {
                // Wait for the gyroscope data to be ready
                flags.wait_all(DATA_READY_FLAG);
                // Read the data from the gyroscope
                GetCalibratedRawData();
                // Add the converted data to the gesture_key vector
                temp_key.push_back({ConvertToDPS(raw_data.x_raw), ConvertToDPS(raw_data.y_raw), ConvertToDPS(raw_data.z_raw)});
                ThisThread::sleep_for(100ms); // 10Hz
            }
            timer.stop();  // Stop timer
            timer.reset(); // Reset timer
            printf("========[Recording finish.]========\r\n");
        }

        // TODO: add some data filtering there

        // check the flag see if it is recording or unlocking
        if (flag_check & KEY_FLAG)
        {

            if (gesture_key.empty())
            {
                printf("========[No key in the system, Saving key...]========\r\n");
                gesture_key = temp_key;
                temp_key.clear();
            }
            else
            {
                printf("========[The old key will be removed!!!!!]========\r\n");
                ThisThread::sleep_for(1s);
                // TODO: Better to have a interrupt here to ask the user if they want to remove the old key
                gesture_key.clear();
                gesture_key = temp_key;
                printf("========[Old key has been removed, new key is saved. ]========\r\n");
                temp_key.clear();
            }

            // Print the data
            ThisThread::sleep_for(1s);
            printf("========[Printing data in guesture key...]========\r\n");
            printf("There are %d data in the vector.\r\n", gesture_key.size());
            for (size_t i = 0; i < gesture_key.size(); i++)
            {
                printf("x: %f, y: %f, z: %f\r\n", gesture_key[i][0], gesture_key[i][1], gesture_key[i][2]);
            }
            printf("========[Printing finish.]========\r\n");

            // TODO: potential imrovement, save the gesture key to flash
            // storeGyroDataToFlash(temp_key, 0x080E0000);
            // printf("========[Key saving finish.]========\r\n");
            // gesture_key = readGyroDataFromFlash(0x080E0000, temp_key.size());
        }
        else if (flag_check & UNLOCK_FLAG)
        {
            flags.clear(UNLOCK_FLAG);
            printf("========[Unlocking...]========\r\n");
            unlocking_record = temp_key;
            temp_key.clear();

            // check if the gesture key is empty
            if (gesture_key.empty())
            {
                printf("========[No key saved. Please record it! ]========\r\n");
                unlocking_record.clear();
            }
            else
            {
                // calculate the similarity of the gesture key and the unlocking record
                float similarity = dtw(gesture_key, unlocking_record);
                printf("========[Similarity: %f]========\r\n", similarity);
                ThisThread::sleep_for(1s);
                if (similarity > 100)
                {
                    printf("========[Unlocking failed.]========\r\n");
                    unlocking_record.clear();
                }
                else
                {
                    printf("========[Unlocking success.]========\r\n");
                    unlocking_record.clear();
                }
            }
        }

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
                printf("========[Recording after 3 seconds....]========\r\n");
                ThisThread::sleep_for(3s);
                flags.set(KEY_FLAG);
            }

            // Check if the touch is inside button 2
            if (is_touch_inside_button(touch_x, touch_y, button1_x - 30, button1_y - 30, button2_width, button2_height))
            {
                printf("========[Unlock Recording after 3 seconds....]========\r\n");
                ThisThread::sleep_for(3s);
                flags.set(UNLOCK_FLAG);
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

/*******************************************************************************
 *
 * @brief draw button
 * @param x: x coordinate of the button
 * @param y: y coordinate of the button
 * @param width: width of the button
 * @param height: height of the button
 * @param label: label of the button
 *
 * ****************************************************************************/
void draw_button(int x, int y, int width, int height, const char *label)
{
    lcd.SetTextColor(LCD_COLOR_BLUE);
    lcd.FillRect(x, y, width, height);
    // lcd.SetTextColor(LCD_COLOR_WHITE);
    lcd.DisplayStringAt(x + width / 2 - strlen(label) * 19, y + height / 2 - 8, (uint8_t *)label, CENTER_MODE);
}

/*******************************************************************************
 *
 * @brief Check if the touch point is inside the button
 * @param touch_x: x coordinate of the touch point
 * @param touch_y: y coordinate of the touch point
 * @param button_x: x coordinate of the button
 * @param button_y: y coordinate of the button
 * @param button_width: width of the button
 * @param button_height: height of the button
 *
 * ****************************************************************************/
bool is_touch_inside_button(int touch_x, int touch_y, int button_x, int button_y, int button_width, int button_height)
{
    return (touch_x >= button_x && touch_x <= button_x + button_width &&
            touch_y >= button_y && touch_y <= button_y + button_height);
}

/*******************************************************************************
 *
 * @brief Calculate the euclidean distance between two vectors
 * @param a: the first vector
 * @param b: the second vector
 * @return the euclidean distance between the two vectors
 *
 * ****************************************************************************/
float euclidean_distance(const std::array<float, 3> &a, const std::array<float, 3> &b)
{
    float sum = 0;
    for (size_t i = 0; i < 3; ++i)
    {
        sum += (a[i] - b[i]) * (a[i] - b[i]);
    }
    return std::sqrt(sum);
}

/*******************************************************************************
 *
 * @brief Calculate the DTW distance between two vectors
 * @param s: the first vector
 * @param t: the second vector
 * @return the DTW distance between the two vectors
 *
 * ****************************************************************************/
float dtw(const std::vector<std::array<float, 3>> &s, const std::vector<std::array<float, 3>> &t)
{
    std::vector<std::vector<float>> dtw_matrix(s.size() + 1, std::vector<float>(t.size() + 1, std::numeric_limits<float>::infinity()));

    dtw_matrix[0][0] = 0;

    for (size_t i = 1; i <= s.size(); ++i)
    {
        for (size_t j = 1; j <= t.size(); ++j)
        {
            float cost = euclidean_distance(s[i - 1], t[j - 1]);
            dtw_matrix[i][j] = cost + std::min({dtw_matrix[i - 1][j], dtw_matrix[i][j - 1], dtw_matrix[i - 1][j - 1]});
        }
    }

    return dtw_matrix[s.size()][t.size()];
}