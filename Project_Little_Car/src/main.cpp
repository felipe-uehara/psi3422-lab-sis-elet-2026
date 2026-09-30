#include "mbed.h"
#include "nRF24L01P.h"
#include "MMA8451Q.h"

// O endereço I2C do acelerômetro na placa FRDM-KL25Z é 0x1D
#define MMA8451_I2C_ADDRESS (0x1D << 1)

// Pinos I2C internos da placa (SDA, SCL)
MMA8451Q acc(PTE25, PTE24, MMA8451_I2C_ADDRESS);

#define MOSI    PTD2    
#define MISO    PTD3    
#define SCK     PTC5    
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

const float distMax = 26.0;
const float PULSOS_POR_VOLTA = 31.0; 
const float DIAMETRO_RODA_CM = 8.45; 
const float CIRCUNFERENCIA_RODA = 3.14159 * DIAMETRO_RODA_CM;

const float baseSpeed = 0.8; //varia de 0.0 até 1.0
const float Kp = 0.20f; 

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
    // 1. Lê a aceleração lateral. 
    float erro_lateral = acc.getAccY(); 
    
    // 2. Aplica a correção diferencial
    float pwm_esq = (0.85f * baseSpeed) - erro_lateral*Kp; 
    float pwm_dir = baseSpeed + erro_lateral*Kp;
    
    // 3. Saturação (Clamp) 
    if (pwm_esq > 1.0f) pwm_esq = 1.0f;
    if (pwm_esq < 0.0f) pwm_esq = 0.0f;
    if (pwm_dir > 1.0f) pwm_dir = 1.0f;
    if (pwm_dir < 0.0f) pwm_dir = 0.0f;
    
    // 4. Atualiza a Ponte H
    motor_esq_in1.write(pwm_esq);
    motor_esq_in2.write(0.0f);
    motor_dir_in1.write(pwm_dir);
    motor_dir_in2.write(0.0f);
}

// Força um disparo do ultrassom e aguarda a resposta (usado no ciclo de giro)
void atualizar_ultrassom_bloqueante() {
    ultrassom_trig = 1;
    wait_us(10);
    ultrassom_trig = 0;
    // Em vez de esperar um tempo fixo, espera ativamente que a leitura termine
    // (A ISR isr_echo_descida atualizará distancia_frontal_cm)
    ThisThread::sleep_for(50ms); 
}

// Gira ativamente até encontrar um caminho livre
// dir = 0 (Esquerda), dir = 1 (Direita)
void procurar_caminho_livre(int dir) {
    float limite_seguro = distMax + 45.0f;
    
    // Inicia a rotação
    if (dir == 0) { // Esquerda
        motor_esq_in1.write(0.0f);
        motor_esq_in2.write(0.46f * baseSpeed); // Ré
        motor_dir_in1.write(0.46f * baseSpeed); // Frente
        motor_dir_in2.write(0.0f);
    } else { // Direita
        motor_esq_in1.write(0.46f * baseSpeed); // Frente
        motor_esq_in2.write(0.0f);
        motor_dir_in1.write(0.0f);
        motor_dir_in2.write(0.46f * baseSpeed); // Ré
    }

    // Mantém-se a girar enquanto o caminho estiver bloqueado
    while (distancia_frontal_cm < limite_seguro) {
        atualizar_ultrassom_bloqueante();
    }
    
    // Encontrou um caminho livre!
    parar_motores();
}


/* --- 6. Loop Principal --- */
int main() {
    uint8_t id = acc.getWhoAmI();
    printf("Acelerometro OK! ID: 0x%02X\n", id); 

    radio.powerUp();
    radio.setTransferSize(TRANSFER_SIZE); 
    radio.setReceiveMode();
    radio.enable();

    encoder_esq.rise(&isr_conta_pulso_esq);
    encoder_dir.rise(&isr_conta_pulso_dir); 
    ultrassom_echo.rise(&isr_echo_subida);
    ultrassom_echo.fall(&isr_echo_descida);

    motor_esq_in1.period(0.01f);
    motor_esq_in2.period(0.01f);
    motor_dir_in1.period(0.01f);
    motor_dir_in2.period(0.01f);
    parar_motores();

    printf("Robo Iniciado. Radio frequencia configurada.\n");

    int direcao_fuga = 0; // 0 = Esquerda, 1 = Direita

    while (true) {
        // Disparar o sensor de Ultrassom no loop principal
        ultrassom_trig = 1;
        wait_us(10);
        ultrassom_trig = 0;

        float dist_esq = (pulsos_esq / PULSOS_POR_VOLTA) * CIRCUNFERENCIA_RODA;
        float dist_dir = (pulsos_dir / PULSOS_POR_VOLTA) * CIRCUNFERENCIA_RODA;
        float dist_media = (dist_esq + dist_dir) / 2.0;
        float tempo_segundos = timer_percurso.read_ms() / 1000.0; 

        float eixo_x = acc.getAccX();
        float eixo_y = acc.getAccY();
        float eixo_z = acc.getAccZ();

        if (radio.readable()) {
            char comando_recebido = 0;
            radio.read(NRF24L01P_PIPE_P0, &comando_recebido, 1);

            switch (comando_recebido) {
                case 'R': 
                    estado_atual = RUN;
                    timer_percurso.start(); 
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
       
        // 4. Máquina de Estados Principal
        if (estado_atual == RUN) {
            
            // Lógica Reativa de Desvio
            if (distancia_frontal_cm < distMax) {
                ajustar_cor_led(1, 1, 1); // Branco: Obstáculo detetado
                parar_motores();
                ThisThread::sleep_for(100ms); 

                // Gira para a esquerda até encontrar um caminho livre
                procurar_caminho_livre(0); 
                ThisThread::sleep_for(100ms); // Estabiliza após parar
            } 
            else {
                avancar();
                ajustar_cor_led(0, 1, 0); // Verde: Caminho livre
            }

            if (eixo_z < 0.0f || eixo_x > 0.8f) {
                parar_motores();
                ajustar_cor_led(1, 0, 1); // Roxo: Alerta de inclinação
            }
            
        }

        // 4. Máquina de Estados Principal
        /*if(estado_atual == RUN) {
            // Lógica Reativa de Desvio
            if (distancia_frontal_cm < distMax) {
                ajustar_cor_led(1, 1, 1); // Branco: Obstáculo detetado
                parar_motores();
                ThisThread::sleep_for(100ms); 

                // Gira para a direção atual até encontrar caminho livre
                procurar_caminho_livre(direcao_fuga); 
                
                // Inverte a direção para o PRÓXIMO obstáculo (Se era 0 vira 1, se era 1 vira 0)
                direcao_fuga = !direcao_fuga;

                ThisThread::sleep_for(100ms); // Estabiliza após parar
            } 
            else {
                avancar();
                ajustar_cor_led(0, 1, 0); // Verde: Caminho livre
            }
        }*/

        else if (estado_atual == STOP) {
            parar_motores();
            timer_percurso.stop(); 
            ajustar_cor_led(0, 0, 1); 
        } 
        else if (estado_atual == CLEAR) {
            pulsos_esq = 0; 
            pulsos_dir = 0;
            timer_percurso.reset(); 
            estado_atual = STOP; 
        } 
        else if (estado_atual == ENVIAR) {
            // Converte a distância para inteiro (0 a 255)
            int dist_inteira = (int)dist_dir;
            if (dist_inteira > 255) dist_inteira = 255;
            
            char dado_enviar = (char)dist_inteira;
            
            radio.setTransmitMode();
            // Envia o byte diretamente a partir do endereço da variável
            radio.write(NRF24L01P_PIPE_P0, &dado_enviar, 1);
            radio.setReceiveMode();
            
            estado_atual = STOP; 
        }

        ThisThread::sleep_for(50ms); 
    }
}