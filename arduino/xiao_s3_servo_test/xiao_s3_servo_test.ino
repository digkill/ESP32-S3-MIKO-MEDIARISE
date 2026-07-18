// Сигнальные пины серво переводим в высокоимпедансное состояние (INPUT),
// чтобы XIAO гарантированно не подавал импульсы на серво.
static const int kServoSignalPins[] = { D1, D2, D8, D9 };

static void servos_safe_off() {
  for (int pin : kServoSignalPins) {
    ledcDetach(pin);          // снять возможный LEDC-канал с пина
    pinMode(pin, INPUT);      // Hi-Z: никаких PWM-импульсов
  }
}

void setup() {
  Serial.begin(115200);
  delay(2000);

  servos_safe_off();
  Serial.println("Servo signal pins (D1,D2,D8,D9) -> INPUT (Hi-Z), servos disabled");

  Serial.printf("Flash size: %u MB\n", ESP.getFlashChipSize() / 1024 / 1024);
  Serial.printf("PSRAM size: %u MB\n", ESP.getPsramSize() / 1024 / 1024);
}

void loop() {}