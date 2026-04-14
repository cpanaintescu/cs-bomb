#include <Arduino.h>
#include <Keypad.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <RTClib.h>
#include <mp3tf16p.h>
#include <PCF8574.h>

#define TIME_TO_EXPLODE 40

PCF8574 pcf(0x20);

const byte ROWS = 4;
const byte COLS = 3;
char keys[ROWS][COLS] = 
{
    {'1', '2', '3'},
    {'4', '5', '6'},
    {'7', '8', '9'},
    {'*', '0', '#'}
};

// Define the connections to the keypad
byte rowPins[4] = {1,6,5,3};
byte colPins[3] = {2,0,4};


Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

LiquidCrystal_I2C lcd(0x27, 16, 2);

char code[] = "7355608";
char enteredCode[9] = "*******";
char secondCode[9] = "*******";

int brightness = 0; // Initial brightness
int fadeAmount = 3; // Fade amount (adjust to change the speed)

MP3Player mp3(10, 11);

const int buzzerPin = 13;
const int redLed = 12;
const int greenLed = 9;

unsigned long previousMillis = 0;

////////RTC SETUP//////////////
RTC_PCF8563 rtc;

// Initialize the RTC
const long interval = 1000; // interval at which to update (milliseconds)

void setup() 
{
    // Start the I2C communication
    Wire.begin();

    // Initialize the Serial communication for debugging
    Serial.begin(9600);
    mp3.initialize();

    
    while (!Serial) 
    {
        // Wait for Serial to be ready (for Leonardo and other boards)
    }


    // Initialize the RTC

    
    if (!rtc.begin()) 
    {
        Serial.println("Couldn't find RTC");
        while (1); // Loop forever if RTC is not found

    }
    

    // Uncomment this section to manually set the RTC time
    /*
    Serial.println("Enter the current date and time in the format YYYY-MM-DDTHH:MM:SS");
    while(!Serial.available())
    {
        Serial.read();
    }

    String dateTimeString = Serial.readStringUntil('\n');
    DateTime dt = DateTime(dateTimeString.c_str());
    
    rtc.adjust(dt);
    Serial.println("RTC time set!");
    */

    // Initialize the LCD
    lcd.init(); // Use .init() instead of .begin()
    lcd.backlight(); // Turn on the backlight

    // Initial message on the LCD
    lcd.setCursor(0, 0);
    lcd.print("Starting...");
    delay(1000); // Wait for 1 second
    lcd.clear();

    // Set pin modes
    pinMode(buzzerPin, OUTPUT);
    pinMode(redLed, OUTPUT);
    pinMode(greenLed, OUTPUT);
}


int _owner = 0;
int _prev_owner = 0;

int lcd_take_ownership(int owner) 
{
    _prev_owner = _owner;
    _owner = owner;
    return _prev_owner;
}

int lcd_owner() 
{
    return _owner;
}

int lcd_prev_owner() 
{
    return _prev_owner;
}

/////END of RTC SETUP////////////////

void updateTimeDisplay() 
{
    // Get the current time from the RTC
    /*
    */
    DateTime now = rtc.now();

    // Read the current time once and use this consistent reading
    uint16_t year = now.year();
    uint8_t month = now.month();
    uint8_t day = now.day();
    uint8_t hour = now.hour();
    uint8_t minute = now.minute();
    uint8_t second = now.second();

    bool need_update = lcd_prev_owner() != lcd_owner();

    static char previousDate[11] = "";
    char currentDate[11];
    snprintf(currentDate, sizeof(currentDate), "%02d-%02d-%04d", day, month, year);

    // Update the current time on the LCD only if it has changed
    if (need_update || strcmp(currentDate, previousDate) != 0) 
    {
        lcd.setCursor(0, 0);
        lcd.print("            "); // Clear the line
        lcd.setCursor(3, 0);
        lcd.print(currentDate);
        strcpy(previousDate, currentDate);
    }

    static char previousTime[9] = "";
    char currentTime[9];
    snprintf(currentTime, sizeof(currentTime), "%02d:%02d:%02d", hour, minute, second);
    // snprintf(currentTime, sizeof(currentTime), "%02d:%02d:%02d", hour, minute, minute);
    if (need_update || strcmp(currentTime, previousTime) != 0) 
    {
        lcd.setCursor(0, 1);
        lcd.print("        "); // Clear the line
        lcd.setCursor(4, 1);
        lcd.print(currentTime);
        strcpy(previousTime, currentTime);
    }
}


void displayCurrentTime() 
{
    updateTimeDisplay();
}

enum ProgramState 
{
    STATE_DISPLAY_TIME,   // New state to display the time 
    STATE_PREPLANT,
    STATE_PLANTING,
    STATE_PLANT_SETUP,
    STATE_PLANTED,
    STATE_DEFUSE_ATTEMPT,
    STATE_DEFUSED,
    STATE_EXPLODED,
    STATE_LISTEN_MUSIC,
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

void show_timer(int seconds, int milliseconds) 
{
    static int last_seconds = -1;
    static int last_milliseconds = -1;

    // Check if the time has changed
    if (last_seconds == seconds && last_milliseconds == milliseconds) 
    {
        return;
    }

    // Calculate remaining seconds
    int remaining_seconds = seconds % 60;

    // Display the time
    lcd.setCursor(3, 0); // Set the cursor to (3,0)
    lcd.print("00:"); // Display fixed "00:" for minutes

    if (remaining_seconds < 10) 
    {
        lcd.print("0");
    }

    lcd.print(remaining_seconds);
    lcd.print(".");

    if (milliseconds < 100) 
    {
        lcd.print("0");
    }

    if (milliseconds < 10) 
    {
        lcd.print("0");
    }

    lcd.print(milliseconds);
    lcd.print("       "); // Add spaces to ensure nothing else appears

    // Update the last time
    last_seconds = seconds;
    last_milliseconds = milliseconds;
}

void blink_buz(int time) 
{
    static bool ledState = LOW;
    static unsigned long previousMillis = 0; // Initial previous millis
    unsigned long currentMillis = millis();
    unsigned long blinkInterval = 1000; // Initial blink interval
    unsigned int buzzerFrequency = 1049; // Initial buzzer frequency

    if (time <= 0) 
    {
        blinkInterval = 10; // Set blink interval to a very small value for the last second
        buzzerFrequency = 4000; // Double the buzzer frequency for the last second

    } else if (time > 10) 
    {
        // Linear decrease from 1000ms to 500ms between 40 and 15 seconds
        blinkInterval = 1000 - (500 * (TIME_TO_EXPLODE - time) / (TIME_TO_EXPLODE - 15));

    } else if (time > 0) 
    {
        // Linear decrease from 400ms to 40ms between 10 and 1 seconds
        blinkInterval = 400 - (40 * (10 - time));

        // Adjust the blink interval for the penultimate second
        if (time == 1) 
        {
            blinkInterval = blinkInterval / 4; // Set blink interval to 1/3 of the blink interval from the penultimate second
        } else if(time<1)
        {
            blinkInterval = blinkInterval / 5;
        }
    } else 
    {
        // In the last second, set the blink interval to 17 ms
        blinkInterval = 17;
    }

    // Print the remaining time and blink interval to the serial monitor
    if (time >= 0) 
    {
        Serial.print("Time remaining: ");
        Serial.print(time);
        Serial.print("s, Blink interval: ");
        Serial.println(blinkInterval);
    }

    // Blink LED and beep buzzer based on the adjusted blink interval and buzzer frequency
    if (currentMillis - previousMillis >= blinkInterval) 
    {
        previousMillis = currentMillis;
        ledState = !ledState;
        digitalWrite(redLed, ledState);

        if (ledState == HIGH) 
        {
            // Beep the buzzer with the adjusted frequency
            tone(buzzerPin, buzzerFrequency+100, blinkInterval / 2);
            delay(30); // Keep the LED on for a brief period to create a blitz effect
            digitalWrite(redLed, LOW);
        }
    }
}




void play(int track_number, int volume_level)
{
    mp3.playTrackNumber(track_number, volume_level);
}
    
unsigned long prev_millis =0;

char getKey() {

  for (int r = 0; r < 4; r++) {

    byte mask = 0xFF;
    mask &= ~(1 << rowPins[r]);

    pcf.write8(mask);
    delay(1);

    byte state = pcf.read8();

    for (int c = 0; c < 3; c++) {
      if ((state & (1 << colPins[c])) == 0) {

        // wait release
        do {
          state = pcf.read8();
        } while ((state & (1 << colPins[c])) == 0);

        return keys[r][c];
      }
    }
  }

  return '\0';
}


void loop() 
{

    char key;
    unsigned i;
    static bool flag_show_timer = false;
    static bool flag_display_time = true;
    static bool mp3Playing = false;


    static short enter_code_len = 0;
    static unsigned long entered_time = 0, elapsed_time = 0;
    static ProgramState state = STATE_DISPLAY_TIME; 
    static bool plantedSoundPlayed = false;
  


    if (!flag_show_timer && !flag_display_time) 
    {
        breathEffect();
    }

   
    lcd_take_ownership(state);

    int elapsed_seconds = 0;
    int elapsed_milliseconds = 0;

    switch (state) 
    {   
        case STATE_LISTEN_MUSIC:
            mp3Playing = false;
            lcd.clear();
            lcd.setCursor(3,0);
            lcd.print("Music Play");
            delay(50);
            key = getKey();
            if (key) 
            {   
                if (key == '#') 
                {
                    if(!mp3Playing)
                    {
                        key = getKey();
                        play(85,22);
                        
                       
                    }
                

                   
                    Serial.print("Key pressed: ");
                    Serial.println(key);
                    mp3Playing = false;
                    flag_display_time = true;
                    state = STATE_DISPLAY_TIME;
                }
                if (key == '*')
                {
                    state = STATE_DISPLAY_TIME;
                }

                if (key == '0')
                {
                    state = STATE_PREPLANT;
                }
                if (key == '#')
                {
                    state = STATE_LISTEN_MUSIC;
                }
            }
            break;

        case STATE_DISPLAY_TIME:
            displayCurrentTime();
            key = getKey();
            if (key) 
            {
                Serial.print("Key pressed: ");
                Serial.println(key);
                if (key == '0') 
                {   
                    lcd.setCursor(1,0);
                    lcd.print("Enter the code!");
                    state = STATE_PREPLANT;
                    enter_code_len = 0; // Reset code length
                    for (i = 0; i < 7; i++) 
                    {
                        enteredCode[i] = '*'; // Clear entered code
                    }
                    lcd.clear();
                }
                else if (key == '#')  
                {   
                    state = STATE_LISTEN_MUSIC;
                    mp3Playing = true;
                }

            }
            key = getKey();
            if (key) 
            {
                Serial.print("Key pressed: ");
                Serial.println(key);
                if (key == '#') 
                {
                    state = STATE_LISTEN_MUSIC;
                    enter_code_len = 0; // Reset code length
                    for (i = 0; i < 7; i++) 
                    {
                        enteredCode[i] = '*'; // Clear entered code
                    }
                    lcd.clear();
                    
                }
            }
            break;

        case STATE_PREPLANT:

            
            if(!mp3Playing)
            {
                lcd.setCursor(1,0);
                lcd.print("Enter the code!");
                play(5,15);

            }
            mp3Playing = false;
            digitalWrite(redLed, LOW);
            flag_display_time = false;
            enter_code_len = 0; // Reset code length

            for (i = 0; i < 7; i++) 
            {
                enteredCode[i] = '*'; // Clear entered code
            }

            write_init_screen();
            state = STATE_PLANTING;
            break;

        case STATE_PLANTING:
            key = getKey();
            if (key) 
            {
                Serial.print("Key pressed: ");
                Serial.println(key);
                lcd.setCursor(4, 0);
                enteredCode[enter_code_len] = key;
                enter_code_len++;
                lcd.print(enteredCode);

                if (enter_code_len == 7) 
                {
                    state = strcmp(enteredCode, code) == 0 ? STATE_PLANT_SETUP : STATE_PREPLANT;
                    enter_code_len = 0;

                }
                if (key == '*') 
                {
                    flag_display_time = 1;
                    flag_show_timer = 0;
                    state = STATE_DISPLAY_TIME;
                    lcd.clear();
                    digitalWrite(greenLed, 0);

                }

                if (key == '#') 
                {
                    flag_display_time = 1;
                    flag_show_timer = 0;
                    state = STATE_LISTEN_MUSIC;
                    lcd.clear();
                    digitalWrite(greenLed, 0);

                }
            }
            break;

        case STATE_PLANT_SETUP:
            mp3Playing = false;
            entered_time = millis();
            flag_show_timer = true;
            state = STATE_PLANTED;
            break;

        case STATE_PLANTED:
            mp3Playing = false;
            if (flag_show_timer) 
            {
                analogWrite(greenLed, LOW);

            }

            for (i = 0; i < 7; i++) 
            {
                secondCode[i] = '*';

            }

            lcd.setCursor(4, 1);
            lcd.print(secondCode);
            state = STATE_DEFUSE_ATTEMPT;
           
            // Turn off the greenLed
            analogWrite(greenLed, 0);
            break;

        case STATE_DEFUSE_ATTEMPT:
            
            if(!plantedSoundPlayed)
            {   
                plantedSoundPlayed = true;
                lcd.clear();
                lcd.setCursor(4,0);
                lcd.print("Planted!");
                delay(50);
                play(3,25);
                delay(50);
                lcd.setCursor(4,1);
                lcd.print("*******");
                
                
            }

            elapsed_time = millis() - entered_time;
            elapsed_seconds = elapsed_time / 1000;
            elapsed_milliseconds = elapsed_time % 1000;
            blink_buz(TIME_TO_EXPLODE - elapsed_seconds);
            show_timer(TIME_TO_EXPLODE - elapsed_seconds, elapsed_milliseconds);


            if (elapsed_seconds-1 >= TIME_TO_EXPLODE) 
            {
                state = STATE_EXPLODED;
                break;

            }
            key = getKey();
            if (key) 
            {
                Serial.print("Key pressed: ");
                Serial.println(key);
                secondCode[enter_code_len] = key;
                enter_code_len++;
                lcd.setCursor(4, 1);
                lcd.print(secondCode);

                if (enter_code_len == 7) 
                {
                    state = strcmp(secondCode, code) == 0 ? STATE_DEFUSED : STATE_PLANTED;
                    enter_code_len = 0;

                }
            }
            
            break;

        case STATE_EXPLODED:
            mp3Playing = false;
            plantedSoundPlayed = false;
            if(!mp3Playing)
            {
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Terrorists win!");
            delay(50);
            play(33,25);
            }
            state = STATE_PREPLANT;
            mp3Playing = true;
            delay(5000);
            
            
            flag_show_timer = false;
            break;

        case STATE_DEFUSED:
            mp3Playing = false;
            plantedSoundPlayed = false;
            if(!mp3Playing)
            {   
                lcd.clear();
                lcd.setCursor(4, 0);
                lcd.print("Counter");
                lcd.setCursor(0, 1);
                lcd.print("Terrorists win!");
                delay(50);
                play(2,25);
    
            }
                delay(5000);

                mp3Playing = true;
                flag_show_timer = false;
                state = STATE_PREPLANT;
            break;

        default:
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("?>>> win!");
            delay(2000);
            flag_show_timer = false;
            state = STATE_PREPLANT; // Ensure a proper state transition after default
            break;   
    }
}
