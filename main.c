#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include "driver/mcpwm.h" 


#define PIN_STEP 26
#define PIN_DIR 27
#define PIN_TRIG 5
#define PIN_ECHO 18
#define PIN_BOTAO 14


#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);


volatile bool sentidoAvanco = true;
volatile bool mudarSentido = false;
float distancia_cm = 0;
uint32_t frequencia_pwm = 0; // Controla a velocidade do passo
bool motorAtivo = false;

//  Configurações Wi-Fi 
const char* ssid = "Wokwi-GUEST";
const char* password = "";
WiFiServer server(80);

//  Interrupção do Botão 
void IRAM_ATTR isr_botao() {
  sentidoAvanco = !sentidoAvanco;
  mudarSentido = true;
}

//  Configuração do MCPWM para Motor de Passo (via A4988)
void setupMCPWM() {
  Serial.println("Inicializando MCPWM para Driver A4988...");
  
  // O MCPWM gera os pulsos de STEP. O DIR é um pino digital normal.
  pinMode(PIN_DIR, OUTPUT);
  digitalWrite(PIN_DIR, sentidoAvanco);

  // Inicializa o pino GPIO para uso com MCPWM
  mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, PIN_STEP);

  // Configuração básica do PWM
  mcpwm_config_t pwm_config;
  pwm_config.frequency = 1000; // Frequência inicial (1kHz)
  pwm_config.cmpr_a = 50.0;    // Duty cycle de 50% (essencial para pulsos limpos)
  pwm_config.cmpr_b = 0.0;
  pwm_config.counter_mode = MCPWM_UP_COUNTER;
  pwm_config.duty_mode = MCPWM_DUTY_MODE_0; // Ativo em nível alto
  
  mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &pwm_config);
  
  // Inicia o motor parado
  mcpwm_stop(MCPWM_UNIT_0, MCPWM_TIMER_0);
}

// --- Função para ler o Sensor Ultrassônico HC-SR04 ---
float lerDistancia() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  long duration = pulseIn(PIN_ECHO, HIGH, 30000); // Timeout de 30ms para evitar travamentos
  if (duration == 0) return 400.0; // Se falhar ou timeout, assume distância máxima
  return (duration * 0.0343) / 2.0; // Converte tempo em distância (cm)
}

void setup() {
  Serial.begin(115200);
  
  // Configura pinos do sensor e botão
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  pinMode(PIN_BOTAO, INPUT_PULLUP);
  
  // Configura a interrupção externa
  attachInterrupt(digitalPinToInterrupt(PIN_BOTAO), isr_botao, FALLING);

  // Inicializa OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("Falha ao inicializar OLED SSD1306"));
    for(;;); // Trava se falhar
  }
  display.clearDisplay();
  display.setTextColor(WHITE);

  // Inicializa o Hardware MCPWM
  setupMCPWM();

  // Inicializa Wi-Fi (Diferencial)
  Serial.print("Conectando ao WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Conectado!");
  Serial.print("Endereço IP: ");
  Serial.println(WiFi.localIP());
  
  // Inicia o servidor Web na porta 80
  server.begin();
}

void loop() {
  // 1. Atualiza direção via interrupção
  if (mudarSentido) {
    digitalWrite(PIN_DIR, sentidoAvanco);
    Serial.print("Sentido alterado para: ");
    Serial.println(sentidoAvanco ? "Avanço" : "Retorno");
    mudarSentido = false;
  }

  // 2. Leitura do Sensor Ultrassônico
  distancia_cm = lerDistancia();

  // 3. CORREÇÃO DA LÓGICA DE CONTROLE (MCPWM Frequency)
  // Se o objeto estiver na faixa de operação (5cm a 100cm), o motor roda.
  if (distancia_cm > 5.0 && distancia_cm <= 100.0) {
    // Mapeia a distância (5 a 100) para frequência de passos (100Hz a 2000Hz)
    // Mais perto (5cm) = mais lento (100Hz) | Mais longe (100cm) = mais rápido (2000Hz)
    frequencia_pwm = map(distancia_cm, 5, 100, 100, 2000);
    
    // Atualiza a frequência no driver
    mcpwm_set_frequency(MCPWM_UNIT_0, MCPWM_TIMER_0, frequencia_pwm);
    
    // AJUSTE CRUCIAL:
    // Garante o Duty Cycle em 50% E forçamos o início/reaplicação da geração de sinal
    // para garantir que o A4988 receba os pulsos com os parâmetros atualizados.
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, 50.0);
    mcpwm_start(MCPWM_UNIT_0, MCPWM_TIMER_0); // Força a re-aplicação dos parâmetros
    
    motorAtivo = true;
  } else {
    // Se estiver fora da faixa (muito perto ou muito longe), para o motor imediatamente.
    if (motorAtivo) {
      mcpwm_stop(MCPWM_UNIT_0, MCPWM_TIMER_0); // Para a geração de pulsos
      frequencia_pwm = 0; // Zera para visualização
      motorAtivo = false;
    }
  }

  // 4. Atualiza o Display OLED (Comunicação I2C)
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.println("--- ESTEIRA PASSO ---");
  
  display.setCursor(0, 15);
  display.print("Dist: ");
  display.print(distancia_cm, 1);
  display.println(" cm");

  display.setCursor(0, 30);
  display.print("Status: ");
  display.setTextColor(motorAtivo ? BLACK, WHITE : WHITE); // Inverte cores se rodando
  display.println(motorAtivo ? " MOVENDO " : " PARADO ");
  display.setTextColor(WHITE);

  display.setCursor(0, 45);
  display.print("Freq STEP: ");
  display.print(frequencia_pwm);
  display.println(" Hz");

  display.display();

  // 5. Trata Clientes Wi-Fi (Diferencial - Web Server)
  WiFiClient client = server.available();
  if (client) {
    String currentLine = "";
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        if (c == '\n') {
          if (currentLine.length() == 0) {
            // Resposta HTTP padrão
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();
            // Página HTML
            client.println("<!DOCTYPE html><html><head><meta charset=\"UTF-8\">");
            client.println("<title>Telemetria Esteira</title>");
            client.println("<meta http-equiv=\"refresh\" content=\"1\">"); // Auto-refresh 1s
            client.println("</head><body style=\"font-family: sans-serif; text-align: center;\">");
            client.println("<h2 style=\"color: #333;\">Status do Sistema MCPWM</h2>");
            client.print("<p><b>Distância medida:</b> <span style=\"font-size: 1.2em; color: blue;\">");
            client.print(distancia_cm);
            client.println(" cm</span></p>");
            client.print("<p><b>Status Motor:</b> ");
            client.print(motorAtivo ? "<span style='color: green;'>Rodando</span>" : "<span style='color: red;'>Parado</span>");
            client.println("</p>");
            client.print("<p><b>Frequência de Passos:</b> ");
            client.print(frequencia_pwm);
            client.println(" Hz</p>");
            client.print("<p><b>Direção Física:</b> ");
            client.print(sentidoAvanco ? "Avanço" : "Retorno");
            client.println("</p></body></html>");
            client.println();
            break;
          } else {
            currentLine = "";
          }
        } else if (c != '\r') {
          currentLine += c;
        }
      }
    }
    client.stop(); // Fecha conexão
  }
  
  delay(10); // Pequeno delay para estabilidade do simulador
}