#include <Arduino.h>
// библиотека для сна
#include <avr/sleep.h>

//!!!!!!!!!!!!!!! Внимание !!!!!!!!!!!!!!!!!!!!!!!!!!!!! Функция FadeOut - может потребовать дополнительной настройке при смене кол-ва диодов
//НЕИЗМЕНЯЕМЫЕ ПАРАМЕТРЫ: ========================================================

#define OUTPUT_PIN 11
// пин, к которому подключен светодиод индикатор
#define LED_PIN 9
// пин, к которому подключена кнопка (1йконтакт - пин, 2йконтакт - GND, INPUT_PULLUP, default - HIGH);
#define BUTTON_PIN 3
// Аналоговый пин для подключения фотосенсора
#define SENSOR_PIN 0
// Аналоговый пин для подключения потенциометра
#define POT_PIN 2
//время удержания кнопки для регистрации длинного нажатия (default - 150)
#define HOLD_DURATION 150

//НАСТРОИВАЕМЫЕ ПАРАМЕТРЫ:=========================================================
// задержка выключения [ms] (default 1 800 000ms = 30min)
#define TIMER_DELAY 1800000
// задержка для анимации RGB светодиодов [ms]
#define THIS_DELAY 300

//НАСТРОИВАЕМЫЕ ПАРАМЕТРЫ:=========================================================
bool prevFlag;
int prevBr;
int br;
// перменные для debounce() .......................................................
// стартовая позиция для отсчёта прерывания на основе millis() в методе debounce();
unsigned long start = 0;
// гистерезис функции debounce
#define DEBOUNCE_DELAY 50
volatile int pressTimer = 0;
//переменные для poll()............................................................
// время выключения
unsigned long shutDown = 0;
// перменные для pulse() ..........................................................
// Яркость мигающего диода таймера
#define PULSE_BRIGHTNES 250
unsigned long pulseDelay;
unsigned long pulseStart;
byte pulseFade = 0;
byte pulseFadeStep = 0;
int mode = 1;
// перменные для interrupt() ..........................................................
// Яркость мигающего диода таймера
// Button input related values
static const int STATE_NORMAL = 0; // no button activity
static const int STATE_SHORT = 1;  // short button press
static const int STATE_LONG = 2;   // long button press
volatile int resultButton = 0;     // global value set by checkButton()

//FLAGS: ==========================================================================
// флаг для buttonRoutine()
bool modeFlag = false;
bool glowFlag = false;
bool timerFlag = false;
bool pulseFlag = false;
byte buttonState;
byte *c;
bool reset = false;
bool forceExit = false;

// Иннициаллизация функций ========================================================
void interrupt();
bool debounce();
byte *Wheel(byte WheelPos);
void heat();
void pwrDown();
void pulse();
void poll();
void blink();
void fadeOut();
void kill();
void animation1(byte speedMultiplier);
void animation2(byte hueDelta, byte speedMultiplier);
void resetCheck();
void onButtonClick();
void onButtonHold();
void resetColor();

//SETUP ===========================================================================
void setup()
{
  delay(100);
  Serial.begin(9600);
  pinMode(BUTTON_PIN, INPUT_PULLUP); // defaul HIGH
  pinMode(LED_PIN, OUTPUT);
  pinMode(OUTPUT_PIN, OUTPUT);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), interrupt, CHANGE); // triggers when LOW
  heat();
}

//LOOP ============================================================================
void loop()
{
  forceExit = false;
  int potVal = analogRead(POT_PIN);
  Serial.print("Potentiometer val = ");
  Serial.println(potVal);
  int senVal = map(analogRead(SENSOR_PIN), 0, 1024, 1024, 0) + 20;
  Serial.print("Sensor val = ");
  Serial.println(senVal);

  if (modeFlag)
  {
    if (senVal >= potVal)
    {
      //Если темно
      glowFlag = true;
      br = 255;
    }
    else if (senVal < potVal)
    {
      //Если светло
      glowFlag = false;
      br = 0;
    }

    Serial.print("\nglowFlag vs prevGlowFlag ");
    Serial.print(glowFlag);
    Serial.print(" vs ");
    Serial.print(prevFlag);
    Serial.print("\n");

    if (prevFlag != glowFlag)
    {
      if (!prevFlag)
      {
        heat();
      }
      else if (prevFlag)
      {
        fadeOut();
      }
    }
  }
  else
  {
    br = map(potVal, 0, 1024, 255, 0);
  }

  analogWrite(OUTPUT_PIN, br);
  prevFlag = glowFlag;
  prevBr = br;

  Serial.print("Brightnes = ");
  Serial.println(br);
  Serial.println(" ");
  // delay(1000);
}

//FUNCTIONS =======================================================================

void heat()
{
  for (size_t i = 0; i < 254; i++)
  {
    analogWrite(OUTPUT_PIN, i);
    delay(5);
  }
}

void fadeOut()
{
  for (size_t i = 255; i > 0; i--)
  {
    analogWrite(OUTPUT_PIN, i);
    delay(5);
  }
}

void blink()
{
  int blinkTimes = 2;
  for (size_t j = 0; j < blinkTimes; j++)
  {
    for (size_t i = 0; i <= 125; i++)
    {
      analogWrite(OUTPUT_PIN, i);
      delay(1);
    }
    for (size_t i = 125; i > 50; i--)
    {
      analogWrite(OUTPUT_PIN, i);
      delay(2);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

void interrupt()
{
  /*
  * This function implements software debouncing for a two-state button.
  * It responds to a short press and a long press and identifies between
  * the two states. Your sketch can continue processing while the button
  * function is driven by pin changes.
  */

  const unsigned long LONG_DELTA = 500ul;               // hold seconds for a long press
  const unsigned long DEBOUNCE_DELTA = 50ul;            // debounce time
  static int lastButtonStatus = HIGH;                   // HIGH indicates the button is NOT pressed
  int buttonStatus;                                     // button atate Pressed/LOW; Open/HIGH
  static unsigned long longTime = 0ul, shortTime = 0ul; // future times to determine if button has been poressed a short or long time
  boolean Released = true, Transition = false;          // various button states
  boolean timeoutShort = false, timeoutLong = false;    // flags for the state of the presses

  buttonStatus = digitalRead(BUTTON_PIN); // read the button state on the pin "BUTTON_PIN"
  timeoutShort = (millis() > shortTime);  // calculate the current time states for the button presses
  timeoutLong = (millis() > longTime);

  if (buttonStatus != lastButtonStatus)
  { // reset the timeouts if the button state changed
    shortTime = millis() + DEBOUNCE_DELTA;
    longTime = millis() + LONG_DELTA;
  }

  Transition = (buttonStatus != lastButtonStatus);   // has the button changed state
  Released = (Transition && (buttonStatus == HIGH)); // for input pullup circuit

  lastButtonStatus = buttonStatus; // save the button status

  if (!Transition)
  { //without a transition, there's no change in input
    // if there has not been a transition, don't change the previous result
    resultButton = STATE_NORMAL | resultButton;
    return;
  }

  if (timeoutLong && Released)
  {                                           // long timeout has occurred and the button was just released
    resultButton = STATE_LONG | resultButton; // ensure the button result reflects a long press
    onButtonHold();
  }
  else if (timeoutShort && Released)
  {                                            // short timeout has occurred (and not long timeout) and button was just released
    resultButton = STATE_SHORT | resultButton; // ensure the button result reflects a short press
    onButtonClick();
  }
  else
  {                                             // else there is no change in status, return the normal state
    resultButton = STATE_NORMAL | resultButton; // with no change in status, ensure no change in button status
  }
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////

void pulse()
{
  if (pulseDelay != 0)
  {
    // мерцание
    unsigned long current = millis();
    if (abs(current - pulseStart) > pulseDelay)
    {
      if (pulseFlag == false)
      {
        pulseFade = constrain((pulseFade + pulseFadeStep), 0, 250);
        analogWrite(LED_PIN, constrain((PULSE_BRIGHTNES - pulseFade), 1, 255));
        pulseFlag = true;
      }
      else
      {
        analogWrite(LED_PIN, 0);
        pulseFlag = false;
        pulseStart = millis();
      }
    }
  }
}

void poll()
{
  if (timerFlag == true && shutDown != 0)
  { // если таймер влючен
    if (millis() >= shutDown)
    { // если время срабатывания таймера настало
      pwrDown();
    }
  }
  // Serial.print("До выключения (сек.) ");
  // Serial.println((shutDown - millis()) / 1000);
}

void pwrDown()
{
  reset = true;
  analogWrite(LED_PIN, 0);
  fadeOut();
  timerFlag = false;
  // погасить все светодиоды
  delay(1000);
  kill();
}

void kill()
{
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  sleep_cpu();
}

void resetCheck()
{
  if (reset == true)
  {
    reset = false;
    asm volatile("  jmp 0");
  }
}

void onButtonClick()
{
  resetCheck();
  if (buttonState == 0)
  {
    pulseFade = 0;
    buttonState += 1;
    timerFlag = true;
    shutDown = millis() + TIMER_DELAY;
    pulseDelay = 100;
    pulseFadeStep = 2;
  }
  else if (buttonState == 1)
  {
    pulseFade = 0;
    buttonState += 1;
    timerFlag = true;
    shutDown = millis() + TIMER_DELAY * 2;
    pulseDelay = 1000;
    pulseFadeStep = 10;
  }
  else if (buttonState == 2)
  {
    // cброс всех флажков
    timerFlag = false;
    analogWrite(LED_PIN, 0);
    shutDown = 0;
    pulseDelay = 0;
    buttonState = 0;
    pulseFade = 0;
    pulseFadeStep = 0;
  }
}

void onButtonHold()
{
  modeFlag = !modeFlag;
  blink();
}
