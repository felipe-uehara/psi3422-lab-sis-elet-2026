#include "raspberry.hpp"
#include <iostream>
#include <vector>
#include <cmath>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace cv;
using namespace std;

struct Pico {
    int l, c;
    float val;
    int escalaIdx;
};

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Uso: ./cliente4 <IP_DA_RASPBERRY> [videosaida.avi] [t/c]\n");
        return -1;
    }

    char* ip = argv[1];
    char* nome_video = (argc > 2) ? argv[2] : (char*)"videosaida.avi";
    bool grava_tela = (argc > 3 && argv[3][0] == 't');

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) erro("Erro ao criar o socket");

    sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(5000);
    inet_pton(AF_INET, ip, &serv_addr.sin_addr);

    printf("A ligar ao servidor em %s...\n", ip);
    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        erro("Erro: Nao foi possivel ligar ao servidor da Raspberry");
    }

    int nl = 240, nc = 320;
    VideoWriter vo(nome_video, CV_FOURCC('X','V','I','D'), 20, Size(nc, nl));

    Mat_<COR> temp = imread("quadrado.png", 1);
    if (temp.total() == 0) erro("Erro leitura quadrado.png");
    Mat_<FLT> T; 
    converte(temp, T);

    vector<Mat_<FLT>> modelosCC(10);
    vector<Mat_<FLT>> modelosNCC(10);
    float escala_ini = 0.1721, escala_fim = 0.0473;
    float razao = pow(escala_fim / escala_ini, 1.0 / 9.0);

    #pragma omp parallel for schedule(static)
    for (int i = 0; i < 10; i++) {
        float s = escala_ini * pow(razao, i);
        Mat_<FLT> T_redim;
        resize(T, T_redim, Size(), s, s, INTER_NEAREST);
        modelosNCC[i] = dcReject(T_redim, 1.0);
        modelosCC[i] = somaAbsDois(modelosNCC[i]);
    }

    Mat_<COR> frame(nl, nc);
    namedWindow("Transmissao Carrinho", WINDOW_AUTOSIZE);
    printf("Transmissao iniciada. Pressione ESC para sair.\n");

    while (true) {
        int bytes_recebidos = recv(sock, frame.data, nl * nc * sizeof(COR), MSG_WAITALL);
        if (bytes_recebidos <= 0) break;

        Mat_<FLT> f;
        converte(frame, f);

        float melhor_ncc = -1.0;
        Pico melhor_deteccao;
        melhor_deteccao.l = 0; melhor_deteccao.c = nc / 2; melhor_deteccao.escalaIdx = 0;

        vector<Pico> picos_candidatos;

        for (int i = 0; i < 10; i++) {
            Mat_<FLT> resCC = matchTemplateSame(f, modelosCC[i], CV_TM_CCORR, 0.0);
            double minVal, maxVal;
            Point minLoc, maxLoc;
            minMaxLoc(resCC, &minVal, &maxVal, &minLoc, &maxLoc);

            Pico p;
            p.c = maxLoc.x;
            p.l = maxLoc.y;
            p.val = maxVal;
            p.escalaIdx = i;
            picos_candidatos.push_back(p);
        }

        for (const auto& pico : picos_candidatos) {
            Mat_<FLT> resNCC = matchTemplateSame(f, modelosNCC[pico.escalaIdx], CV_TM_CCOEFF_NORMED, 0.0);
            float valNCC = resNCC(pico.l, pico.c);

            if (valNCC > melhor_ncc) {
                melhor_ncc = valNCC;
                melhor_deteccao = pico;
            }
        }

        int vE = 0, vD = 0;
        if (melhor_ncc > 0.55) {
            int erro_x = melhor_deteccao.c - (nc / 2); // Usa apenas o eixo X conforme o enunciado
            int velocidade_base = 45;
            int correcao = erro_x / 2;

            vE = velocidade_base + correcao;
            vD = velocidade_base - correcao;

            if (grava_tela) {
                int larg = modelosNCC[melhor_deteccao.escalaIdx].cols;
                int alt = modelosNCC[melhor_deteccao.escalaIdx].rows;
                rectangle(frame, Rect(melhor_deteccao.c - larg/2, melhor_deteccao.l - alt/2, larg, alt), Scalar(0, 255, 255), 2);
            }
        } else {
            vE = 0;
            vD = 0;
        }

        int velocidades[2] = {vE, vD};
        if (send(sock, velocidades, sizeof(velocidades), 0) < 0) break;

        // Mostra a transmissão em tempo real numa aba/janela no PC
        imshow("Transmissao Carrinho", frame);

        vo << frame;

        if (waitKey(1) == 27) break; // Sai ao pressionar ESC
    }

    close(sock);
    printf("Cliente encerrado.\n");
    return 0;
}