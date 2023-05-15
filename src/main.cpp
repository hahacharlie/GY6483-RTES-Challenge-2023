#include <mbed.h>
#include <vector>
#include <array>
#include <limits>
#include <cmath>
#include <math.h>
#include "gyro.h"
#include "drivers/LCD_DISCO_F429ZI.h"
#include "drivers/TS_DISCO_F429ZI.h"

// Event flags
#define KEY_FLAG 1
#define UNLOCK_FLAG 2
#define ERASE_FLAG 4

#define DATA_READY_FLAG 8

#define FONT_SIZE 16

// the unlocking threshold
#define CORRELATION_THRESHOLD 0.3f

InterruptIn gyro_int2(PA_2, PullDown);
InterruptIn user_button(USER_BUTTON, PullDown);

DigitalOut green_led(LED1);
DigitalOut red_led(LED2);

LCD_DISCO_F429ZI lcd;
TS_DISCO_F429ZI ts;

EventFlags flags;

Timer timer;

void draw_button(int x, int y, int width, int height, const char *label);
bool is_touch_inside_button(int touch_x, int touch_y, int button_x, int button_y, int button_width, int button_height);
float euclidean_distance(const array<float, 3> &a, const array<float, 3> &b);
float dtw(const vector<array<float, 3>> &s, const vector<array<float, 3>> &t);
void trim_gyro_data(vector<array<float, 3>> &data);
float correlation(const vector<float> &a, const vector<float> &b);
array<float, 3> calculateCorrelationVectors(vector<array<float, 3>>& vec1, vector<array<float, 3>>& vec2);
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
float movingAverageFilter(float input, float display_buffer[], size_t N, size_t &index, float &sum);

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
const int button2_y = 180;
const int button2_width = 120;
const int button2_height = 50;
const char *button2_label = "UNLOCK";
const int message_x = 5;
const int message_y = 30;
const char *message = "GESTURE UNLOCKER";
const int text_x = 5;
const int text_y = 270;
const char *text_0 = "NO KEY RECORDED";
const char *text_1 = "LOCKED";
int err = 0;

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

    // Display the welcome message
    lcd.DisplayStringAt(message_x, message_y, (uint8_t *)message, CENTER_MODE);

    // initialize all interrupts
    user_button.rise(&button_press);
    gyro_int2.rise(&onGyroDataReady);

    // initialize LEDs
    if (gesture_key.empty())
    {
        red_led = 0;
        green_led = 1;
        lcd.DisplayStringAt(text_x, text_y, (uint8_t *)text_0, CENTER_MODE);
    }
    else
    {
        red_led = 1;
        green_led = 0;
        lcd.DisplayStringAt(text_x, text_y, (uint8_t *)text_1, CENTER_MODE);
    }

    // Create the gyroscope thread
    Thread key_saving;
    key_saving.start(callback(gyroscope_thread));
    // printf("Gesture Key recording started\r\n");
    // Create the touch screen thread
    Thread touch_thread;
    touch_thread.start(callback(touch_screen_thread));
    // printf("Touch screen thread started\r\n");

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

    // initialize a string display_buffer that can be draw on the LCD to dispaly the status
    char display_buffer[50];

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
            sprintf(display_buffer, "Erasing....");
            lcd.SetTextColor(LCD_COLOR_BLACK);                  // Set the color to the background color
            lcd.FillRect(0, text_y, lcd.GetXSize(), FONT_SIZE); // Clear a specific line
            lcd.SetTextColor(LCD_COLOR_BLUE);                   // Reset the text color
            lcd.DisplayStringAt(text_x, text_y, (uint8_t *)display_buffer, CENTER_MODE);
            gesture_key.clear();
            sprintf(display_buffer, "Key Erasing finish.");
            lcd.SetTextColor(LCD_COLOR_BLACK);                  // Set the color to the background color
            lcd.FillRect(0, text_y, lcd.GetXSize(), FONT_SIZE); // Clear a specific line
            lcd.SetTextColor(LCD_COLOR_BLUE);                   // Reset the text color
            lcd.DisplayStringAt(text_x, text_y, (uint8_t *)display_buffer, CENTER_MODE);
            unlocking_record.clear();
            sprintf(display_buffer, "All Erasing finish.");
            lcd.SetTextColor(LCD_COLOR_BLACK);                  // Set the color to the background color
            lcd.FillRect(0, text_y, lcd.GetXSize(), FONT_SIZE); // Clear a specific line
            lcd.SetTextColor(LCD_COLOR_BLUE);                   // Reset the text color
            lcd.DisplayStringAt(text_x, text_y, (uint8_t *)display_buffer, CENTER_MODE);
            green_led = 1;
            red_led = 0;
        }

        if (flag_check & (KEY_FLAG | UNLOCK_FLAG))
        {
            sprintf(display_buffer, "Hold On");
            lcd.SetTextColor(LCD_COLOR_BLACK);                  // Set the color to the background color
            lcd.FillRect(0, text_y, lcd.GetXSize(), FONT_SIZE); // Clear a specific line
            lcd.SetTextColor(LCD_COLOR_BLUE);                   // Reset the text color
            lcd.DisplayStringAt(text_x, text_y, (uint8_t *)display_buffer, CENTER_MODE);
            ThisThread::sleep_for(1s);
            sprintf(display_buffer, "Calibrating...");
            lcd.SetTextColor(LCD_COLOR_BLACK);                  // Set the color to the background color
            lcd.FillRect(0, text_y, lcd.GetXSize(), FONT_SIZE); // Clear a specific line
            lcd.SetTextColor(LCD_COLOR_BLUE);                   // Reset the text color
            lcd.DisplayStringAt(text_x, text_y, (uint8_t *)display_buffer, CENTER_MODE);
            // Initiate gyroscope
            InitiateGyroscope(&init_parameters, &raw_data);
            // start recording gesture
            sprintf(display_buffer, "Recording in 3...");
            lcd.SetTextColor(LCD_COLOR_BLACK);                  // Set the color to the background color
            lcd.FillRect(0, text_y, lcd.GetXSize(), FONT_SIZE); // Clear a specific line
            lcd.SetTextColor(LCD_COLOR_BLUE);                   // Reset the text color
            lcd.DisplayStringAt(text_x, text_y, (uint8_t *)display_buffer, CENTER_MODE);
            ThisThread::sleep_for(1s);
            sprintf(display_buffer, "Recording in 2...");
            lcd.SetTextColor(LCD_COLOR_BLACK);                  // Set the color to the background color
            lcd.FillRect(0, text_y, lcd.GetXSize(), FONT_SIZE); // Clear a specific line
            lcd.SetTextColor(LCD_COLOR_BLUE);                   // Reset the text color
            lcd.DisplayStringAt(text_x, text_y, (uint8_t *)display_buffer, CENTER_MODE);
            ThisThread::sleep_for(1s);
            sprintf(display_buffer, "Recording in 1...");
            lcd.SetTextColor(LCD_COLOR_BLACK);                  // Set the color to the background color
            lcd.FillRect(0, text_y, lcd.GetXSize(), FONT_SIZE); // Clear a specific line
            lcd.SetTextColor(LCD_COLOR_BLUE);                   // Reset the text color
            lcd.DisplayStringAt(text_x, text_y, (uint8_t *)display_buffer, CENTER_MODE);
            ThisThread::sleep_for(1s);
            sprintf(display_buffer, "Recording...");
            lcd.SetTextColor(LCD_COLOR_BLACK);                  // Set the color to the background color
            lcd.FillRect(0, text_y, lcd.GetXSize(), FONT_SIZE); // Clear a specific line
            lcd.SetTextColor(LCD_COLOR_BLUE);                   // Reset the text color
            lcd.DisplayStringAt(text_x, text_y, (uint8_t *)display_buffer, CENTER_MODE);
            timer.start();
            while (timer.elapsed_time() < 5s)
            {
                // Wait for the gyroscope data to be ready
                flags.wait_all(DATA_READY_FLAG);
                // Read the data from the gyroscope
                GetCalibratedRawData();
                // Add the converted data to the gesture_key vector
                temp_key.push_back({ConvertToDPS(raw_data.x_raw), ConvertToDPS(raw_data.y_raw), ConvertToDPS(raw_data.z_raw)});
                ThisThread::sleep_for(50ms); // 20Hz
            }
            timer.stop();  // Stop timer
            timer.reset(); // Reset timer

            // trim zeros
            trim_gyro_data(temp_key);

            sprintf(display_buffer, "Finished...");
            lcd.SetTextColor(LCD_COLOR_BLACK);                  // Set the color to the background color
            lcd.FillRect(0, text_y, lcd.GetXSize(), FONT_SIZE); // Clear a specific line
            lcd.SetTextColor(LCD_COLOR_BLUE);                   // Reset the text color
            lcd.DisplayStringAt(text_x, text_y, (uint8_t *)display_buffer, CENTER_MODE);
        }

        // check the flag see if it is recording or unlocking
        if (flag_check & KEY_FLAG)
        {
            if (gesture_key.empty())
            {
                sprintf(display_buffer, "Saving Key...");
                lcd.SetTextColor(LCD_COLOR_BLACK);                  // Set the color to the background color
                lcd.FillRect(0, text_y, lcd.GetXSize(), FONT_SIZE); // Clear a specific line
                lcd.SetTextColor(LCD_COLOR_BLUE);                   // Reset the text color
                lcd.DisplayStringAt(text_x, text_y, (uint8_t *)display_buffer, CENTER_MODE);

                gesture_key = temp_key;

                temp_key.clear();
                red_led = 1;
                green_led = 0;

                sprintf(display_buffer, "Key saved...");
                lcd.SetTextColor(LCD_COLOR_BLACK);                  // Set the color to the background color
                lcd.FillRect(0, text_y, lcd.GetXSize(), FONT_SIZE); // Clear a specific line
                lcd.SetTextColor(LCD_COLOR_BLUE);                   // Reset the text color
                lcd.DisplayStringAt(text_x, text_y, (uint8_t *)display_buffer, CENTER_MODE);
            }
            else
            {
                sprintf(display_buffer, "Removing old key...");
                lcd.SetTextColor(LCD_COLOR_BLACK);                  // Set the color to the background color
                lcd.FillRect(0, text_y, lcd.GetXSize(), FONT_SIZE); // Clear a specific line
                lcd.SetTextColor(LCD_COLOR_BLUE);                   // Reset the text color
                lcd.DisplayStringAt(text_x, text_y, (uint8_t *)display_buffer, CENTER_MODE);
                ThisThread::sleep_for(1s);
                gesture_key.clear();
                gesture_key = temp_key;
                sprintf(display_buffer, "New key is saved.");
                lcd.SetTextColor(LCD_COLOR_BLACK);                  // Set the color to the background color
                lcd.FillRect(0, text_y, lcd.GetXSize(), FONT_SIZE); // Clear a specific line
                lcd.SetTextColor(LCD_COLOR_BLUE);                   // Reset the text color
                lcd.DisplayStringAt(text_x, text_y, (uint8_t *)display_buffer, CENTER_MODE);
                temp_key.clear();
                red_led = 1;
                green_led = 0;
            }
        }
        else if (flag_check & UNLOCK_FLAG)
        {
            flags.clear(UNLOCK_FLAG);
            sprintf(display_buffer, "Unlocking...");
            lcd.SetTextColor(LCD_COLOR_BLACK);                  // Set the color to the background color
            lcd.FillRect(0, text_y, lcd.GetXSize(), FONT_SIZE); // Clear a specific line
            lcd.SetTextColor(LCD_COLOR_BLUE);                   // Reset the text color
            lcd.DisplayStringAt(text_x, text_y, (uint8_t *)display_buffer, CENTER_MODE);
            unlocking_record = temp_key;
            temp_key.clear();

            // check if the gesture key is empty
            if (gesture_key.empty())
            {
                sprintf(display_buffer, "NO KEY SAVED.");
                lcd.SetTextColor(LCD_COLOR_BLACK);                  // Set the color to the background color
                lcd.FillRect(0, text_y, lcd.GetXSize(), FONT_SIZE); // Clear a specific line
                lcd.SetTextColor(LCD_COLOR_BLUE);                   // Reset the text color
                lcd.DisplayStringAt(text_x, text_y, (uint8_t *)display_buffer, CENTER_MODE);
                unlocking_record.clear();
                green_led = 1;
                red_led = 0;
            }
            else
            {
                // calculate the similarity of the gesture key and the unlocking record
                int unlock = 0;

                array<float, 3> correlationResult = calculateCorrelationVectors(gesture_key, unlocking_record);

                if (err != 0)
                {
                    printf("Error calculating correlation: vectors have different sizes\n");
                }
                else
                {
                    printf("Correlation values: x = %f, y = %f, z = %f\n", correlationResult[0], correlationResult[1], correlationResult[2]);
                    // iterate through correlationResult to check if any of the values are below the threshold
                    for (size_t i = 0; i < correlationResult.size(); i++)
                    {
                        if (correlationResult[i] > CORRELATION_THRESHOLD)
                        {
                            unlock++;
                        }
                    }
                }

                // sprintf(display_buffer, "Similarity: %f", similarity);
                // lcd.SetTextColor(LCD_COLOR_BLACK);                  // Set the color to the background color
                // lcd.FillRect(0, text_y, lcd.GetXSize(), FONT_SIZE); // Clear a specific line
                // lcd.SetTextColor(LCD_COLOR_BLUE);                   // Reset the text color
                // lcd.DisplayStringAt(text_x, text_y, (uint8_t *)display_buffer, CENTER_MODE);

                if (unlock==3) // TODO: need to find a better threshold
                {
                    sprintf(display_buffer, "UNLOCK: SUCCESS");
                    lcd.SetTextColor(LCD_COLOR_BLACK);                  // Set the color to the background color
                    lcd.FillRect(0, text_y, lcd.GetXSize(), FONT_SIZE); // Clear a specific line
                    lcd.SetTextColor(LCD_COLOR_BLUE);                   // Reset the text color
                    lcd.DisplayStringAt(text_x, text_y, (uint8_t *)display_buffer, CENTER_MODE);
                    
                    green_led = 1;
                    red_led = 0;

                    // ThisThread::sleep_for(3s);

                    // sprintf(display_buffer, "R:x=%3fy=%3fz=%3f", correlationResult[0], correlationResult[1], correlationResult[2]);
                    // lcd.SetTextColor(LCD_COLOR_BLACK);                  // Set the color to the background color
                    // lcd.FillRect(0, text_y, lcd.GetXSize(), FONT_SIZE); // Clear a specific line
                    // lcd.SetTextColor(LCD_COLOR_BLUE);                   // Reset the text color
                    // lcd.DisplayStringAt(text_x, text_y, (uint8_t *)display_buffer, CENTER_MODE);

                    unlocking_record.clear();
                    unlock = 0;
                }
                else
                {
                    sprintf(display_buffer, "UNLOCK: FAILED");
                    lcd.SetTextColor(LCD_COLOR_BLACK);                  // Set the color to the background color
                    lcd.FillRect(0, text_y, lcd.GetXSize(), FONT_SIZE); // Clear a specific line
                    lcd.SetTextColor(LCD_COLOR_BLUE);                   // Reset the text color
                    lcd.DisplayStringAt(text_x, text_y, (uint8_t *)display_buffer, CENTER_MODE);

                    green_led = 0;
                    red_led = 1;

                    // ThisThread::sleep_for(3s);

                    // sprintf(display_buffer, "R:x=%3fy=%3fz=%3f", correlationResult[0], correlationResult[1], correlationResult[2]);
                    // lcd.SetTextColor(LCD_COLOR_BLACK);                  // Set the color to the background color
                    // lcd.FillRect(0, text_y, lcd.GetXSize(), FONT_SIZE); // Clear a specific line
                    // lcd.SetTextColor(LCD_COLOR_BLUE);                   // Reset the text color
                    // lcd.DisplayStringAt(text_x, text_y, (uint8_t *)display_buffer, CENTER_MODE);

                    unlocking_record.clear();
                    unlock = 0;
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

    // initialize a string display_buffer that can be draw on the LCD to dispaly the status
    char display_buffer[50];

    while (1)
    {
        ts.GetState(&ts_state);
        if (ts_state.TouchDetected)
        {
            int touch_x = ts_state.X;
            int touch_y = ts_state.Y;

            // Check if the touch is inside record button
            if (is_touch_inside_button(touch_x, touch_y, button2_x, button2_y, button1_width, button1_height))
            {
                sprintf(display_buffer, "Recording Initiated...");
                lcd.SetTextColor(LCD_COLOR_BLACK);                  // Set the color to the background color
                lcd.FillRect(0, text_y, lcd.GetXSize(), FONT_SIZE); // Clear a specific line
                lcd.SetTextColor(LCD_COLOR_BLUE);                   // Reset the text color
                lcd.DisplayStringAt(text_x, text_y, (uint8_t *)display_buffer, CENTER_MODE);
                ThisThread::sleep_for(1s);
                flags.set(KEY_FLAG);
            }

            // Check if the touch is inside unlock button
            if (is_touch_inside_button(touch_x, touch_y, button1_x, button1_y, button2_width, button2_height))
            {
                sprintf(display_buffer, "Unlocking Initiated...");
                lcd.SetTextColor(LCD_COLOR_BLACK);                  // Set the color to the background color
                lcd.FillRect(0, text_y, lcd.GetXSize(), FONT_SIZE); // Clear a specific line
                lcd.SetTextColor(LCD_COLOR_BLUE);                   // Reset the text color
                lcd.DisplayStringAt(text_x, text_y, (uint8_t *)display_buffer, CENTER_MODE);
                ThisThread::sleep_for(1s);
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
float euclidean_distance(const array<float, 3> &a, const array<float, 3> &b)
{
    float sum = 0;
    for (size_t i = 0; i < 3; ++i)
    {
        sum += (a[i] - b[i]) * (a[i] - b[i]);
    }
    return sqrt(sum);
}

/*******************************************************************************
 *
 * @brief Calculate the DTW distance between two vectors
 * @param s: the first vectorS
 * @param t: the second vector
 * @return the DTW distance between the two vectors
 *
 * ****************************************************************************/
float dtw(const vector<array<float, 3>> &s, const vector<array<float, 3>> &t)
{
    vector<vector<float>> dtw_matrix(s.size() + 1, vector<float>(t.size() + 1, numeric_limits<float>::infinity()));

    dtw_matrix[0][0] = 0;

    for (size_t i = 1; i <= s.size(); ++i)
    {
        for (size_t j = 1; j <= t.size(); ++j)
        {
            float cost = euclidean_distance(s[i - 1], t[j - 1]);
            dtw_matrix[i][j] = cost + min({dtw_matrix[i - 1][j], dtw_matrix[i][j - 1], dtw_matrix[i - 1][j - 1]});
        }
    }

    return dtw_matrix[s.size()][t.size()];
}

/*******************************************************************************
 *
 * @brief Trim the gyro data
 * @param data: the gyro data to trim
 *
 * ****************************************************************************/
void trim_gyro_data(vector<array<float, 3>> &data)
{
    float threshold = 0.00001;
    auto ptr = data.begin();
    // find the first element where data from any
    // one direction is larger than the threshold
    while (abs((*ptr)[0]) <= threshold && abs((*ptr)[1]) <= threshold && abs((*ptr)[2]) <= threshold)
    {
        ptr++;
    }
    if (ptr == data.end())
        return;      // all data less than threshold
    auto lptr = ptr; // record the left bound
    // start searching from end to front
    ptr = data.end() - 1;
    while (abs((*ptr)[0]) <= threshold && abs((*ptr)[1]) <= threshold && abs((*ptr)[2]) <= threshold)
    {
        ptr--;
    }
    auto rptr = ptr; // record the right bound
    // start moving elements to the front
    auto replace_ptr = data.begin();
    for (; replace_ptr != lptr && lptr <= rptr; replace_ptr++, lptr++)
    {
        *replace_ptr = *lptr;
    }
    // trim the end
    if (lptr > rptr)
    {
        data.erase(replace_ptr, data.end());
    }
    else
    {
        data.erase(rptr + 1, data.end());
    }
}

/*******************************************************************************
 *
 * @brief Calculate the correlation between two vectors
 * @param a: the first vector
 * @param b: the second vector
 * @return the correlation between the two vectors
 *
 * ****************************************************************************/
float correlation(const vector<float> &a, const vector<float> &b)
{
    if (a.size() != b.size())
    {
        err = -1;
        return 0.0f;
    }

    float sum_a = 0, sum_b = 0, sum_ab = 0, sq_sum_a = 0, sq_sum_b = 0;
    for (size_t i = 0; i < a.size(); ++i)
    {
        sum_a += a[i];
        sum_b += b[i];
        sum_ab += a[i] * b[i];
        sq_sum_a += a[i] * a[i];
        sq_sum_b += b[i] * b[i];
    }

    size_t n = a.size();
    float numerator = n * sum_ab - sum_a * sum_b;
    float denominator = sqrt((n * sq_sum_a - sum_a * sum_a) * (n * sq_sum_b - sum_b * sum_b));
    return numerator / denominator;
}

/*******************************************************************************
 *
 * @brief Calculate the correlation between two vectors
 * @param a: the first vector
 * @param b: the second vector
 * @return the correlation between the two vectors
 *
 * ****************************************************************************/
array<float, 3> calculateCorrelationVectors(vector<array<float, 3>>& vec1, vector<array<float, 3>>& vec2) {
    array<float, 3> result;

    // Calculate the correlation for each coordinate
    for (int i = 0; i < 3; i++) {
        vector<float> a;
        vector<float> b;

        // Populate 'a' and 'b' with the ith coordinates of vec1 and vec2
        for (const auto& arr : vec1) {
            a.push_back(arr[i]);
        }
        for (const auto& arr : vec2) {
            b.push_back(arr[i]);
        }

        // Resize 'a' to match the size of 'b', if necessary
        if (a.size() > b.size()) {
            a.resize(b.size(), 0);
        } else if (b.size() > a.size()) {
            b.resize(a.size(), 0);
        }

        // Calculate the correlation and store the result
        result[i] = correlation(a, b);
    }

    return result;
}
