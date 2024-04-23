#include <Arduino.h>

#include <Keypad.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define TIME_TO_EXPLODE 40

const byte ROWS = 4;
const byte COLS = 3;
char keys[ROWS][COLS] = {
    {'1', '2', '3'},
    {'4', '5', '6'},
    {'7', '8', '9'},
    {'*', '0', '#'}};

// byte rowPins[ROWS] = {5, 6, 7, 8};
// byte colPins[COLS] = {2, 3, 4};
// X C2 R1 C1 C4 C3 R3 R2 X
byte rowPins[ROWS] = {3, 8, 7, 5};
byte colPins[COLS] = {4, 2, 6};

// X C2 R1 C1 C4
// X R2 R3 C3

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

LiquidCrystal_I2C lcd(0x27, 16, 2);

char code[] = "7355608";
char enteredCode[9] = "*******";
char secondCode[9] = "*******";

int brightness = 0; // Initial brightness
int fadeAmount = 3; // Fade amount (adjust to change the speed)

const int greenLed = 9;
const int buzzerPin = 10;
const int redLed = 11;

unsigned long previousMillis = 0;

void setup()
{
  Serial.begin(9600);
  lcd.init();
  lcd.backlight();

  pinMode(greenLed, OUTPUT);
  pinMode(redLed, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
}

void print_timer()
{
}

enum ProgramState
{
  STATE_PREPLANT,
  STATE_PLANTING,
  STATE_PLANT_SETUP,
  STATE_PLANTED,
  STATE_DEFUSE_ATEMPT,
  STATE_DEFUSED,
  STATE_EXPLODED,
};

void write_init_screen()
{
  lcd.clear();
  lcd.setCursor(4, 0);
  lcd.print(enteredCode);
}

void breathEffect()
{
  analogWrite(greenLed, brightness); // Set the brightness of the LED

  brightness = brightness + fadeAmount; // Increase brightness

  // Reverse the direction of the fade when brightness reaches 0 or 255
  if (brightness <= 0 || brightness >= 180)
  {
    fadeAmount = -fadeAmount;
  }

  delay(30); // Wait for 30 milliseconds to see the dimming effect
}

void show_timer(int time)
{
  static int last_time{0};
  if (last_time == time)
  {
    return;
  }
  lcd.setCursor(4, 0);
  lcd.print("Timer: ");

  if (time < 10)
  {
    lcd.print("0");
  }
  lcd.print(time);
  last_time = time;
}

void blink_buz(int time)
{
  static bool ledState = LOW;
  static unsigned long blinkInterval = 1000; // Initial blink interval

  if (time <= 0)
  {
    blinkInterval = 1000;
  }
  Serial.println(blinkInterval);

  blinkInterval = ((float)(time) / TIME_TO_EXPLODE) * ((float)(time + (time > 1 ? (TIME_TO_EXPLODE - time) : 1)) / TIME_TO_EXPLODE) * 1000 + TIME_TO_EXPLODE;

  // Blink LED and beep buzzer based on the adjusted blink interval
  if (millis() - previousMillis >= blinkInterval)
  {
    previousMillis = millis();
    ledState = !ledState;
    digitalWrite(redLed, ledState);

    if (ledState == HIGH)
    {
      // Beep the buzzer with a frequency that corresponds to the blink interval
      tone(buzzerPin, 1049, blinkInterval / 2);
    }
  }

  if (blinkInterval <= 55)
  {
    blinkInterval = 20;
  }
}

void loop()
{
  char key;
  unsigned i;
  static bool flag_show_timer{0};
  static short enter_code_len{0};
  static unsigned long entered_time{0}, elapsed_time{0};
  // static ProgramState state{ STATE_PLANT_SETUP };
  static ProgramState state{STATE_PREPLANT};

  if (flag_show_timer == false)
    breathEffect();

  switch (state)
  {
  case STATE_PREPLANT:

    for (i = 0; i < 7; i++)
    {
      enteredCode[i] = '*';
    }
    write_init_screen();
    state = STATE_PLANTING;
    break;
  case STATE_PLANTING:
    key = keypad.getKey();
    if (!key)
      break;
    lcd.setCursor(4, 0);
    enteredCode[enter_code_len] = key;
    enter_code_len++;
    lcd.print(enteredCode);
    if (enter_code_len == 7)
    {
      state = strcmp(enteredCode, code) == 0 ? STATE_PLANT_SETUP : STATE_PREPLANT;
      enter_code_len = 0;
    }
    break;
  case STATE_PLANT_SETUP:
    entered_time = millis();
    flag_show_timer = true;
    state = STATE_PLANTED;
    break;
  case STATE_PLANTED:

    if (flag_show_timer == true)
    {
      analogWrite(greenLed, LOW);
    }

    for (i = 0; i < 7; i++)
    {
      secondCode[i] = '*';
    }
    lcd.setCursor(4, 1);
    lcd.print(secondCode);
    state = STATE_DEFUSE_ATEMPT;
    // Turn off the greenLed
    analogWrite(greenLed, 0);
    break;
  case STATE_DEFUSE_ATEMPT:

    elapsed_time = TIME_TO_EXPLODE - (millis() - entered_time) / 1000;
    blink_buz(elapsed_time);
    show_timer(elapsed_time);
    key = keypad.getKey();

    if (elapsed_time <= 0)
    {
      state = STATE_EXPLODED;
    }
    if (!key)
      break;
    secondCode[enter_code_len] = key;
    enter_code_len++;
    lcd.setCursor(4, 1);
    lcd.print(secondCode);
    if (enter_code_len == 7)
    {
      state = strcmp(secondCode, code) == 0 ? STATE_DEFUSED : STATE_PLANTED;
      enter_code_len = 0;
    }
    break;
  case STATE_EXPLODED:
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Terrorists win!");
    delay(2000);
    digitalWrite(redLed, LOW);
    flag_show_timer = false;
    state = STATE_PREPLANT;
    break;
  case STATE_DEFUSED:
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("CT win!");
    delay(2000);
    digitalWrite(redLed, LOW);
    flag_show_timer = false;
    state = STATE_PREPLANT;
    break;

  default:
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("?>>> win!");
    delay(2000);
    flag_show_timer = false;
    break;
  }
}
