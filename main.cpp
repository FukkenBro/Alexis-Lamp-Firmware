#include <Arduino.h>
#include <avr/sleep.h> // библиотека для сна

//ИНТЕРФЕЙСЫ: ========================================================
#define OUTPUT_PIN 11 // пин, к которому подключена нагрузка
#define LED_PIN 9     // пин, к которому подключен светодиод индикатор
#define BUTTON_PIN 3  // пин, к которому подключена кнопка (1йконтакт - пин, 2йконтакт - GND, INPUT_PULLUP, default - HIGH);
#define SENSOR_PIN 0  // Аналоговый пин для подключения фотосенсора
#define POT_PIN 2     // Аналоговый пин для подключения потенциометра

//НАСТРОИВАЕМЫЕ ПАРАМЕТРЫ:=========================================================
#define HOLD_DURATION 150 //время удержания кнопки для регистрации длинного нажатия (default - 150)
#define TIMER_DELAY 3000  // задержка выключения [ms] (default 1 800 000ms = 30min)
#define THIS_DELAY 300    // задержка для анимации RGB светодиодов [ms]

//ИННЦИАЛИЗАЦИЯ ПЕРЕМЕННЫХ: =========================================================
int potVal;    //loop() показания потенциометра
int senVal;    //loop() показания фотосенсора
bool prevFlag; //
int prevBr;    //
int br;        //

unsigned long brDelay = 500; // mode2()
unsigned long brStart;       // mode2()

unsigned long start = 0;     // debounce() стартовая позиция для отсчёта прерывания на основе millis() в методе debounce();
#define DEBOUNCE_DELAY 50    // debounce() гистерезис функции debounce
volatile int pressTimer = 0; // debounce()

unsigned long shutDown = 0; // poll() время выключения

#define PULSE_BRIGHTNES 250 // pulse() Яркость мигающего диода таймера
unsigned long pulseDelay;   // pulse()
unsigned long pulseStart;   // pulse()
byte pulseFade = 0;         // pulse()
byte pulseFadeStep = 0;     // pulse()
int mode = 1;               // pulse()

// Button input related values
static const int STATE_NORMAL = 0; // interrupt() no button activity
static const int STATE_SHORT = 1;  // interrupt() short button press
static const int STATE_LONG = 2;   // interrupt() long button press
volatile int resultButton = 0;     // interrupt() global value set by checkButton()

//FLAGS: ==========================================================================
// флаг для buttonRoutine()
volatile bool modeFlag;
bool glowFlag = false;
bool timerFlag = false;
bool pulseFlag = false;
byte buttonState;

// Иннициаллизация функций ========================================================
void interrupt();
bool debounce();
void heat(int heatTo);
void fadeOut(int fadeFrom);
void pwrDown();
void pulse();
void poll();
void blink();
void kill();
void onButtonClick();
void onButtonHold();
void mode1();
void mode2();

//SETUP ===========================================================================
void setup()
{
  delay(100);
  // Serial.begin(9600);
  pinMode(BUTTON_PIN, INPUT_PULLUP); // defaul HIGH
  pinMode(LED_PIN, OUTPUT);
  pinMode(OUTPUT_PIN, OUTPUT);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), interrupt, CHANGE); // triggers when LOW
  potVal = analogRead(POT_PIN);
  br = map(potVal, 0, 1024, 0, 255);
  prevBr = br;
  heat(br);
}

//LOOP ============================================================================
void loop()
{
  if (timerFlag == true)
  {
    pulse();
    poll();
  }

  if (modeFlag)
  {
    mode1();
  }
  else if (!modeFlag)
  {
    // (defaul режим)
    mode2();
  }
}

//FUNCTIONS =======================================================================

//Адаптивная яркость
void mode1()
{
  potVal = analogRead(POT_PIN);
  senVal = constrain(map(analogRead(SENSOR_PIN), 0, 1024, 1024, 0) - 100, 0, 1024);
  // Serial.print("MODE1  ");
  // Serial.print("Potentiometer val = ");
  // Serial.println(potVal);
  // Serial.print("ModeFlag = ");
  // Serial.println(modeFlag);
  // Serial.print("Sensor val = ");
  // Serial.println(senVal);
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
  // Serial.print("\nglowFlag vs prevGlowFlag ");
  // Serial.print(glowFlag);
  // Serial.print(" vs ");
  // Serial.print(prevFlag);
  // Serial.print("\n");
  if (prevFlag != glowFlag)
  {
    if (!prevFlag)
    {
      heat(255);
    }
    else if (prevFlag)
    {
      fadeOut(255);
    }
  }
  analogWrite(OUTPUT_PIN, br);
  prevFlag = glowFlag;
  prevBr = br;

  Serial.print("Brightness = ");
  Serial.println(br);
  Serial.println(" ");
}

//Ручная настройка яркости (defaul режим)
void mode2()
{
  Serial.print("MODE2  ");

  unsigned long currentBr = millis();

  /////

  if (abs(brStart - currentBr) > 500)
  {
    //body
    potVal = analogRead(POT_PIN);
    Serial.print("Potentiometer val = ");
    Serial.println(potVal);
    Serial.print("ModeFlag = ");
    Serial.println(modeFlag);
    br = map(potVal, 0, 1024, 0, 255);

    // компенсация мерцания на низких значениях яркости

    if (br < 5)
    {
      br = 0;
    }
    if (br <= 10 && br >= 5)
    {
      br = 5;
    }
    else if (br <= 15 && br > 10)
    {
      br = 10;
    }
    else if (br <= 20 && br > 15)
    {
      br = 15;
    }

    if (prevBr > br)
    {
      for (int i = prevBr; i > br; i--)
      {
        analogWrite(OUTPUT_PIN, i);
        delay(2);
      }
    }
    else if (prevBr < br)
    {
      for (int i = prevBr; i < br; i++)
      {
        analogWrite(OUTPUT_PIN, i);
        delay(2);
      }
    }
  }
  prevFlag = glowFlag;
  prevBr = br;

  Serial.print("Brightness = ");
  Serial.println(br);
  Serial.println(" ");
  //body
  brStart = millis();
}

////

void heat(int heatTo)
{
  for (int i = 0; i < heatTo; i++)
  {
    analogWrite(OUTPUT_PIN, i);
    delay(5);
  }
}

void fadeOut(int fadeFrom)
{
  for (int i = fadeFrom; i > 0; i--)
  {
    analogWrite(OUTPUT_PIN, i);
    delay(5);
  }
}

void blink()
{
  int blinkTimes = 2;
  for (int j = 0; j < blinkTimes; j++)
  {
    for (size_t i = 0; i <= 125; i++)
    {
      analogWrite(OUTPUT_PIN, i);
      delay(1);
    }
    for (int i = 125; i > 50; i--)
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
  fadeOut(br);
  analogWrite(OUTPUT_PIN, 0);
  analogWrite(LED_PIN, 0);
  timerFlag = false;
  delay(1000);
  kill();
}

void kill()
{
  buttonState = 3;
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  sleep_cpu();
}

void onButtonClick()
{
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
  else if (buttonState == 3)
  {
    // cброс всех флажков
    timerFlag = false;
    analogWrite(LED_PIN, 0);
    shutDown = 0;
    pulseDelay = 0;
    buttonState = 0;
    pulseFade = 0;
    pulseFadeStep = 0;
    asm volatile("  jmp 0");
  }
  else
  {
    // cброс всех флажков
    timerFlag = false;
    analogWrite(LED_PIN, 0);
    shutDown = 0;
    pulseDelay = 0;
    buttonState = 0;
    pulseFade = 0;
    pulseFadeStep = 0;
    asm volatile("  jmp 0");
  }
}

void onButtonHold()
{
  modeFlag = !modeFlag;
  blink();
}
