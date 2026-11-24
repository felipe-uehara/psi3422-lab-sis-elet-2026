// fase5.cpp
// compila fase5 -ocv -v3 -omp
// Uso: fase5 capturado.avi quadrado.png locarec.avi
// Localiza a placa (mesma lógica do fase3p) e, quando a placa está perto (escala 0 ou 1),
// lê o dígito manuscrito com FLANN treinado no MNIST e escreve o dígito no vídeo de saída.
// Os arquivos do MNIST (train-images.idx3-ubyte e train-labels.idx1-ubyte) devem estar no diretório atual.
#include "projeto.hpp"
#include <omp.h>

int main(int argc, char** argv) {
    if (argc != 4) erro("Uso: fase5 capturado.avi quadrado.png locarec.avi"); // Exige exatamente 3 argumentos: vídeo de entrada, modelo e vídeo de saída

    // ---------------- Parâmetros da detecção ----------------
    const int num_escalas = 10;        // Quantidade de modelos (escalas)
    const float esc_ini = 0.1721f;     // Escala do maior modelo: 401 * 0,1721 = 69x69 pixels
    const float esc_fim = 0.0473f;     // Escala do menor modelo: 401 * 0,0473 = 19x19 pixels
    const int num_cand = 20;           // Quantidade de picos CC (candidatos) a examinar
    const int dist_min = 10;           // Distância mínima, em pixels, entre dois candidatos
    const double limiar_ncc = 0.55;    // NCC mínima para considerar que a placa foi encontrada
    const FLT apagado = -1e9f;         // Valor que marca um pixel já usado no mapa de picos (a CC real fica entre -1 e 1)

    // ---------------- Parâmetros da leitura do dígito ----------------
    const int escala_leitura = 1;      // Só lê o dígito se a placa estiver na escala 0 ou 1 (placa perto, dígito grande)
    const double contraste_min = 0.15; // Diferença mínima entre o pixel mais claro e o mais escuro da área do dígito; abaixo disso não há dígito
    const FLT preto = 0.2f;            // Depois de normalizar (0 a 1), valores <= 0.2 viram preto (0)
    const FLT branco = 0.6f;           // e valores >= 0.6 viram branco (1); entre os dois, escala linear
    const int lado_mnist = 14;         // Tamanho (14x14) das imagens do MNIST e da imagem do dígito classificada
    const string dir_mnist = "/home/tomi/cekeikon5/tiny_dnn/data";      // Diretório dos arquivos do MNIST (diretório atual)

    // ---------------- Leitura do MNIST e treino do FLANN ----------------
    MnistFlann mnist(lado_mnist, true, true); // 14x14, inverte cores (fundo branco, dígito preto), elimina bordas brancas (bbox)
    mnist.le(dir_mnist, 60000, 0);     // Lê as 60000 imagens de treino; 0 = não lê as imagens de teste (não são usadas)
    mnist.train();                     // Monta o índice FLANN (árvores kd) com as 60000 imagens de treino

    // ---------------- Leitura e pré-processamento do modelo ----------------
    Mat_<COR> temp = imread(argv[2], 1);        // Lê a imagem-modelo (401x401) em cores (1 = colorida, BGR)
    if (temp.total()==0) erro("Erro leitura " + string(argv[2])); // Se a imagem não tem pixels, a leitura falhou, aborta
    Mat_<FLT> T; converte(temp, T);             // Converte para cinza float (0 = preto, 1 = branco)

    // Área do dígito no modelo: o quadrado "don't care" (pixels exatamente 1.0) que contém o centro da placa
    int cx = T.cols/2, cy = T.rows/2;           // Centro do modelo
    if (T(cy,cx) != 1.0f) erro("Erro: o centro de quadrado.png nao e' don't care"); // O centro tem que estar dentro do quadrado amarelo
    int esq = cx;   while (esq > 0         && T(cy, esq-1) == 1.0f) esq--;   // Anda para a esquerda enquanto o pixel for don't care
    int dir = cx;   while (dir < T.cols-1  && T(cy, dir+1) == 1.0f) dir++;   // Anda para a direita
    int cima = cy;  while (cima > 0        && T(cima-1, cx) == 1.0f) cima--; // Anda para cima
    int baixo = cy; while (baixo < T.rows-1 && T(baixo+1, cx) == 1.0f) baixo++; // Anda para baixo
    Rect area_digito_T(esq, cima, dir-esq+1, baixo-cima+1); // Retângulo da área do dígito em coordenadas do modelo 401x401

    vector<Mat_<FLT>> modelos_cc(num_escalas);  // Modelos pré-processados para CC (dcReject + somaAbsDois)
    vector<Mat_<FLT>> modelos_ncc(num_escalas); // Modelos para NCC (só dcReject, para manter os pixels "don't care")
    float razao = pow(esc_fim / esc_ini, 1.0f / (num_escalas - 1)); // Razão da progressão geométrica: esc_ini * razao^9 = esc_fim

    for (int i = 0; i < num_escalas; i++) {     // Gera um modelo para cada escala
        float escala = esc_ini * pow(razao, i); // Escala i = esc_ini * razao^i
        Mat_<FLT> T_reduzido;                   // Matriz que vai receber o modelo redimensionado
        resize(T, T_reduzido, Size(), escala, escala, INTER_NEAREST); // Vizinho mais próximo: não cria tons novos, então os pixels 1.0 continuam exatamente 1.0
        modelos_cc[i] = somaAbsDois(dcReject(T_reduzido, 1.0));       // Pixels 1.0 viram 0 ("don't care"), os demais têm a média subtraída; soma dos absolutos = 2
        modelos_ncc[i] = dcReject(T_reduzido, 1.0);                   // Mesmo dcReject; a NCC normaliza por conta própria
    }

    // ---------------- Vídeos de entrada e saída ----------------
    VideoCapture w(argv[1]);                  // Abre o vídeo de entrada (capturado pela câmera do carrinho)
    if (!w.isOpened()) erro("Erro abrindo " + string(argv[1]));  // Aborta se o arquivo não existe ou o codec não é suportado
    int nc = w.get(CV_CAP_PROP_FRAME_WIDTH);  // Largura dos quadros do vídeo de entrada
    int nl = w.get(CV_CAP_PROP_FRAME_HEIGHT); // Altura dos quadros do vídeo de entrada
    int lp = nl/2;                            // Lado de cada imagem do painel de depuração (2 imagens empilhadas = altura do quadro)
    double fps_entrada = w.get(CV_CAP_PROP_FPS);                 // Taxa de quadros gravada no vídeo de entrada
    if (fps_entrada <= 0) fps_entrada = 20;   // Se o arquivo não informar a taxa, usa 20 fps
    VideoWriter vo(argv[3], CV_FOURCC('X','V','I','D'), fps_entrada, Size(nc + lp, nl)); // Saída: quadro + painel de depuração à direita
    if (!vo.isOpened()) erro("Erro criando " + string(argv[3])); // Aborta se não conseguiu criar o arquivo de saída

    Mat_<COR> a;                          // Quadro colorido lido do vídeo (recebe os desenhos)
    Mat_<COR> a_limpo;                    // Cópia do quadro sem desenhos, para mostrar o dígito extraído no painel
    Mat_<FLT> f;                          // Versão em cinza float do quadro, usada nos casamentos de modelos e na leitura do dígito
    vector<Mat_<FLT>> R_cc(num_escalas);  // Mapas de correlação CC, um por escala
    vector<Mat_<FLT>> R_ncc(num_escalas); // Mapas de correlação NCC, um por escala
    int quadros = 0;                      // Contador de quadros processados

    double t1 = timeSinceEpoch();         // Marca o início do processamento (depois do treino), em segundos

    while (true) {                        // Laço principal: um quadro por iteração
        w >> a;                           // Lê o próximo quadro do vídeo de entrada
        if (!a.data) break;               // Sem dados: acabou o vídeo, sai do laço
        quadros++;                        // Conta mais um quadro processado
        a_limpo = a.clone();              // Guarda o quadro antes de desenhar
        converte(a, f);                   // Converte o quadro colorido para cinza float em "f"

        // ---- 1) Em paralelo: 10 casamentos CC e 10 casamentos NCC ----
        #pragma omp parallel for          // Distribui as 10 escalas entre as threads (cada iteração escreve só em R_cc[i] e R_ncc[i])
        for (int i = 0; i < num_escalas; i++) {                                   // Uma escala por iteração
            R_cc[i]  = matchTemplateSame(f, modelos_cc[i],  CV_TM_CCORR);         // Mapa CC do tamanho de f, alinhado pelo centro do modelo
            R_ncc[i] = matchTemplateSame(f, modelos_ncc[i], CV_TM_CCOEFF_NORMED); // Mapa NCC (-1 a 1), calculado uma vez só para ler depois nos 20 candidatos
        }

        // ---- 2) Junta as 10 escalas: para cada pixel, a maior CC e a escala que a produziu ----
        Mat_<FLT> cc_max(nl, nc, apagado);            // Maior CC entre as escalas em cada pixel (começa com um valor menor que qualquer CC)
        Mat_<int> esc_max(nl, nc, -1);                // Escala que deu essa maior CC em cada pixel (-1 = nenhuma)
        for (int i = 0; i < num_escalas; i++)         // Para cada escala
            for (int l = 0; l < nl; l++)              // para cada linha
                for (int c = 0; c < nc; c++)          // para cada coluna
                    if (R_cc[i](l,c) > cc_max(l,c)) { // se esta escala tem CC maior neste pixel
                        cc_max(l,c) = R_cc[i](l,c);   // guarda o valor
                        esc_max(l,c) = i;             // e a escala
                    }

        // ---- 3) Os 20 picos CC mais altos, separados por pelo menos 10 pixels ----
        vector<Point>  cand_loc;                   // Posição (centro) de cada candidato
        vector<int>    cand_esc;                   // Escala de cada candidato
        vector<double> cand_cc;                    // Valor CC de cada candidato
        vector<double> cand_ncc;                   // Valor NCC de cada candidato (mesma posição e mesma escala)
        for (int k = 0; k < num_cand; k++) {       // Procura um pico por vez, do mais alto para o mais baixo
            double maxVal; Point maxLoc;           // Valor e posição do maior pico restante
            minMaxLoc(cc_max, NULL, &maxVal, NULL, &maxLoc); // Encontra o maior valor que ainda não foi apagado
            if (maxVal <= apagado) break;          // Mapa todo apagado: não há mais picos (não acontece em 320x240)
            int e = esc_max(maxLoc.y, maxLoc.x);   // Escala que deu este pico
            cand_loc.push_back(maxLoc);            // Guarda a posição do pico
            cand_esc.push_back(e);                 // Guarda a escala do pico
            cand_cc.push_back(maxVal);             // Guarda o valor CC do pico
            cand_ncc.push_back(R_ncc[e](maxLoc.y, maxLoc.x)); // Guarda a NCC na mesma posição e na mesma escala do pico CC

            for (int dl = -(dist_min-1); dl <= dist_min-1; dl++)       // Percorre a vizinhança do pico (linhas)
                for (int dc = -(dist_min-1); dc <= dist_min-1; dc++) { // e colunas
                    if (dl*dl + dc*dc >= dist_min*dist_min) continue;  // Só apaga pixels a menos de 10 pixels do pico (círculo)
                    int l = maxLoc.y + dl, c = maxLoc.x + dc;          // Coordenadas do pixel vizinho
                    if (0 <= l && l < nl && 0 <= c && c < nc)          // Se está dentro da imagem
                        cc_max(l,c) = apagado;                         // apaga, para que o próximo pico fique a >= 10 pixels deste
                }
        }

        // ---- 4) Entre os candidatos, o de maior NCC ----
        int melhor = -1;                                 // Índice do melhor candidato (-1 = nenhum)
        for (int k = 0; k < (int)cand_loc.size(); k++) { // Percorre os candidatos
            int e = cand_esc[k];                         // Escala do candidato
            int w_t = modelos_cc[e].cols;                // Largura do modelo nesta escala
            int h_t = modelos_cc[e].rows;                // Altura do modelo nesta escala
            Rect r(cand_loc[k].x - (w_t-1)/2, cand_loc[k].y - (h_t-1)/2, w_t, h_t); // Retângulo do candidato, com o mesmo deslocamento que matchTemplateSame usa
            rectangle(a, r, Scalar(255, 255, 0), 1);     // Desenha o candidato em azul, espessura 1
            Point p(r.x + r.width + 1, r.y + 7);         // Ponto à direita do retângulo para escrever os números
            putText(a, to_string(e), p, FONT_HERSHEY_SIMPLEX, 0.25, Scalar(255, 255, 0), 1); // Linha 1: escala
            putText(a, to_string((int)lround(cand_cc[k]*100)),  p + Point(0, 8),  FONT_HERSHEY_SIMPLEX, 0.25, Scalar(255, 255, 0), 1); // Linha 2: CC x 100
            putText(a, to_string((int)lround(cand_ncc[k]*100)), p + Point(0, 16), FONT_HERSHEY_SIMPLEX, 0.25, Scalar(255, 255, 0), 1); // Linha 3: NCC x 100
            if (melhor < 0 || cand_ncc[k] > cand_ncc[melhor]) melhor = k; // Guarda o candidato com a maior NCC
        }

        // ---- 5) Quadro de saída: imagem à esquerda, painel de depuração cinza à direita ----
        Mat_<COR> saida(nl, nc + lp, COR(128, 128, 128)); // Quadro de saída preenchido de cinza
        Rect r_quadro(0, 0, nc, nl);                      // Onde vai o quadro do vídeo
        Rect r_proc(nc, 0, lp, lp);                       // Painel superior: dígito pré-processado (14x14 ampliado)
        Rect r_extr(nc, lp, lp, lp);                      // Painel inferior: dígito extraído do quadro, com o dígito lido

        // ---- 6) Decisão: há placa se a maior NCC passar do limiar ----
        if (melhor >= 0 && cand_ncc[melhor] > limiar_ncc) { // Placa encontrada
            Point best_loc = cand_loc[melhor];              // Posição da placa
            int best_scale = cand_esc[melhor];              // Escala da placa
            int w_t = modelos_cc[best_scale].cols;          // Largura do modelo vencedor
            int h_t = modelos_cc[best_scale].rows;          // Altura do modelo vencedor
            Rect r(best_loc.x - (w_t-1)/2, best_loc.y - (h_t-1)/2, w_t, h_t); // Retângulo da placa
            rectangle(a, r, Scalar(0, 255, 255), 2);        // Desenha a placa em amarelo, espessura 2

            putText(a, to_string(best_scale), Point(r.x + r.width, r.y),      FONT_HERSHEY_SIMPLEX, 0.4, Scalar(0, 0, 0), 1); // Escala vencedora
            putText(a, to_string((int)lround(cand_cc[melhor]*100)),                Point(r.x + r.width, r.y + 12), FONT_HERSHEY_SIMPLEX, 0.4, Scalar(0, 0, 0), 1); // CC*100
            putText(a, to_string((int)lround(cand_ncc[melhor]*100)),               Point(r.x + r.width, r.y + 24), FONT_HERSHEY_SIMPLEX, 0.4, Scalar(0, 0, 0), 1); // NCC*100

            // ---- 7) Leitura do dígito, só com a placa perto (escala 0 ou 1) ----
            if (best_scale <= escala_leitura) {
                // 7a) Área do dígito no quadro: a área do modelo 401x401 reduzida para o tamanho da placa encontrada
                double k = (double)w_t / T.cols;                 // Fator de escala entre o modelo original e a placa no quadro
                Rect rd(r.x + lround(area_digito_T.x * k),       // Canto esquerdo da área do dígito no quadro
                        r.y + lround(area_digito_T.y * k),       // Canto superior
                        lround(area_digito_T.width * k),         // Largura
                        lround(area_digito_T.height * k));       // Altura
                int m = rd.width / 10;                           // Margem de 10% para não pegar a borda do quadrado amarelo
                rd = Rect(rd.x + m, rd.y + m, rd.width - 2*m, rd.height - 2*m); // Encolhe a área pela margem
                rd = rd & Rect(0, 0, nc, nl);                    // Recorta o que estiver fora do quadro

                if (rd.width >= 4 && rd.height >= 4) {           // Só continua se sobrou uma área útil
                    Mat_<FLT> d = f(rd).clone();                 // Copia o dígito em cinza float (fundo cinza claro, traço cinza escuro)
                    double vmin, vmax;                           // Pixel mais escuro e mais claro da área
                    minMaxLoc(d, &vmin, &vmax);                  // Mede o contraste da área
                    if (vmax - vmin >= contraste_min) {          // Há algo escrito (contraste suficiente)
                        // 7b) Ajuste de brilho/contraste: fundo branco (1), traço preto (0)
                        d = normaliza(d);                        // Estica para 0 (mais escuro) a 1 (mais claro)
                        for (int l = 0; l < d.rows; l++)         // Para cada linha
                            for (int c = 0; c < d.cols; c++)     // e cada coluna
                                d(l,c) = min(1.0f, max(0.0f, (d(l,c) - preto) / (branco - preto))); // <= preto vira 0, >= branco vira 1

                        // 7c) Elimina linhas/colunas brancas da borda e reduz para 14x14 (pixels <= 0.5 contam como traço)
                        Mat_<FLT> b = mnist.bbox(d);             // Mesmo pré-processamento das imagens de treino do MNIST
                        if (mnist.localizou) {                   // bbox encontrou traço suficiente
                            // 7d) Classificação com FLANN (vizinho mais próximo entre as 60000 imagens de treino)
                            Mat_<FLT> linha(1, b.total());       // FLANN recebe a imagem como um vetor linha de 196 valores
                            for (unsigned i = 0; i < b.total(); i++) linha(i) = b(i); // Copia os 14x14 pixels para a linha
                            int digito = (int)mnist.predictInterno(linha); // Rótulo (0 a 9) da imagem de treino mais parecida

                            // 7e) Escreve o dígito lido no quadro e monta o painel de depuração
                            putText(a, to_string(digito), Point(r.x - 28, r.y + r.height), FONT_HERSHEY_SIMPLEX, 1.2, Scalar(0, 0, 255), 3); // Dígito em vermelho à esquerda da placa

                            Mat_<COR> b_cor, b_grande;           // Dígito pré-processado em cores, e ampliado
                            converte(b, b_cor);                  // Converte FLT (0 a 1) para COR
                            resize(b_cor, b_grande, Size(lp, lp), 0, 0, INTER_NEAREST); // Amplia 14x14 para o tamanho do painel (sem suavizar)
                            b_grande.copyTo(saida(r_proc));      // Coloca no painel superior

                            Mat_<COR> extr_grande;               // Dígito extraído ampliado
                            resize(a_limpo(rd), extr_grande, Size(lp, lp)); // Amplia o dígito extraído (sem desenhos)
                            extr_grande.copyTo(saida(r_extr));   // Coloca no painel inferior
                            putText(saida, to_string(digito), Point(nc + lp/4, nl - lp/5), FONT_HERSHEY_SIMPLEX, 2.5, Scalar(0, 0, 255), 4); // Dígito lido em vermelho sobre o painel inferior
                        }
                    }
                }
            }
        }

        a.copyTo(saida(r_quadro));     // Copia o quadro com os desenhos para a parte esquerda da saída
        vo << saida;                   // Grava o quadro de saída
    }

    double t2 = timeSinceEpoch();      // Marca o fim do processamento
    double t_total = t2 - t1;          // Tempo total, em segundos
    printf("Quadros: %d | Tempo: %.2fs | Desempenho: %.2f fps\n", quadros, t_total, quadros / t_total); // Quadros, tempo e taxa média (sem imshow, para mais fps)
    return 0;
}
