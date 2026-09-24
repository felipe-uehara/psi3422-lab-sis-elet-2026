#include "mbed.h"
//#include "nRF24L01P.h" // Biblioteca do Rádio

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

// LEDs RGB Onboard (Lógica Invertida)
DigitalOut led_vermelho(PTB18, 1); 
DigitalOut led_verde(PTB19, 1);    
DigitalOut led_azul(PTD1, 1);      

// nRF24L01+ (MOSI, MISO, SCK, CSN, CE, IRQ)
// O pino IRQ não será usado aqui (vamos ler continuamente no loop)
//nRF24L01P radio(PTE1, PTE3, PTE2, PTE4, PTE5, NC); 

/* --- 2. Variáveis de Controle e Odometria --- */
enum EstadoRobo { STOP, RUN };
EstadoRobo estado_atual = STOP;

volatile uint32_t tempo_eco_us = 0;
volatile float distancia_frontal_cm = 999.0;

volatile int pulsos_esq = 0;
volatile int pulsos_dir = 0;

const float distMax = 15.0;

const float PULSOS_POR_VOLTA = 4.0; 
const float DIAMETRO_RODA_CM = 8.45; 
const float CIRCUNFERENCIA_RODA = 3.14159 * DIAMETRO_RODA_CM;

/* --- 3. Função Auxiliar para os LEDs --- */
void ajustar_cor_led(bool r, bool g, bool b) {
    led_vermelho = !r;
    led_verde = !g;
    led_azul = !b;
}

/* --- 4. Rotinas de Interrupção (ISR) --- */
void isr_conta_pulso_esq() {
    pulsos_esq++;
}

void isr_conta_pulso_dir() {
    pulsos_dir++;
}

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
    motor_esq_in1.write(0.85f);
    motor_esq_in2.write(0.0f);
    motor_dir_in1.write(1.0f);
    motor_dir_in2.write(0.0f);
}

void rodar_sobre_eixo(int dir) {
    int t =0 ;
    if (dir){
        motor_esq_in1.write(0.8f);
        motor_esq_in2.write(0.0f);
        motor_dir_in1.write(0.0f);
        motor_dir_in2.write(0.4f);
    }
    if (!dir){
        motor_dir_in1.write(0.8f);
        motor_dir_in2.write(0.0f);
        motor_esq_in1.write(0.0f);
        motor_esq_in2.write(0.4f);
    }
    if (dir == 2){
        motor_dir_in1.write(0.8f);
        motor_dir_in2.write(0.0f);
        motor_esq_in1.write(0.0f);
        motor_esq_in2.write(0.4f);
        t = 350;
        ThisThread::sleep_for(std::chrono::milliseconds(350-t));
    }
    ThisThread::sleep_for(std::chrono::milliseconds(800-t));
}

/* --- 6. Loop Principal --- */
int main() {
    // Configuração dos Encoders
    encoder_esq.rise(&isr_conta_pulso_esq);
    encoder_dir.rise(&isr_conta_pulso_dir); 

    // Configuração do Ultrassom
    ultrassom_echo.rise(&isr_echo_subida);
    ultrassom_echo.fall(&isr_echo_descida);

    // Configuração dos Motores (100 Hz)
    motor_esq_in1.period(0.01f);
    motor_esq_in2.period(0.01f);
    motor_dir_in1.period(0.01f);
    motor_dir_in2.period(0.01f);
    parar_motores();

    /*// Configuração do nRF24L01+
    radio.powerUp();
    radio.setTransferSize(1);     // Tamanho do pacote de recepção (1 byte para os comandos R,S,C,D)
    radio.setReceiveMode();
    radio.enable();*/

    printf("Robo Iniciado. Radio frequencia configurada.\n");

    while (true) {
        // 1. Disparar o sensor de Ultrassom
        ultrassom_trig = 1;
        wait_us(10);
        ultrassom_trig = 0;
        isr_echo_descida();

        // 2. Atualizar Odometria
        float dist_esq = (pulsos_esq / PULSOS_POR_VOLTA) * CIRCUNFERENCIA_RODA;
        float dist_dir = (pulsos_dir / PULSOS_POR_VOLTA) * CIRCUNFERENCIA_RODA;
        float dist_media = (dist_esq + dist_dir) / 2.0;

        // 3. Checar Comunicação Wireless (nRF24L01+)
        /*if (radio.readable()) {
            char comando_recebido = 0;
            radio.read(NRF24L01P_PIPE_P0, &comando_recebido, 1);

            switch (comando_recebido) {
                case 'R': 
                    estado_atual = RUN; 
                    printf("Comando recebido: RUN\n");
                    break;
                case 'S': 
                    estado_atual = STOP; 
                    printf("Comando recebido: STOP\n");
                    break;
                case 'C': 
                    pulsos_esq = 0; 
                    pulsos_dir = 0; 
                    printf("Comando recebido: CLEAR (Distancia apagada)\n");
                    break;
                case 'D': {
                    // Prepara uma string com a distância e envia para o PC
                    char buffer_dist[32];
                    int tamanho = sprintf(buffer_dist, "Dist: %.2f cm", dist_media);
                    
                    radio.setTransmitMode();
                    radio.setTransferSize(tamanho);
                    radio.write(NRF24L01P_PIPE_P0, buffer_dist, tamanho);
                    
                    // Retorna o rádio para o modo de escuta (Receber)
                    radio.setReceiveMode();
                    radio.setTransferSize(1);
                    printf("Comando recebido: D (Distancia enviada)\n");
                    break;
                }
            }
        }*/

        estado_atual = RUN;
        // 4. Lógica da Máquina de Estados e Movimento
        if (estado_atual == RUN) {
            //rodar_sobre_eixo(0);
            // Verifica o ultrassom e a meta de odometria
            if (distancia_frontal_cm < distMax) {
                rodar_sobre_eixo(0);
                isr_echo_descida();
                if(distancia_frontal_cm < distMax){
                    rodar_sobre_eixo(2);
                }
                /*if (distancia_frontal_cm < 15){
                    rodar_sobre_eixo(0);
                }*/
                ajustar_cor_led(1, 1, 1); // Branco: Obstáculo
            }
            /*else if (pulsos_dir >= PULSOS_POR_VOLTA || pulsos_esq >= PULSOS_POR_VOLTA) {
                estado_atual = STOP;
                parar_motores();
                ajustar_cor_led(0, 0, 1); // Azul: Chegou ao destino / completou as voltas
            } */
            else {
                avancar();
                ajustar_cor_led(0, 1, 0); // Verde: Andando livremente
            }
        } else {
            parar_motores();
            ajustar_cor_led(1, 0, 0); // Vermelho: Parado (STOP)
        }

        // Pequeno atraso para estabilizar leituras
        ThisThread::sleep_for(50ms); 
    }
}