#include <mbed.h>

// define the SPI pins connection to the gyroscope
#define SPI_MOSI PF_9
#define SPI_MISO PF_8
#define SPI_SCK PF_7
#define SPI_CS PC_1
// define the interrupt pin connection to the gyroscope
#define INT1 PA_1
// define the FIFO interrupt pin connection to the gyroscope
#define INT2 PA_2
#define OUT_X_L 0x28
// register fields(bits): data_rate(2), Bandwidth(2), Power_down(1), Zen(1), Yen(1), Xen(1)
#define CTRL_REG1 0x20
// configuration: 200Hz ODR,50Hz cutoff, Power on, Z on, Y on, X on
#define CTRL_REG1_CONFIG 0b01'10'1'1'1'1
// register fields(bits): reserved(1), endian-ness(1),Full scale sel(2), reserved(1),self-test(2), SPI mode(1)
#define CTRL_REG4 0x23
// configuration: reserved,little endian,500 dps,reserved,disabled,4-wire mode
#define CTRL_REG4_CONFIG 0b0'0'01'0'00'0
// register fields(bits): I1_Int1 (1), I1_Boot(1), H_Lactive(1), PP_OD(1), I2_DRDY(1), I2_WTM(1), I2_ORun(1), I2_Empty(1)
#define CTRL_REG3 0x22
// configuration: Int1 disabled, Boot status disabled, active high interrupts, push-pull, enable Int2 data ready, disable fifo interrupts
#define CTRL_REG3_CONFIG 0b0'0'0'0'1'000
// define SPI flag
#define SPI_FLAG 1
// define data ready flag
#define DATA_READY_FLAG 2
// define the low pass filter size
#define FILTER_SIZE 10


// initialize the gyroscope SPI communication
SPI gyro(SPI_MOSI, SPI_MISO, SPI_SCK, SPI_CS, use_gpio_ssel);
// initialize the gyroscope fifo interrupt pin
InterruptIn fifo_int(INT2, PullDown);
// initialize read and write buffers
uint8_t read_buf[32];
uint8_t write_buf[32];
// event flags to synchronize the spi callback and the data callback
EventFlags flags; 

// callback functions for the spi and data ready interrupts
void spi_cb(int event)
{
  flags.set(SPI_FLAG);
};
void data_cb()
{
  flags.set(DATA_READY_FLAG);
};

// the filter that removes the noise from the gyroscope data
float moving_average_filter(float new_sample, float *buffer, uint16_t buffer_size)
{
  static uint16_t index = 0;
  static float sum = 0;
  // Remove the oldest sample from the sum
  sum -= buffer[index];
  // Add the new sample to the buffer and to the sum
  buffer[index] = new_sample;
  sum += new_sample;
  // Update the index, wrapping around if necessary
  index = (index + 1) % buffer_size;
  // Calculate and return the average value
  return sum / (float)buffer_size;
};

int main()
{
  // Setup the spi for 8 bit data, high steady state clock,
  // second edge capture, with a 1MHz clock rate
  gyro.format(8, 3);
  gyro.frequency(1'000'000);

  write_buf[0] = CTRL_REG1;
  write_buf[1] = CTRL_REG1_CONFIG;
  gyro.transfer(write_buf, 2, read_buf, 2, spi_cb, SPI_EVENT_COMPLETE);
  flags.wait_all(SPI_FLAG);

  write_buf[0] = CTRL_REG4;
  write_buf[1] = CTRL_REG4_CONFIG;
  gyro.transfer(write_buf, 2, read_buf, 2, spi_cb, SPI_EVENT_COMPLETE);
  flags.wait_all(SPI_FLAG);

  // when the fifo interrupt is triggered, call the data_cb function to set the data ready flag
  fifo_int.rise(&data_cb);

  write_buf[0] = CTRL_REG3;
  write_buf[1] = CTRL_REG3_CONFIG;
  gyro.transfer(write_buf, 2, read_buf, 2, spi_cb, SPI_EVENT_COMPLETE);
  flags.wait_all(SPI_FLAG);

  // enable the fifo interrupt
  if (!(flags.get() & DATA_READY_FLAG) && (fifo_int.read() == 1))
  {
    flags.set(DATA_READY_FLAG);
  }
  
  while (1)
  {
    // initialize the raw and scaled gyroscope data
    int16_t raw_gx, raw_gy, raw_gz;
    float gx, gy, gz;

    // wait until new sample is ready
    flags.wait_all(DATA_READY_FLAG);
    // prepare the write buffer to trigger a sequential read
    write_buf[0] = OUT_X_L | 0x80 | 0x40;

    // start sequential sample reading
    gyro.transfer(write_buf, 7, read_buf, 8, spi_cb, SPI_EVENT_COMPLETE);
    flags.wait_all(SPI_FLAG);

    // read_buf after transfer: garbage byte, gx_low, gx_high, gy_low, gy_high, gz_low, gz_high
    // Put the high and low bytes in the correct order lowB,HighB -> HighB,LowB
    raw_gx = (((uint16_t)read_buf[2]) << 8) | ((uint16_t)read_buf[1]);
    raw_gy = (((uint16_t)read_buf[4]) << 8) | ((uint16_t)read_buf[3]);
    raw_gz = (((uint16_t)read_buf[6]) << 8) | ((uint16_t)read_buf[5]);

    gx = ((float)raw_gx) * (17.5f * 0.017453292519943295769236907684886f / 1000.0f);
    gy = ((float)raw_gy) * (17.5f * 0.017453292519943295769236907684886f / 1000.0f);
    gz = ((float)raw_gz) * (17.5f * 0.017453292519943295769236907684886f / 1000.0f);
    
    // print the actual gyroscope data
    printf("Actual|\t gx: %4.5f \t gy: %4.5f \t gz: %4.5f\n", gx, gy, gz);
  }
}