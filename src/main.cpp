// Check out the `snippets.txt` file
// for the code snippets to follow along 
// during the live coding session

#include <mbed.h>

volatile float read_value;
volatile float voltage;

AnalogIn ain(PA_6);
Timeout t_out;
volatile bool sample_ready=false;
void cb(){
  sample_ready=true;
}
int main(){
  t_out.attach(cb,500ms);
  while(1){
    while(!sample_ready){
      
    }
    sample_ready=false;
    t_out.attach(cb,500ms);
    read_value=ain.read();
    voltage=read_value*3.3f;
    printf("read value: %f  read voltage: %f V\n",read_value,voltage);
  }
}