
// cliente8.cpp (Processa visão no computador)
// compila cliente8 -ocv -v3 -omp
// Uso: cliente8 <IP_RASPBERRY> [videosaida.avi] [t/c]
// É o cliente7 com um servidor web embutido: no navegador do celular (mesmo Wi-Fi), abra
//   http://IP_DO_COMPUTADOR:8080
// O celular vê a tela completa (teclado + câmera + painel do dígito) e controla o carrinho tocando
// no teclado virtual da própria imagem, igual ao mouse no computador. O mouse continua funcionando.
//   modo manual: dirige pelo teclado virtual 3x3 (mouse ou toque)
//   botão "M/A" (centro do teclado): passa de manual para automático e vice-versa
//   modo automático: máquina de estados do cliente6; ao ler dígito 0 ou 1, volta para o manual
//   ESC (no computador): termina os dois programas
// Os arquivos do MNIST (train-images.idx3-ubyte e train-labels.idx1-ubyte) devem estar no diretório atual.
#include "projeto.hpp"
#include <omp.h>
#include <thread>                      // std::thread: o servidor web roda em paralelo com o laço principal
#include <mutex>                       // std::mutex: protege o último JPEG, compartilhado entre threads
#include <atomic>                      // std::atomic: variáveis lidas/escritas por várias threads sem mutex
#include <condition_variable>          // Acorda as threads de vídeo quando chega um quadro novo

// ---------------- Teclado virtual (variáveis globais porque o callback do mouse e o servidor web precisam delas) ----------------
const int fator = 2;                   // A tela é mostrada ampliada 2 vezes (mesma proporção, para o mouse cair no lugar certo)
const int lado_tecla = 80;             // Lado de cada tecla virtual (3 teclas = 240 = altura da imagem da câmera)
atomic<int> botao(0);                  // Tecla de movimento apertada agora (1 a 9, exceto 5); 0 = nenhuma
atomic<bool> pediu_troca(false);       // true quando o usuário tocou/clicou em "M/A" e o laço ainda não trocou o modo
atomic<bool> botao_web(false);         // true se a tecla atual veio do celular (só essa é vigiada pelo watchdog)
atomic<double> t_ultimo_web(0);        // Instante do último comando recebido do celular

// ---------------- Servidor web ----------------
const int porta_web = 8080;            // Porta HTTP (a 3490 já é usada com o Raspberry)
const double timeout_web = 0.5;        // Segundos sem comando do celular para considerar a conexão perdida e parar
const int qualidade_web = 70;          // Qualidade do JPEG enviado ao celular (menor = mais leve na rede)
mutex mtx_jpeg;                        // Protege jpeg_atual e num_quadro
condition_variable cv_jpeg;            // Avisa as threads de vídeo que há um quadro novo
vector<BYTE> jpeg_atual;               // Último quadro da tela, já comprimido em JPEG
long num_quadro = 0;                   // Contador de quadros publicados (para cada espectador saber se já viu o atual)
atomic<int> espectadores(0);           // Quantos navegadores estão assistindo (se 0, nem comprime o JPEG)

// Página enviada ao celular. LADO é trocado pelo lado da tecla em pixels da imagem ampliada.
const string pagina_html = R"HTML(<!DOCTYPE html>
<html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=no">
<title>Carrinho</title>
<style>
 body { margin:0; background:#222; color:#ccc; font-family:sans-serif; text-align:center; }
 img  { width:100%; touch-action:none; user-select:none; -webkit-user-select:none;
        -webkit-touch-callout:none; display:block; }
 p    { font-size:14px; margin:6px; }
</style></head>
<body>
<img id="tela" src="/stream" draggable="false">
<p>Toque e segure uma flecha para andar. Toque no centro (M/A) para trocar o modo.
Use o celular deitado para teclas maiores.</p>
<script>
const LADO = LADO_TECLA;               // Lado de uma tecla em pixels da imagem recebida
const img = document.getElementById('tela');
let atual = 0, timer = null;           // Tecla apertada e repetidor de comando
img.oncontextmenu = () => false;       // Impede o menu de toque longo
function tecla(ev) {                   // Converte o toque na tecla 1..9 (0 = fora do teclado)
  const r = img.getBoundingClientRect();
  const x = (ev.clientX - r.left) * img.naturalWidth  / r.width;
  const y = (ev.clientY - r.top)  * img.naturalHeight / r.height;
  if (x < 0 || y < 0 || x >= 3*LADO || y >= 3*LADO) return 0;
  return (2 - Math.floor(y/LADO))*3 + Math.floor(x/LADO) + 1;
}
function envia(q) { fetch('/cmd?' + q, {cache:'no-store'}).catch(() => {}); }
img.addEventListener('pointerdown', ev => {
  ev.preventDefault();
  img.setPointerCapture(ev.pointerId); // Continua recebendo movimentos mesmo se o dedo sair da imagem
  const t = tecla(ev);
  if (t == 5) { envia('ma=1'); return; }        // M/A: um pedido por toque
  atual = t; envia('b=' + atual);
  clearInterval(timer);
  timer = setInterval(() => envia('b=' + atual), 200); // Repete enquanto segura (watchdog no computador)
});
img.addEventListener('pointermove', ev => {     // Arrastou o dedo para outra tecla
  if (timer === null) return;
  let t = tecla(ev); if (t == 5) t = 0;
  if (t != atual) { atual = t; envia('b=' + atual); }
});
function solta() { clearInterval(timer); timer = null; atual = 0; envia('b=0'); }
img.addEventListener('pointerup', solta);
img.addEventListener('pointercancel', solta);
</script></body></html>)HTML";

bool envia_tudo(int fd, const void* buf, size_t n) { // Envia n bytes pelo socket fd; false se o navegador desconectou
    const char* p = (const char*)buf;  // Ponteiro para o próximo byte a enviar
    while (n > 0) {                    // Até enviar tudo
        ssize_t k = send(fd, p, n, MSG_NOSIGNAL); // MSG_NOSIGNAL: não mata o programa se o navegador fechou
        if (k <= 0) return false;      // Erro ou conexão fechada
        p += k; n -= k;                // Avança o que já foi enviado
    }
    return true;                       // Enviou tudo
}

bool envia_texto(int fd, const string& s) { return envia_tudo(fd, s.data(), s.size()); } // Atalho para strings

void transmite_video(int fd) {         // Mantém a conexão aberta e envia cada quadro novo (MJPEG)
    if (!envia_texto(fd, "HTTP/1.1 200 OK\r\n"
                         "Content-Type: multipart/x-mixed-replace; boundary=quadro\r\n"
                         "Cache-Control: no-cache\r\nConnection: close\r\n\r\n")) return; // Cabeçalho do fluxo MJPEG
    espectadores++;                    // Passa a existir alguém assistindo
    long visto = -1;                   // Número do último quadro enviado a este navegador
    while (true) {                     // Um quadro por iteração
        vector<BYTE> vb;               // Cópia local do JPEG (para não segurar o mutex durante o envio)
        {
            unique_lock<mutex> lk(mtx_jpeg);                                  // Trava o JPEG compartilhado
            cv_jpeg.wait_for(lk, chrono::seconds(1), [&]{ return num_quadro != visto; }); // Espera um quadro novo (no máximo 1 s)
            if (num_quadro == visto || jpeg_atual.empty()) continue;          // Nada novo ainda: volta a esperar
            vb = jpeg_atual;                                                  // Copia o quadro
            visto = num_quadro;                                               // Marca como visto
        }
        string cab = "--quadro\r\nContent-Type: image/jpeg\r\nContent-Length: " + to_string(vb.size()) + "\r\n\r\n"; // Cabeçalho da parte
        if (!envia_texto(fd, cab) || !envia_tudo(fd, vb.data(), vb.size()) || !envia_texto(fd, "\r\n")) break; // Navegador fechou: termina
    }
    espectadores--;                    // Este navegador saiu
}

void atende(int fd) {                  // Atende uma conexão HTTP (roda numa thread própria)
    string req;                        // Requisição recebida
    char buf[1024];                    // Buffer de leitura
    while (req.find("\r\n\r\n") == string::npos && req.size() < 4096) { // Lê até o fim do cabeçalho
        ssize_t k = recv(fd, buf, sizeof buf, 0);  // Recebe um pedaço
        if (k <= 0) { close(fd); return; }        // Conexão fechada ou erro
        req.append(buf, k);                        // Acumula
    }
    istringstream is(req);             // Primeira linha: "GET /caminho HTTP/1.1"
    string metodo, caminho;            // Método e caminho pedidos
    is >> metodo >> caminho;           // Separa os dois

    if (caminho == "/") {                                         // Página principal
        string html = pagina_html;                                // Copia o modelo da página
        size_t pos = html.find("LADO_TECLA");                     // Lugar do lado da tecla no JavaScript
        if (pos != string::npos) html.replace(pos, 10, to_string(lado_tecla*fator)); // Lado da tecla na imagem ampliada (160)
        envia_texto(fd, "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: "
                        + to_string(html.size()) + "\r\nConnection: close\r\n\r\n" + html); // Envia a página
    } else if (caminho == "/stream") {                            // Vídeo da tela
        transmite_video(fd);                                      // Fica aqui enquanto o navegador assistir
    } else if (caminho.compare(0, 5, "/cmd?") == 0) {             // Comando do teclado
        size_t pma = caminho.find("ma=1");                        // Toque no M/A?
        size_t pb  = caminho.find("b=");                          // Tecla de movimento?
        if (pma != string::npos) {                                // M/A
            pediu_troca = true;                                   // Pede a troca de modo ao laço principal
        } else if (pb != string::npos) {                          // Tecla de movimento (0 = soltou)
            int t = atoi(caminho.c_str() + pb + 2);               // Número da tecla
            if (t < 0 || t > 9 || t == 5) t = 0;                  // Valor inválido ou M/A: parado
            botao = t;                                            // Atualiza a tecla apertada
            botao_web = true;                                     // Veio do celular: o watchdog passa a vigiar
            t_ultimo_web = timeSinceEpoch();                      // Marca o instante do comando
        }
        envia_texto(fd, "HTTP/1.1 204 No Content\r\nConnection: close\r\n\r\n"); // Resposta sem conteúdo
    } else {                                                      // Qualquer outro caminho
        envia_texto(fd, "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n"); // Não existe
    }
    close(fd);                         // Fecha a conexão
}

void servidor_web() {                  // Aceita conexões do navegador para sempre (roda numa thread própria)
    int s = socket(AF_INET, SOCK_STREAM, 0);       // Socket TCP IPv4
    if (s == -1) erro("web: erro em socket");      // Falhou ao criar
    int sim = 1;                                   // Opção ligada
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &sim, sizeof sim); // Permite reabrir a porta logo após fechar o programa
    struct sockaddr_in end;                        // Endereço local
    memset(&end, 0, sizeof end);                   // Zera a estrutura
    end.sin_family = AF_INET;                      // IPv4
    end.sin_addr.s_addr = htonl(INADDR_ANY);       // Aceita conexões em qualquer interface (inclusive o Wi-Fi)
    end.sin_port = htons(porta_web);               // Porta 8080 na ordem de bytes da rede
    if (::bind(s, (struct sockaddr*)&end, sizeof end) == -1) erro("web: porta 8080 ocupada"); // Reserva a porta
    if (listen(s, 10) == -1) erro("web: erro em listen");     // Até 10 conexões na fila
    printf("web: abra http://IP_DO_COMPUTADOR:%d no celular (veja o IP com: hostname -I)\n", porta_web); // Instrução no terminal
    while (true) {                                 // Para sempre
        int fd = accept(s, NULL, NULL);            // Espera um navegador conectar
        if (fd == -1) continue;                    // Erro passageiro: tenta de novo
        thread(atende, fd).detach();               // Atende em outra thread (o vídeo fica conectado muito tempo)
    }
}

int tecla_em(int c, int l) {           // Converte uma posição (c, l) da tela original na tecla 1..9 (0 = fora do teclado)
    if (c < 0 || c >= 3*lado_tecla || l < 0 || l >= 3*lado_tecla) return 0; // Fora do teclado
    int col = c / lado_tecla;          // Coluna da tecla (0 a 2)
    int lin = l / lado_tecla;          // Linha da tecla (0 a 2)
    return (2 - lin)*3 + col + 1;      // Mesma disposição do teclado numérico: 7 8 9 / 4 5 6 / 1 2 3
}

void on_mouse(int event, int c, int l, int flags, void* userdata) { // Chamada pelo OpenCV a cada evento de mouse na janela
    int t = tecla_em(c / fator, l / fator);   // Tecla sob o cursor (desfaz a ampliação da tela)
    if (event == EVENT_LBUTTONDOWN) {         // Apertou o botão esquerdo
        if (t == 5) pediu_troca = true;       // "M/A": pede a troca de modo (uma vez por clique)
        else { botao = t; botao_web = false; } // Outra tecla: começa o movimento (comando local, sem watchdog)
    } else if (event == EVENT_LBUTTONUP) {    // Soltou o botão
        botao = 0; botao_web = false;         // Para o movimento
    } else if (event == EVENT_MOUSEMOVE && (flags & EVENT_FLAG_LBUTTON)) { // Arrastou com o botão apertado
        botao = (t == 5) ? 0 : t;             // Segue a tecla sob o cursor (fora do teclado ou em "M/A" = parado)
        botao_web = false;                    // Comando local
    }
}

void desenha_teclado(Mat_<COR>& tela, int apertado, bool automatico) { // Desenha o teclado 3x3 no canto esquerdo da tela
    Scalar escuro(0, 0, 128), claro(0, 0, 255), preto(0, 0, 0);       // Vermelho escuro (solto), vermelho (apertado), grade
    for (int t = 1; t <= 9; t++) {                                     // Para cada tecla
        int col = (t-1) % 3, lin = 2 - (t-1) / 3;                      // Posição da tecla na grade
        int x0 = col*lado_tecla, y0 = lin*lado_tecla;                  // Canto superior esquerdo da tecla
        int cx = x0 + lado_tecla/2, cy = y0 + lado_tecla/2, d = lado_tecla*3/10; // Centro da tecla e meio tamanho da flecha
        rectangle(tela, Rect(x0, y0, lado_tecla, lado_tecla), preto, 1); // Borda da tecla
        Scalar cor = (t == apertado) ? claro : escuro;                 // Tecla apertada em vermelho
        if (t == 7) arrowedLine(tela, Point(cx+d, cy+d), Point(cx-d, cy-d), cor, 3, 8, 0, 0.3); // 7: frente virando à esquerda
        if (t == 8) arrowedLine(tela, Point(cx, cy+d),   Point(cx, cy-d),   cor, 3, 8, 0, 0.3); // 8: frente
        if (t == 9) arrowedLine(tela, Point(cx-d, cy+d), Point(cx+d, cy-d), cor, 3, 8, 0, 0.3); // 9: frente virando à direita
        if (t == 1) arrowedLine(tela, Point(cx+d, cy-d), Point(cx-d, cy+d), cor, 3, 8, 0, 0.3); // 1: ré virando à esquerda
        if (t == 2) arrowedLine(tela, Point(cx, cy-d),   Point(cx, cy+d),   cor, 3, 8, 0, 0.3); // 2: ré
        if (t == 3) arrowedLine(tela, Point(cx-d, cy-d), Point(cx+d, cy+d), cor, 3, 8, 0, 0.3); // 3: ré virando à direita
        if (t == 4) {                                                  // 4: gira no lugar para a esquerda (flecha em "U" invertido)
            line(tela, Point(cx+d, cy+d), Point(cx+d, cy-d), cor, 3);  // Sobe pela direita
            line(tela, Point(cx+d, cy-d), Point(cx-d, cy-d), cor, 3);  // Atravessa para a esquerda
            arrowedLine(tela, Point(cx-d, cy-d), Point(cx-d, cy+d), cor, 3, 8, 0, 0.3); // Desce com a ponta
        }
        if (t == 6) {                                                  // 6: gira no lugar para a direita (espelho da tecla 4)
            line(tela, Point(cx-d, cy+d), Point(cx-d, cy-d), cor, 3);  // Sobe pela esquerda
            line(tela, Point(cx-d, cy-d), Point(cx+d, cy-d), cor, 3);  // Atravessa para a direita
            arrowedLine(tela, Point(cx+d, cy-d), Point(cx+d, cy+d), cor, 3, 8, 0, 0.3); // Desce com a ponta
        }
        if (t == 5) putText(tela, automatico ? "A" : "M", Point(cx-16, cy+16), FONT_HERSHEY_SIMPLEX, 1.4, automatico ? claro : escuro, 3); // 5: modo atual
    }
}

int main(int argc, char** argv) {
    if (argc < 2) erro("Uso: cliente7 <IP_RASPBERRY> [videosaida.avi] [t/c]");

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
    const double contraste_min = 0.15; // Diferença mínima entre o pixel mais claro e o mais escuro da área do dígito
    const FLT preto = 0.2f;            // Depois de normalizar (0 a 1), valores <= 0.2 viram preto (0)
    const FLT branco = 0.6f;           // e valores >= 0.6 viram branco (1); entre os dois, escala linear
    const int lado_mnist = 14;         // Tamanho (14x14) das imagens do MNIST
    const string dir_mnist = "/home/tomi/cekeikon5/tiny_dnn/data";      // Diretório dos arquivos do MNIST (diretório atual)

    // ---------------- Parâmetros de movimento (calibrar no carrinho) ----------------
    const int vel_base_esq = 90;       // Potência base da roda esquerda para andar reto
    const int vel_base_dir = 100;      // Potência base da roda direita, maior para compensar o motor mais fraco
    const float k_proporcional = 0.6f; // Ganho do controle proporcional ao seguir a placa
    const int vel_giro = 80;           // Potência das rodas ao girar no lugar (uma para frente, outra para trás)
    const double t_parar = 0.7;        // Segundos esperando o carrinho parar (estados 1 e 5)
    const double t_90 = 0.6;           // Segundos girando para virar 90 graus
    const double t_180 = 1.2;          // Segundos girando para virar 180 graus
    const double t_frente = 1.5;       // Segundos andando reto para passar por baixo da placa

    // ---------------- Leitura do MNIST e treino do FLANN ----------------
    MnistFlann mnist(lado_mnist, true, true); // 14x14, inverte cores (fundo branco, dígito preto), elimina bordas brancas (bbox)
    mnist.le(dir_mnist, 60000, 0);     // Lê as 60000 imagens de treino; 0 = não lê as imagens de teste
    mnist.train();                     // Monta o índice FLANN com as 60000 imagens de treino

    // ---------------- Leitura e pré-processamento do modelo ----------------
    Mat_<COR> temp = imread("quadrado.png", 1);                       // Lê a imagem-modelo (401x401) em cores (1 = colorida, BGR)
    if (temp.total()==0) erro("Erro leitura quadrado.png");           // Se a imagem não tem pixels, a leitura falhou: aborta
    Mat_<FLT> T; converte(temp, T);                                   // Converte para cinza float (0 = preto, 1 = branco); só o branco (255,255,255) vira exatamente 1.0

    // Área do dígito no modelo: o quadrado "don't care" (pixels exatamente 1.0) que contém o centro da placa
    int cx = T.cols/2, cy = T.rows/2;           // Centro do modelo
    if (T(cy,cx) != 1.0f) erro("Erro: o centro de quadrado.png nao e' don't care"); // O centro tem que estar dentro do quadrado amarelo
    int esq = cx;   while (esq > 0          && T(cy, esq-1) == 1.0f) esq--;   // Anda para a esquerda enquanto o pixel for don't care
    int dir = cx;   while (dir < T.cols-1   && T(cy, dir+1) == 1.0f) dir++;   // Anda para a direita
    int cima = cy;  while (cima > 0         && T(cima-1, cx) == 1.0f) cima--; // Anda para cima
    int baixo = cy; while (baixo < T.rows-1 && T(baixo+1, cx) == 1.0f) baixo++; // Anda para baixo
    Rect area_digito_T(esq, cima, dir-esq+1, baixo-cima+1); // Retângulo da área do dígito em coordenadas do modelo 401x401
    
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

    int nl = 240;                         // Altura esperada da imagem da câmera
    int nc = 320;                         // Largura esperada da imagem da câmera
    int largura_tela = 3*lado_tecla + nc + nl/2; // Teclado (240) + câmera (320) + painel do dígito (120)

    Mat_<COR> a;                          // Imagem colorida recebida da câmera (onde serão feitos os desenhos)
    Mat_<COR> imagem_limpa;               // Cópia da imagem da câmera sem desenhos (modo 'c' e painel do dígito)
    Mat_<FLT> f;                          // Versão em cinza float da imagem, usada nos casamentos de modelos e na leitura do dígito
    vector<Mat_<FLT>> R_cc(num_escalas);  // Mapas de correlação CC, um por escala
    vector<Mat_<FLT>> R_ncc(num_escalas); // Mapas de correlação NCC, um por escala

    // ---------------- Gravação de vídeo opcional ----------------
    VideoWriter vo;                    // Objeto que grava o vídeo de saída
    bool gravar_video = false;         // Só grava se o nome do arquivo for passado
    bool gravar_tela = true;           // true = grava a tela ampliada com teclado e desenhos ('t', padrão); false = grava a câmera limpa ('c')

    if (argc >= 3) {                   // Se foi passado o nome do arquivo de vídeo
        gravar_video = true;           // ativa a gravação
        if (argc >= 4) {               // Se também foi passado o modo de gravação
            string modo = argv[3];     // lê o modo ('t' ou 'c')
            if (modo == "c") gravar_tela = false; // 'c' grava apenas a câmera limpa
        }
        if (gravar_tela) vo.open(argv[2], CV_FOURCC('X','V','I','D'), fps_video, Size(largura_tela*fator, nl*fator)); // Modo 't': tela ampliada
        else             vo.open(argv[2], CV_FOURCC('X','V','I','D'), fps_video, Size(nc, nl));                       // Modo 'c': câmera, 320x240
        if (!vo.isOpened()) erro("Erro criando " + string(argv[2])); // Aborta se não conseguiu criar o arquivo
    }

    namedWindow("Carrinho");                  // Janela de tamanho fixo (coordenadas do mouse = pixels da tela ampliada)
    setMouseCallback("Carrinho", on_mouse);   // Liga o mouse da janela à função on_mouse

    thread(servidor_web).detach();     // Liga o servidor web numa thread própria (termina junto com o programa)

    CLIENT client(argv[1]);            // Conecta ao servidor depois do treino, para o carrinho não esperar o MNIST

    // ---------------- Modo e máquina de estados ----------------
    bool automatico = false;           // O sistema começa no modo manual
    int estado = 0;                    // Estado do modo automático (começa procurando a placa)
    double t_estado = timeSinceEpoch(); // Instante em que entrou no estado atual (cronômetro dos estados 1, 3 e 5)
    int digito_lido = -1;              // Dígito guardado no estado 2 e executado no estado 3 (-1 = nenhum)

    while (true) {                      // Laço principal: um quadro por iteração
        BYTE sinal_ok = 1;              // Sinal de controle: 1 = continuar
        client.sendBytes(1, &sinal_ok); // Pede o próximo quadro ao Raspberry
        client.receiveImgComp(a);       // Recebe a imagem JPEG e descomprime em "a"
        if (a.empty()) erro("Erro: imagem recebida vazia"); // Falha ao decodificar o JPEG: encerra
        
        imagem_limpa = a.clone();      // Guarda a câmera antes de desenhar
        int L = a.rows, C = a.cols;    // Tamanho real do quadro recebido
        int lp = L/2;                  // Lado de cada imagem do painel de depuração

        // ---- Troca de modo pedida pelo botão "M/A" ----
        // ---- Watchdog do celular: se os comandos pararam de chegar (Wi-Fi caiu), solta a tecla ----
        if (botao_web && timeSinceEpoch() - t_ultimo_web > timeout_web) { // Tecla do celular sem renovação há mais de 0,5 s
            botao = 0;                 // Para o carrinho
            botao_web = false;         // Nada mais a vigiar
        }
        int tecla_atual = botao;       // Lê a tecla uma vez só (ela pode mudar a qualquer momento em outra thread)

        if (pediu_troca.exchange(false)) { // O usuário tocou/clicou em "M/A" desde o último quadro (lê e zera de uma vez)
            automatico = !automatico;  // Manual -> automático ou automático -> manual
            estado = 0;                // O automático sempre recomeça procurando a placa
            t_estado = timeSinceEpoch(); // Zera o cronômetro
            digito_lido = -1;          // Esquece o dígito anterior
        }

        // ---- Tela: teclado à esquerda, câmera no meio, painel do dígito à direita ----
        Mat_<COR> tela(L, 3*lado_tecla + C + lp, COR(128, 128, 128)); // Tela preenchida de cinza
        Rect r_camera(3*lado_tecla, 0, C, L);                         // Onde vai a imagem da câmera
        Rect r_proc(3*lado_tecla + C, 0, lp, lp);                     // Painel superior: dígito pré-processado (14x14 ampliado)
        Rect r_extr(3*lado_tecla + C, lp, lp, lp);                    // Painel inferior: dígito extraído, com o dígito lido

        int vel_esq = 0, vel_dir = 0;  // Potências enviadas ao servidor (0, 0 = parado)
        int deslocamento = 0;          // Posição x da placa em relação ao centro da imagem (negativo = esquerda)

        if (!automatico) {
            // =================== MODO MANUAL: potências pela tecla virtual apertada ===================
            if      (tecla_atual == 8) { vel_esq = vel_base_esq;    vel_dir = vel_base_dir; }    // 8: frente
            else if (tecla_atual == 2) { vel_esq = -vel_base_esq;   vel_dir = -vel_base_dir; }   // 2: ré
            else if (tecla_atual == 7) { vel_esq = vel_base_esq/2;  vel_dir = vel_base_dir; }    // 7: frente virando à esquerda
            else if (tecla_atual == 9) { vel_esq = vel_base_esq;    vel_dir = vel_base_dir/2; }  // 9: frente virando à direita
            else if (tecla_atual == 4) { vel_esq = -vel_giro;       vel_dir = vel_giro; }        // 4: gira no lugar para a esquerda
            else if (tecla_atual == 6) { vel_esq = vel_giro;        vel_dir = -vel_giro; }       // 6: gira no lugar para a direita
            else if (tecla_atual == 1) { vel_esq = -vel_base_esq/2; vel_dir = -vel_base_dir; }   // 1: ré virando à esquerda
            else if (tecla_atual == 3) { vel_esq = -vel_base_esq;   vel_dir = -vel_base_dir/2; } // 3: ré virando à direita
        } else {
            // =================== MODO AUTOMÁTICO: mesma lógica do cliente6 ===================
            converte(a, f);            // Converte o quadro colorido para cinza float em "f"

            // ---- 1) Em paralelo: 10 casamentos CC e 10 casamentos NCC ----
            #pragma omp parallel for                // Distribui as 10 escalas entre as threads (cada iteração escreve só em R_cc[i] e R_ncc[i])
            for (int i = 0; i < num_escalas; i++) { // Uma escala por iteração
                R_cc[i]  = matchTemplateSame(f, modelos_cc[i],  CV_TM_CCORR);         // Mapa CC do tamanho de f, alinhado pelo centro do modelo
                R_ncc[i] = matchTemplateSame(f, modelos_ncc[i], CV_TM_CCOEFF_NORMED); // Mapa NCC (-1 a 1), lido depois só nos 20 candidatos
            }

            // ---- 2) Junta as 10 escalas: para cada pixel, a maior CC e a escala que a produziu ----
            Mat_<FLT> cc_max(L, C, apagado);              // Maior CC entre as escalas em cada pixel
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

                for (int dl = -(dist_min-1); dl <= dist_min-1; dl++)     // Percorre a vizinhança do pico (linhas)
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
                Rect r(cand_loc[k].x - (w_t-1)/2, cand_loc[k].y - (h_t-1)/2, w_t, h_t); // Retângulo do candidato
                rectangle(a, r, Scalar(255, 255, 0), 1);     // Desenha o candidato em ciano, espessura 1
                Point p(r.x + r.width + 1, r.y + 7);         // Ponto à direita do retângulo para escrever os números
                putText(a, to_string(e), p, FONT_HERSHEY_SIMPLEX, 0.25, Scalar(255, 255, 0), 1); // Linha 1: escala
                putText(a, to_string((int)lround(cand_cc[k]*100)),  p + Point(0, 8),  FONT_HERSHEY_SIMPLEX, 0.25, Scalar(255, 255, 0), 1); // Linha 2: CC x 100
                putText(a, to_string((int)lround(cand_ncc[k]*100)), p + Point(0, 16), FONT_HERSHEY_SIMPLEX, 0.25, Scalar(255, 255, 0), 1); // Linha 3: NCC x 100
                if (melhor < 0 || cand_ncc[k] > cand_ncc[melhor]) melhor = k; // Guarda o candidato com a maior NCC
            }

            // ---- 5) Placa encontrada? Se sim, deslocamento e (se perto) leitura do dígito ----
            bool placa = false;            // Há placa neste quadro?
            int best_scale = -1;           // Escala da placa (-1 = sem placa)
            int digito = -1;               // Dígito lido neste quadro (-1 = não leu)

            if (melhor >= 0 && cand_ncc[melhor] > limiar_ncc) { // Placa encontrada
                placa = true;                                   // Marca que há placa
                Point best_loc = cand_loc[melhor];              // Posição da placa
                best_scale = cand_esc[melhor];                  // Escala da placa
                deslocamento = best_loc.x - C/2;                // best_loc já é o centro da placa; diferença em relação ao centro da imagem
                int w_t = modelos_cc[best_scale].cols;          // Largura do modelo vencedor
                int h_t = modelos_cc[best_scale].rows;          // Altura do modelo vencedor
                Rect r(best_loc.x - (w_t-1)/2, best_loc.y - (h_t-1)/2, w_t, h_t); // Retângulo da placa
                rectangle(a, r, Scalar(153, 255, 255), 2);      // Desenha a placa em amarelo claro, espessura 2

                char str_cc[10], str_ncc[10];               // Buffers para os valores em texto
                sprintf(str_cc, "%.2f", cand_cc[melhor]);   // CC com 2 casas decimais
                sprintf(str_ncc, "%.2f", cand_ncc[melhor]); // NCC com 2 casas decimais
                putText(a, to_string(best_scale), Point(r.x + r.width, r.y),      FONT_HERSHEY_SIMPLEX, 0.4, Scalar(153, 255, 255), 1); // Escala vencedora
                putText(a, str_cc,                Point(r.x + r.width, r.y + 12), FONT_HERSHEY_SIMPLEX, 0.4, Scalar(255, 255, 255), 1); // CC em branco
                putText(a, str_ncc,               Point(r.x + r.width, r.y + 24), FONT_HERSHEY_SIMPLEX, 0.4, Scalar(255, 255, 255), 1); // NCC em branco

                if (best_scale <= escala_leitura) {                  // Leitura do dígito, só com a placa perto (escala 0 ou 1)
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

                            Mat_<FLT> b = mnist.bbox(d);             // Elimina bordas brancas e reduz para 14x14
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
                                putText(tela, to_string(digito), Point(r_extr.x + lp/4, L - lp/5), FONT_HERSHEY_SIMPLEX, 2.5, Scalar(0, 0, 255), 4); // Dígito lido sobre o painel inferior
                            }
                        }
                    }
                }
            }

            // ---- 6) Máquina de estados (mesma do cliente6) ----
            // Os estados são testados em sequência (if, não else if): quando um estado muda para o próximo,
            // o novo estado já roda no mesmo quadro.
            if (estado == 0) {                                          // Estado 0: detecte e siga a placa
                if (placa && best_scale <= escala_leitura) {            // Placa grande (perto): para
                    estado = 1;                                         // Vai para o estado 1
                    t_estado = timeSinceEpoch();                        // Começa a contar o tempo de parada
                } else if (placa) {                                     // Placa pequena: anda em direção a ela
                    int diff = deslocamento * k_proporcional;           // Correção de curva, proporcional ao deslocamento
                    vel_esq = vel_base_esq + diff;                      // Placa à direita (diff > 0): roda esquerda acelera
                    vel_dir = vel_base_dir - diff;                      // e a direita desacelera (ou o contrário)
                }                                                       // Sem placa: fica parado
            }

            if (estado == 1) {                                          // Estado 1: pare o carrinho
                if (timeSinceEpoch() - t_estado >= t_parar) estado = 2; // Já parou: vai ler o dígito
            }

            if (estado == 2) {                                          // Estado 2: leia o dígito
                if (!placa || digito < 0) {                             // Placa não visível ou dígito não lido
                    estado = 0;                                         // Volta a procurar a placa
                } else if (digito == 0 || digito == 1) {                // Dígito 0 ou 1: fim da trajetória
                    digito_lido = digito;                               // Guarda o dígito (para mostrar na tela)
                    estado = 4;                                         // Vai para o fim da trajetória
                } else {                                                // Dígito de 2 a 9: um movimento
                    digito_lido = digito;                               // Guarda o dígito lido
                    estado = 3;                                         // Vai executar o movimento
                    t_estado = timeSinceEpoch();                        // Inicializa o cronômetro do movimento
                }
            }

            if (estado == 3) {                                          // Estado 3: faça o movimento indicado pelo dígito
                double duracao = 0;                                     // Tempo programado do movimento
                int ve = 0, vd = 0;                                     // Potências do movimento
                if (digito_lido == 2)      { duracao = t_180;    ve = -vel_giro;     vd = vel_giro; }     // 2: 180 graus à esquerda
                else if (digito_lido == 3) { duracao = t_180;    ve = vel_giro;      vd = -vel_giro; }    // 3: 180 graus à direita
                else if (digito_lido <= 5) { duracao = t_frente; ve = vel_base_esq;  vd = vel_base_dir; } // 4 ou 5: reto, passa por baixo da placa
                else if (digito_lido <= 7) { duracao = t_90;     ve = -vel_giro;     vd = vel_giro; }     // 6 ou 7: 90 graus à esquerda
                else                       { duracao = t_90;     ve = vel_giro;      vd = -vel_giro; }    // 8 ou 9: 90 graus à direita

                if (timeSinceEpoch() - t_estado < duracao) {            // Ainda dentro do tempo programado
                    vel_esq = ve;                                       // Repete o movimento
                    vel_dir = vd;
                } else {                                                // Acabou o tempo
                    estado = 5;                                         // Vai parar o carrinho
                    t_estado = timeSinceEpoch();                        // Começa a contar o tempo de parada
                }
            }

            if (estado == 5) {                                          // Estado 5: termine o movimento, pare o carrinho
                if (timeSinceEpoch() - t_estado >= t_parar) estado = 0; // Já parou: procura a próxima placa
            }

            if (estado == 4) {                                          // Estado 4: fim da trajetória automática
                automatico = false;                                     // Volta para o modo manual (motores ficam em 0 neste quadro)
                estado = 0;                                             // Prepara o próximo automático
            }
        }

        vel_esq = max(-100, min(100, vel_esq));                     // Trava de ±100
        vel_dir = max(-100, min(100, vel_dir));                     // Trava de ±100
        client.sendBytes(sizeof(int), (BYTE*)&vel_esq);             // Envia a potência da roda esquerda (4 bytes)
        client.sendBytes(sizeof(int), (BYTE*)&vel_dir);             // Envia a potência da roda direita (4 bytes)

        // ---- Exibição e gravação ----
        a.copyTo(tela(r_camera));                                   // Copia a câmera (com desenhos no automático) para o meio da tela
        desenha_teclado(tela, automatico ? 0 : tecla_atual, automatico);  // Desenha o teclado; no automático nenhuma flecha fica acesa
        if (automatico) {                                           // No automático, mostra o estado como nas figuras da apostila
            char str_estado[64];                                    // Texto de estado para depuração
            sprintf(str_estado, "estado=%d dirx=%d digito=%d", estado, deslocamento, digito_lido); // Estado, deslocamento e dígito guardado
            putText(tela, str_estado, Point(3, 10), FONT_HERSHEY_SIMPLEX, 0.35, Scalar(0, 0, 255), 1); // Em vermelho no canto superior esquerdo
        }

        Mat_<COR> tela_ampliada;                                    // Tela ampliada para exibição
        resize(tela, tela_ampliada, Size(), fator, fator, INTER_NEAREST); // Amplia mantendo a proporção (o mouse depende disso)
        imshow("Carrinho", tela_ampliada);                          // Mostra na janela

        if (espectadores > 0) {                                     // Só comprime se algum navegador estiver assistindo
            vector<BYTE> vb;                                        // JPEG da tela completa
            vector<int> param{IMWRITE_JPEG_QUALITY, qualidade_web}; // Qualidade do JPEG
            imencode(".jpg", tela_ampliada, vb, param);             // Comprime a mesma tela mostrada no computador
            {
                lock_guard<mutex> lk(mtx_jpeg);                     // Trava o JPEG compartilhado
                jpeg_atual.swap(vb);                                // Publica o quadro novo
                num_quadro++;                                       // Conta mais um quadro publicado
            }
            cv_jpeg.notify_all();                                   // Acorda as threads de vídeo
        }
        
        if (gravar_video) {                                         // Se a gravação está ativa
            if (gravar_tela) vo << tela_ampliada;                   // modo 't': grava a tela com teclado e desenhos
            else             vo << imagem_limpa;                    // modo 'c': grava a câmera sem desenhos
        }
        
        if ((waitKey(1) & 0xff) == 27) {          // Processa os eventos do mouse e verifica ESC
            sinal_ok = 0;                         // Sinal de controle: 0 = encerrar
            client.sendBytes(1, &sinal_ok);       // Avisa o Raspberry, que sai do laço e desliga os motores
            break;                                // Sai do laço principal
        }
    }
    return 0;                          // Fim do programa
}
