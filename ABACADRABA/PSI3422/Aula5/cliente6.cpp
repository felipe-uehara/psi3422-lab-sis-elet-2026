// cliente6.cpp
// compila cliente6 -ocv -v3 -omp
#include "projeto.hpp"
#include <omp.h>

int main(int argc, char** argv) {
    if (argc < 2) erro("Uso: cliente6 <IP_RASPBERRY> [videosaida.avi] [t/c]");

    // ---------------- Parâmetros da detecção (iguais ao fase3p) ----------------
    const int num_escalas = 10;        // Quantidade de modelos (escalas)
    const float esc_ini = 0.1721f;     // Escala do maior modelo: 401 * 0,1721 = 69x69 pixels
    const float esc_fim = 0.0473f;     // Escala do menor modelo: 401 * 0,0473 = 19x19 pixels
    const int num_cand = 20;           // Quantidade de picos CC (candidatos) a examinar
    const int dist_min = 10;           // Distância mínima, em pixels, entre dois candidatos
    const double limiar_ncc = 0.55;    // NCC mínima para considerar que a placa foi encontrada
    const double fps_video = 14;       // Taxa de quadros gravada no vídeo de saída
    const FLT apagado = -2;            // Valor que marca um pixel já usado no mapa de picos (a CC real fica entre -1 e 1)

    // ---------------- Parâmetros da leitura do dígito (iguais ao fase5) ----------------
    const int escala_leitura = 1;      // Placa na escala 0 ou 1 = perto: para e lê o dígito
    const double contraste_min = 0.15; // Diferença mínima entre o pixel mais claro e o mais escuro da área do dígito, abaixo disso não há dígito
    const FLT preto = 0.2f;            // Depois de normalizar (0 a 1), valores <= 0.2 viram preto (0)
    const FLT branco = 0.6f;           // e valores >= 0.6 viram branco (1); entre os dois, escala linear
    const int lado_mnist = 14;         // Tamanho (14x14) das imagens do MNIST
    const string dir_mnist = "/home/tomi/cekeikon5/tiny_dnn/data"; // Diretório dos arquivos do MNIST 

    // ---------------- Parâmetros de movimento (calibrar no carrinho) ----------------
    const int vel_base_esq = 90;       // Potência base da roda esquerda para andar reto
    const int vel_base_dir = 100;      // Potência base da roda direita, maior para compensar o motor mais fraco
    const float k_proporcional = 0.6f; // Ganho do controle proporcional ao seguir a placa
    const int vel_giro = 100;          // Potência das rodas ao girar no lugar (uma para frente, outra para trás)
    const double t_parar = 0.7;        // Segundos esperando o carrinho parar (estados 1 e 5)
    const double t_90 = 1.6;           // Segundos girando para virar 90 graus
    const double t_180 = 3.2;          // Segundos girando para virar 180 graus
    const double t_frente = 8;         // Segundos andando reto para passar por baixo da placa

    // ---------------- Leitura do MNIST e treino do FLANN ----------------
    MnistFlann mnist(lado_mnist, true, true); // 14x14, inverte cores (fundo branco, dígito preto), elimina bordas brancas (bbox)
    mnist.le(dir_mnist, 60000, 0);            // Lê as 60000 imagens de treino, 0 = não lê as imagens de teste
    mnist.train();                            // Monta o índice FLANN com as 60000 imagens de treino

    // ---------------- Leitura e pré-processamento do modelo ----------------
    Mat_<COR> temp = imread("quadrado.png", 1);                       // Lê a imagem-modelo (401x401) em cores (1 = colorida)
    if (temp.total()==0) erro("Erro leitura quadrado.png");           // Se a imagem não tem pixels, a leitura falhou: aborta
    Mat_<FLT> T; converte(temp, T);                                   // Converte para cinza float (0 = preto, 1 = branco); só o branco (255,255,255) vira exatamente 1.0

    // Área do dígito no modelo: o quadrado "don't care" (pixels exatamente 1.0) que contém o centro da placa
    int cx = T.cols/2, cy = T.rows/2;                                               // Centro do modelo
    if (T(cy,cx) != 1.0f) erro("Erro: o centro de quadrado.png nao e' don't care"); // O centro tem que estar dentro do quadrado amarelo
    int esq = cx;   while (esq > 0          && T(cy, esq-1) == 1.0f) esq--;     // Anda para a esquerda enquanto o pixel for don't care
    int dir = cx;   while (dir < T.cols-1   && T(cy, dir+1) == 1.0f) dir++;     // Anda para a direita
    int cima = cy;  while (cima > 0         && T(cima-1, cx) == 1.0f) cima--;   // Anda para cima
    int baixo = cy; while (baixo < T.rows-1 && T(baixo+1, cx) == 1.0f) baixo++; // Anda para baixo
    Rect area_digito_T(esq, cima, dir-esq+1, baixo-cima+1);                     // Retângulo da área do dígito em coordenadas do modelo 401x401
    
    vector<Mat_<FLT>> modelos_cc(num_escalas);                        // Modelos pré-processados para CC (dcReject + somaAbsDois)
    vector<Mat_<FLT>> modelos_ncc(num_escalas);                       // Modelos para NCC (só dcReject, para manter os pixels "don't care")
    float razao = pow(esc_fim / esc_ini, 1.0f / (num_escalas - 1));   // Razão da progressão geométrica: esc_ini * razao^9 = esc_fim

    for (int i = 0; i < num_escalas; i++) {                           // Gera um modelo para cada escala
        float escala = esc_ini * pow(razao, i);                       // Escala i = esc_ini * razao^i
        Mat_<FLT> T_reduzido;                                         // Matriz que vai receber o modelo redimensionado
        resize(T, T_reduzido, Size(), escala, escala, INTER_NEAREST); // Vizinho mais próximo: não cria tons novos, então os pixels 1.0 continuam exatamente 1.0
        modelos_cc[i] = somaAbsDois(dcReject(T_reduzido, 1.0));       // Pixels 1.0 viram 0 ("don't care"), os demais têm a média subtraída; soma dos absolutos = 2
        modelos_ncc[i] = dcReject(T_reduzido, 1.0);                   // Mesmo dcReject; a NCC normaliza por conta própria
    }

    int nc = 320;                         // Largura esperada da imagem da câmera
    int nl = 240;                         // Altura esperada da imagem da câmera

    Mat_<COR> a;                          // Imagem colorida recebida da câmera (onde serão feitos os desenhos)
    Mat_<COR> imagem_limpa;               // Cópia da imagem da câmera sem desenhos (modo 'c' e painel do dígito)
    Mat_<FLT> f;                          // Versão em cinza float da imagem, usada nos casamentos de modelos e na leitura do dígito
    vector<Mat_<FLT>> R_cc(num_escalas);  // Mapas de correlação CC, um por escala
    vector<Mat_<FLT>> R_ncc(num_escalas); // Mapas de correlação NCC, um por escala

    // ---------------- Gravação de vídeo opcional ----------------
    VideoWriter vo;                    // Objeto que grava o vídeo de saída
    bool gravar_video = false;         // Só grava se o nome do arquivo for passado
    bool gravar_tela = true;           // true = grava a tela ampliada com desenhos ('t', padrão); false = grava a câmera limpa ('c')

    if (argc >= 3) {                              // Se foi passado o nome do arquivo de vídeo
        gravar_video = true;                      // ativa a gravação
        if (argc >= 4) {                          // Se também foi passado o modo de gravação
            string modo = argv[3];                // lê o modo ('t' ou 'c')
            if (modo == "c") gravar_tela = false; // 'c' grava apenas a câmera limpa
        }
        if (gravar_tela) vo.open(argv[2], CV_FOURCC('X','V','I','D'), fps_video, Size(1600, 900)); // Modo 't': tela ampliada, 1600x900
        else             vo.open(argv[2], CV_FOURCC('X','V','I','D'), fps_video, Size(nc, nl));    // Modo 'c': câmera, 320x240
        if (!vo.isOpened()) erro("Erro criando " + string(argv[2]));                               // Aborta se não conseguiu criar o arquivo
    }

    CLIENT client(argv[1]);            // Conecta ao servidor depois do treino, para o carrinho não esperar o MNIST

    // ---------------- Máquina de estados ----------------
    int estado = 0;                     // Estado atual (começa procurando a placa)
    double t_estado = timeSinceEpoch(); // Instante em que entrou no estado atual (cronômetro dos estados 1, 3 e 5)
    int digito_lido = -1;               // Dígito guardado no estado 2 e executado no estado 3 (-1 = nenhum)

    while (true) {                      // Laço principal: um quadro por iteração
        BYTE sinal_ok = 1;              // Sinal de controle: 1 = continuar
        client.sendBytes(1, &sinal_ok); // Pede o próximo quadro ao Raspberry
        client.receiveImgComp(a);       // Recebe a imagem JPEG e descomprime em "a"
        if (a.empty()) erro("Erro: imagem recebida vazia"); // Falha ao decodificar o JPEG: encerra
        
        imagem_limpa = a.clone();      // Guarda a câmera antes de desenhar
        converte(a, f);                // Converte o quadro colorido para cinza float em "f"
        int L = f.rows, C = f.cols;    // Tamanho real do quadro recebido
        int lp = L/2;                  // Lado de cada imagem do painel de depuração (2 imagens empilhadas = altura do quadro)

        // ---- 1) Em paralelo: 10 casamentos CC e 10 casamentos NCC ----
        #pragma omp parallel for                // Distribui as 10 escalas entre as threads (cada iteração escreve só em R_cc[i] e R_ncc[i])
        for (int i = 0; i < num_escalas; i++) { // Uma escala por iteração
            R_cc[i]  = matchTemplateSame(f, modelos_cc[i],  CV_TM_CCORR);         // Mapa CC do tamanho de f, alinhado pelo centro do modelo
            R_ncc[i] = matchTemplateSame(f, modelos_ncc[i], CV_TM_CCOEFF_NORMED); // Mapa NCC (-1 a 1), lido depois só nos 20 candidatos
        }

        // ---- 2) Junta as 10 escalas: para cada pixel, a maior CC e a escala que a produziu ----
        Mat_<FLT> cc_max(L, C, apagado);              // Maior CC entre as escalas em cada pixel (começa com um valor menor que qualquer CC)
        Mat_<int> esc_max(L, C, -1);                  // Escala que deu essa maior CC em cada pixel (-1 = nenhuma)
        for (int i = 0; i < num_escalas; i++)         // Para cada escala
            for (int l = 0; l < L; l++)               // para cada linha
                for (int c = 0; c < C; c++)           // para cada coluna
                    if (R_cc[i](l,c) > cc_max(l,c)) { // se esta escala tem CC maior neste pixel
                        cc_max(l,c) = R_cc[i](l,c);   // guarda o valor
                        esc_max(l,c) = i;             // e a escala
                    }

        // ---- 3) Os 20 picos CC mais altos, separados por pelo menos 10 pixels ----
        vector<Point>  cand_loc;       // Posição (centro) de cada candidato
        vector<int>    cand_esc;       // Escala de cada candidato
        vector<double> cand_cc;        // Valor CC de cada candidato
        vector<double> cand_ncc;       // Valor NCC de cada candidato (mesma posição e mesma escala)
        for (int k = 0; k < num_cand; k++) {       // Procura um pico por vez, do mais alto para o mais baixo
            double maxVal; Point maxLoc;           // Valor e posição do maior pico restante
            minMaxLoc(cc_max, NULL, &maxVal, NULL, &maxLoc); // Encontra o maior valor que ainda não foi apagado
            if (maxVal <= apagado) break;          // Mapa todo apagado: não há mais picos
            int e = esc_max(maxLoc.y, maxLoc.x);   // Escala que deu este pico
            cand_loc.push_back(maxLoc);            // Guarda a posição do pico
            cand_esc.push_back(e);                 // Guarda a escala do pico
            cand_cc.push_back(maxVal);             // Guarda o valor CC do pico
            cand_ncc.push_back(R_ncc[e](maxLoc.y, maxLoc.x)); // Guarda a NCC na mesma posição e na mesma escala do pico CC

            for (int dl = -(dist_min-1); dl <= dist_min-1; dl++)       // Percorre a vizinhança do pico linhas
                for (int dc = -(dist_min-1); dc <= dist_min-1; dc++) { // e colunas
                    if (dl*dl + dc*dc >= dist_min*dist_min) continue;  // Só apaga pixels a menos de 10 pixels do pico (círculo)
                    int l = maxLoc.y + dl, c = maxLoc.x + dc;          // Coordenadas do pixel vizinho
                    if (0 <= l && l < L && 0 <= c && c < C)            // Se está dentro da imagem
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
            rectangle(a, r, Scalar(255, 255, 0), 1);     // Desenha o candidato em ciano, espessura 1
            Point p(r.x + r.width + 1, r.y + 7);         // Ponto à direita do retângulo para escrever os números
            putText(a, to_string(e), p, FONT_HERSHEY_SIMPLEX, 0.25, Scalar(255, 255, 0), 1); // Linha 1: escala
            putText(a, to_string((int)lround(cand_cc[k]*100)),  p + Point(0, 8),  FONT_HERSHEY_SIMPLEX, 0.25, Scalar(255, 255, 0), 1); // Linha 2: CC x 100
            putText(a, to_string((int)lround(cand_ncc[k]*100)), p + Point(0, 16), FONT_HERSHEY_SIMPLEX, 0.25, Scalar(255, 255, 0), 1); // Linha 3: NCC x 100
            if (melhor < 0 || cand_ncc[k] > cand_ncc[melhor]) melhor = k; // Guarda o candidato com a maior NCC
        }

        // ---- 5) Tela: imagem à esquerda, painel de depuração cinza à direita ----
        Mat_<COR> tela(L, C + lp, COR(128, 128, 128)); // Tela preenchida de cinza
        Rect r_proc(C, 0, lp, lp);                     // Painel superior: dígito pré-processado (14x14 ampliado)
        Rect r_extr(C, lp, lp, lp);                    // Painel inferior: dígito extraído do quadro, com o dígito lido

        // ---- 6) Decisão: placa encontrada? Se sim, deslocamento e (se perto) leitura do dígito ----
        bool placa = false;            // Há placa neste quadro?
        int best_scale = -1;           // Escala da placa (-1 = sem placa)
        int deslocamento = 0;          // Posição x da placa em relação ao centro da imagem (negativo = esquerda)
        int digito = -1;               // Dígito lido neste quadro (-1 = não leu)

        if (melhor >= 0 && cand_ncc[melhor] > limiar_ncc) { // Placa encontrada
            placa = true;                                   // Marca que há placa
            Point best_loc = cand_loc[melhor];              // Posição da placa
            best_scale = cand_esc[melhor];                  // Escala da placa
            deslocamento = best_loc.x - C/2;                // best_loc já é o centro da placa; diferença em relação ao centro da imagem
            int w_t = modelos_cc[best_scale].cols;          // Largura do modelo vencedor
            int h_t = modelos_cc[best_scale].rows;          // Altura do modelo vencedor
            Rect r(best_loc.x - (w_t-1)/2, best_loc.y - (h_t-1)/2, w_t, h_t); // Retângulo da placa
            rectangle(a, r, Scalar(0, 255, 255), 2);    // Desenha a placa em amarelo, espessura 2

            char str_cc[10], str_ncc[10];               // Buffers para os valores em texto
            sprintf(str_cc, "%.2f", cand_cc[melhor]);   // CC com 2 casas decimais
            sprintf(str_ncc, "%.2f", cand_ncc[melhor]); // NCC com 2 casas decimais
            putText(a, to_string(best_scale), Point(r.x + r.width, r.y),      FONT_HERSHEY_SIMPLEX, 0.4, Scalar(0, 0, 0), 1); // Escala vencedora
            putText(a, str_cc,                Point(r.x + r.width, r.y + 12), FONT_HERSHEY_SIMPLEX, 0.4, Scalar(0, 0, 0), 1); // CC 
            putText(a, str_ncc,               Point(r.x + r.width, r.y + 24), FONT_HERSHEY_SIMPLEX, 0.4, Scalar(0, 0, 0), 1); // NCC 

            // ---- Leitura do dígito, só com a placa perto (escala 0 ou 1) ----
            if (best_scale <= escala_leitura) {
                double k = (double)w_t / T.cols;                 // Fator de escala entre o modelo original e a placa no quadro
                Rect rd(r.x + lround(area_digito_T.x * k),       // Canto esquerdo da área do dígito no quadro
                        r.y + lround(area_digito_T.y * k),       // Canto superior
                        lround(area_digito_T.width * k),         // Largura
                        lround(area_digito_T.height * k));       // Altura
                int m = rd.width / 10;                           // Margem de 10% para não pegar a borda do quadrado amarelo
                rd = Rect(rd.x + m, rd.y + m, rd.width - 2*m, rd.height - 2*m); // Encolhe a área pela margem
                rd = rd & Rect(0, 0, C, L);                      // Recorta o que estiver fora do quadro

                if (rd.width >= 4 && rd.height >= 4) {           // Só continua se sobrou uma área útil
                    Mat_<FLT> d = f(rd).clone();                 // Copia o dígito em cinza float
                    double vmin, vmax;                           // Pixel mais escuro e mais claro da área
                    minMaxLoc(d, &vmin, &vmax);                  // Mede o contraste da área
                    if (vmax - vmin >= contraste_min) {          // Há algo escrito
                        d = normaliza(d);                        // Estica para 0 (mais escuro) a 1 (mais claro)
                        for (int l = 0; l < d.rows; l++)         // Para cada linha
                            for (int c = 0; c < d.cols; c++)     // e cada coluna
                                d(l,c) = min(1.0f, max(0.0f, (d(l,c) - preto) / (branco - preto))); // <= preto vira 0, >= branco vira 1

                        Mat_<FLT> b = mnist.bbox(d);             // Elimina bordas brancas e reduz para 14x14 (pixels <= 0.5 contam como traço)
                        if (mnist.localizou) {                   // bbox encontrou traço suficiente
                            Mat_<FLT> linha(1, b.total());       // FLANN recebe a imagem como um vetor linha de 196 valores
                            for (unsigned i = 0; i < b.total(); i++) linha(i) = b(i); // Copia os 14x14 pixels para a linha
                            digito = (int)mnist.predictInterno(linha); // Rótulo (0 a 9) da imagem de treino mais parecida

                            putText(a, to_string(digito), Point(r.x - 28, r.y + r.height), FONT_HERSHEY_SIMPLEX, 1.2, Scalar(0, 0, 255), 3); // Dígito em vermelho à esquerda da placa

                            Mat_<COR> b_cor, b_grande;           // Dígito pré-processado em cores, e ampliado
                            converte(b, b_cor);                  // Converte FLT (0 a 1) para COR
                            resize(b_cor, b_grande, Size(lp, lp), 0, 0, INTER_NEAREST); // Amplia 14x14 para o tamanho do painel
                            b_grande.copyTo(tela(r_proc));       // Coloca no painel superior

                            Mat_<COR> extr_grande;               // Dígito extraído ampliado
                            resize(imagem_limpa(rd), extr_grande, Size(lp, lp)); // Amplia o dígito extraído (sem desenhos)
                            extr_grande.copyTo(tela(r_extr));    // Coloca no painel inferior
                            putText(tela, to_string(digito), Point(C + lp/4, L - lp/5), FONT_HERSHEY_SIMPLEX, 2.5, Scalar(0, 0, 255), 4); // Dígito lido sobre o painel inferior
                        }
                    }
                }
            }
        }

        // ---- 7) Máquina de estados: decide as potências das rodas neste quadro ----
        // Os estados são testados em sequência (if, não else if): quando um estado muda para o próximo,
        // o novo estado já roda no mesmo quadro. Assim o movimento do estado 3 começa sem esperar um quadro.
        int vel_esq = 0, vel_dir = 0;  // Potências enviadas ao servidor (0, 0 = parado)

        if (estado == 0) {                                          // Estado 0: detecte e siga a placa
            if (placa && best_scale <= escala_leitura) {            // Placa grande (perto): para para não bater e ler o dígito
                estado = 1;                                         // Vai para o estado 1
                t_estado = timeSinceEpoch();                        // Começa a contar o tempo de parada
            } else if (placa) {                                     // Placa pequena: anda em direção a ela
                int diff = deslocamento * k_proporcional;           // Correção de curva, proporcional ao deslocamento
                vel_esq = vel_base_esq + diff;                      // Placa à direita (diff > 0): roda esquerda acelera
                vel_dir = vel_base_dir - diff;                      // e a direita desacelera (ou o contrário)
            }                                                       // Sem placa: fica parado (0, 0) no estado 0
        }

        if (estado == 1) {                                          // Estado 1: pare o carrinho
            if (timeSinceEpoch() - t_estado >= t_parar) {           // O carrinho já parou completamente
                estado = 2;                                         // Vai ler o dígito
            }
        }

        if (estado == 2) {                                          // Estado 2: leia o dígito (carrinho parado, imagem nítida)
            if (!placa || digito < 0) {                             // Placa não visível ou dígito não lido
                estado = 0;                                         // Volta a procurar a placa
            } else if (digito == 0 || digito == 1) {                // Dígito 0 ou 1: fim do trajeto
                digito_lido = digito;                               // Guarda o dígito (para mostrar na tela)
                estado = 4;                                         // Vai para o fim
            } else {                                                // Dígito de 2 a 9: um movimento
                digito_lido = digito;                               // Guarda o dígito lido
                estado = 3;                                         // Vai executar o movimento
                t_estado = timeSinceEpoch();                        // Inicializa o cronômetro do movimento
            }
        }

        if (estado == 3) {                                          // Estado 3: faça o movimento indicado pelo dígito
            double duracao = 0;                                     // Tempo programado do movimento
            int ve = 0, vd = 0;                                     // Potências do movimento
            if (digito_lido == 2)      { duracao = t_180;    ve = -vel_giro;     vd = vel_giro; }     // 2: 180 graus à esquerda (gira no lugar)
            else if (digito_lido == 3) { duracao = t_180;    ve = vel_giro;      vd = -vel_giro; }    // 3: 180 graus à direita
            else if (digito_lido <= 5) { duracao = t_frente; ve = vel_base_esq;  vd = vel_base_dir; } // 4 ou 5: reto, passa por baixo da placa
            else if (digito_lido <= 7) { duracao = t_90;     ve = -vel_giro;     vd = vel_giro; }     // 6 ou 7: 90 graus à esquerda
            else                       { duracao = t_90;     ve = vel_giro;      vd = -vel_giro; }    // 8 ou 9: 90 graus à direita

            if (timeSinceEpoch() - t_estado < duracao) {            // Ainda dentro do tempo programado
                vel_esq = ve;                                       // Repete o movimento
                vel_dir = vd;
            } else {                                                // Acabou o tempo
                estado = 5;                                         // Vai parar o carrinho (motores em 0 neste quadro)
                t_estado = timeSinceEpoch();                        // Começa a contar o tempo de parada
            }
        }

        if (estado == 5) {                                          // Estado 5: termine o movimento, pare o carrinho
            if (timeSinceEpoch() - t_estado >= t_parar) {           // O carrinho já parou completamente
                estado = 0;                                         // Volta a procurar a próxima placa
            }
        }
        // Estado 4 (fim): potências ficam em 0; o programa encerra logo abaixo

        vel_esq = max(-100, min(100, vel_esq));                     // Trava de ±100
        vel_dir = max(-100, min(100, vel_dir));                     // Trava de ±100
        client.sendBytes(sizeof(int), (BYTE*)&vel_esq);             // Envia a potência da roda esquerda (4 bytes)
        client.sendBytes(sizeof(int), (BYTE*)&vel_dir);             // Envia a potência da roda direita (4 bytes)

        // ---- 8) Exibição e gravação ----
        char str_estado[64];                          // Texto de estado para depuração
        sprintf(str_estado, "estado=%d dirx=%d digito=%d", estado, deslocamento, digito_lido); // Estado, deslocamento e dígito guardado
        putText(a, str_estado, Point(3, 12), FONT_HERSHEY_SIMPLEX, 0.4, Scalar(0, 0, 255), 1); // Escreve em vermelho no canto superior esquerdo
        a.copyTo(tela(Rect(0, 0, C, L)));             // Copia o quadro com os desenhos para a parte esquerda da tela

        Mat_<COR> tela_ampliada;                      // Tela ampliada para exibição
        resize(tela, tela_ampliada, Size(1600, 900)); // Amplia para 1600x900
        imshow("Visão Carrinho", tela_ampliada);      // Mostra na janela "Visão Carrinho"
        
        if (gravar_video) {                           // Se a gravação está ativa
            if (gravar_tela) vo << tela_ampliada;     // modo 't': grava a tela com os desenhos
            else             vo << imagem_limpa;      // modo 'c': grava a câmera sem desenhos
        }
        
        if (estado == 4 || waitKey(1) == 27) {        // Fim do trajeto (dígito 0 ou 1) ou ESC
            sinal_ok = 0;                             // Sinal de controle: 0 = encerrar
            client.sendBytes(1, &sinal_ok);           // Avisa o Raspberry, que sai do laço e desliga os motores
            break;                                    // Sai do laço principal
        }
    }
    return 0;                                         // Fim do programa
}
