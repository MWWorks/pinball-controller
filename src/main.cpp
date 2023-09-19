#include <Arduino.h>

//note we are using hid-project for this rather than keyboard & joystick libraries:
//stock keyboard library does not support all keys, eg volume keys
//and hid project does not play well with the others - had issues not being able to upload - they need to be removed

#include <Bounce2.h>
#include <HID-Project.h>
#include <MPU6050_tockn.h>
#include <Wire.h>
#include <RotaryEncoder.h>


#define LED_BUILTIN 17
#define BUTTON_COUNT 11
// #define CALIBRATE
// #define GAMEPAD

//variables for buttons; help from
//https://stackoverflow.com/questions/33507329/arduino-keystrokes-via-push-buttons-how-to-debounce-when-to-use-matrix
Bounce debouncer[BUTTON_COUNT];
bool button_prev_pressed[BUTTON_COUNT];
KeyboardKeycode button_keys[BUTTON_COUNT] = {
    KEY_LEFT_SHIFT, KEY_RIGHT_SHIFT, KEY_LEFT_CTRL, KEY_RIGHT_CTRL, KEY_1, KEY_5, KEY_RETURN, KEY_ESC, KEY_RESERVED, KEY_RESERVED, KEY_1
};

//note the interface is wired as
//21 20 19 18 15 14 16 10 - main buttons
// 8 9 - extra two
// 4 - encoder click

int  button_pins[BUTTON_COUNT] = {
    21, 20, 19, 18, 15, 14, 16, 10, 8, 9, 4
};

#ifdef GAMEPAD
//variables for joystick;
MPU6050 mpu6050(Wire);
int gamepad_x_last = 0;
int gamepad_x_min_lo = 1200; 
int gamepad_x_min_hi = 3300; 
int gamepad_y_last = 0;
int gamepad_y_min_lo = 700; 
int gamepad_y_min_hi = 1400; 
#endif

//encoder
RotaryEncoder encoder(0, 1, RotaryEncoder::LatchMode::FOUR3);


void setup()
{
  Serial.begin(115200);
  Serial.println("Start");
  // pinMode(LED_BUILTIN, OUTPUT);

  //set up buttons
  pinMode(LED_BUILTIN_TX,INPUT); //stop lighting  up on every tx!
  Keyboard.begin();
  Consumer.begin(); //for media keys
  for (int i = 0; i < BUTTON_COUNT; i++){
    button_prev_pressed[i] = false;
    pinMode(button_pins[i],INPUT_PULLUP);
    debouncer[i].attach(button_pins[i]);
    debouncer[i].interval(10);
  }

  //set up joystick/accelerometer
  #ifdef GAMEPAD
  Wire.begin();
  Gamepad.begin();
  mpu6050.begin();
  // mpu6050.calcGyroOffsets(true);
  #endif

}

void loop()
{

  //poll buttons and remember previous state
  for (int i = 0; i < BUTTON_COUNT; i++){
    if(button_keys[i] != KEY_RESERVED){
      debouncer[i].update();
      bool button_current_pressed = !debouncer[i].read();
      if(button_current_pressed != button_prev_pressed[i]){
        if(button_current_pressed){

          if(i == 10){ //nasty hack here! should build this in but i want the wheel button to do win-1 to launch py
            Keyboard.press(KEY_LEFT_GUI);            
          }
          Keyboard.press(button_keys[i]);

        }else{

          Keyboard.release(button_keys[i]);
          if(i == 10){
            Keyboard.release(KEY_LEFT_GUI);            
          }

          //temp hack here to register joystick for testing
          if(i == 7){
            Gamepad.press(1);
            Gamepad.write();
            delay(100);
            Gamepad.release(1);
            Gamepad.write();
          }
        }
        button_prev_pressed[i] = button_current_pressed;
      }

    }
  }

  //now poll accelerometer - the limits the library seems to provide are float +-2
  //cheap hack is to multiply these by 1000 to get numbers we can use with map
  #ifdef GAMEPAD
  mpu6050.update();
  int gamepad_x = map(mpu6050.getAccX()*1000, -2000 , 2000, -32768, 32767);
  int gamepad_y = map(mpu6050.getAccY()*1000, -2000 , 2000, -32768, 32767);
  #endif

  #if defined(CALIBRATE) && defined(GAMEPAD)
    //keep track of the lows and highs for calibration
    static int gamepad_x_l = 32767;
    static int gamepad_x_h = -32768;
    if(gamepad_x<gamepad_x_l){
      gamepad_x_l = gamepad_x;
    }
    if(gamepad_x>gamepad_x_h){
      gamepad_x_h = gamepad_x;
    }
    static int gamepad_y_l = 32767;
    static int gamepad_y_h = -32768;
    if(gamepad_y<gamepad_y_l){
      gamepad_y_l = gamepad_y;
    }
    if(gamepad_y>gamepad_y_h){
      gamepad_y_h = gamepad_y;
    }
    Serial.print(gamepad_x_l); 
    Serial.print(" "); 
    Serial.print(gamepad_x_h); 
    Serial.print(" "); 
    Serial.print(gamepad_y_l); 
    Serial.print(" "); 
    Serial.println(gamepad_y_h); 
  #endif

  #ifdef GAMEPAD
  if(gamepad_x<gamepad_x_min_lo){
    gamepad_x = map(gamepad_x, -32768, gamepad_x_min_lo, -32768, 0);
  }else if(gamepad_x>gamepad_x_min_hi){
    gamepad_x = map(gamepad_x, 0, 32767, gamepad_x_min_lo, 32767);
  }else{
    gamepad_x = 0;
  }
  if(gamepad_y<gamepad_y_min_lo){
    gamepad_y = map(gamepad_y, -32768, gamepad_y_min_lo, -32768, 0);
  }else if(gamepad_y>gamepad_y_min_hi){
    gamepad_y = map(gamepad_y, 0, 32767, gamepad_y_min_lo, 32767);
  }else{
    gamepad_y = 0;
  }
  if((gamepad_x != gamepad_x_last) || (gamepad_y != gamepad_y_last)){
    Gamepad.xAxis(gamepad_x);
    Gamepad.yAxis(gamepad_y);
    // Gamepad.zAxis(map(mpu6050.getAccZ()*1000, -2000 , 2000, -128, 127));
    Gamepad.write();
    gamepad_x_last = gamepad_x;
    gamepad_y_last = gamepad_y;
  }
  #endif

  //and poll the encoder - note this uses consumer.write rather than keyboard - seems required for media keys
  static int encoder_last_pos = 0;
  encoder.tick();
  int encoder_new_pos = encoder.getPosition();
  if (encoder_last_pos != encoder_new_pos) {
    if(encoder.getDirection()==RotaryEncoder::Direction::CLOCKWISE){
      Consumer.write(MEDIA_VOLUME_UP);
    }else{
      Consumer.write(MEDIA_VOLUME_DOWN);
    }
    encoder_last_pos = encoder_new_pos;
  }


}
