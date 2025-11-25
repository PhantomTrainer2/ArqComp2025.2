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
    case LED_ALL_OFF:
    default:
      break;
  }
}

void handleInterruptAnimation() {
  MFS.write("Intr");
  MFS.writeLeds(LED_ALL, OFF);

  MFS.beep(50, 50, 3);

  MIN_SET = RANGE_NOT_SET;
  MAX_SET = RANGE_NOT_SET;

  delay(2000);
  MFS.write("off");
  currentState = GENERATOR_STOPPED;
}

void stop() {
  currentState = GENERATOR_STOPPED;
  MFS.write("off");
  MFS.blinkDisplay(DIGIT_ALL, OFF);
}


void executeStopped() {
  byte btn = MFS.getButton();
  if (!btn) return;

  switch (btn) {
    case BUTTON_1_SHORT_RELEASE:
      if (MIN_SET == RANGE_SET && MAX_SET == RANGE_SET) {
        currentState = GENERATOR_STARTED;
      }
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
      currentState = SETTING_RANGE_MIN_NUM_STARTED;
      break;

    default:
      break;
  }
}

void executeMaxNum() {
  MFS.write(MAX_NUM);
  MFS.blinkDisplay(DIGIT_ALL, ON);

  byte btn = MFS.getButton();
  if (!btn) return;

  switch (btn) {
    case BUTTON_1_SHORT_RELEASE:
      MFS.blinkDisplay(DIGIT_ALL, OFF);
      MAX_SET = RANGE_SET;
      MFS.writeLeds(LED_1, ON);
      stop();
      break;

    case BUTTON_2_PRESSED:
    case BUTTON_2_LONG_PRESSED:
      if (MAX_NUM + 100 <= RANGE_MAX_NUMBER) {
        MAX_NUM += 100;
        MFS.write(MAX_NUM);
      }
      break;

    case BUTTON_3_PRESSED:
    case BUTTON_3_LONG_PRESSED:
      if (MAX_NUM - 100 >= MIN_NUM) {
        MAX_NUM -= 100;
        MFS.write(MAX_NUM);
      }
      break;

    default:
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
      MIN_SET = RANGE_SET;
      MFS.writeLeds(LED_2, ON);
      stop();
      break;

    case BUTTON_2_PRESSED:
    case BUTTON_2_LONG_PRESSED:
      if (MIN_NUM + 100 <= MAX_NUM) {
        MIN_NUM += 100;
        MFS.write(MIN_NUM);
      }
      break;

    case BUTTON_3_PRESSED:
    case BUTTON_3_LONG_PRESSED:
      if (MIN_NUM - 100 >= RANGE_MIN_NUMBER) {
        MIN_NUM -= 100;
        MFS.write(MIN_NUM);
      }
      break;

    default:
      break;
  }
}

void executeStarted() {
  int randomNumber = random(MIN_NUM, MAX_NUM + 1);

  MFS.beep(50);
  delay(50);

  int unidade  = randomNumber % 10;
  int dezena   = (randomNumber / 10)  % 10;
  int centena  = (randomNumber / 100) % 10;
  int milhar   = (randomNumber / 1000) % 10;

  ledModeValues ledStates[] = {
    LED_ALL_OFF,
    LED_1_ON,
    LED_2_ON,
    LED_3_ON,
    LED_4_ON
  };

  int ledIndex = 0;

  auto animationStep = [&](int valueToDisplay) -> bool {
    MFS.write(valueToDisplay);

    setLed(ledStates[ledIndex]);
    ledIndex = (ledIndex + 1) % 5;

    byte btn = MFS.getButton();
    if (btn == BUTTON_1_SHORT_RELEASE) {
      handleInterruptAnimation();
      return false;
    }

    delay(100);
    return true;
  };

  for (int c = 0; c <= 9; c++) {
    int value = c * 1000 + c * 100 + c * 10 + c;
    if (!animationStep(value)) return;
  }

  MFS.write(unidade);

  for (int c = 0; c <= 9; c++) {
    int value = c * 1000 + c * 100 + c * 10 + unidade;
    if (!animationStep(value)) return;
  }

  MFS.write(dezena * 10 + unidade);

  for (int c = 0; c <= 9; c++) {
    int value = c * 1000 + c * 100 + dezena * 10 + unidade;
    if (!animationStep(value)) return;
  }

  MFS.write(centena * 100 + dezena * 10 + unidade);

  for (int c = 0; c <= 9; c++) {
    int value = c * 1000 + centena * 100 + dezena * 10 + unidade;
    if (!animationStep(value)) return;
  }

  MFS.write(randomNumber);

  MFS.blinkDisplay(DIGIT_ALL, ON);
  for (int i = 0; i < 3; i++) {
    MFS.beep(500);
    delay(500);
    delay(500);
  }
  MFS.blinkDisplay(DIGIT_ALL, OFF);

  setLed(LED_ALL_OFF);

  currentState = GENERATOR_STOPPED;
}

void setup() {
  MIN_NUM = RANGE_MIN_NUMBER;
  MAX_NUM = RANGE_MAX_NUMBER;
  MIN_SET = RANGE_NOT_SET;
  MAX_SET = RANGE_NOT_SET;

  Timer1.initialize();
  MFS.initialize(&Timer1);

  randomSeed(analogRead(A0));

  stop();
}

void loop() {
  switch (currentState) {
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

  delay(20);
}