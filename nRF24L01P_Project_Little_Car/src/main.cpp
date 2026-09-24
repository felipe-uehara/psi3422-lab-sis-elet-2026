#include "mbed.h"
#include "nRF24L01P.h"

// IMPORTANTE: Voltámos ao PTC5 para o SCK para evitar o erro fatal do LED!
#define MOSI    PTD2    
#define MISO    PTD3    
#define SCK     PTC5    
#define CS      PTD0    
#define CE      PTD5    
#define IRQ     PTA13   

#define TRANSFER_SIZE   1 // 1 byte para as letras R, S ou C

// Instancia o rádio
nRF24L01P radio(MOSI, MISO, SCK, CS, CE, IRQ);

// Cria o objeto para comunicar com o PC via USB
static UnbufferedSerial pc(USBTX, USBRX);

int main() {
    // Configura o terminal para não bloquear o código enquanto espera que digite
    pc.set_blocking(false);

    printf("\n--- Transmissor Manual (R, S, C) ---\n");
    printf("Pressione 'R', 'S' ou 'C' no teclado...\n\n");

    radio.powerUp();
    radio.setTransferSize(TRANSFER_SIZE);
    radio.setReceiveMode();
    radio.enable();

    char txData[TRANSFER_SIZE];
    char rxData[TRANSFER_SIZE];

    while (true) {
        // 1. VERIFICA SE O UTILIZADOR DIGITOU ALGO NO TECLADO (Monitor Série)
        char caractere_teclado;
        if (pc.read(&caractere_teclado, 1) > 0) {
            
            // Transforma letras minúsculas em maiúsculas (ex: 'r' passa a 'R')
            if (caractere_teclado >= 'a' && caractere_teclado <= 'z') {
                caractere_teclado -= 32; 
            }

            // Filtra e transmite apenas se a tecla for R, S ou C (Ignora a tecla Enter e outras)
            if (caractere_teclado == 'R' || caractere_teclado == 'S' || caractere_teclado == 'C') {
                
                txData[0] = caractere_teclado;
                
                // Troca para o modo TX, envia 1 byte e volta para RX instantaneamente
                radio.setTransmitMode();
                radio.write(NRF24L01P_PIPE_P0, txData, TRANSFER_SIZE);
                radio.setReceiveMode();
                
                printf("<< [COMANDO ENVIADO] : %c\n", txData[0]);
            }
        }

        // 2. RECEBE DADOS DO ROBÔ (Caso o robô envie alguma confirmação de volta)
        if (radio.readable()) {
            radio.read(NRF24L01P_PIPE_P0, rxData, TRANSFER_SIZE);
            printf(">> [RESPOSTA DO ROBO]: %c\n", rxData[0]);
        }

        ThisThread::sleep_for(10ms);
    }
}