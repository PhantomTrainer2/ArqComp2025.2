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

// Estado geral do sorteador
GeneratorModeValues currentState;
int MIN_NUM, MAX_NUM;
RangeStatusValues MIN_SET, MAX_SET;

// ------- Funções auxiliares -------

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
      // Já apagamos todos acima
      break;
  }
}

void handleInterruptAnimation() {
  // ANIMATION_INTERRUPTED
  MFS.write("Intr");
  MFS.writeLeds(LED_ALL, OFF);

  // 3 beeps curtos de 50 ms intercalados por 50 ms de silêncio
  for (int i = 0; i < 3; i++) {
    MFS.beep(50);
    delay(50);
  }

  // Configuração volta para não ajustada
  MIN_SET = RANGE_NOT_SET;
  MAX_SET = RANGE_NOT_SET;

  // Volta para parado
  MFS.blinkDisplay(DIGIT_ALL, OFF);
  currentState = GENERATOR_STOPPED;
}

// Estado "parado": mostra "off" e apaga LEDs
void stop() {
  currentState = GENERATOR_STOPPED;
  MFS.write("off");
  MFS.writeLeds(LED_ALL, OFF);
  MFS.blinkDisplay(DIGIT_ALL, OFF);
}

// ------- Lógicas de cada estado -------

// 1) GENERATOR_STOPPED
void executeStopped() {
  byte btn = MFS.getButton();
  if (!btn) return;

  switch (btn) {
    case BUTTON_1_SHORT_RELEASE:
      // Só pode iniciar se os dois limites estiverem configurados
      if (MIN_SET == RANGE_SET && MAX_SET == RANGE_SET) {
        currentState = GENERATOR_STARTED;
      }
      break;

    case BUTTON_2_SHORT_RELEASE:
      // Mostrar valor máximo atual
      MFS.write(MAX_NUM);
      break;

    case BUTTON_2_LONG_PRESSED:
      // Entrar no modo de configuração do valor máximo
      currentState = SETTING_RANGE_MAX_NUM_STARTED;
      break;

    case BUTTON_3_SHORT_RELEASE:
      // Mostrar valor mínimo atual
      MFS.write(MIN_NUM);
      break;

    case BUTTON_3_LONG_PRESSED:
      // Entrar no modo de configuração do valor mínimo
      currentState = SETTING_RANGE_MIN_NUM_STARTED;
      break;

    default:
      // Outros eventos ignorados neste estado
      break;
  }
}

// 3) SETTING_RANGE_MAX_NUM_STARTED
void executeMaxNum() {
  // Mostrar valor máximo atual piscando
  MFS.write(MAX_NUM);
  MFS.blinkDisplay(DIGIT_ALL, ON);

  byte btn = MFS.getButton();
  if (!btn) return;

  switch (btn) {
    case BUTTON_1_SHORT_RELEASE:
      // Confirmar configuração
      MFS.blinkDisplay(DIGIT_ALL, OFF);
      MAX_SET = RANGE_SET;
      MFS.writeLeds(LED_1, ON); // LED 1 indica MAX configurado
      stop();
      break;

    case BUTTON_2_PRESSED:
    case BUTTON_2_LONG_PRESSED:
      // Incrementar de 100, sem ultrapassar o limite máximo
      if (MAX_NUM + 100 <= RANGE_MAX_NUMBER) {
        MAX_NUM += 100;
        MFS.write(MAX_NUM);
      }
      break;

    case BUTTON_3_PRESSED:
    case BUTTON_3_LONG_PRESSED:
      // Decrementar de 100, garantindo que MAX >= MIN
      if (MAX_NUM - 100 >= MIN_NUM) {
        MAX_NUM -= 100;
        MFS.write(MAX_NUM);
      }
      break;

    default:
      // A1 longo ignorado, outros também
      break;
  }
}

// 4) SETTING_RANGE_MIN_NUM_STARTED
void executeMinNum() {
  // Mostrar valor mínimo atual piscando
  MFS.write(MIN_NUM);
  MFS.blinkDisplay(DIGIT_ALL, ON);

  byte btn = MFS.getButton();
  if (!btn) return;

  switch (btn) {
    case BUTTON_1_SHORT_RELEASE:
      // Confirmar configuração
      MFS.blinkDisplay(DIGIT_ALL, OFF);
      MIN_SET = RANGE_SET;
      MFS.writeLeds(LED_2, ON); // LED 2 indica MIN configurado
      stop();
      break;

    case BUTTON_2_PRESSED:
    case BUTTON_2_LONG_PRESSED:
      // Incrementar de 100, sem ultrapassar MAX_NUM
      if (MIN_NUM + 100 <= MAX_NUM) {
        MIN_NUM += 100;
        MFS.write(MIN_NUM);
      }
      break;

    case BUTTON_3_PRESSED:
    case BUTTON_3_LONG_PRESSED:
      // Decrementar de 100, sem ficar abaixo de RANGE_MIN_NUMBER
      if (MIN_NUM - 100 >= RANGE_MIN_NUMBER) {
        MIN_NUM -= 100;
        MFS.write(MIN_NUM);
      }
      break;

    default:
      // A1 longo ignorado
      break;
  }
}

// 2) GENERATOR_STARTED
void executeStarted() {
  // Sorteia o número aleatório na faixa [MIN_NUM, MAX_NUM]
  int randomNumber = random(MIN_NUM, MAX_NUM + 1);

  // Beep curto de 50 ms no início
  MFS.beep(50);
  delay(50);

  // Separa dígitos do número sorteado
  int unidade  = randomNumber % 10;
  int dezena   = (randomNumber / 10)  % 10;
  int centena  = (randomNumber / 100) % 10;
  int milhar   = (randomNumber / 1000) % 10;

  // Lista de estados de LED para animação
  ledModeValues ledStates[] = {
    LED_ALL_OFF,
    LED_1_ON,
    LED_2_ON,
    LED_3_ON,
    LED_4_ON
  };

  int ledIndex = 0;

  // Função lambda para atualizar o LED, checar interrupção e esperar 100 ms
  auto animationStep = [&](int valueToDisplay) -> bool {
    // Atualiza display
    MFS.write(valueToDisplay);

    // Atualiza LED
    setLed(ledStates[ledIndex]);
    ledIndex = (ledIndex + 1) % 5;

    // Verifica se houve evento de botão A1 curto (interrupção)
    byte btn = MFS.getButton();
    if (btn == BUTTON_1_SHORT_RELEASE) {
      handleInterruptAnimation();
      return false; // interrompe animação
    }

    // Intervalo de 100 ms entre passos
    delay(100);
    return true; // continua animação
  };

  // --------- Animação dos displays (4 estágios) ---------

  // Estágio 1: 4 dígitos contando 0..9
  for (int c = 0; c <= 9; c++) {
    int value = c * 1000 + c * 100 + c * 10 + c;
    if (!animationStep(value)) return; // interrompido
  }
  // Final do estágio 1: fixa unidade
  MFS.write(unidade); // exibe 000U

  // Estágio 2: milhar, centena, dezena contam 0..9, unidade fixa
  for (int c = 0; c <= 9; c++) {
    int value = c * 1000 + c * 100 + c * 10 + unidade;
    if (!animationStep(value)) return;
  }
  // Final do estágio 2: fixa dezena
  MFS.write(dezena * 10 + unidade); // exibe 00TU

  // Estágio 3: milhar e centena contam 0..9, dezena e unidade fixas
  for (int c = 0; c <= 9; c++) {
    int value = c * 1000 + c * 100 + dezena * 10 + unidade;
    if (!animationStep(value)) return;
  }
  // Final do estágio 3: fixa centena
  MFS.write(centena * 100 + dezena * 10 + unidade); // exibe 0HTU

  // Estágio 4: milhar conta 0..9, demais fixos
  for (int c = 0; c <= 9; c++) {
    int value = c * 1000 + centena * 100 + dezena * 10 + unidade;
    if (!animationStep(value)) return;
  }
  // Final do estágio 4: mostra o número sorteado completo
  MFS.write(randomNumber);

  // --------- Conclusão da animação ---------

  // Displays piscando + 3 beeps longos de 500 ms com 500 ms de silêncio
  MFS.blinkDisplay(DIGIT_ALL, ON);
  for (int i = 0; i < 3; i++) {
    MFS.beep(500);
    delay(500);  // duração do beep
    delay(500);  // silêncio
  }
  MFS.blinkDisplay(DIGIT_ALL, OFF);

  // Apaga LEDs ao final
  setLed(LED_ALL_OFF);

  // Volta para o estado parado (valores continuam configurados)
  currentState = GENERATOR_STOPPED;
}

// ------- setup / loop -------

void setup() {
  MIN_NUM = RANGE_MIN_NUMBER;
  MAX_NUM = RANGE_MAX_NUMBER;
  MIN_SET = RANGE_NOT_SET;
  MAX_SET = RANGE_NOT_SET;

  Timer1.initialize();
  MFS.initialize(&Timer1);

  // Semente para gerador aleatório
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

  // Pequena pausa para não ficar consultando o botão em loop apertado
  delay(20);
}