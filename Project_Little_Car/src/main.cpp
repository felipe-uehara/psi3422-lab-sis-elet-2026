#include "mbed.h"
#include "nRF24L01P.h"
#include "MMA8451Q.h"

// O endereço I2C do acelerômetro na placa FRDM-KL25Z é 0x1D
#define MMA8451_I2C_ADDRESS (0x1D << 1)

// Pinos I2C internos da placa (SDA, SCL)
MMA8451Q acc(PTE25, PTE24, MMA8451_I2C_ADDRESS);

#define MOSI    PTD2    
#define MISO    PTD3    
#define SCK     PTD1    
#define CS      PTD0    
#define CE      PTD5    
#define IRQ     PTA13   
#define TRANSFER_SIZE   1 

nRF24L01P radio(MOSI, MISO, SCK, CS, CE, IRQ); 

/* --- 1. Mapeamento de Pinos --- */
PwmOut motor_esq_in1(PTE20);
PwmOut motor_esq_in2(PTE21);
PwmOut motor_dir_in1(PTE22);
PwmOut motor_dir_in2(PTE23);

InterruptIn encoder_esq(PTA5);
InterruptIn encoder_dir(PTA4);

DigitalOut ultrassom_trig(PTA12);
InterruptIn ultrassom_echo(PTD4);
Timer timer_eco;
Timer timer_percurso; // Cronômetro global da corrida

// LEDs RGB Onboard (Lógica Invertida)
DigitalOut led_vermelho(PTB18, 1); 
DigitalOut led_verde(PTB19, 1);    
DigitalOut led_azul(PTD1, 1);      

/* --- 2. Variáveis de Controle e Odometria --- */
enum EstadoRobo { STOP, RUN, CLEAR, ENVIAR };
EstadoRobo estado_atual = STOP;

volatile uint32_t tempo_eco_us = 0;
volatile float distancia_frontal_cm = 999.0;

volatile int pulsos_esq = 0;
volatile int pulsos_dir = 0;

const float distMax = 15.0;
const float PULSOS_POR_VOLTA = 31.0; // Corrigido para as 31 listras
const float DIAMETRO_RODA_CM = 8.45; 
const float CIRCUNFERENCIA_RODA = 3.14159 * DIAMETRO_RODA_CM;

const float baseSpeed = 0.8; //varia de 0.0 até 1.0
const float Kp = 0.15f; 

/* --- 3. Função Auxiliar para os LEDs --- */
void ajustar_cor_led(bool r, bool g, bool b) {
    led_vermelho = !r;
    led_verde = !g;
    led_azul = !b;
}

/* --- 4. Rotinas de Interrupção (ISR) --- */
void isr_conta_pulso_esq() { pulsos_esq++; }
void isr_conta_pulso_dir() { pulsos_dir++; }

void isr_echo_subida() {
    timer_eco.reset();
    timer_eco.start();
}

void isr_echo_descida() {
    timer_eco.stop();
    tempo_eco_us = timer_eco.read_us();
    distancia_frontal_cm = tempo_eco_us / 58.0; 
}

/* --- 5. Funções de Movimento --- */
void parar_motores() {
    motor_dir_in1.write(0.0f);
    motor_dir_in2.write(0.0f);
    motor_esq_in1.write(0.0f);
    motor_esq_in2.write(0.0f);
}

void avancar() {
    /*motor_esq_in1.write(0.85 * baseSpeed);
    motor_esq_in2.write(0.0f);
    motor_dir_in1.write(baseSpeed);
    motor_dir_in2.write(0.0f);*/

    // 1. Lê a aceleração lateral. 
    // Assumindo que o eixo Y cruza o carrinho de uma roda à outra. Se for o X, troque aqui.
    float erro_lateral = acc.getAccY(); 
    
    // 2. Ganho Proporcional (Kp) - Este é o parâmetro de "Tuning" do seu controle.
    // Se o robô oscilar muito ("ziguezague"), diminua este valor. Se demorar a corrigir, aumente.
    
    // 3. Calcula a correção do PWM
    
    // 4. Aplica a correção diferencial (O fator 0.85 compensa a assimetria mecânica)
    float pwm_esq = (0.85f * baseSpeed) - erro_lateral*Kp; 
    float pwm_dir = baseSpeed + erro_lateral*Kp;
    
    // 5. Saturação (Clamp) para garantir que o sinal PWM não saia dos limites do Mbed (0.0 a 1.0)
    if (pwm_esq > 1.0f) pwm_esq = 1.0f;
    if (pwm_esq < 0.0f) pwm_esq = 0.0f;
    if (pwm_dir > 1.0f) pwm_dir = 1.0f;
    if (pwm_dir < 0.0f) pwm_dir = 0.0f;
    
    // 6. Atualiza a Ponte H
    motor_esq_in1.write(pwm_esq);
    motor_esq_in2.write(0.0f);
    motor_dir_in1.write(pwm_dir);
    motor_dir_in2.write(0.0f);
}

// Função de manobra não-bloqueante
void rodar_sobre_eixo(int dir) {
    if (dir == 1) { // Rodar para a direita
        motor_esq_in1.write(0.8 * baseSpeed);
        motor_esq_in2.write(0.0f);
        motor_dir_in1.write(0.0f);
        motor_dir_in2.write(0.4 * baseSpeed);
    } 
    else if (dir == 0) { // Rodar para a esquerda
        motor_dir_in1.write(0.8 * baseSpeed);
        motor_dir_in2.write(0.0f);
        motor_esq_in1.write(0.0f);
        motor_esq_in2.write(0.4f * baseSpeed);
    }
}

/* --- 6. Loop Principal --- */
int main() {
    // Config do acelerômetro
    uint8_t id = acc.getWhoAmI();
    printf("Acelerometro OK! ID: 0x%02X\n", id); 

    // Configuração do nRF24L01
    radio.powerUp();
    radio.setTransferSize(TRANSFER_SIZE); 
    radio.setReceiveMode();
    radio.enable();

    // Configuração dos Encoders e Ultrassom
    encoder_esq.rise(&isr_conta_pulso_esq);
    encoder_dir.rise(&isr_conta_pulso_dir); 
    ultrassom_echo.rise(&isr_echo_subida);
    ultrassom_echo.fall(&isr_echo_descida);

    // Configuração dos Motores (100 Hz)
    motor_esq_in1.period(0.01f);
    motor_esq_in2.period(0.01f);
    motor_dir_in1.period(0.01f);
    motor_dir_in2.period(0.01f);
    parar_motores();

    printf("Robo Iniciado. Radio frequencia configurada.\n");

    while (true) {
        // 1. Disparar o sensor de Ultrassom (Atualiza distancia_frontal_cm via ISR)
        ultrassom_trig = 1;
        wait_us(10);
        ultrassom_trig = 0;

        // 2. Atualizar Odometria, Tempo e Acelerômetro
        float dist_esq = (pulsos_esq / PULSOS_POR_VOLTA) * CIRCUNFERENCIA_RODA;
        float dist_dir = (pulsos_dir / PULSOS_POR_VOLTA) * CIRCUNFERENCIA_RODA;
        float dist_media = (dist_esq + dist_dir) / 2.0;
        float tempo_segundos = timer_percurso.read_ms() / 1000.0; // Converte para segundos decimais

        // Ler Acelerômetro nativamente
       float eixo_x = acc.getAccX();
        float eixo_y = acc.getAccY();
     float eixo_z = acc.getAccZ();

    printf("X: %.2f | Y: %.2f | Z: %.2f\n", eixo_x, eixo_y, eixo_z);



        // 3. Checar Comunicação Wireless (nRF24L01+)
        if (radio.readable()) {
            char comando_recebido = 0;
            radio.read(NRF24L01P_PIPE_P0, &comando_recebido, 1);

            switch (comando_recebido) {
                case 'R': 
                    estado_atual = RUN;
                    timer_percurso.start(); // Retoma a contagem 
                    printf("Comando recebido: RUN\n");
                    break;
                case 'S': 
                    estado_atual = STOP;
                    printf("Comando recebido: STOP\n");
                    break;
                case 'C': 
                    estado_atual = CLEAR;
                    printf("Comando recebido: CLEAR\n");
                    break;
                case 'D': 
                    estado_atual = ENVIAR;
                    printf("Comando recebido: D\n");
                    break;
            }
        }
       //estado_atual=RUN;

        // 4. Máquina de Estados Principal
        if (estado_atual == RUN) {
            
            // Lógica de Desvio Básica
            if (distancia_frontal_cm < distMax) {
                ajustar_cor_led(1, 1, 1); // Branco: Obstáculo
                rodar_sobre_eixo(0); // Gira para a esquerda até o caminho ficar livre
            } 
            else {
                ajustar_cor_led(0, 1, 0); // Verde: Caminho livre
                avancar();
            }

            // Exemplo de uso do acelerômetro: Parar o robô se ele capotar (Eixo Z invertido) ou levantar muito
            if (eixo_z < 0.0f || eixo_x > 0.8f) {
                parar_motores();
                ajustar_cor_led(1, 0, 1); // Roxo: Alerta de inclinação/capotamento!
            }
            
        } 
        else if (estado_atual == STOP) {
            parar_motores();
            timer_percurso.stop(); 
            ajustar_cor_led(0, 0, 1); // Azul: Parado
        } 
        else if (estado_atual == CLEAR) {
            pulsos_esq = 0; 
            pulsos_dir = 0;
            timer_percurso.reset(); 
            estado_atual = STOP; // Volta para STOP após limpar
        } 
        else if (estado_atual == ENVIAR) {
            char buffer_dist[64];
            // Agora enviamos a Distância, o Tempo em Segundos, e a inclinação em X
            int mensagem = sprintf(buffer_dist, "D:%.1fcm T:%.1fs X:%.1fg", dist_media, tempo_segundos, eixo_x);
            
            radio.setTransmitMode();
            radio.setTransferSize(mensagem);
            radio.write(NRF24L01P_PIPE_P0, buffer_dist, mensagem);
                    
            radio.setReceiveMode();
            radio.setTransferSize(1);
            
            estado_atual = STOP; // Volta para STOP após transmitir
        }

        ThisThread::sleep_for(50ms); 
    }
}