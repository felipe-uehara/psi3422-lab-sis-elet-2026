#include "raspberry.hpp"
#include <wiringPi.h>
#include <softPwm.h>
#include <cstdlib>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

using namespace cv;
using namespace std;

// Função para aplicar as velocidades nos motores
void moveMotores(int vE, int vD) {
    if (vE >= 0) {
        softPwmWrite(0, 0);
        softPwmWrite(1, vE);
    } else {
        softPwmWrite(0, -vE);
        softPwmWrite(1, 0);
    }

    if (vD >= 0) {
        softPwmWrite(2, 0);
        softPwmWrite(3, vD);
    } else {
        softPwmWrite(2, -vD);
        softPwmWrite(3, 0);
    }
}

int main() {
    // 1. INICIA A WEBCAM PRIMEIRO (Garante que o hardware abre antes de mexer nos motores)
    VideoCapture w(0);
    if(!w.isOpened()){
        fprintf(stderr, "Erro: Abertura de webcam 0.\n");
        exit(1);
    }
    w.set(CAP_PROP_FRAME_WIDTH, 320);
    w.set(CAP_PROP_FRAME_HEIGHT, 240);
    printf("Webcam aberta com sucesso!\n");

    // 2. Inicializa o wiringPi e pinos PWM
    wiringPiSetup();
    if(softPwmCreate(0,0,100)) exit(1);
    if(softPwmCreate(1,0,100)) exit(1);
    if(softPwmCreate(2,0,100)) exit(1);
    if(softPwmCreate(3,0,100)) exit(1);

    moveMotores(0, 0);

    // 3. Configuração do Socket Servidor TCP na porta 5000
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) erro("Erro ao criar socket servidor");

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(5000);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        erro("Erro no bind do socket");
    }

    if (listen(server_fd, 3) < 0) erro("Erro no listen");

    printf("Servidor pronto na Raspberry Pi. Aguardando conexao do cliente...\n");
    int sock = accept(server_fd, NULL, NULL);
    if (sock < 0) erro("Erro no accept");
    printf("Cliente conectado!\n");

    // 4. ARRRIANCA OS MOTORES DEPOIS (Impulso inicial para a frente)
    printf("A dar arrancada inicial para a frente...\n");
    softPwmWrite(0, 0); softPwmWrite(1, 80);
    softPwmWrite(2, 0); softPwmWrite(3, 90);
    delay(600);
    moveMotores(0, 0);

    int nl = 240, nc = 320;
    Mat_<Vec3b> frame(nl, nc);

    // 5. Ciclo principal de transmissão e controlo
    while(true){
        w >> frame;
        if(!frame.data) break;

        // Envia o frame da webcam para o cliente no WSL
        if (send(sock, frame.data, nl * nc * sizeof(Vec3b), 0) < 0) break;

        // Recebe as velocidades [vE, vD] calculadas pelo cliente
        int velocidades[2] = {0, 0};
        if (recv(sock, velocidades, sizeof(velocidades), 0) <= 0) break;

        // Aplica as velocidades nos motores
        moveMotores(velocidades[0], velocidades[1]);
    }

    moveMotores(0, 0);
    close(sock);
    close(server_fd);
    softPwmStop(0); softPwmStop(1); softPwmStop(2); softPwmStop(3);
    
    printf("Servidor encerrado.\n");
    return 0;
}