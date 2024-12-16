#include "Arduino.h"
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <ESP32Servo.h>

// Configurações do Display TFT
#define TFT_CS   5
#define TFT_DC   26
#define TFT_MOSI 23
#define TFT_MISO 19
#define TFT_SCLK 18
#define TFT_RST  -1 // Conectar ao 3V3
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST, TFT_MISO);

// Pinos
#define LDR_PIN 15
#define LDR_PIN2 14
#define SWITCH_1_POS1 34
#define SWITCH_1_POS2 35
#define SWITCH_2_POS1 19
#define SWITCH_2_POS2 21
#define LIGHT_PIN_1 32
#define LIGHT_PIN_2 33
#define LIGHT_PIN_3 2
#define LIGHT_PIN_4 22
#define POT_PIN 4
#define SERVO_PIN_1 13
#define SERVO_PIN_2 12
#define SERVO_PIN_3 25
#define SERVO_PIN_4 27

Servo servo1;
Servo servo2;
Servo servo3;
Servo servo4;

// Prototipacao das tarefas
void Task1(void *pvParameters); // Atualizacao do grafico
void Task3(void *pvParameters); // Logica principal do sistema
void Task4(void *pvParameters); // Controle do servo
void Task5(void *pvParameters); // Leitura do potenciometro
void TemporaryTask(void *pvParameters);
static bool tempTaskCreated = false;
// Variáveis e fila para comunicação entre tarefas
QueueHandle_t queue;
SemaphoreHandle_t xDisplaySemaphore;
int state = 0; // Estados: 0 - OFF, 1 - ON, 2 - AUTO ON, etc.

void setup() {
  Serial.begin(115200);
  delay(1000); // Pequeno atraso para estabilizar

  // Configuração de pinos
  pinMode(LDR_PIN, INPUT);
  pinMode(LDR_PIN2, INPUT);
  pinMode(SWITCH_1_POS1, INPUT_PULLDOWN);
  pinMode(SWITCH_1_POS2, INPUT_PULLDOWN);
  pinMode(SWITCH_2_POS1, INPUT_PULLDOWN);
  pinMode(SWITCH_2_POS2, INPUT_PULLDOWN);
  pinMode(LIGHT_PIN_1, OUTPUT);
  pinMode(LIGHT_PIN_2, OUTPUT);
  pinMode(LIGHT_PIN_3, OUTPUT);
  pinMode(LIGHT_PIN_4, OUTPUT);

  // Inicializacao do servo e display
  servo1.attach(SERVO_PIN_1);
  servo2.attach(SERVO_PIN_2);
  servo3.attach(SERVO_PIN_3);
  servo4.attach(SERVO_PIN_4);

  tft.begin();
  tft.setRotation(2);
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.setCursor(12, 15);
  tft.println("Sistema de Farois");
  tft.setCursor(50, 35);
  tft.println("Direcionais");
  tft.setCursor(55, 60);
  tft.println("2024/2025");
  tft.drawRect(5, 5, 230, 90, ILI9341_RED);
  tft.drawRect(6, 6, 228, 88, ILI9341_RED);
  tft.drawRect(3, 105, 232, 40, ILI9341_WHITE);

  //organização dos indicadores de farois
  tft.drawCircle(30, 175, 26, ILI9341_BLUE);
  tft.drawCircle(85, 175, 26, ILI9341_GREEN);

  //Setup de motores
  servo1.write(100);
  servo2.write(100);
  delay(1000);
  servo1.write(60);
  servo2.write(130);
  delay(1000);
  servo1.write(130);
  servo2.write(60);
  delay(1000);
  servo1.write(100);
  servo2.write(100);
  servo3.write(90);
  servo4.write(30);
  delay(1000);
  servo3.write(60);
  servo4.write(60);

  // Criacao da fila e tarefas
  queue = xQueueCreate(10, sizeof(int));
  xDisplaySemaphore = xSemaphoreCreateMutex();

  if (queue != NULL && xDisplaySemaphore != NULL) {
    xTaskCreate(Task1, "Task1", 4096, NULL, 2, NULL);
    xTaskCreate(Task3, "Task3", 4096, NULL, 2, NULL);
    xTaskCreate(Task4, "Task4", 4096, NULL, 1, NULL);
    xTaskCreate(Task5, "Task5", 4096, NULL, 3, NULL);
  } else {
    Serial.println("EXIT CODE: NO_MEMORY");
    while (1);
  }
}

void loop() {
  // O loop permanece vazio com FreeRTOS
}

// Implementacao da Task1
void Task1(void *pvParameters) {
  int lastAngleValue = 0; // Ultimo valor do angulo
  for (;;) {
    int angleValue = 0;
    if (xQueueReceive(queue, &angleValue, portMAX_DELAY) == pdPASS) {
      if (xSemaphoreTake(xDisplaySemaphore, portMAX_DELAY) == pdTRUE) {
        // Limpar a barra anterior
        if (lastAngleValue < 0) { // Limpar parte esquerda
          int lastLeftLength = abs(lastAngleValue);
          tft.fillRect(119 - lastLeftLength, 106, lastLeftLength, 38, ILI9341_BLACK); // Preenche com preto
        } else { // Limpar parte direita
          int lastRightLength = lastAngleValue;
          tft.fillRect(119, 106, lastRightLength, 38, ILI9341_BLACK); // Preenche com preto
        }

        // Desenhar nova barra
        if (angleValue < 0) { // Atualizar parte esquerda
          int leftLength = abs(angleValue);
          tft.fillRect(119 - leftLength, 106, leftLength, 38, ILI9341_GREEN); // Preenche com verde
        } else { // Atualizar parte direita
          int rightLength = angleValue;
          tft.fillRect(119, 106, rightLength, 38, ILI9341_GREEN); // Preenche com verde
        }

        // Liberar semaforo apos atualizacao
        xSemaphoreGive(xDisplaySemaphore);
      }
      // Atualizar o ultimo valor
      lastAngleValue = angleValue;
    }

    vTaskDelay(pdMS_TO_TICKS(100)); // Pequeno atraso para evitar execucao continua
  }
}

// Implementacao da Task3
void Task3(void *pvParameters) {
  for (;;) {
    int ldrValue = analogRead(LDR_PIN);
    int ldrValue2 = analogRead(LDR_PIN2);
    bool switch1 = digitalRead(SWITCH_1_POS1);
    bool switch2 = digitalRead(SWITCH_1_POS2);
    bool switch3 = digitalRead(SWITCH_2_POS1);
    bool switch4 = digitalRead(SWITCH_2_POS2);

    if (xSemaphoreTake(xDisplaySemaphore, portMAX_DELAY) == pdTRUE) {
      // Determinacao do estado baseado nos sensores e switches
      switch (state) {
        case 1:
        	tempTaskCreated = false;
        	tft.fillCircle(85, 175, 25, ILI9341_BLACK);
          tft.fillCircle(30, 175, 25, ILI9341_BLUE);
          digitalWrite(LIGHT_PIN_1, 1);
          digitalWrite(LIGHT_PIN_2, 0);
          digitalWrite(LIGHT_PIN_3, 1);
          digitalWrite(LIGHT_PIN_4, 0);
          servo3.write(30);
          servo4.write(30);
          Serial.print("\nSYSTEM ON");
          break;
        case 2:
        	tempTaskCreated = false;
          tft.fillCircle(30, 175, 25, ILI9341_BLUE);
          tft.fillCircle(85, 175, 25, ILI9341_BLACK);
          tft.drawChar(220, 165, 'A', ILI9341_WHITE, ILI9341_BLACK, 3);
          digitalWrite(LIGHT_PIN_1, 1);
                    digitalWrite(LIGHT_PIN_2, 0);
                    digitalWrite(LIGHT_PIN_3, 1);
                    digitalWrite(LIGHT_PIN_4, 0);
          servo3.write(30);
          servo4.write(30);
          Serial.print("\nSYSTEM AUTO ON");
          break;
        case 3:
        	tempTaskCreated = false;
          tft.fillCircle(30, 175, 25, ILI9341_BLACK);
          tft.fillCircle(85, 175, 25, ILI9341_BLACK);
          tft.drawChar(220, 165, 'A', ILI9341_WHITE, ILI9341_BLACK, 3);
          Serial.print("\nSYSTEM AUTO OFF");
          digitalWrite(LIGHT_PIN_1, 0);
          digitalWrite(LIGHT_PIN_2, 0);
          digitalWrite(LIGHT_PIN_3, 0);
          digitalWrite(LIGHT_PIN_4, 0);
          servo3.write(60);
          servo4.write(60);
          break;
        case 4:
        	tempTaskCreated = false;
          tft.fillCircle(85, 175, 25, ILI9341_GREEN);
          tft.fillCircle(30, 175, 25, ILI9341_BLACK);
          tft.drawChar(220, 165, 'A', ILI9341_WHITE, ILI9341_BLACK, 3);
          Serial.print("\nSYSTEM AUTO MED");
          digitalWrite(LIGHT_PIN_1, 0);
          digitalWrite(LIGHT_PIN_2, 1);
          digitalWrite(LIGHT_PIN_3, 0);
          digitalWrite(LIGHT_PIN_4, 1);
          servo3.write(60);
          servo4.write(60);
          break;
        case 5:
        	tempTaskCreated = false;
        	tft.fillCircle(30, 175, 25, ILI9341_BLACK);
          tft.fillCircle(85, 175, 25, ILI9341_GREEN);
          Serial.print("\nSYSTEM MED");
          digitalWrite(LIGHT_PIN_1, 0);
          digitalWrite(LIGHT_PIN_2, 1);
          digitalWrite(LIGHT_PIN_3, 0);
          digitalWrite(LIGHT_PIN_4, 1);
          servo3.write(60);
          servo4.write(60);
          break;
        default:
        	tft.fillCircle(30, 175, 25, ILI9341_BLACK);
        	tft.fillCircle(85, 175, 25, ILI9341_BLACK);
        	tft.drawChar(220, 165, 'A', ILI9341_BLACK, ILI9341_BLACK, 3);
          servo3.write(60);
          servo4.write(60);
          digitalWrite(LIGHT_PIN_1, 0);
          digitalWrite(LIGHT_PIN_2, 0);
          digitalWrite(LIGHT_PIN_3, 0);
          digitalWrite(LIGHT_PIN_4, 0);
          Serial.print("\nSYSTEM OFF");

            if (!tempTaskCreated) {
              BaseType_t result = xTaskCreate(TemporaryTask, "TempTask", 2048, NULL, 1, NULL);
              if (result == pdPASS) {
                tempTaskCreated = true;
              } else {
                Serial.println("Erro: Não foi possível criar a tarefa temporária.");
              }
            }
          break;
      }
      xSemaphoreGive(xDisplaySemaphore);
    }

    if (switch1) {
      state = switch3 ? 1 : (switch4 ? 5 : state);
    } else if (switch2) {
      state = (ldrValue < 500) ? (switch4 ? 4 : (switch3 && ldrValue2 > 500 ? 4 : 2)) : 3;
    } else {
      state = 0;
    }

    Serial.println(ldrValue2);
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

// Implementacao da Task4
void Task4(void *pvParameters) {
  for (;;) {
    int angleValue = 0;
    if (xQueueReceive(queue, &angleValue, portMAX_DELAY) == pdPASS) {
      int leftAngle = map(angleValue, -115, 115, 130, 45);
      int rightAngle = map(angleValue, -115, 115, 130, 45);
      servo1.write(leftAngle);
      servo2.write(rightAngle);



    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

// Implementacao da Task5
void Task5(void *pvParameters) {
  for (;;) {
    int potValue = analogRead(POT_PIN);
    int angleValue = map(potValue, 0, 4095, -115, 115);
    if (xQueueSend(queue, &angleValue, portMAX_DELAY) == pdPASS) {
      // Sucesso ao enviar
    } else {
      Serial.println("Erro: Não foi possível enviar para a fila!");
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}


void TemporaryTask(void *pvParameters) {
 
  for (int i = 0; i < 2; i++) {
    digitalWrite(LIGHT_PIN_4, HIGH);
    digitalWrite(LIGHT_PIN_2, LOW);
    vTaskDelay(pdMS_TO_TICKS(500));
    digitalWrite(LIGHT_PIN_4, LOW);
    digitalWrite(LIGHT_PIN_2, HIGH);
    vTaskDelay(pdMS_TO_TICKS(500));
  }
  
  vTaskDelete(NULL); // Apaga a própria tarefa
}
