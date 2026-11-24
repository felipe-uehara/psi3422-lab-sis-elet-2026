#include "raspberry.hpp"
#include <iostream>
#include <vector>
#include <cmath>

using namespace cv;
using namespace std;

// Estrutura para guardar os picos candidatos da deteção CC
struct Pico {
    int l, c;
    float val;
    int escalaIdx;
};

int main(int argc, char** argv) {
    if (argc != 4) {
        cout << "Uso: ./fase3 <video_entrada> <modelo.png> <video_saida>" << endl;
        return -1;
    }

    // 1. Abertura de vídeo
    VideoCapture w(argv[1]);
    if (!w.isOpened()) erro("Erro: Abertura de video");
    
    int nl = 240, nc = 320;
    VideoWriter vo(argv[3], CV_FOURCC('X','V','I','D'), 20, Size(nc, nl));

    // 2. Leitura do modelo e preparação do "don't care"[cite: 7]
    Mat_<COR> temp = imread(argv[2], 1);
    if (temp.total() == 0) erro("Erro leitura quadrado.png");
    Mat_<FLT> T; 
    converte(temp, T);

    // 3. Pré-processamento das 10 escalas[cite: 8]
    vector<Mat_<FLT>> modelosCC(10);
    vector<Mat_<FLT>> modelosNCC(10);
    float escala_ini = 0.1721, escala_fim = 0.0473;
    float razao = pow(escala_fim / escala_ini, 1.0 / 9.0);

    // 3. Pré-processamento das 10 escalas paralelizado com OpenMP
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < 10; i++) {
        float s = escala_ini * pow(razao, i);
        Mat_<FLT> T_redim;
        resize(T, T_redim, Size(), s, s, INTER_NEAREST); //[cite: 7]
        modelosNCC[i] = dcReject(T_redim, 1.0);           //[cite: 6, 7]
        modelosCC[i] = somaAbsDois(modelosNCC[i]);       //[cite: 3, 7]
    }

    for (int i = 0; i < 10; i++) {
        float s = escala_ini * pow(razao, i);
        Mat_<FLT> T_redim;
        resize(T, T_redim, Size(), s, s, INTER_NEAREST); 
        modelosNCC[i] = dcReject(T_redim, 1.0); 
        modelosCC[i] = somaAbsDois(modelosNCC[i]);
    }

    Mat_<COR> a;
    Mat_<FLT> f;
    int quadros = 0;
    double t1 = timeSinceEpoch();

    while (true) {
        w >> a;
        if (!a.data) break;
        converte(a, f);

        vector<Pico> picos_candidatos;
        float melhor_ncc = -1.0;
        Pico melhor_deteccao;
        melhor_deteccao.l = 0; melhor_deteccao.c = 0; melhor_deteccao.escalaIdx = 0;

        // 4. Lógica CC: Encontra o pico máximo para cada uma das 10 escalas (simplificação)
        for (int i = 0; i < 10; i++) {
            Mat_<FLT> resCC = matchTemplateSame(f, modelosCC[i], CV_TM_CCORR, 0.0);
            
            double minVal, maxVal;
            Point minLoc, maxLoc;
            minMaxLoc(resCC, &minVal, &maxVal, &minLoc, &maxLoc);
            
            Pico p;
            p.c = maxLoc.x; // coluna (x)
            p.l = maxLoc.y; // linha (y)
            p.val = maxVal;
            p.escalaIdx = i;
            picos_candidatos.push_back(p);
        }

        // 5. Validação NCC: Verifica os picos candidatos gerados pelo CC[cite: 8]
        for (const auto& pico : picos_candidatos) {
            Mat_<FLT> resNCC = matchTemplateSame(f, modelosNCC[pico.escalaIdx], CV_TM_CCOEFF_NORMED, 0.0);
            float valNCC = resNCC(pico.l, pico.c);
            
            if (valNCC > melhor_ncc) {
                melhor_ncc = valNCC;
                melhor_deteccao = pico;
            }
        }

        // 6. Desenho das marcações se o NCC for maior que 0.55[cite: 8]
        if (melhor_ncc > 0.55) {
            // (Opcional) Desenhar os candidatos do CC em ciano
            for (const auto& pico : picos_candidatos) {
                int larg = modelosNCC[pico.escalaIdx].cols;
                int alt = modelosNCC[pico.escalaIdx].rows;
                // Subtrai metade da largura/altura porque matchTemplateSame dá o centro[cite: 3]
                rectangle(a, Rect(pico.c - larg/2, pico.l - alt/2, larg, alt), Scalar(255, 255, 0), 1); 
            }

            // Desenhar a melhor deteção em amarelo
            int larg_final = modelosNCC[melhor_deteccao.escalaIdx].cols;
            int alt_final = modelosNCC[melhor_deteccao.escalaIdx].rows;
            rectangle(a, Rect(melhor_deteccao.c - larg_final/2, melhor_deteccao.l - alt_final/2, larg_final, alt_final), Scalar(0, 255, 255), 2);
            
            // Texto da escala e valor NCC
            char texto[50];
            sprintf(texto, "E:%d NCC:%.2f", melhor_deteccao.escalaIdx, melhor_ncc);
            putText(a, texto, Point(melhor_deteccao.c - larg_final/2, melhor_deteccao.l - alt_final/2 - 5), FONT_HERSHEY_SIMPLEX, 0.4, Scalar(0, 255, 255), 1);
        }

        vo << a;
        quadros++;
    }

    double t2 = timeSinceEpoch();
    printf("Processados %d quadros a %.2f fps\n", quadros, quadros / (t2 - t1));

    return 0;
}