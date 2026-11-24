// servidor6.cpp
// compila servidor6 -ocv -w
// Roda no Raspberry. Envia os quadros da câmera ao cliente6 e aplica nos motores as potências
// que o cliente6 manda de volta. Toda a lógica (seguir a placa, ler o dígito, virar) fica no cliente6.

#include "projeto.hpp"
#include <wiringPi.h>
#include <softPwm.h>

void desliga_motores() {                     // Chamada automaticamente por exit() e pelo return de main (registrada com atexit)
    softPwmStop(0);                          // Para a thread de PWM do pino 0 e deixa o pino em nível baixo
    softPwmStop(1);                          // Idem para o pino 1
    softPwmStop(2);                          // Idem para o pino 2
    softPwmStop(3);                          // Idem para o pino 3
    printf("server: motores desligados.\n"); // Confirma no terminal que os motores foram desligados
}

int main() {                                 // Função principal do servidor (roda no Raspberry)
    if (wiringPiSetup() == -1) erro("Erro: wiringPiSetup falhou"); // Inicializa a wiringPi (numeração própria de pinos); -1 = falha

    atexit(desliga_motores);                 // Roda desliga_motores() no return da main e também em exit(), inclusive dentro de erro()

    // Configura os 4 pinos (0, 1, 2, 3) para PWM por software
    // Pinos 0 e 1 controlam o Motor Esquerdo; Pinos 2 e 3 controlam o Motor Direito
    if (softPwmCreate(0, 0, 100)) erro("Erro: softPwmCreate pino 0"); // Pino 0: PWM com valor inicial 0 e faixa 0..100, retorno != 0 = falha
    if (softPwmCreate(1, 0, 100)) erro("Erro: softPwmCreate pino 1"); // Pino 1: segunda entrada da ponte H do motor esquerdo
    if (softPwmCreate(2, 0, 100)) erro("Erro: softPwmCreate pino 2"); // Pino 2: primeira entrada da ponte H do motor direito
    if (softPwmCreate(3, 0, 100)) erro("Erro: softPwmCreate pino 3"); // Pino 3: segunda entrada da ponte H do motor direito

    // Inicializa a Câmera
    VideoCapture w(0);                    // Abre a câmera de índice 0 (/dev/video0)
    if (!w.isOpened()) erro("Erro: nao consegui abrir a camera"); // Aborta antes de aceitar conexão se a câmera não abriu
    w.set(CV_CAP_PROP_FRAME_WIDTH, 320);  // Pede largura de 320 pixels
    w.set(CV_CAP_PROP_FRAME_HEIGHT, 240); // Pede altura de 240 pixels
    Mat_<COR> a;                          // Imagem colorida (BGR) que recebe cada quadro capturado

    SERVER server;                       // Cria o socket TCP, faz bind na porta 3490 e espera a conexão do cliente
    server.waitConnection();             // Bloqueia até o cliente (PC) conectar, depois para de esperar conexão
    
    while (true) {                         // Laço principal: um quadro por iteração
        BYTE sinal_ok = 0;                 // Sinal de controle; começa em 0 (parar)
        server.receiveBytes(1, &sinal_ok); // Espera 1 byte do cliente: 1 = enviar próximo quadro, 0 = encerrar
        if (sinal_ok == 0) break;          // Sai se o cliente mandar encerrar (ESC ou dígito 0/1 lido)

        w.grab();                          // Captura um quadro e o descarta, para usar uma imagem mais recente que a do buffer da câmera
        w >> a;                            // Captura e decodifica o quadro seguinte em "a" 
        if (a.empty()) erro("Erro: quadro vazio da camera"); // Câmera falhou, encerra 
        server.sendImgComp(a);             // Comprime em JPEG e envia

        int vel_esq = 0, vel_dir = 0;      // Potências pedidas pelo cliente (-100 a 100; negativo = ré)
        server.receiveBytes(sizeof(int), (BYTE*)&vel_esq); // Recebe a potência da roda esquerda (4 bytes)
        server.receiveBytes(sizeof(int), (BYTE*)&vel_dir); // Recebe a potência da roda direita (4 bytes)

        // Travas de limite máximo absoluto (100), por segurança
        if (vel_esq > 100) vel_esq = 100;   // Limita a roda esquerda a +100% 
        if (vel_esq < -100) vel_esq = -100; // Limita a roda esquerda a -100%
        if (vel_dir > 100) vel_dir = 100;   // Limita a roda direita a +100%
        if (vel_dir < -100) vel_dir = -100; // Limita a roda direita a -100%

        // RODA ESQUERDA (Motor A: Pinos 0 e 1)
        if (vel_esq >= 0) {              // Potência positiva (ou zero): gira para frente (ou para)
            softPwmWrite(0, 0);          // Entrada 0 em nível baixo
            softPwmWrite(1, vel_esq);    // PWM na entrada 1 com a potência desejada
        } else {                         // Potência negativa: gira para trás
            softPwmWrite(0, -vel_esq);   // PWM na entrada 0 com o módulo da potência
            softPwmWrite(1, 0);          // Entrada 1 em nível baixo
        }

        // RODA DIREITA (Motor B: Pinos 2 e 3)
        if (vel_dir >= 0) {              // Potência positiva (ou zero): gira para frente (ou para)
            softPwmWrite(2, 0);          // Entrada 2 em nível baixo
            softPwmWrite(3, vel_dir);    // PWM na entrada 3 com a potência desejada
        } else {                         // Potência negativa: gira para trás
            softPwmWrite(2, -vel_dir);   // PWM na entrada 2 com o módulo da potência
            softPwmWrite(3, 0);          // Entrada 3 em nível baixo
        }
    }
    
    return 0;                            // Fim; desliga_motores() roda automaticamente (atexit)
}
