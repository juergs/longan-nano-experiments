#include <Arduino.h>
#include <Stepper.h>
#include <stdio.h>

extern "C" {
#include "lcd/lcd.h"
}

namespace {

// set the number of steps of the motor 
constexpr int STEPS = 2048;

constexpr int IN1 = PA0;
constexpr int IN2 = PA1;
constexpr int IN3 = PA2;
constexpr int IN4 = PA3;

Stepper stepmotor(STEPS, IN1, IN2, IN3, IN4);

// char constexpr commands[] = "fbvr+-ce";
char constexpr commands[] = "frfr";

int idx = 0;

char next_command(void) {
  char const ch = commands[idx];
  idx = (idx + 1) % (sizeof(commands) - 1);

  return ch;
}

void standby(void)
{ // Motor soll nicht heiß werden
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

static void longan_oled_init(void)
{
    Lcd_Init();			// init OLED
    LCD_Clear(BLACK);
    BACK_COLOR = BLACK;
}

uint64_t longan_micros(void){
  return static_cast<uint64_t>(get_timer_value() * (SystemCoreClock / 4000000.0));
}

}

void setup() {
  micros();
  longan_oled_init();

  LCD_ShowString(24, 0, (u8 const *) "Starting!", GBLUE);
  delay(500);

  stepmotor.setSpeed(5);
  standby();
  LCD_ShowString(24, 0, (u8 const *) "Initialized!", GBLUE);
  delay(500);

#if 0
  LCD_Clear(BLACK);
  uint64_t const now = longan_micros();
  delay(5000);

  uint64_t const then = longan_micros();
  uint64_t const diff = then - now;

  char buf[64];
  sprintf(buf, "stdel %x%08x",
    static_cast<unsigned int>(diff >> 32),
    static_cast<unsigned int>(diff));
  LCD_ShowString(0, 1 * 16, (u8 const *) buf, GBLUE);

  while (1);
#endif
}

void loop() {
  // LCD_Clear(BLACK);

  char ch = next_command();
  char const chs = ch;
  int val;

  switch (ch) {    
    case 'f': val =  2048; ch='\0'; break;   // 
    case 'b': val = -2048; ch='\0'; break;   // 
    case 'v': val =  512;  ch='\0'; break;   // 
    case 'r': val = -512;  ch='\0'; break;   // 
    case '+': val =  16;   ch='\0'; break;   // 
    case '-': val = -16;   ch='\0'; break;   // 
    case 'c': val = 512;            break;   // run continous forward
    case 'e':  val = 0;    ch='\0'; break;   // 
    case '\0': val = 0;    ch='\0'; break;   // reset, nichts tun
    default: {             // falscher Befehl. Hilfe zeigen:
      // Serial.print(ch); Serial.println(": command not available, try: ");
      // Serial.println("  f/b 1T, v/r 1/4T, +/- 1/128T, c ontinue, e nd"); 
      val = 0; ch = '\0';   // reset
    } break;
  }

  char buf[64];
  sprintf(buf, "i %d val %d ch %c", idx, val, chs);
  LCD_ShowString(0, 0, (u8 const *) buf, GBLUE);

  // move the number of steps
  stepmotor.step(val);

  // go into standby state
  if (ch != 'c') {
    delay(10);          // warte auf Stillstand
    standby();
  }

  LCD_ShowString(24, 64, (u8 const *) "Loop End!", GBLUE);
  delay(500);
}