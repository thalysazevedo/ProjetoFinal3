# ProjetoFinal3
# Sistema Inteligente de Esteira de Inspeção Automática

**Aluno:** Thalys de Azevedo Leite- 15580274

-## 1. O que o projeto faz (Lógica de Funcionamento)
Este projeto simula uma esteira industrial inteligente que transporta peças para inspeção. Em vez de rodar sempre na mesma velocidade, o sistema "enxerga" o ambiente e reage a ele em tempo real. 

**Na prática, quando o sistema é ligado, o seguinte comportamento acontece:**
1. **Leitura Contínua:** O sensor ultrassônico HC-SR04 monitora constantemente a distância à sua frente.
2. **Área Segura (Motor Parado):** Se não houver nenhum objeto próximo (distância maior que 100cm) ou se o objeto chegar perto demais a ponto de colidir (menos de 5cm), o sistema corta os pulsos e o motor de passo fica totalmente estático.
3. **Aproximação e Redução de Velocidade:** Quando um objeto entra na zona de detecção (entre 100cm e 5cm), o motor começa a girar. A inteligência do sistema atua aqui: **quanto mais perto o objeto chega, mais devagar a esteira roda**. Isso simula a redução de velocidade necessária para inspecionar detalhadamente uma peça que está chegando.
4. **Inversão de Rotação:** A qualquer momento, se o operador pressionar o botão físico, a esteira para e inverte o seu sentido de rotação imediatamente.
5. **Telemetria:** O operador pode acompanhar a distância da peça, o status do motor e a velocidade (em Hz) tanto pela tela OLED física acoplada à máquina, quanto por uma página de internet local gerada pelo próprio ESP32 via Wi-Fi.

---

## 2. Conceitos Envolvidos na Implementação
A implementação deste sistema baseia-se em conceitos avançados de controle de hardware em sistemas embarcados:
* **Geração de Sinais por Hardware (MCPWM):** Diferente da função `delay()` ou `delayMicroseconds()` comum para motores de passo (que trava o processador), o módulo MCPWM utiliza os temporizadores de hardware do ESP32 para gerar pulsos elétricos exatos em segundo plano.
* **Modulação de Frequência Dinâmica:** Para alterar a velocidade do motor de passo sem perder torque, mantivemos a largura do pulso (*Duty Cycle*) fixa em 50% e variamos dinamicamente apenas a **frequência** do sinal PWM (de 100Hz a 2000Hz).
* **Interrupções de Hardware (ISR):** O controle de direção do motor é feito por um botão configurado com interrupção externa. Isso garante que o comando do operador seja executado instantaneamente, atropelando o fluxo normal do código.
* **Sistemas Concorrentes (Multitarefa não-preemptiva):** O ESP32 realiza a leitura de sensores, atualização do display I2C e roteamento de rede Wi-Fi dentro do mesmo laço, sem causar engasgos no motor, comprovando a eficiência de delegar o PWM ao hardware.

---

## 3. Bibliotecas Empregadas
* **`driver/mcpwm.h`:** Biblioteca nativa e de baixo nível do ecossistema ESP-IDF, voltada para controle industrial de motores. Permite configurar geradores de PWM, definindo pinos, *Duty Cycle*, frequência e estados dos registradores de forma direta.
* **`Wire.h`:** Biblioteca padrão do Arduino Core para comunicação no barramento I2C (utilizada pelo display).
* **`Adafruit_GFX.h` e `Adafruit_SSD1306.h`:** Framework gráfico utilizado para renderizar textos informativos na tela OLED de 128x64 pixels.
* **`WiFi.h`:** Biblioteca que gerencia o rádio Wi-Fi do ESP32, utilizada neste projeto para instanciar um *Web Server* na porta 80 e fornecer dados de telemetria.

---

## 4. Diagrama do Circuito e Montagem (Wokwi)
A montagem no Wokwi exigiu configurações específicas. Destaca-se a necessidade de interligar os pinos **RST** e **SLP** do driver A4988 para desativar o modo de hibernação padrão do componente, além de garantir alimentação de 5V no pino VMOT para que o motor virtual tivesse energia simulada suficiente para girar.

| Componente | Pino do Componente | Conexão ESP32 / Circuito |
| :--- | :--- | :--- |
| **Driver A4988** | `STEP` | GPIO 26 |
| **Driver A4988** | `DIR` | GPIO 27 |
| **Driver A4988** | `RST` e `SLP` | Interligados entre si (Jumper) |
| **Driver A4988** | `VMOT` | 5V |
| **HC-SR04** | `TRIG` | GPIO 5 |
| **HC-SR04** | `ECHO` | GPIO 18 |
| **OLED SSD1306** | `SDA` e `SCL` | GPIO 21 e GPIO 22 |
| **Botão (Pushbutton)**| Sinal | GPIO 14 (Com Pull-up) |

**Captura do Diagrama Implementado:**
> *[INSERIR IMAGEM AQUI - Faça o upload do print do seu circuito completo no Wokwi]*

---

## 5. Registros da Simulação em Funcionamento
O sistema foi validado no simulador computacional. Deslizar a barra de distância no HC-SR04 causou alteração imediata na contagem de passos (Steps) do motor, bem como a atualização dos dados nos monitores.

**Esteira em Movimento (Objeto detectado na faixa de operação):**
> *[INSERIR IMAGEM AQUI - Print do motor rodando com o objeto entre 5 e 100cm]*

**Esteira Parada (Objeto fora da faixa ou perto demais):**
> *[INSERIR IMAGEM AQUI - Print do status no momento em que o objeto passa de 100cm]*

---

## 6. Trechos de Código em Destaque

**A. Configuração Estrutural do MCPWM**
Configuramos o sinal no pino associado (STEP) com um Duty Cycle perfeito de 50%, garantindo que os pulsos quadrados acionem corretamente as bobinas através do driver.
```cpp
mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, PIN_STEP);
mcpwm_config_t pwm_config;
pwm_config.frequency = 1000; // Começa em 1kHz
pwm_config.cmpr_a = 50.0;    // 50% de Duty Cycle
pwm_config.cmpr_b = 0.0;
pwm_config.counter_mode = MCPWM_UP_COUNTER;
pwm_config.duty_mode = MCPWM_DUTY_MODE_0;
mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &pwm_config);
