/* Programa de teste dos Botões do Shield Multi-função
  Blog Eletrogate - https://blog.eletrogate.com/guia-completo-do-shield-multi-funcoes-para-arduino
  Arduino UNO - IDE 1.8.5 - Shield Multi-função para Arduino
  Gustavo Murta   13/junho/2018
  Baseado em http://www.cohesivecomputing.co.uk/hackatronics/arduino-multi-function-shield/part-1/
*/
 
#include <TimerOne.h>                     // Biblioteca TimerOne 
#include <MultiFuncShield.h>              // Biblioteca Multifunction shield
 
void setup()
{
  Serial.begin(9600);                              // monitor serial 9600 bauds
  Timer1.initialize();                             // inicializa o Timer 1
  MFS.initialize(&Timer1);                         // initializa a biblioteca multi-função
  Serial.println("Pressione um ou mais Botoes");   // imprime mensagem
}
 
void loop()
{
  byte btn = MFS.getButton();                            // verifica o status dos botões
 
  if (btn)                                               // se valor <> zero, botão pressionado  
  {
    byte buttonNumber = btn & B00111111;                 // AND bit a bit para identificar o botão
    byte buttonAction = btn & B11000000;                 // AND bit a bit para identificar o evento
 
    Serial.print("BOTAO_");                              // imprime mensagem
    Serial.write(buttonNumber + '0');                    // imprime o caractere da tabela ASCII correspondente ao código fornecido
    Serial.print("_");                                   // imprime caracttere sublinhado
 
    if (buttonAction == BUTTON_PRESSED_IND)              // compara o evento do botão 
    {
      Serial.println("PRESSIONADO");                     // imprime mensagem
    }
    else if (buttonAction == BUTTON_SHORT_RELEASE_IND)   // compara o evento do botão
    {
      Serial.println("SOLTA RAPIDO");                    // imprime mensagem
    }
    else if (buttonAction == BUTTON_LONG_PRESSED_IND)    // compara o evento do botão
    {
      Serial.println("PRESSIONADO LONGO");               // imprime mensagem
    }
    else if (buttonAction == BUTTON_LONG_RELEASE_IND)    // compara o evento do botão
    {
      Serial.println("SOLTA LONGO");                     // imprime mensagem
    }
  }
}
