/* Programa de teste dos LEDs do Shield Multi-função
  Blog Eletrogate - https://blog.eletrogate.com/guia-completo-do-shield-multi-funcoes-para-arduino
  Arduino UNO - IDE 1.8.5 - Shield Multi-função para Arduino
  Gustavo Murta   13/junho/2018
  Baseado em http://www.cohesivecomputing.co.uk/hackatronics/arduino-multi-function-shield/part-1/
*/
 
#include <TimerOne.h>                     // Bibliotec TimerOne 
#include <MultiFuncShield.h>              // Biblioteca Multifunction shield
 
void setup()
{
  Timer1.initialize();                    // inicializa o Timer 1
  MFS.initialize(&Timer1);                // initializa a biblioteca Multi função
}
 
void loop()
{
  MFS.writeLeds(LED_ALL, ON);             // acende todos os LEDs
  delay(1000);                            // atraso de 1 segundo
  MFS.blinkLeds(LED_1, ON);               // pisca LED 1
  delay(1000);                            // atraso de 1 segundo
  MFS.blinkLeds(LED_2, ON);               // pisca LED 2
  delay(1000);                            // atraso de 1 segundo
  MFS.blinkLeds(LED_1 | LED_2, OFF);      // apaga LED 1 e 2
  MFS.blinkLeds(LED_3 | LED_4, ON);       // pisca LED 3 e 4
  delay(1000);                            // atraso de 1 segundo
  MFS.blinkLeds(LED_ALL, ON);             // pisca todos os LEDs
  delay(1000);                            // atraso de 1 segundo
  MFS.blinkLeds(LED_ALL, OFF);            // apaga o pisca dos LEDs
  MFS.writeLeds(LED_ALL, OFF);            // apaga o acender dos LEDs
  delay(1000);                            // atraso de 1 segundo
}
