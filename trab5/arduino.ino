#include <TimerOne.h>
#include <MultiFuncShield.h>

#define RANGE_MAX_NUMBER 9999
#define RANGE_MIN_NUMBER 0000

enum GeneratorModeValues {
  GENERATOR_STOPPED,
  GENERATOR_STARTED,
  SETTING_RANGE_MAX_NUM_STARTED,
  SETTING_RANGE_MIN_NUM_STARTED
};

enum RangeStatusValues {
  RANGE_NOT_SET,
  RANGE_SET
};

enum ledModeValues {
  LED_ALL_OFF,
  LED_1_ON,
  LED_2_ON,
  LED_3_ON,
  LED_4_ON
};

enum AnimationModeValues {
  ANIMATION_STOPPED,
  ANIMATION_STARTED,
  ANIMATION_STAGE1,
  ANIMATION_STAGE2,
  ANIMATION_STAGE3,
  ANIMATION_STAGE4,
  ANIMATION_INTERRUPTED
};

GeneratorModeValues currentState;
int MIN_NUM, MAX_NUM;
RangeStatusValues MIN_SET, MAX_SET;

void stop() {
  currentState = GENERATOR_STOPPED;
  MFS.write("OFF");
}

void executeStopped() {
  byte btn = MFS.getButton();
  if (!btn) return;

  switch (btn) {
    case BUTTON_1_SHORT_RELEASE:
      //      if (MIN_SET == RANGE_SET && MAX_SET == RANGE_SET)
      currentState = GENERATOR_STARTED;
      break;

    case BUTTON_2_SHORT_RELEASE:
      MFS.write(MAX_NUM);
      break;

    case BUTTON_2_LONG_PRESSED:
      currentState = SETTING_RANGE_MAX_NUM_STARTED;
      break;

    case BUTTON_3_SHORT_RELEASE:
      MFS.write(MIN_NUM);
      break;

    case BUTTON_3_LONG_PRESSED:
      MFS.write("    ");
      currentState = SETTING_RANGE_MIN_NUM_STARTED;
      break;
  }
}

void setLed(ledModeValues value) {
  MFS.writeLeds(LED_ALL, OFF);
  switch (value) {
    case LED_1_ON:
      MFS.writeLeds(LED_1, ON);
      break;
    case LED_2_ON:
      MFS.writeLeds(LED_2, ON);
      break;
    case LED_3_ON:
      MFS.writeLeds(LED_3, ON);
      break;
    case LED_4_ON:
      MFS.writeLeds(LED_4, ON);
      break;
  }
}

void stepAnimation(AnimationModeValues& stage, int number) {
  switch (stage) {
      case ANIMATION_STOPPED:
          MFS.write("    ");
          stage = ANIMATION_STARTED;
          break;
      case ANIMATION_STAGE1:
//          MFS.write
          break;
//      ANIMATION_STAGE2,
//      ANIMATION_STAGE3,
//      ANIMATION_STAGE4,
//      ANIMATION_INTERRUPTED
  }

}

String generateString(int to_fill, int randomValue, int currentFill) {
    String s;

    if (to_fill == 0) {
      return String(randomValue);
    }
    if (to_fill == 4) {
      return String(currentFill) + String(currentFill) + String(currentFill) + String(currentFill);
    }

    int mod = 1;
    for (int i = 0; i < 4-to_fill; i++) mod *= 10;
    for (int i = 0; i < to_fill; i++) {
      s += String(currentFill);
    }
    if (to_fill != 4) {
      s += String(randomValue % mod);
    }
    return s;
}
int randomNumber = 1234;
void executeStarted() {
  ledModeValues ledStates[] = {LED_ALL_OFF, LED_1_ON, LED_2_ON, LED_3_ON, LED_4_ON};
  int currentLedState = 0, currentAnimationState = 0;
  int currentFill = 0;
  while (true) {
    setLed(ledStates[currentLedState++ % 5]);
    switch (currentAnimationState) {
      case ANIMATION_STAGE1:
          MFS.write(generateString(4, randomNumber, currentFill++));
          if (currentFill == 10) {
            currentAnimationState = ANIMATION_STAGE2;
            currentFill = 0;
          }
          break;
      case ANIMATION_STAGE2:
          MFS.write(generateString(3, randomNumber, currentFill++));
          if (currentFill == 10) {
            currentAnimationState = ANIMATION_STAGE3;
            currentFill = 0;
          }
          break;
      case ANIMATION_STAGE3:
          MFS.write(generateString(2, randomNumber, currentFill++));
          if (currentFill == 10) {
            currentAnimationState = ANIMATION_STAGE4;
            currentFill = 0;
          }
          break;
      case ANIMATION_STAGE4:
          MFS.write(generateString(1, randomNumber, currentFill++));
          if (currentFill == 10) {
            currentAnimationState = ANIMATION_STOPPED;
            currentFill = 0;
          }
          break;
    }
    delay(100);
  }

  MFS.write("gold");
  MFS.beep(500);
  delay(500);


}

void executeMaxNum() {
  MFS.write(MAX_NUM);
  MFS.blinkDisplay(DIGIT_ALL, ON);
  byte btn = MFS.getButton();
  if (!btn) return;

  switch (btn) {
    case BUTTON_1_SHORT_RELEASE:
      MFS.blinkDisplay(DIGIT_ALL, OFF);
      MFS.writeLeds(LED_1, true);
      MAX_SET = RANGE_SET;
      stop();
      break;
    case BUTTON_2_PRESSED:
    case BUTTON_2_LONG_PRESSED:
      if (MAX_NUM + 100 <= RANGE_MAX_NUMBER)
        MAX_NUM += 100;
      break;
    case BUTTON_3_PRESSED:
    case BUTTON_3_LONG_PRESSED:
      if (MAX_NUM - 100 > RANGE_MIN_NUMBER)
        MAX_NUM -= 100;
      break;

  }
}

void executeMinNum() {
  MFS.write(MIN_NUM);
  MFS.blinkDisplay(DIGIT_ALL, ON);
  byte btn = MFS.getButton();
  if (!btn) return;

  switch (btn) {
    case BUTTON_1_SHORT_RELEASE:
      MFS.blinkDisplay(DIGIT_ALL, OFF);
      MFS.writeLeds(LED_2, true);
      MIN_SET = RANGE_SET;
      stop();
      break;
    case BUTTON_2_PRESSED:
    case BUTTON_2_LONG_PRESSED:
      if (MIN_NUM + 100 < RANGE_MAX_NUMBER)
        MIN_NUM += 100;
      break;
    case BUTTON_3_PRESSED:
    case BUTTON_3_LONG_PRESSED:
      if (MIN_NUM - 100 >= RANGE_MIN_NUMBER)
        MIN_NUM -= 100;
      break;

  }
}

void setup()
{
  MIN_NUM = RANGE_MIN_NUMBER;
  MAX_NUM = RANGE_MAX_NUMBER;
  MIN_SET = RANGE_NOT_SET;
  MAX_SET = RANGE_NOT_SET;

  stop();
  Serial.begin(9600);
  Timer1.initialize();
  MFS.initialize(&Timer1);
}

void loop() {
  switch (currentState)
  {
    case GENERATOR_STOPPED:
      executeStopped();
      break;
    case GENERATOR_STARTED:
      executeStarted();
      break;
    case SETTING_RANGE_MAX_NUM_STARTED:
      executeMaxNum();
      break;
    case SETTING_RANGE_MIN_NUM_STARTED:
      executeMinNum();
      break;
  }
  delay(100);
}