// Check out the `snippets.txt` file
// for the code snippets to follow along 
// during the live coding session

#include <mbed.h>

int main()
{
  DigitalOut led(LED1);

  while (1)
  {
    led = !led;
    thread_sleep_for(500);
  }
}