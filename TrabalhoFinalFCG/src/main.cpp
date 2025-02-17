//     Universidade Federal do Rio Grande do Sul
//             Instituto de Informática
//       Departamento de Informática Aplicada
//
//    INF01047 Fundamentos de Computação Gráfica
//               Prof. Eduardo Gastal
//
//       TRABALHO FINAL BASEADO NO LABORATÓRIO 5
//
//          GABRIEL BERTA E GLEYDSON CAMPOS

// Arquivos "headers" padrões de C podem ser incluídos em um
// programa C++, sendo necessário somente adicionar o caractere
// "c" antes de seu nome, e remover o sufixo ".h". Exemplo:
//    #include <stdio.h> // Em C
//  vira
//    #include <cstdio> // Em C++
//
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>

// Headers abaixo são específicos de C++
#include <map>
#include <stack>
#include <string>
#include <vector>
#include <limits>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>

// Headers das bibliotecas OpenGL
#include <glad/glad.h>   // Criação de contexto OpenGL 3.3
#include <GLFW/glfw3.h>  // Criação de janelas do sistema operacional

// Headers da biblioteca GLM: criação de matrizes e vetores.
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <glm/gtc/type_ptr.hpp>

// Headers da biblioteca para carregar modelos obj
#include <tiny_obj_loader.h>

#include <stb_image.h>

// Headers locais, definidos na pasta "include/"
#include "utils.h"
#include "matrices.h"
#include "collisions.hpp"

// Constantes
#define VelocidadeBase 12.0f
#define VelocidadeBike 20.0f
#define SensibilidadeCamera 0.005f
#define ControleVelocidadeCurva 0.5f
#define CameraLivre true
#define CameraLook false
#define M_PI   3.14159265358979323846

// Tempo de Inatividade para trocar de camera
#define INACTIVITY_THRESHOLD  10.0f

// Estrutura que representa um modelo geométrico carregado a partir de um
// arquivo ".obj". Veja https://en.wikipedia.org/wiki/Wavefront_.obj_file .
struct ObjModel
{
    tinyobj::attrib_t                 attrib;
    std::vector<tinyobj::shape_t>     shapes;
    std::vector<tinyobj::material_t>  materials;

    // Este construtor lê o modelo de um arquivo utilizando a biblioteca tinyobjloader.
    // Veja: https://github.com/syoyo/tinyobjloader
    ObjModel(const char* filename, const char* basepath = NULL, bool triangulate = true)
    {
        printf("Carregando objetos do arquivo \"%s\"...\n", filename);

        // Se basepath == NULL, então setamos basepath como o dirname do
        // filename, para que os arquivos MTL sejam corretamente carregados caso
        // estejam no mesmo diretório dos arquivos OBJ.
        std::string fullpath(filename);
        std::string dirname;
        if (basepath == NULL)
        {
            auto i = fullpath.find_last_of("/");
            if (i != std::string::npos)
            {
                dirname = fullpath.substr(0, i+1);
                basepath = dirname.c_str();
            }
        }

        std::string warn;
        std::string err;
        bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename, basepath, triangulate);

        if (!err.empty())
            fprintf(stderr, "\n%s\n", err.c_str());

        if (!ret)
            throw std::runtime_error("Erro ao carregar modelo.");

        for (size_t shape = 0; shape < shapes.size(); ++shape)
        {
            if (shapes[shape].name.empty())
            {
                fprintf(stderr,
                        "*********************************************\n"
                        "Erro: Objeto sem nome dentro do arquivo '%s'.\n"
                        "Veja https://www.inf.ufrgs.br/~eslgastal/fcg-faq-etc.html#Modelos-3D-no-formato-OBJ .\n"
                        "*********************************************\n",
                    filename);
                throw std::runtime_error("Objeto sem nome.");
            }
            printf("- Objeto '%s'\n", shapes[shape].name.c_str());
        }

        printf("OK.\n");
    }
};


// Declaração de funções utilizadas para pilha de matrizes de modelagem.
void PushMatrix(glm::mat4 M);
void PopMatrix(glm::mat4& M);

// Declaração de várias funções utilizadas em main().  Essas estão definidas
// logo após a definição de main() neste arquivo.
void BuildTrianglesAndAddToVirtualScene(ObjModel*); // Constrói representação de um ObjModel como malha de triângulos para renderização
void ComputeNormals(ObjModel* model); // Computa normais de um ObjModel, caso não existam.
void LoadShadersFromFiles(); // Carrega os shaders de vértice e fragmento, criando um programa de GPU
void LoadTextureImage(const char* filename); // Função que carrega imagens de textura
void DrawVirtualObject(const char* object_name); // Desenha um objeto armazenado em g_VirtualScene
GLuint LoadShader_Vertex(const char* filename);   // Carrega um vertex shader
GLuint LoadShader_Fragment(const char* filename); // Carrega um fragment shader
void LoadShader(const char* filename, GLuint shader_id); // Função utilizada pelas duas acima
GLuint CreateGpuProgram(GLuint vertex_shader_id, GLuint fragment_shader_id); // Cria um programa de GPU
void PrintObjModelInfo(ObjModel*); // Função para debugging

// Declaração de funções auxiliares para renderizar texto dentro da janela
// OpenGL. Estas funções estão definidas no arquivo "textrendering.cpp".
void TextRendering_Init();
float TextRendering_LineHeight(GLFWwindow* window);
float TextRendering_CharWidth(GLFWwindow* window);
void TextRendering_PrintString(GLFWwindow* window, const std::string &str, float x, float y, float scale = 1.0f);
void TextRendering_PrintMatrix(GLFWwindow* window, glm::mat4 M, float x, float y, float scale = 1.0f);
void TextRendering_PrintVector(GLFWwindow* window, glm::vec4 v, float x, float y, float scale = 1.0f);
void TextRendering_PrintMatrixVectorProduct(GLFWwindow* window, glm::mat4 M, glm::vec4 v, float x, float y, float scale = 1.0f);
void TextRendering_PrintMatrixVectorProductMoreDigits(GLFWwindow* window, glm::mat4 M, glm::vec4 v, float x, float y, float scale = 1.0f);
void TextRendering_PrintMatrixVectorProductDivW(GLFWwindow* window, glm::mat4 M, glm::vec4 v, float x, float y, float scale = 1.0f);

// Funções abaixo renderizam como texto na janela OpenGL algumas matrizes e
// outras informações do programa. Definidas após main().
void TextRendering_ShowModelViewProjection(GLFWwindow* window, glm::mat4 projection, glm::mat4 view, glm::mat4 model, glm::vec4 p_model);
void TextRendering_ShowEulerAngles(GLFWwindow* window);
void TextRendering_ShowProjection(GLFWwindow* window);
void TextRendering_ShowFramesPerSecond(GLFWwindow* window);

// Funções callback para comunicação com o sistema operacional e interação do
// usuário. Veja mais comentários nas definições das mesmas, abaixo.
void FramebufferSizeCallback(GLFWwindow* window, int width, int height);
void ErrorCallback(int error, const char* description);
void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mode);
void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
void CursorPosCallback(GLFWwindow* window, double xpos, double ypos);
void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);

/// funcoes adicionadas
void SortearItens(int quantidade);
void DrawShoppingList(GLFWwindow* window);
void MarcarItemComoPego(int item_id);
void DrawCashierDialog(GLFWwindow* window);

// Definimos uma estrutura que armazenará dados necessários para renderizar
// cada objeto da cena virtual.
struct SceneObject
{
    std::string  name;        // Nome do objeto
    size_t       first_index; // Índice do primeiro vértice dentro do vetor indices[] definido em BuildTrianglesAndAddToVirtualScene()
    size_t       num_indices; // Número de índices do objeto dentro do vetor indices[] definido em BuildTrianglesAndAddToVirtualScene()
    GLenum       rendering_mode; // Modo de rasterização (GL_TRIANGLES, GL_TRIANGLE_STRIP, etc.)
    GLuint       vertex_array_object_id; // ID do VAO onde estão armazenados os atributos do modelo
    glm::vec3    bbox_min; // Axis-Aligned Bounding Box do objeto
    glm::vec3    bbox_max;
};

// Abaixo definimos variáveis globais utilizadas em várias funções do código.

// A cena virtual é uma lista de objetos nomeados, guardados em um dicionário
// (map).  Veja dentro da função BuildTrianglesAndAddToVirtualScene() como que são incluídos
// objetos dentro da variável g_VirtualScene, e veja na função main() como
// estes são acessados.
std::map<std::string, SceneObject> g_VirtualScene;

// Pilha que guardará as matrizes de modelagem.
std::stack<glm::mat4>  g_MatrixStack;

// Razão de proporção da janela (largura/altura). Veja função FramebufferSizeCallback().
float g_ScreenRatio = 1.0f;

// Ângulos de Euler que controlam a rotação de um dos cubos da cena virtual
float g_AngleX = 0.0f;
float g_AngleY = 0.0f;
float g_AngleZ = 0.0f;

// "g_LeftMouseButtonPressed = true" se o usuário está com o botão esquerdo do mouse
// pressionado no momento atual. Veja função MouseButtonCallback().
bool g_LeftMouseButtonPressed = false;
bool g_RightMouseButtonPressed = false; // Análogo para botão direito do mouse
bool g_MiddleMouseButtonPressed = false; // Análogo para botão do meio do mouse

// Variáveis que definem a câmera em coordenadas esféricas, controladas pelo
// usuário através do mouse (veja função CursorPosCallback()). A posição
// efetiva da câmera é calculada dentro da função main(), dentro do loop de
// renderização.
float g_CameraTheta = 0.0f; // Ângulo no plano ZX em relação ao eixo Z
float g_CameraPhi = 0.0f;   // Ângulo em relação ao eixo Y
float g_CameraDistance = 10.0f; // Distância da câmera para a origem

// Variáveis que controlam rotação do antebraço
float g_ForearmAngleZ = 0.0f;
float g_ForearmAngleX = 0.0f;

// Variáveis que controlam translação do torso
float g_TorsoPositionX = 0.0f;
float g_TorsoPositionY = 0.0f;

// Variável que controla o tipo de projeção utilizada: perspectiva ou ortográfica.
bool g_UsePerspectiveProjection = true;

// Variável que controla se o texto informativo será mostrado na tela.
bool g_ShowInfoText = true;

// Variáveis que definem um programa de GPU (shaders). Veja função LoadShadersFromFiles().
GLuint g_GpuProgramID = 0;
GLint g_model_uniform;
GLint g_view_uniform;
GLint g_projection_uniform;
GLint g_object_id_uniform;
GLint g_bbox_min_uniform;
GLint g_bbox_max_uniform;

// Número de texturas carregadas pela função LoadTextureImage()
GLuint g_NumLoadedTextures = 0;


// Variáveis globais que armazenam a última posição do cursor do mouse, para
// que possamos calcular quanto que o mouse se movimentou entre dois instantes
// de tempo. Utilizadas no callback CursorPosCallback() abaixo.
double g_LastCursorPosX, g_LastCursorPosY;

/// variaveis globais

glm::vec4 g_camera_position_c  = glm::vec4(0.0f,1.0f,3.5f,1.0f); // Posição inicial da câmera
float prev_time = (float)glfwGetTime();
float delta_t;

float tempo_total = 120.0f;  // 2 minutos
float tempo_restante = tempo_total;
bool game_over = false;

bool tecla_W_pressionada = false;
bool tecla_S_pressionada = false;
bool tecla_D_pressionada = false;
bool tecla_A_pressionada = false;
bool tecla_E_pressionada = false;
bool tecla_B_pressionada = false;
bool g_cameraType = CameraLivre;
bool trocaCamera = false;

//curva de bezier

// Pontos de controle da curva de Bézier
glm::vec4 p0 = glm::vec4(-8.0f, 10.0f, -5.0f, 1.0f);  // Ponto inicial
glm::vec4 p1 = glm::vec4(-5.0f, 12.0f, -8.0f, 1.0f);  // 2o
glm::vec4 p2 = glm::vec4(-2.0f, 8.0f, -10.0f, 1.0f);  // 3o
glm::vec4 p3 = glm::vec4(1.0f, 12.0f, -5.0f, 1.0f);   // 4o

glm::vec4 PontoBezier(float t, glm::vec4 p0, glm::vec4 p1, glm::vec4 p2, glm::vec4 p3)
{
    float u = 1.0f - t;
    float tt = t * t;
    float uu = u * u;
    float uuu = uu * u;
    float ttt = tt * t;

    glm::vec4 point = uuu * p0;
    point += 3 * uu * t * p1;
    point += 3 * u * tt * p2;
    point += ttt * p3;

    return point;
}

glm::vec4 AtualizaPonto(float time, glm::vec4 p0, glm::vec4 p1, glm::vec4 p2, glm::vec4 p3)
{
    float t = 0.5f * (1.0f - cos(2.0f * 3.141592f * fmod(time, 1.0f))); // Senoide suave entre 0 e 1
    return PontoBezier(t, p0, p1, p2, p3);
}

// fim da curva de bezier

/// tudo sobre colisoes

//esfera para esfera
const float PLAYER_RADIUS = 1.5f;
const float BUNNY_RADIUS = 1.5f;

//box to box
BoundingBox g_PlayerBox;
BoundingBox g_CashierBox;
BoundingBox g_HouseBox;

// Estruturas para colisões
extern struct BoundingBox g_PlayerBox;
extern struct BoundingBox g_CashierBox;
extern struct BoundingBox g_HouseBox;

glm::vec4 g_CashierPosition = glm::vec4(-5.0f, -1.0f, -20.0f, 1.0f);
glm::vec4 g_BunnyPosition = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);

/// variáveis para controlar se os itens foram pegos
bool bunny_picked = false;
bool baguete_picked = false;
bool egg_picked = false;
bool butter_picked = false;
bool cheese_picked = false;

float g_PlayerMoney = 0.0f;                 // Dinheiro atual do jogador
float g_TotalPurchaseValue = 0.0f;          // Valor total das compras
bool g_HasPaidPurchases = false;            // Flag para indicar se as compras foram pagas
std::map<std::string, float> g_ItemPrices;  // Mapa para armazenar preços dos itens
bool g_InteractingWithCashier = false;      // Flag para controlar a interação com o caixa
std::string g_InputTroco = "";              // String para armazenar o input do usuário
bool g_WaitingForInput = false;             // Flag para controlar se estamos esperando input
float g_CorrectChange = 0.0f;               // Valor correto do troco
bool g_PaymentCompleted = false;            // Flag para indicar se o pagamento foi concluído corretamente
glm::vec4 g_HomePosition = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);  // Posição da casa (ajuste conforme necessário)
bool g_GameWon = false;  // Flag para indicar se o jogador ganhou o jogo

float g_CameraAlturaFixa = 2.0f;

std::map<std::string, glm::mat4> g_object_matrices;
int g_object_highlighted = -1;

std::vector<std::string> todos_itens = {
    "baguete",
    "queijo",
    "ovo",
    "manteiga"
};

std::vector<std::string> itens_para_comprar;

std::vector<bool> itens_pegos;

        // Para facilitar a identificação dos objetos, criamos um enum com os IDs dos itens.

        enum ItemID {
            ITEM_BAGUETE     = 101,
            ITEM_QUEIJO      = 102,
            ITEM_PRESUNTO    = 103,
            ITEM_OVO         = 104,
            ITEM_MANTEIGA    = 105,
        };

        // Mapeamento de IDs para nomes de itens
        std::map<int, std::string> id_para_nome = {
            {ITEM_BAGUETE, "Baguete"},
            {ITEM_QUEIJO, "Queijo"},
            {ITEM_PRESUNTO, "Presunto"},
            {ITEM_OVO, "Ovo"},
            {ITEM_MANTEIGA, "Manteiga"},
        };

void InitializeMoneyAndPrices()
{
    // Gera um valor aleatório entre 50.00 e 150.00
    g_PlayerMoney = (40.0f * 100 + (rand() % (20 * 100))) / 100.0f;
    if (g_PlayerMoney > 150.0f) g_PlayerMoney = 150.0f;

    // Define preços para os itens (valores com 2 casas decimais)
    g_ItemPrices["baguete"] = 15.24f;
    g_ItemPrices["queijo"] = 17.99f;
    g_ItemPrices["presunto"] = 1.75f;
    g_ItemPrices["ovo"] = 14.32f;
    g_ItemPrices["manteiga"] = 13.99f;
}

/// função para desenhar dialogo do pagamento

void DrawCashierDialog(GLFWwindow* window)
{
    if (!g_InteractingWithCashier) return;

    // Posição central na tela
    float x = -0.4f;
    float y = 0.2f;
    float line_height = TextRendering_LineHeight(window);

    // Desenha o diálogo
    TextRendering_PrintString(window, "Caixa: Qual o seu troco?.", x, y, 1.0f);
    TextRendering_PrintString(window, "apenas respostas válidas", x, y - line_height, 1.0f);

    char total_text[64];
    snprintf(total_text, sizeof(total_text), "Total da compra: R$ %.2f", g_TotalPurchaseValue);
    TextRendering_PrintString(window, total_text, x, y - 2*line_height, 1.0f);

    char saldo_text[64];
    snprintf(saldo_text, sizeof(saldo_text), "Voce entregou: R$ %.2f", g_PlayerMoney);
    TextRendering_PrintString(window, saldo_text, x, y - 3*line_height, 1.0f);

    // Área para input do jogador com cursor piscante
    char input_text[64];
    if (g_InputTroco.empty()) {
        snprintf(input_text, sizeof(input_text), "Troco correto: R$ _____");
    } else {
        snprintf(input_text, sizeof(input_text), "Troco correto: R$ %s", g_InputTroco.c_str());
    }
    TextRendering_PrintString(window, input_text, x, y - 4*line_height, 1.0f);

}

/// Destacar objeto

bool RayOBBIntersection(
    glm::vec4 ray_origin,    // Ponto de origem do raio
    glm::vec4 ray_direction, // Direção do raio (normalizado)
    glm::vec4 box_center,    // Centro da box do objeto
    glm::vec4 box_extent,    // Metade das dimensões da box
    glm::mat4 box_rotation   // Matriz de rotação da box
);

int GetObjectUnderCrosshair(
    glm::vec4 camera_position,
    glm::vec4 camera_view,
    std::map<std::string, SceneObject>& virtual_scene,
    std::map<std::string, glm::mat4>& object_matrices
);

/// estrutura de dados usada para instanciar calçadas

struct Calcada {
    glm::vec3 position;
    float rotation;
    glm::mat4 model;
};

std::vector<Calcada> calcadas;

void GerarCalcadas() {
    Calcada calcada;
    float largura_calcada = 1.5f;
    float altura_calcada = -0.75f;
    float profundidade_calcada = 0.5f;
    float posicao_lateral = 6.53f;
    float espacamento = 9.05f;

    /// Array com as 17 posições Z para os segmentos verticais
    float posicoes_z[17];
    posicoes_z[0] = -0.5f;
    for(int i = 1; i < 17; i++) {
        posicoes_z[i] = posicoes_z[0] - (espacamento * i);
    }

    // Lateral direita - os segmentos C verticais
    for(int i = 0; i < 17; i++) {
        calcada.position = glm::vec3(posicao_lateral, altura_calcada, posicoes_z[i]);
        calcada.rotation = M_PI/2;
        calcada.model = Matrix_Translate(calcada.position.x, calcada.position.y, calcada.position.z)
                     * Matrix_Rotate_Y(calcada.rotation)
                     * Matrix_Scale(largura_calcada, 1.0f, profundidade_calcada);
        calcadas.push_back(calcada);
    }

    // Lateral esquerda - os segmentos C verticais
    for(int i = 0; i < 17; i++) {
        calcada.position = glm::vec3(-posicao_lateral, altura_calcada, posicoes_z[i]);
        calcada.rotation = -M_PI/2;
        calcada.model = Matrix_Translate(calcada.position.x, calcada.position.y, calcada.position.z)
                     * Matrix_Rotate_Y(calcada.rotation)
                     * Matrix_Scale(largura_calcada, 1.0f, profundidade_calcada);
        calcadas.push_back(calcada);
    }

    // Segmentos A - horizontais nas pontas superiores
    // Segmentos A da direita (agora com 3 segmentos)
    for(int i = 0; i < 3; i++)
    {
        calcada.position = glm::vec3(posicao_lateral + 5.05 + largura_calcada + (i * (largura_calcada + 7.55)), altura_calcada, 2.0f);
        calcada.rotation = 0.0f;
        calcada.model = Matrix_Translate(calcada.position.x, calcada.position.y, calcada.position.z)
        * Matrix_Rotate_Y(calcada.rotation)
        * Matrix_Scale(largura_calcada, 1.0f, profundidade_calcada);
        calcadas.push_back(calcada);
    }

    // Segmentos A da esquerda (agora com 3 segmentos)
    for(int i = 0; i < 3; i++)
    {
        calcada.position = glm::vec3(-posicao_lateral - 5.05 - largura_calcada - (i * (largura_calcada + 7.55)), altura_calcada, 2.0f);
        calcada.rotation = 0.0f;
        calcada.model = Matrix_Translate(calcada.position.x, calcada.position.y, calcada.position.z)
        * Matrix_Rotate_Y(calcada.rotation)
        * Matrix_Scale(largura_calcada, 1.0f, profundidade_calcada);
        calcadas.push_back(calcada);
    }

    // Segmento A vertical - ponta direita (primeiro segmento)
    float x_pos_direita = posicao_lateral + 7.55 + largura_calcada + (2 * (largura_calcada + 7.55));
    calcada.position = glm::vec3(x_pos_direita, altura_calcada, 8.55f);
    calcada.rotation = M_PI/2;
    calcada.model = Matrix_Translate(calcada.position.x, calcada.position.y, calcada.position.z)
    * Matrix_Rotate_Y(calcada.rotation)
    * Matrix_Scale(largura_calcada, 1.0f, profundidade_calcada);
    calcadas.push_back(calcada);

    // Segmento A vertical - ponta direita (segundo segmento)
    calcada.position = glm::vec3(x_pos_direita, altura_calcada, 17.6f);
    calcada.rotation = M_PI/2;
    calcada.model = Matrix_Translate(calcada.position.x, calcada.position.y, calcada.position.z)
    * Matrix_Rotate_Y(calcada.rotation)
    * Matrix_Scale(largura_calcada, 1.0f, profundidade_calcada);
    calcadas.push_back(calcada);

    // Segmento A vertical - ponta direita (terceiro segmento)
    calcada.position = glm::vec3(x_pos_direita, altura_calcada, 26.65f); // Mantive o mesmo espaçamento (~9.05)
    calcada.rotation = M_PI/2;
    calcada.model = Matrix_Translate(calcada.position.x, calcada.position.y, calcada.position.z)
    * Matrix_Rotate_Y(calcada.rotation)
    * Matrix_Scale(largura_calcada, 1.0f, profundidade_calcada);
    calcadas.push_back(calcada);

    // Segmento A vertical - ponta esquerda (primeiro segmento)
    float x_pos_esquerda = -posicao_lateral - 7.55 - largura_calcada - (2 * (largura_calcada + 7.55));
    calcada.position = glm::vec3(x_pos_esquerda, altura_calcada, 8.55f);
    calcada.rotation = -M_PI/2;
    calcada.model = Matrix_Translate(calcada.position.x, calcada.position.y, calcada.position.z)
    * Matrix_Rotate_Y(calcada.rotation)
    * Matrix_Scale(largura_calcada, 1.0f, profundidade_calcada);
    calcadas.push_back(calcada);

    // Segmento A vertical - ponta esquerda (segundo segmento)
    calcada.position = glm::vec3(x_pos_esquerda, altura_calcada, 17.6f);
    calcada.rotation = -M_PI/2;
    calcada.model = Matrix_Translate(calcada.position.x, calcada.position.y, calcada.position.z)
    * Matrix_Rotate_Y(calcada.rotation)
    * Matrix_Scale(largura_calcada, 1.0f, profundidade_calcada);
    calcadas.push_back(calcada);

    // Segmento A vertical - ponta esquerda (terceiro segmento)
    calcada.position = glm::vec3(x_pos_esquerda, altura_calcada, 26.65f); // Mantive espaçamento similar
    calcada.rotation = -M_PI/2;
    calcada.model = Matrix_Translate(calcada.position.x, calcada.position.y, calcada.position.z)
    * Matrix_Rotate_Y(calcada.rotation)
    * Matrix_Scale(largura_calcada, 1.0f, profundidade_calcada);
    calcadas.push_back(calcada);

        // Segmento A horizontal - extremidade direita do topo
    x_pos_direita = posicao_lateral + 7.55 + largura_calcada + (2 * (largura_calcada + 4.27));
    calcada.position = glm::vec3(x_pos_direita, altura_calcada, 29.15f); // Usando a altura do último segmento vertical
    calcada.rotation = 0.0f; // Horizontal
    calcada.model = Matrix_Translate(calcada.position.x, calcada.position.y, calcada.position.z)
    * Matrix_Rotate_Y(calcada.rotation)
    * Matrix_Scale(largura_calcada, 1.0f, profundidade_calcada);
    calcadas.push_back(calcada);

    x_pos_direita = posicao_lateral + 7.55 + largura_calcada + (2 * (largura_calcada - 0.25));
    calcada.position = glm::vec3(x_pos_direita, altura_calcada, 29.15f); // Usando a altura do último segmento vertical
    calcada.rotation = 0.0f; // Horizontal
    calcada.model = Matrix_Translate(calcada.position.x, calcada.position.y, calcada.position.z)
    * Matrix_Rotate_Y(calcada.rotation)
    * Matrix_Scale(largura_calcada, 1.0f, profundidade_calcada);
    calcadas.push_back(calcada);

    x_pos_direita = posicao_lateral + 7.55 + largura_calcada + (2 * (largura_calcada - 4.77));
    calcada.position = glm::vec3(x_pos_direita, altura_calcada, 29.15f); // Usando a altura do último segmento vertical
    calcada.rotation = 0.0f; // Horizontal
    calcada.model = Matrix_Translate(calcada.position.x, calcada.position.y, calcada.position.z)
    * Matrix_Rotate_Y(calcada.rotation)
    * Matrix_Scale(largura_calcada, 1.0f, profundidade_calcada);
    calcadas.push_back(calcada);


    x_pos_direita = posicao_lateral + 7.55 + largura_calcada + (2 * (largura_calcada - 9.29));
    calcada.position = glm::vec3(x_pos_direita, altura_calcada, 29.15f); // Usando a altura do último segmento vertical
    calcada.rotation = 0.0f; // Horizontal
    calcada.model = Matrix_Translate(calcada.position.x, calcada.position.y, calcada.position.z)
    * Matrix_Rotate_Y(calcada.rotation)
    * Matrix_Scale(largura_calcada, 1.0f, profundidade_calcada);
    calcadas.push_back(calcada);

    // Segmento A horizontal - extremidade esquerda do topo
    x_pos_esquerda = -posicao_lateral - 7.55 - largura_calcada - (2 * (largura_calcada + 4.27));
    calcada.position = glm::vec3(x_pos_esquerda, altura_calcada, 29.15f); // Mesma altura do direito
    calcada.rotation = 0.0f; // Horizontal
    calcada.model = Matrix_Translate(calcada.position.x, calcada.position.y, calcada.position.z)
    * Matrix_Rotate_Y(calcada.rotation)
    * Matrix_Scale(largura_calcada, 1.0f, profundidade_calcada);
    calcadas.push_back(calcada);

    x_pos_esquerda = -posicao_lateral - 7.55 - largura_calcada - (2 * (largura_calcada - 0.25));
    calcada.position = glm::vec3(x_pos_esquerda, altura_calcada, 29.15f); // Mesma altura do direito
    calcada.rotation = 0.0f; // Horizontal
    calcada.model = Matrix_Translate(calcada.position.x, calcada.position.y, calcada.position.z)
    * Matrix_Rotate_Y(calcada.rotation)
    * Matrix_Scale(largura_calcada, 1.0f, profundidade_calcada);
    calcadas.push_back(calcada);

    x_pos_esquerda = -posicao_lateral - 7.55 - largura_calcada - (2 * (largura_calcada - 4.77));
    calcada.position = glm::vec3(x_pos_esquerda, altura_calcada, 29.15f); // Mesma altura do direito
    calcada.rotation = 0.0f; // Horizontal
    calcada.model = Matrix_Translate(calcada.position.x, calcada.position.y, calcada.position.z)
    * Matrix_Rotate_Y(calcada.rotation)
    * Matrix_Scale(largura_calcada, 1.0f, profundidade_calcada);
    calcadas.push_back(calcada);

}

int main(int argc, char* argv[])
{
    // Inicializamos a biblioteca GLFW, utilizada para criar uma janela do
    // sistema operacional, onde poderemos renderizar com OpenGL.
    int success = glfwInit();

    // Inicializa o gerador de números aleatórios
    srand(time(NULL));
    // Sorteia 2 itens para comprar
    SortearItens(2);
    // Inicializa o sistema monetário do jogo
    InitializeMoneyAndPrices();

    if (!success)
    {
        fprintf(stderr, "ERROR: glfwInit() failed.\n");
        std::exit(EXIT_FAILURE);
    }

    // Definimos o callback para impressão de erros da GLFW no terminal
    glfwSetErrorCallback(ErrorCallback);

    // Pedimos para utilizar OpenGL versão 3.3 (ou superior)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

    #ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    #endif

    // Pedimos para utilizar o perfil "core", isto é, utilizaremos somente as
    // funções modernas de OpenGL.
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Criamos uma janela do sistema operacional, com 800 colunas e 600 linhas
    // de pixels, e com título "INF01047 ...".
    GLFWwindow* window;
    window = glfwCreateWindow(800, 600, "Neighborhood", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        fprintf(stderr, "ERROR: glfwCreateWindow() failed.\n");
        std::exit(EXIT_FAILURE);
    }


    /// Desabilita o cursor e o centraliza
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    /// Obtém as dimensões da janela
    int width, height;
    glfwGetWindowSize(window, &width, &height);

    /// Inicializa a última posição do cursor no centro da tela para controlar o primeiro movimento de camera
    g_LastCursorPosX = width/2.0;
    g_LastCursorPosY = height/2.0;

    /// Define a posição do cursor no centro
    glfwSetCursorPos(window, g_LastCursorPosX, g_LastCursorPosY);

    // Definimos a função de callback que será chamada sempre que o usuário
    // pressionar alguma tecla do teclado ...
    glfwSetKeyCallback(window, KeyCallback);
    // ... ou clicar os botões do mouse ...
    glfwSetMouseButtonCallback(window, MouseButtonCallback);
    // ... ou movimentar o cursor do mouse em cima da janela ...
    glfwSetCursorPosCallback(window, CursorPosCallback);
    // ... ou rolar a "rodinha" do mouse.
    glfwSetScrollCallback(window, ScrollCallback);

    // Indicamos que as chamadas OpenGL deverão renderizar nesta janela
    glfwMakeContextCurrent(window);

    GerarCalcadas();

    // Carregamento de todas funções definidas por OpenGL 3.3, utilizando a
    // biblioteca GLAD.
    gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);

    // Definimos a função de callback que será chamada sempre que a janela for
    // redimensionada, por consequência alterando o tamanho do "framebuffer"
    // (região de memória onde são armazenados os pixels da imagem).
    glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);
    FramebufferSizeCallback(window, 800, 600); // Forçamos a chamada do callback acima, para definir g_ScreenRatio.

    // Imprimimos no terminal informações sobre a GPU do sistema
    const GLubyte *vendor      = glGetString(GL_VENDOR);
    const GLubyte *renderer    = glGetString(GL_RENDERER);
    const GLubyte *glversion   = glGetString(GL_VERSION);
    const GLubyte *glslversion = glGetString(GL_SHADING_LANGUAGE_VERSION);

    printf("GPU: %s, %s, OpenGL %s, GLSL %s\n", vendor, renderer, glversion, glslversion);

    // Carregamos os shaders de vértices e de fragmentos que serão utilizados
    // para renderização. Veja slides 180-200 do documento Aula_03_Rendering_Pipeline_Grafico.pdf.
    //
    LoadShadersFromFiles();

    // Carregamos duas imagens para serem utilizadas como textura
    LoadTextureImage("../../data/tc-earth_daymap_surface.jpg");      // TextureImage0
    LoadTextureImage("../../data/tc-earth_nightmap_citylights.gif"); // TextureImage1

    /// texturas adicionadas

    LoadTextureImage("../../data/baguete_COLOR.png");               // TextureImage2
    LoadTextureImage("../../data/asfalto.png");                     // TextureImage3
    LoadTextureImage("../../data/poleTexture.png");                 // TextureImage4
    LoadTextureImage("../../data/TexturaLua.jpg");                  // TextureImage5
    LoadTextureImage("../../data/texturaCalcada.png");              // TextureImage6
    LoadTextureImage("../../data/smallHouseTexture.jpg");           // TextureImage7
    LoadTextureImage("../../data/grassTexture.png");                // TextureImage8
    LoadTextureImage("../../data/gasStationTexture.jpg");           // TextureImage9
    LoadTextureImage("../../data/myhouseTexture.png");              // TextureImage10
    LoadTextureImage("../../data/longHouseTexture.jpg");            // TextureImage11
    LoadTextureImage("../../data/woodHouseTexture.png");            // TextureImage12
    LoadTextureImage("../../data/lastHouseTexture.png");            // TextureImage13
    LoadTextureImage("../../data/lilHouseTexture.png");             // TextureImage14
    LoadTextureImage("../../data/maquinaTextura.png");              // TextureImage15
    LoadTextureImage("../../data/ceuEstrelado.jpg");                // TextureImage16
    LoadTextureImage("../../data/goldTexture.jpg");                 // TextureImage17
    LoadTextureImage("../../data/queijo.jpg");                      // TextureImage18
    LoadTextureImage("../../data/parmaTexture.jpg");                // TextureImage19
    LoadTextureImage("../../data/oldWallTexture.jpg");              // TextureImage20

    // Construímos a representação de objetos geométricos através de malhas de triângulos
    ObjModel spheremodel("../../data/sphere.obj");
    ComputeNormals(&spheremodel);
    BuildTrianglesAndAddToVirtualScene(&spheremodel);

    ObjModel bunnymodel("../../data/bunny.obj");
    ComputeNormals(&bunnymodel);
    BuildTrianglesAndAddToVirtualScene(&bunnymodel);

    ObjModel planemodel("../../data/plane.obj");
    ComputeNormals(&planemodel);
    BuildTrianglesAndAddToVirtualScene(&planemodel);

    /// .obj adicionados

    ObjModel mainbuildmodel("../../data/mainbuild.obj");
    ComputeNormals(&mainbuildmodel);
    BuildTrianglesAndAddToVirtualScene(&mainbuildmodel);

    // Carregando o modelo da calçada
    ObjModel calcadamodel("../../data/calcada.obj");
    ComputeNormals(&calcadamodel);
    BuildTrianglesAndAddToVirtualScene(&calcadamodel);

    // baguete
    ObjModel baguetemodel("../../data/baguete.obj");
    ComputeNormals(&baguetemodel);
    BuildTrianglesAndAddToVirtualScene(&baguetemodel);

    ObjModel eggmodel("../../data/eggs.obj");
    ComputeNormals(&eggmodel);
    BuildTrianglesAndAddToVirtualScene(&eggmodel);

    ObjModel buttermodel("../../data/butter.obj");
    ComputeNormals(&buttermodel);
    BuildTrianglesAndAddToVirtualScene(&buttermodel);

    ObjModel cheesemodel("../../data/cheese.obj");
    ComputeNormals(&cheesemodel);
    BuildTrianglesAndAddToVirtualScene(&cheesemodel);

    ObjModel personagemmodel("../../data/objs/personagem/personagem.obj");
    ComputeNormals(&personagemmodel);
    BuildTrianglesAndAddToVirtualScene(&personagemmodel);

    ObjModel mansionmodel("../../data/mansion.obj");
    ComputeNormals(&mansionmodel);
    BuildTrianglesAndAddToVirtualScene(&mansionmodel);

    ObjModel polemodel("../../data/pole.obj");
    ComputeNormals(&polemodel);
    BuildTrianglesAndAddToVirtualScene(&polemodel);

    ObjModel smallhousemodel("../../data/smallHouse.obj");
    ComputeNormals(&smallhousemodel);
    BuildTrianglesAndAddToVirtualScene(&smallhousemodel);

    ObjModel gasstationmodel("../../data/gasStation.obj");
    ComputeNormals(&gasstationmodel);
    BuildTrianglesAndAddToVirtualScene(&gasstationmodel);

    ObjModel myhousemodel("../../data/myhouse.obj");
    ComputeNormals(&myhousemodel);
    BuildTrianglesAndAddToVirtualScene(&myhousemodel);

    ObjModel longhousemodel("../../data/longHouse.obj");
    ComputeNormals(&longhousemodel);
    BuildTrianglesAndAddToVirtualScene(&longhousemodel);

    ObjModel woodhousemodel("../../data/woodhouse.obj");
    ComputeNormals(&woodhousemodel);
    BuildTrianglesAndAddToVirtualScene(&woodhousemodel);

    ObjModel lasthousemodel("../../data/lasthouse.obj");
    ComputeNormals(&lasthousemodel);
    BuildTrianglesAndAddToVirtualScene(&lasthousemodel);

    ObjModel lilhousemodel("../../data/lilhouse.obj");
    ComputeNormals(&lilhousemodel);
    BuildTrianglesAndAddToVirtualScene(&lilhousemodel);

    ObjModel maquinamodel("../../data/maquina.obj");
    ComputeNormals(&maquinamodel);
    BuildTrianglesAndAddToVirtualScene(&maquinamodel);

    if ( argc > 1 )
    {
        ObjModel model(argv[1]);
        BuildTrianglesAndAddToVirtualScene(&model);
    }

    g_CashierBox.min = glm::vec4(g_CashierPosition.x - 1.0f, g_CashierPosition.y - 1.0f, g_CashierPosition.z - 1.0f, 1.0f);
    g_CashierBox.max = glm::vec4(g_CashierPosition.x + 1.0f, g_CashierPosition.y + 1.0f, g_CashierPosition.z + 1.0f, 1.0f);

    g_HouseBox.min = glm::vec4(-20.0f, -1.3f, 45.0f, 1.0f);
    g_HouseBox.max = glm::vec4(20.0f, 3.0f, 70.0f, 1.0f);


    // Define os planos que limitam o mapa
    Plane boundary_plane_north = {
        glm::vec4(0.0f, 0.0f, 10.0f, 1.0f),   // ponto no plano (mais próximo)
        glm::vec4(0.0f, 0.0f, 1.0f, 0.0f)     // normal apontando para sul
    };

    Plane boundary_plane_south = {
        glm::vec4(0.0f, 0.0f, -10.0f, 1.0f),  // ponto no plano (mais próximo)
        glm::vec4(0.0f, 0.0f, -1.0f, 0.0f)    // normal apontando para norte
    };

    Plane boundary_plane_east = {
        glm::vec4(10.0f, 0.0f, 0.0f, 1.0f),   // ponto no plano (mais próximo)
        glm::vec4(1.0f, 0.0f, 0.0f, 0.0f)     // normal apontando para oeste
    };

    Plane boundary_plane_west = {
        glm::vec4(-10.0f, 0.0f, 0.0f, 1.0f),  // ponto no plano (mais próximo)
        glm::vec4(-1.0f, 0.0f, 0.0f, 0.0f)    // normal apontando para leste
    };

    // Inicializamos o código para renderização de texto.
    TextRendering_Init();

    // Habilitamos o Z-buffer. Veja slides 104-116 do documento Aula_09_Projecoes.pdf.
    glEnable(GL_DEPTH_TEST);

    // Habilitamos o Backface Culling. Veja slides 8-13 do documento Aula_02_Fundamentos_Matematicos.pdf, slides 23-34 do documento Aula_13_Clipping_and_Culling.pdf e slides 112-123 do documento Aula_14_Laboratorio_3_Revisao.pdf.
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    // Ficamos em um loop infinito, renderizando, até que o usuário feche a janela
    while (!glfwWindowShouldClose(window))
    {
        // Aqui executamos as operações de renderização

        // Definimos a cor do "fundo" do framebuffer como branco.  Tal cor é
        // definida como coeficientes RGBA: Red, Green, Blue, Alpha; isto é:
        // Vermelho, Verde, Azul, Alpha (valor de transparência).
        // Conversaremos sobre sistemas de cores nas aulas de Modelos de Iluminação.
        //
        //           R     G     B     A
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);

        // "Pintamos" todos os pixels do framebuffer com a cor definida acima,
        // e também resetamos todos os pixels do Z-buffer (depth buffer).
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Pedimos para a GPU utilizar o programa de GPU criado acima (contendo
        // os shaders de vértice e fragmentos).
        glUseProgram(g_GpuProgramID);

        // Computamos a posição da câmera utilizando coordenadas esféricas.  As
        // variáveis g_CameraDistance, g_CameraPhi, e g_CameraTheta são
        // controladas pelo mouse do usuário. Veja as funções CursorPosCallback()
        // e ScrollCallback().
        float r = g_CameraDistance;
        float y = r*sin(g_CameraPhi);
        float z = r*cos(g_CameraPhi)*cos(g_CameraTheta);
        float x = r*cos(g_CameraPhi)*sin(g_CameraTheta);

        // Abaixo definimos as varáveis que efetivamente definem a câmera virtual.
        // Veja slides 195-227 e 229-234 do documento Aula_08_Sistemas_de_Coordenadas.pdf.
        glm::vec4 camera_position_c  = glm::vec4(x,y,z,1.0f); // Ponto "c", centro da câmera
        glm::vec4 camera_lookat_l    = glm::vec4(0.0f,0.0f,0.0f,1.0f); // Ponto "l", para onde a câmera (look-at) estará sempre olhando
        glm::vec4 camera_view_vector = glm::vec4(-x,-y,-z,0.0f); // Vetor "view" - olhando para frente
        glm::vec4 camera_up_vector = glm::vec4(0.0f,1.0f,0.0f,0.0f); // Vetor "up" fixado para apontar para o "céu" (eito Y global)

         // Matriz view
        glm::mat4 view = Matrix_Camera_View(g_camera_position_c, camera_view_vector, camera_up_vector);

        // Agora computamos a matriz de Projeção.
        glm::mat4 projection;

        // Computamos a posição da câmera utilizando coordenadas esféricas
        float current_time = (float)glfwGetTime();
        delta_t = current_time - prev_time;
        prev_time = current_time;

        // Atualiza o tempo restante
        if (!game_over)
        {
            tempo_restante -= delta_t;
            if (tempo_restante <= 0.0f)
            {
                tempo_restante = 0.0f;
                game_over = true;
            }
        }

        float speed = VelocidadeBase; // Ajuste de velocidade


        if (tecla_B_pressionada)
        {
            speed = VelocidadeBike;
        }


        // Matriz model é a identidade
        glm::mat4 model = Matrix_Identity();


        // para controle de qual camera vai ser usada

        // Variáveis globais para rastrear tempo de inatividade

        static float ultimoMovimento = 0.0f;
        static bool inativo = false;


        // Verifica se houve movimento
        if (tecla_W_pressionada || tecla_S_pressionada || tecla_A_pressionada || tecla_D_pressionada)
        {
            ultimoMovimento = current_time; // Atualiza o último tempo de movimento
            inativo = false;           // Reseta a flag de inatividade
            g_cameraType = CameraLivre;
        }
        else
        {
            // Calcula o tempo de inatividade
            if ((current_time - ultimoMovimento) > INACTIVITY_THRESHOLD)
            {
                inativo = true; // Levanta a flag se passar do limite
            }
        }

        // Verifica se a flag de inatividade foi levantada
        if (inativo)
        {
            g_cameraType = CameraLook;
        }


        if(g_cameraType)
        {

            // Calculamos os vetores da base da câmera
            glm::vec4 w = -camera_view_vector / norm(camera_view_vector);
            glm::vec4 u = crossproduct(camera_up_vector, w);


            // Movimentação WASD
            if (tecla_W_pressionada)
                g_camera_position_c -= w * speed * delta_t;
            if (tecla_S_pressionada)
                g_camera_position_c += w * speed * delta_t;
            if (tecla_A_pressionada)
                g_camera_position_c -= u * speed * delta_t;
            if (tecla_D_pressionada)
                g_camera_position_c += u * speed * delta_t;

            g_camera_position_c.y = g_CameraAlturaFixa;

            glm::vec4 new_camera_position = g_camera_position_c; // Posição atual

            // Atualiza a bounding box do jogador
            g_PlayerBox.min = new_camera_position - glm::vec4(0.5f, 0.5f, 0.5f, 0.0f);
            g_PlayerBox.max = new_camera_position + glm::vec4(0.5f, 0.5f, 0.5f, 0.0f);

            // Verifica e resolve colisão com a casa
            CollisionResult houseCollision = ResolveBoxCollision(g_PlayerBox, g_HouseBox, g_camera_position_c, new_camera_position);
            if (houseCollision.collided) {
                new_camera_position = houseCollision.correctedPosition;
            }

            // Verifica e resolve colisão com a máquina
            CollisionResult cashierCollision = ResolveBoxCollision(g_PlayerBox, g_CashierBox, g_camera_position_c, new_camera_position);
            if (cashierCollision.collided) {
                new_camera_position = cashierCollision.correctedPosition;
            }

            // Verifica colisão com os planos limite do mapa
            float plane_threshold = 1.0f;
            bool collides_with_boundary =
            PointToPlaneCollision(new_camera_position, boundary_plane_north, plane_threshold) ||
            PointToPlaneCollision(new_camera_position, boundary_plane_south, plane_threshold) ||
            PointToPlaneCollision(new_camera_position, boundary_plane_east, plane_threshold) ||
            PointToPlaneCollision(new_camera_position, boundary_plane_west, plane_threshold);

            if (collides_with_boundary)
            {
                new_camera_position = g_camera_position_c;
            }

            if (new_camera_position.x < -85.0f)
                new_camera_position.x = -85.0f;
            if (new_camera_position.x > 85.0f)
                new_camera_position.x = 85.0f;
            if (new_camera_position.z < -195.0f)
                new_camera_position.z = -195.0f;
            if (new_camera_position.z > 83.0f)
                new_camera_position.z = 83.0f;

            // Atualiza a posição final
            g_camera_position_c = new_camera_position;


            // Note que, no sistema de coordenadas da câmera, os planos near e far
            // estão no sentido negativo! Veja slides 176-204 do documento Aula_09_Projecoes.pdf.
            float nearplane = -0.1f;  // Posição do "near plane"
            float farplane  = -500.0f; // Posição do "far plane"

            // Projeção Perspectiva.
            // Para definição do field of view (FOV), veja slides 205-215 do documento Aula_09_Projecoes.pdf.
            float field_of_view = 3.141592 / 3.0f;
            projection = Matrix_Perspective(field_of_view, g_ScreenRatio, nearplane, farplane);
        }

        else
        {
            glm::vec4 camera_lookat_l = g_camera_position_c;
            glm::vec4 camera_up_vector = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);

            // Ângulo de rotação baseado no tempo
            float rotation_speed = 0.5f; // Velocidade de rotação
            float angle = (float)glfwGetTime() * rotation_speed;

            // Posição da câmera girando ao redor do ponto de interesse
            float radius = 5.0f; // Distância fixa do ponto de interesse
            float x = radius * cos(angle);
            float z = radius * sin(angle);
            glm::vec4 camera_position_c = glm::vec4(x, g_CameraAlturaFixa, z, 1.0f); // Altura fixa da câmera

            // Matriz view para a câmera Look-At rotacionando
            view = Matrix_Camera_View(camera_position_c, camera_lookat_l - camera_position_c, camera_up_vector);

            // Configuração da matriz de projeção
            float nearplane = -0.1f;  // Posição do "near plane"
            float farplane  = -100.0f; // Posição do "far plane"

            // Projeção Perspectiva para Look-At
            float field_of_view = 3.141592 / 3.0f;
            projection = Matrix_Perspective(field_of_view, g_ScreenRatio, nearplane, farplane);

        }

        // Enviamos as matrizes "view" e "projection" para a placa de vídeo
        // (GPU). Veja o arquivo "shader_vertex.glsl", onde estas são
        // efetivamente aplicadas em todos os pontos.
        glUniformMatrix4fv(g_view_uniform       , 1 , GL_FALSE , glm::value_ptr(view));
        glUniformMatrix4fv(g_projection_uniform , 1 , GL_FALSE , glm::value_ptr(projection));

        #define SPHERE 0
        #define BUNNY  1
        #define PLANE  2
        #define MAINBUILD 3
        #define BAGUETE 4
        #define EGG 5
        #define BUTTER 6
        #define CHEESE 7
        #define LUA 8
        #define MANSION 9
        #define PLANE_ASPHALT 10
        #define PLANE_GRASS 11
        #define SMALLHOUSE 12
        #define GASSTATION 13
        #define MYHOUSE 14
        #define PERSONAGEM 15
        #define LONGHOUSE 16
        #define WOODHOUSE 17
        #define LASTHOUSE 18
        #define POLE 19
        #define CALCADA 20
        #define LILHOUSE 21
        #define MAQUINA 22
        #define SKY 23

        /// desenhos adicionados

        // Skybox
        glCullFace(GL_FRONT);
        glDepthMask(GL_FALSE);
        model = Matrix_Translate(camera_position_c.x, camera_position_c.y, camera_position_c.z - 50.0)
        * Matrix_Scale(200.0f, 200.0f, 200.0f);  // Aumenta o tamanho para evitar flickering nas bordas
        glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
        glUniform1i(g_object_id_uniform, SKY);
        DrawVirtualObject("the_sphere");
        // Reativa escrita no z-buffer
        glDepthMask(GL_TRUE);
        glCullFace(GL_BACK);

        // Construções
        model = Matrix_Translate(13.0f,-1.0f,-165.0f)  // x, y, z (y = -1.1f coloca no mesmo nível do chão)
        * Matrix_Scale(0.4f, 0.4f, 0.4f)
        * Matrix_Rotate(165.0f, glm::vec4(0.0f, 1.0f, 0.0f, 0.0f));
        glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
        glUniform1i(g_object_id_uniform, MAINBUILD);
        DrawVirtualObject("the_mainbuild");

        // Desenhamos o modelo da casa
        model = Matrix_Translate(-25.0f, -1.1f, -20.0f)
        * Matrix_Rotate_Y(M_PI/2.0f)
        * Matrix_Scale(0.7f, 0.7f, 0.7f);
        glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
        glUniform1i(g_object_id_uniform, SMALLHOUSE);
        DrawVirtualObject("the_smallHouse");

        model = Matrix_Translate(33.0f, -1.1f, -75.0f)
        * Matrix_Rotate_Y(M_PI*2)
        * Matrix_Scale(0.7f, 0.7f, 0.7f);
        glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
        glUniform1i(g_object_id_uniform, SMALLHOUSE);
        DrawVirtualObject("the_smallHouse");

        // Desenhamos o modelo do posto de gasolina
        model = Matrix_Translate(-70.0f, -1.3f, -105.0f)
        * Matrix_Rotate_Y(M_PI)
        * Matrix_Scale(0.55f, 0.55f, 0.55f);
        glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
        glUniform1i(g_object_id_uniform, GASSTATION);
        DrawVirtualObject("the_gasstation");

        // Desenhamos o modelo da nossa casa
        model = Matrix_Translate(0.0f, -1.3f, 58.0f)
        * Matrix_Rotate_Y(M_PI/2)
        * Matrix_Scale(1.0f, 1.0f, 1.0f);
        glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
        glUniform1i(g_object_id_uniform, MYHOUSE);
        DrawVirtualObject("myHouse");

        // Salvamos a matriz do caixa para uso no raycasting
        g_object_matrices["myHouse"] = model;

        if (g_object_highlighted == MYHOUSE && tecla_E_pressionada && g_PaymentCompleted)
        {
            printf("Interagindo com a casa\n");
            g_GameWon = true;
            tecla_E_pressionada = false;
        }

        // Desenhamos o modelo de uma das casas
        model = Matrix_Translate(40.0f, -1.3f, -30.0f)
        * Matrix_Rotate_Y(M_PI)
        * Matrix_Scale(0.90f, 0.90f, 0.90f);
        glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
        glUniform1i(g_object_id_uniform, LONGHOUSE);
        DrawVirtualObject("the_longhouse");

        // Desenhamos o modelo da casa de madeira 1
        model = Matrix_Translate(60.0f, -1.3f, 17.50f)
        * Matrix_Rotate_Y(M_PI)
        * Matrix_Scale(0.40f, 0.40f, 0.40f);
        glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
        glUniform1i(g_object_id_uniform, WOODHOUSE);
        DrawVirtualObject("the_woodhouse");

        // Desenhamos o modelo da casa de madeira 2
        model = Matrix_Translate(-60.0f, -1.3f, 17.50f)
        * Matrix_Rotate_Y(M_PI*2)
        * Matrix_Scale(0.40f, 0.40f, 0.40f);
        glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
        glUniform1i(g_object_id_uniform, WOODHOUSE);
        DrawVirtualObject("the_woodhouse");

        // Desenhamos o modelo da casa de madeira 3
        model = Matrix_Translate(-30.0f, -1.3f, -50.50f)
        * Matrix_Rotate_Y(M_PI/2)
        * Matrix_Scale(0.40f, 0.40f, 0.40f);
        glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
        glUniform1i(g_object_id_uniform, WOODHOUSE);
        DrawVirtualObject("the_woodhouse");

       // Desenhamos todas as instâncias da calçada
        for(const Calcada& calcada : calcadas) {
            glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(calcada.model));
            glUniform1i(g_object_id_uniform, CALCADA);
            DrawVirtualObject("calcada");


            /// Desenhamos os planos do chão

            //asfalto
            model = Matrix_Translate(0.0f,-1.1f,-73.5f)
            * Matrix_Scale(5.0f, 1.0f, 76.5f); // Aumentar o tamanho do plano
            glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
            glUniform1i(g_object_id_uniform, PLANE);
            DrawVirtualObject("the_plane");

            model = Matrix_Translate(0.0f,-1.1f,16.0f)
            * Matrix_Scale(35.0f, 1.0f, 13.0f);
            glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
            glUniform1i(g_object_id_uniform, PLANE_ASPHALT);
            DrawVirtualObject("the_plane");
        }

        //grama
        model = Matrix_Translate(45.0f,-1.1f,-97.0f)
        * Matrix_Scale(40.0f, 1.0f, 100.0f); // Aumentar o tamanho do plano
        glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
        glUniform1i(g_object_id_uniform, PLANE_GRASS);
        DrawVirtualObject("the_plane");

        model = Matrix_Translate(-45.0f,-1.1f,-97.0f)
        * Matrix_Scale(40.0f, 1.0f, 100.0f); // Aumentar o tamanho do plano
        glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
        glUniform1i(g_object_id_uniform, PLANE_GRASS);
        DrawVirtualObject("the_plane");

        model = Matrix_Translate(60.0f,-1.1f,43.0f)
        * Matrix_Scale(25.0f, 1.0f, 40.0f); // Aumentar o tamanho do plano
        glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
        glUniform1i(g_object_id_uniform, PLANE_GRASS);
        DrawVirtualObject("the_plane");

        model = Matrix_Translate(-60.0f,-1.1f,43.0f)
        * Matrix_Scale(25.0f, 1.0f, 40.0f); // Aumentar o tamanho do plano
        glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
        glUniform1i(g_object_id_uniform, PLANE_GRASS);
        DrawVirtualObject("the_plane");

        model = Matrix_Translate(0.0f,-1.1f,55.5f)
        * Matrix_Scale(35.0f, 1.0f, 27.5f); // Aumentar o tamanho do plano
        glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
        glUniform1i(g_object_id_uniform, PLANE_GRASS);
        DrawVirtualObject("the_plane");

        model = Matrix_Translate(0.0f,-1.1f,-173.5f)
        * Matrix_Scale(5.0f, 1.0f, 23.5f); // Aumentar o tamanho do plano
        glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
        glUniform1i(g_object_id_uniform, PLANE_GRASS);
        DrawVirtualObject("the_plane");

        // Desenhamos o poste
        model = Matrix_Translate(-7.0f, -1.1f, -5.5f)
        * Matrix_Rotate_Y(-M_PI/2)
        * Matrix_Scale(0.6f, 0.6f, 0.6f);
        glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
        glUniform1i(g_object_id_uniform, POLE);
        DrawVirtualObject("the_pole");

        // Desenhamos o poste
        model = Matrix_Translate(-7.0f, -1.1f, -42.0f)
        * Matrix_Rotate_Y(-M_PI/2)
        * Matrix_Scale(0.6f, 0.6f, 0.6f);
        glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
        glUniform1i(g_object_id_uniform, POLE);
        DrawVirtualObject("the_pole");

        // Desenhamos o poste
        model = Matrix_Translate(-7.0f, -1.1f, -78.5f)
        * Matrix_Rotate_Y(-M_PI/2)
        * Matrix_Scale(0.6f, 0.6f, 0.6f);
        glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
        glUniform1i(g_object_id_uniform, POLE);
        DrawVirtualObject("the_pole");

        //Desenhamos o modelo da lua
        model = Matrix_Translate(15.0, 60.0, -100.0f)
        * Matrix_Rotate_Y(g_AngleY/10)
        * Matrix_Rotate_Z(g_AngleY/5)
        * Matrix_Rotate_X(g_AngleY/10)
        * Matrix_Scale(6.0f, 6.0f, 6.0f);
        glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
        glUniform1i(g_object_id_uniform, LUA);
        DrawVirtualObject("the_sphere");

        //Desenhamos o modelo da maquina de pagamento
        model = Matrix_Translate(15.0f, -1.1f, -147.5f)
        * Matrix_Rotate_Y(M_PI*2)
        * Matrix_Scale(1.5f, 1.5f, 1.5f);
        glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
        glUniform1i(g_object_id_uniform, MAQUINA);
        DrawVirtualObject("maquina_pagamento");

        // Salvamos a matriz do caixa para uso no raycasting
        g_object_matrices["maquina_pagamento"] = model;

        if (g_object_highlighted == MAQUINA && tecla_E_pressionada && !g_HasPaidPurchases)
        {
            printf("Tentando interagir com o caixa\n"); // Debug

            bool all_items_collected = true;
            int items_coletados = 0;
            g_TotalPurchaseValue = 0.0f;


            // Verifica quantos itens foram coletados
            for (size_t i = 0; i < itens_pegos.size(); i++)
            {
                if (itens_pegos[i])
                {
                    items_coletados++;
                    g_TotalPurchaseValue += g_ItemPrices[itens_para_comprar[i]];
                }
                else
                {
                    all_items_collected = false;
                }
            }

            printf("Items coletados: %d de %zu\n", items_coletados, itens_pegos.size()); // Debug

            if (all_items_collected)
            {
                printf("Iniciando interação com caixa. Valor total: %.2f\n", g_TotalPurchaseValue); // Debug
                g_InteractingWithCashier = true;
                tecla_E_pressionada = false;
            }
            else
            {
                printf("Ainda faltam itens para coletar!\n"); // Debug
            }
        }


       // Desenhamos o modelo do queijo
        if (!cheese_picked)
        {
            model = Matrix_Translate(10.0f, -1.0f, -156.0f)
            * Matrix_Scale(0.5f, 0.5f, 0.5f);
            glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
            glUniform1i(g_object_id_uniform, CHEESE);
            DrawVirtualObject("the_cheese");

            // Salvamos a matriz do objeto para uso no raycasting
            g_object_matrices["the_cheese"] = model;
        }

        if (g_object_highlighted == CHEESE && tecla_E_pressionada && !cheese_picked)
        {
            cheese_picked = true;
            MarcarItemComoPego(ITEM_QUEIJO);
            tecla_E_pressionada = false;

            // Verifica se o objeto está na lista de compras e marca como pego
            for (size_t i = 0; i < itens_para_comprar.size(); ++i)
            {
                if (itens_para_comprar[i] == "queijo")
                {
                    itens_pegos[i] = true;
                    printf("Item comprado: %s\n", itens_para_comprar[i].c_str());
                    break;
                }
            }
        }

        // Desenhamos o modelo da manteiga
        if (!butter_picked)
        {
            model = Matrix_Translate(10.0f, -1.0f, -160.0f)
            * Matrix_Scale(0.3f, 0.3f, 0.3f);
            glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
            glUniform1i(g_object_id_uniform, BUTTER);
            DrawVirtualObject("the_butter");

            // Salvamos a matriz do objeto para uso no raycasting
            g_object_matrices["the_butter"] = model;
        }

        if (g_object_highlighted == BUTTER && tecla_E_pressionada && !butter_picked)
        {
            butter_picked = true;
            MarcarItemComoPego(ITEM_MANTEIGA);
            tecla_E_pressionada = false;

            // Verifica se o objeto está na lista de compras e marca como pego
            for (size_t i = 0; i < itens_para_comprar.size(); ++i)
            {
                if (itens_para_comprar[i] == "manteiga")
                {
                    itens_pegos[i] = true;
                    printf("Item comprado: %s\n", itens_para_comprar[i].c_str());
                    break;
                }
            }
        }


        // Desenhamos o modelo do ovo
        if (!egg_picked)
        {
            model = Matrix_Translate(-6.0f, -1.0f, -156.0f)
            * Matrix_Scale(0.60f, 0.60f, 0.60f);
            glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
            glUniform1i(g_object_id_uniform, EGG);
            DrawVirtualObject("the_eggs");

            // Salvamos a matriz do ovo para uso no raycasting
            g_object_matrices["the_eggs"] = model;
        }

        if (g_object_highlighted == EGG && tecla_E_pressionada && !egg_picked)
        {
            egg_picked = true;
            MarcarItemComoPego(ITEM_OVO);
            tecla_E_pressionada = false;

            // Verifica se o ovo está na lista de compras e marca como pego
            for (size_t i = 0; i < itens_para_comprar.size(); ++i)
            {
                if (itens_para_comprar[i] == "ovo")
                {
                    itens_pegos[i] = true;
                    printf("Item comprado: %s\n", itens_para_comprar[i].c_str());
                    break;
                }
            }
        }

        // Desenhamos o modelo da baguete
        if (!baguete_picked)
        {
            model = Matrix_Translate(-6.0f, 0.0f, -160.0f)
            * Matrix_Scale(0.15f, 0.15f, 0.15f);
            glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
            glUniform1i(g_object_id_uniform, BAGUETE);
            DrawVirtualObject("the_baguete");

            // Salvamos a matriz da baguete para uso no raycasting
            g_object_matrices["the_baguete"] = model;
        }

        if (g_object_highlighted == BAGUETE && tecla_E_pressionada && !baguete_picked)
        {
            baguete_picked = true;
            MarcarItemComoPego(ITEM_BAGUETE);
            tecla_E_pressionada = false;

            // Verificamos se a baguete está na lista de compras e marca como pego
            for (size_t i = 0; i < itens_para_comprar.size(); ++i)
            {
                if (itens_para_comprar[i] == "baguete")
                {
                    itens_pegos[i] = true;
                    printf("Item comprado: %s\n", itens_para_comprar[i].c_str());
                    break;
                }
            }
        }

        if (!bunny_picked)
        {
            model = Matrix_Translate(50.0f,0.0f,-40.0f)
            * Matrix_Rotate_X(g_AngleX + (float)glfwGetTime() * 0.1f);
            glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
            glUniform1i(g_object_id_uniform, BUNNY);
            DrawVirtualObject("the_bunny");

            glm::vec4 bunny_position = model * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

            // Verifica colisão com o jogador
            if (SphereToSphereCollision(g_camera_position_c, PLAYER_RADIUS, bunny_position, BUNNY_RADIUS))
            {
                bunny_picked = true;
                g_PlayerMoney += 50.0f;
                printf("Coelho encontrado! +R$ 50.00\n");
            }
        }
        // Salvamos a matriz do coelho para uso no raycasting
        g_object_matrices["the_bunny"] = model;

        // Verificamos qual objeto está sob o crosshair
        g_object_highlighted = GetObjectUnderCrosshair(
            g_camera_position_c,
            camera_view_vector,
            g_VirtualScene,
            g_object_matrices
        );


        // Se o coelho está sob o crosshair, desenhamos ele novamente com destaque amarelo
        if (g_object_highlighted == BUNNY && !bunny_picked) {
            glEnable(GL_STENCIL_TEST);
            glStencilFunc(GL_ALWAYS, 1, 0xFF);
            glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
            glStencilMask(0xFF);

            // Transformações geométricas do coelho
            model = Matrix_Translate(1.0f,0.0f,0.0f)
                  * Matrix_Scale(1.3f, 1.3f, 1.3f)  // escala que aumentamos o coelho
                  * Matrix_Rotate_X(g_AngleX + (float)glfwGetTime() * 0.1f);

            glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));

            // Ativamos a sobreposição de cor
            glUniform1i(glGetUniformLocation(g_GpuProgramID, "use_color_override"), true);
            glUniform4f(glGetUniformLocation(g_GpuProgramID, "color_override"), 1.0f, 1.0f, 0.0f, 1.0f); // cor que destacamos ele

            DrawVirtualObject("the_bunny");

            // Desativamos a sobreposição de cor para os próximos objetos
            glUniform1i(glGetUniformLocation(g_GpuProgramID, "use_color_override"), false);
            glDisable(GL_STENCIL_TEST);
        }

        else if (g_object_highlighted == BAGUETE && !baguete_picked)
        {
            glEnable(GL_STENCIL_TEST);
            glStencilFunc(GL_ALWAYS, 1, 0xFF);
            glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
            glStencilMask(0xFF);

            model = Matrix_Translate(-6.0f, 0.0f, -160.0f)
            * Matrix_Scale(0.195f, 0.195f, 0.195f);  // Escala um pouco maior para o highlight

            glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));

            // Ativamos a sobreposição de cor
            glUniform1i(glGetUniformLocation(g_GpuProgramID, "use_color_override"), true);
            glUniform4f(glGetUniformLocation(g_GpuProgramID, "color_override"), 1.0f, 1.0f, 0.0f, 1.0f);

            DrawVirtualObject("the_baguete");

            // Desativamos a sobreposição de cor para os próximos objetos
            glUniform1i(glGetUniformLocation(g_GpuProgramID, "use_color_override"), false);
            glDisable(GL_STENCIL_TEST);
        }
        else if (g_object_highlighted == EGG && !egg_picked)
        {
            glEnable(GL_STENCIL_TEST);
            glStencilFunc(GL_ALWAYS, 1, 0xFF);
            glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
            glStencilMask(0xFF);

            model = Matrix_Translate(-6.0f, -1.0f, -156.0f)
            * Matrix_Scale(1.20f, 1.20f, 1.20f);  // Escala um pouco maior para o highlight

            glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));

            // Ativamos a sobreposição de cor
            glUniform1i(glGetUniformLocation(g_GpuProgramID, "use_color_override"), true);
            glUniform4f(glGetUniformLocation(g_GpuProgramID, "color_override"), 1.0f, 1.0f, 0.0f, 1.0f);

            DrawVirtualObject("the_eggs");

            // Desativamos a sobreposição de cor para os próximos objetos
            glUniform1i(glGetUniformLocation(g_GpuProgramID, "use_color_override"), false);
            glDisable(GL_STENCIL_TEST);
        }

        else if (g_object_highlighted == BUTTER && !butter_picked)
        {
            glEnable(GL_STENCIL_TEST);
            glStencilFunc(GL_ALWAYS, 1, 0xFF);
            glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
            glStencilMask(0xFF);

            model = Matrix_Translate(10.0f, -1.0f, -160.0f)
            * Matrix_Scale(0.5f, 0.5f, 0.5f);  // Escala um pouco maior para o highlight

            glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));

            // Ativamos a sobreposição de cor
            glUniform1i(glGetUniformLocation(g_GpuProgramID, "use_color_override"), true);
            glUniform4f(glGetUniformLocation(g_GpuProgramID, "color_override"), 1.0f, 1.0f, 0.0f, 1.0f);

            DrawVirtualObject("the_butter");

            // Desativamos a sobreposição de cor para os próximos objetos
            glUniform1i(glGetUniformLocation(g_GpuProgramID, "use_color_override"), false);
            glDisable(GL_STENCIL_TEST);
        }

        else if (g_object_highlighted == CHEESE && !cheese_picked)
        {
            glEnable(GL_STENCIL_TEST);
            glStencilFunc(GL_ALWAYS, 1, 0xFF);
            glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
            glStencilMask(0xFF);

            model = Matrix_Translate(10.0f, -1.0f, -156.0f)
            * Matrix_Scale(0.6f, 0.6f, 0.6f);  // Escala um pouco maior para o highlight

            glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));

            // Ativamos a sobreposição de cor
            glUniform1i(glGetUniformLocation(g_GpuProgramID, "use_color_override"), true);
            glUniform4f(glGetUniformLocation(g_GpuProgramID, "color_override"), 1.0f, 1.0f, 0.0f, 1.0f);

            DrawVirtualObject("the_cheese");

            // Desativamos a sobreposição de cor para os próximos objetos
            glUniform1i(glGetUniformLocation(g_GpuProgramID, "use_color_override"), false);
            glDisable(GL_STENCIL_TEST);
        }
        else if (g_object_highlighted == MAQUINA)
        {
            glEnable(GL_STENCIL_TEST);
            glStencilFunc(GL_ALWAYS, 1, 0xFF);
            glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
            glStencilMask(0xFF);

            model = Matrix_Translate(15.0f, -1.1f, -147.5f)
            * Matrix_Scale(1.9f, 1.9f, 1.9f);  // Escala um pouco maior para o highlight

            glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));

            // Ativamos a sobreposição de cor
            glUniform1i(glGetUniformLocation(g_GpuProgramID, "use_color_override"), true);
            glUniform4f(glGetUniformLocation(g_GpuProgramID, "color_override"), 1.0f, 1.0f, 0.0f, 1.0f);

            DrawVirtualObject("maquina_pagamento");

            // Desativamos a sobreposição de cor para os próximos objetos
            glUniform1i(glGetUniformLocation(g_GpuProgramID, "use_color_override"), false);
            glDisable(GL_STENCIL_TEST);
        }

        else if (g_object_highlighted == MYHOUSE && g_PaymentCompleted)
        {
            glEnable(GL_STENCIL_TEST);
            glStencilFunc(GL_ALWAYS, 1, 0xFF);
            glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
            glStencilMask(0xFF);

            model = Matrix_Translate(0.0f, -1.3f, 58.0f)
                  * Matrix_Scale(1.3f, 1.3f, 1.3f);  // Escala um pouco maior para o highlight

            glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));

            // Ativamos a sobreposição de cor
            glUniform1i(glGetUniformLocation(g_GpuProgramID, "use_color_override"), true);
            glUniform4f(glGetUniformLocation(g_GpuProgramID, "color_override"), 1.0f, 1.0f, 0.0f, 1.0f);

            DrawVirtualObject("myHouse");

            // Desativamos a sobreposição de cor para os próximos objetos
            glUniform1i(glGetUniformLocation(g_GpuProgramID, "use_color_override"), false);
            glDisable(GL_STENCIL_TEST);
        }


        //esfera seguindo a curva de bezier
        glm::vec4 sphere_position = AtualizaPonto(current_time * ControleVelocidadeCurva , p0, p1, p2, p3);
        model = Matrix_Translate(sphere_position.x, sphere_position.y, sphere_position.z)
            * Matrix_Scale(0.1f, 0.1f, 0.1f);
        glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
        glUniform1i(g_object_id_uniform, SPHERE);
        DrawVirtualObject("the_sphere");



        ///crosshair("+")
        glUseProgram(g_GpuProgramID);

        // Salvamos as matrizes atuais para restaurar depois
        glm::mat4 saved_projection = projection;
        glm::mat4 saved_view = view;

        // Configuramos uma projeção ortográfica especial para o crosshair
        glm::mat4 crosshair_projection = Matrix_Orthographic(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);
        // View matrix identidade para o crosshair ficar fixo na tela
        glm::mat4 crosshair_view = Matrix_Identity();

        // Enviamos as novas matrizes para a GPU
        glUniformMatrix4fv(g_projection_uniform, 1, GL_FALSE, glm::value_ptr(crosshair_projection));
        glUniformMatrix4fv(g_view_uniform, 1, GL_FALSE, glm::value_ptr(crosshair_view));

        // Configuramos a cor do crosshair para preto
        glUniform4f(glGetUniformLocation(g_GpuProgramID, "color"), 0.0f, 0.0f, 0.0f, 1.0f);

        glm::mat4 crosshair_model = Matrix_Identity();
        glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(crosshair_model));

        // Desabilitamos o teste de profundidade para o crosshair sempre aparecer sobre tudo
        glDisable(GL_DEPTH_TEST);

        // Definimos os vértices do "+" (duas linhas perpendiculares)
        float crosshair_vertices[] = {
            // Linha horizontal
            -0.02f,  0.0f, 0.0f, 1.0f,
             0.02f,  0.0f, 0.0f, 1.0f,
            // Linha vertical
             0.0f,  -0.02f, 0.0f, 1.0f,
             0.0f,   0.02f, 0.0f, 1.0f
        };

        GLuint VBO_crosshair;
        glGenBuffers(1, &VBO_crosshair);
        glBindBuffer(GL_ARRAY_BUFFER, VBO_crosshair);
        glBufferData(GL_ARRAY_BUFFER, sizeof(crosshair_vertices), crosshair_vertices, GL_STATIC_DRAW);

        GLuint VAO_crosshair;
        glGenVertexArrays(1, &VAO_crosshair);
        glBindVertexArray(VAO_crosshair);

        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(0);

        // Definimos a largura da linha
        glLineWidth(2.0f);

        // Desenhamos as duas linhas
        glDrawArrays(GL_LINES, 0, 4);

        // Reabilitamos o teste de profundidade
        glEnable(GL_DEPTH_TEST);

        // Deletamos os buffers do crosshair
        glDeleteBuffers(1, &VBO_crosshair);
        glDeleteVertexArrays(1, &VAO_crosshair);

        // Restauramos as matrizes originais
        glUniformMatrix4fv(g_projection_uniform, 1, GL_FALSE, glm::value_ptr(saved_projection));
        glUniformMatrix4fv(g_view_uniform, 1, GL_FALSE, glm::value_ptr(saved_view));

/// ######

        // Imprimimos na tela informação sobre o número de quadros renderizados
        // por segundo (frames per second).
        TextRendering_ShowFramesPerSecond(window);

        DrawShoppingList(window);

        // Desenha o diálogo do caixa se estiver interagindo
        if (g_InteractingWithCashier) {
            DrawCashierDialog(window);
        }

        // O framebuffer onde OpenGL executa as operações de renderização não
        // é o mesmo que está sendo mostrado para o usuário, caso contrário
        // seria possível ver artefatos conhecidos como "screen tearing". A
        // chamada abaixo faz a troca dos buffers, mostrando para o usuário
        // tudo que foi renderizado pelas funções acima.
        // Veja o link: https://en.wikipedia.org/w/index.php?title=Multiple_buffering&oldid=793452829#Double_buffering_in_computer_graphics

        glfwSwapBuffers(window);

        // Verificamos com o sistema operacional se houve alguma interação do
        // usuário (teclado, mouse, ...). Caso positivo, as funções de callback
        // definidas anteriormente usando glfwSet*Callback() serão chamadas
        // pela biblioteca GLFW.
        glfwPollEvents();
    }

    // Finalizamos o uso dos recursos do sistema operacional
    glfwTerminate();

    // Fim do programa
    return 0;
}

/// destacar objeto
bool RayOBBIntersection(glm::vec4 ray_origin, glm::vec4 ray_direction, glm::vec4 box_center, glm::vec4 box_extent, glm::mat4 box_rotation)
{
    // Transformamos o raio para o espaço do objeto
    glm::mat4 box_rotation_inverse = glm::inverse(box_rotation);
    glm::vec4 r_origin = box_rotation_inverse * (ray_origin - box_center);
    glm::vec4 r_direction = box_rotation_inverse * ray_direction;

    float tmin = -INFINITY;
    float tmax = INFINITY;

    for (int i = 0; i < 3; i++) {
        if (abs(r_direction[i]) < 0.001f) {
            if (r_origin[i] < -box_extent[i] || r_origin[i] > box_extent[i])
                return false;
        }
        else {
            float t1 = (-box_extent[i] - r_origin[i]) / r_direction[i];
            float t2 = (box_extent[i] - r_origin[i]) / r_direction[i];

            if (t1 > t2) std::swap(t1, t2);

            tmin = glm::max(tmin, t1);
            tmax = glm::min(tmax, t2);

            if (tmin > tmax) return false;
        }
    }

    return true;
}

int GetObjectUnderCrosshair(glm::vec4 camera_position, glm::vec4 camera_view, std::map<std::string, SceneObject>& virtual_scene, std::map<std::string, glm::mat4>& object_matrices)
{
    // Direção do raio é a direção da visão da câmera
    glm::vec4 ray_direction = normalize(camera_view);

    // Testamos interseção com cada objeto da cena
    for (const auto& obj : virtual_scene) {
        if (obj.first == "the_bunny") {
            glm::vec4 box_center = object_matrices[obj.first] * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
            glm::vec4 box_extent = glm::vec4(0.5f, 0.5f, 0.5f, 0.0f);

            if (RayOBBIntersection(camera_position, ray_direction, box_center, box_extent, object_matrices[obj.first])) {
                return BUNNY;
            }
        }
        else if (obj.first == "the_baguete") {
            glm::vec4 box_center = object_matrices[obj.first] * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
            glm::vec4 box_extent = glm::vec4(0.5f, 0.5f, 0.5f, 0.0f);

            if (RayOBBIntersection(camera_position, ray_direction, box_center, box_extent, object_matrices[obj.first])) {
                return BAGUETE;
            }
        }
        else if (obj.first == "the_eggs")
        {
            glm::vec4 box_center = object_matrices[obj.first] * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
            glm::vec4 box_extent = glm::vec4(0.5f, 0.5f, 0.5f, 0.0f);

            if (RayOBBIntersection(camera_position, ray_direction, box_center, box_extent, object_matrices[obj.first]))
            {
                return EGG;
            }
        }

        else if (obj.first == "the_butter")
        {
            glm::vec4 box_center = object_matrices[obj.first] * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
            glm::vec4 box_extent = glm::vec4(0.5f, 0.5f, 0.5f, 0.0f);

            if (RayOBBIntersection(camera_position, ray_direction, box_center, box_extent, object_matrices[obj.first]))
            {
                return BUTTER;
            }
        }

        else if (obj.first == "the_cheese")
        {
            glm::vec4 box_center = object_matrices[obj.first] * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
            glm::vec4 box_extent = glm::vec4(0.5f, 0.5f, 0.5f, 0.0f);

            if (RayOBBIntersection(camera_position, ray_direction, box_center, box_extent, object_matrices[obj.first]))
            {
                return CHEESE;
            }
        }

        else if (obj.first == "maquina_pagamento")
        {
            glm::vec4 box_center = object_matrices[obj.first] * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
            glm::vec4 box_extent = glm::vec4(0.5f, 0.5f, 0.5f, 0.0f);

            if (RayOBBIntersection(camera_position, ray_direction, box_center, box_extent, object_matrices[obj.first]))
            {
                return MAQUINA;
            }
        }

        else if (obj.first == "myHouse")
        {
            glm::vec4 box_center = object_matrices[obj.first] * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
            glm::vec4 box_extent = glm::vec4(0.5f, 0.5f, 0.5f, 0.0f);

            if (RayOBBIntersection(camera_position, ray_direction, box_center, box_extent, object_matrices[obj.first]))
            {
                return MYHOUSE;
            }
        }

    }
    return -1; // Nenhum objeto encontrado
}


// Função que carrega uma imagem para ser utilizada como textura
void LoadTextureImage(const char* filename)
{
    printf("Carregando imagem \"%s\"... ", filename);

    // Primeiro fazemos a leitura da imagem do disco
    stbi_set_flip_vertically_on_load(true);
    int width;
    int height;
    int channels;
    unsigned char *data = stbi_load(filename, &width, &height, &channels, 3);

    if ( data == NULL )
    {
        fprintf(stderr, "ERROR: Cannot open image file \"%s\".\n", filename);
        std::exit(EXIT_FAILURE);
    }

    printf("OK (%dx%d).\n", width, height);

    // Agora criamos objetos na GPU com OpenGL para armazenar a textura
    GLuint texture_id;
    GLuint sampler_id;
    glGenTextures(1, &texture_id);
    glGenSamplers(1, &sampler_id);

    // Veja slides 95-96 do documento Aula_20_Mapeamento_de_Texturas.pdf
    glSamplerParameteri(sampler_id, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glSamplerParameteri(sampler_id, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Parâmetros de amostragem da textura.
    glSamplerParameteri(sampler_id, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glSamplerParameteri(sampler_id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Agora enviamos a imagem lida do disco para a GPU
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);

    GLuint textureunit = g_NumLoadedTextures;
    glActiveTexture(GL_TEXTURE0 + textureunit);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindSampler(textureunit, sampler_id);

    stbi_image_free(data);

    g_NumLoadedTextures += 1;
}

// Função que desenha um objeto armazenado em g_VirtualScene. Veja definição
// dos objetos na função BuildTrianglesAndAddToVirtualScene().
void DrawVirtualObject(const char* object_name)
{
    // "Ligamos" o VAO. Informamos que queremos utilizar os atributos de
    // vértices apontados pelo VAO criado pela função BuildTrianglesAndAddToVirtualScene(). Veja
    // comentários detalhados dentro da definição de BuildTrianglesAndAddToVirtualScene().
    glBindVertexArray(g_VirtualScene[object_name].vertex_array_object_id);

    // Setamos as variáveis "bbox_min" e "bbox_max" do fragment shader
    // com os parâmetros da axis-aligned bounding box (AABB) do modelo.
    glm::vec3 bbox_min = g_VirtualScene[object_name].bbox_min;
    glm::vec3 bbox_max = g_VirtualScene[object_name].bbox_max;
    glUniform4f(g_bbox_min_uniform, bbox_min.x, bbox_min.y, bbox_min.z, 1.0f);
    glUniform4f(g_bbox_max_uniform, bbox_max.x, bbox_max.y, bbox_max.z, 1.0f);

    // Pedimos para a GPU rasterizar os vértices dos eixos XYZ
    // apontados pelo VAO como linhas. Veja a definição de
    // g_VirtualScene[""] dentro da função BuildTrianglesAndAddToVirtualScene(), e veja
    // a documentação da função glDrawElements() em
    // http://docs.gl/gl3/glDrawElements.
    glDrawElements(
        g_VirtualScene[object_name].rendering_mode,
        g_VirtualScene[object_name].num_indices,
        GL_UNSIGNED_INT,
        (void*)(g_VirtualScene[object_name].first_index * sizeof(GLuint))
    );

    // "Desligamos" o VAO, evitando assim que operações posteriores venham a
    // alterar o mesmo. Isso evita bugs.
    glBindVertexArray(0);
}

// Função que carrega os shaders de vértices e de fragmentos que serão
// utilizados para renderização. Veja slides 180-200 do documento Aula_03_Rendering_Pipeline_Grafico.pdf.
//
void LoadShadersFromFiles()
{
    // Note que o caminho para os arquivos "shader_vertex.glsl" e
    // "shader_fragment.glsl" estão fixados, sendo que assumimos a existência
    // da seguinte estrutura no sistema de arquivos:
    //
    //    + FCG_Lab_01/
    //    |
    //    +--+ bin/
    //    |  |
    //    |  +--+ Release/  (ou Debug/ ou Linux/)
    //    |     |
    //    |     o-- main.exe
    //    |
    //    +--+ src/
    //       |
    //       o-- shader_vertex.glsl
    //       |
    //       o-- shader_fragment.glsl
    //
    GLuint vertex_shader_id = LoadShader_Vertex("../../src/shader_vertex.glsl");
    GLuint fragment_shader_id = LoadShader_Fragment("../../src/shader_fragment.glsl");

    // Deletamos o programa de GPU anterior, caso ele exista.
    if ( g_GpuProgramID != 0 )
        glDeleteProgram(g_GpuProgramID);

    // Criamos um programa de GPU utilizando os shaders carregados acima.
    g_GpuProgramID = CreateGpuProgram(vertex_shader_id, fragment_shader_id);

    // Buscamos o endereço das variáveis definidas dentro do Vertex Shader.
    // Utilizaremos estas variáveis para enviar dados para a placa de vídeo
    // (GPU)! Veja arquivo "shader_vertex.glsl" e "shader_fragment.glsl".
    g_model_uniform      = glGetUniformLocation(g_GpuProgramID, "model"); // Variável da matriz "model"
    g_view_uniform       = glGetUniformLocation(g_GpuProgramID, "view"); // Variável da matriz "view" em shader_vertex.glsl
    g_projection_uniform = glGetUniformLocation(g_GpuProgramID, "projection"); // Variável da matriz "projection" em shader_vertex.glsl
    g_object_id_uniform  = glGetUniformLocation(g_GpuProgramID, "object_id"); // Variável "object_id" em shader_fragment.glsl
    g_bbox_min_uniform   = glGetUniformLocation(g_GpuProgramID, "bbox_min");
    g_bbox_max_uniform   = glGetUniformLocation(g_GpuProgramID, "bbox_max");

    // Variáveis em "shader_fragment.glsl" para acesso das imagens de textura
    glUseProgram(g_GpuProgramID);
    glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage0"), 0);
    glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage1"), 1);

    /// Variáveis em "shader_fragment.glsl" para acesso das imagens de textura adicionadas
    glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage2"), 2); // Textura da baguete
    glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage3"), 3); // Textura do asfalto
    glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage4"), 4); // Textura do poste
    glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage5"), 5); // Textura do lua
    glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage6"), 6); // Textura do calcada
    glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage7"), 7); // Textura da casa pequena
    glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage8"), 8); // Textura da grama
    glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage9"), 9); // Textura do posto de gasolina
    glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage10"), 10); // Textura do nossa casa
    glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage11"), 11); // Textura de uma das casa
    glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage12"), 12); // Textura de uma da casa de madeira
    glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage13"), 13); // Textura da ultima casa
    glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage14"), 14); // Textura da casa pequena
    glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage15"), 15); // Textura da maquina de pagamento
    glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage16"), 16); // Textura do ceu estrelado
    glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage17"), 17); // Textura do coelho dourado
    glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage18"), 18); // Textura do queijo
    glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage19"), 19); // Textura do queijo
    glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage20"), 20); // Textura do queijo
    glUseProgram(0);
}

// Função que pega a matriz M e guarda a mesma no topo da pilha
void PushMatrix(glm::mat4 M)
{
    g_MatrixStack.push(M);
}

// Função que remove a matriz atualmente no topo da pilha e armazena a mesma na variável M
void PopMatrix(glm::mat4& M)
{
    if ( g_MatrixStack.empty() )
    {
        M = Matrix_Identity();
    }
    else
    {
        M = g_MatrixStack.top();
        g_MatrixStack.pop();
    }
}

// Função que computa as normais de um ObjModel, caso elas não tenham sido
// especificadas dentro do arquivo ".obj"
void ComputeNormals(ObjModel* model)
{
    if ( !model->attrib.normals.empty() )
        return;

    // Primeiro computamos as normais para todos os TRIÂNGULOS.
    // Segundo, computamos as normais dos VÉRTICES através do método proposto
    // por Gouraud, onde a normal de cada vértice vai ser a média das normais de
    // todas as faces que compartilham este vértice.

    size_t num_vertices = model->attrib.vertices.size() / 3;

    std::vector<int> num_triangles_per_vertex(num_vertices, 0);
    std::vector<glm::vec4> vertex_normals(num_vertices, glm::vec4(0.0f,0.0f,0.0f,0.0f));

    for (size_t shape = 0; shape < model->shapes.size(); ++shape)
    {
        size_t num_triangles = model->shapes[shape].mesh.num_face_vertices.size();

        for (size_t triangle = 0; triangle < num_triangles; ++triangle)
        {
            assert(model->shapes[shape].mesh.num_face_vertices[triangle] == 3);

            glm::vec4  vertices[3];
            for (size_t vertex = 0; vertex < 3; ++vertex)
            {
                tinyobj::index_t idx = model->shapes[shape].mesh.indices[3*triangle + vertex];
                const float vx = model->attrib.vertices[3*idx.vertex_index + 0];
                const float vy = model->attrib.vertices[3*idx.vertex_index + 1];
                const float vz = model->attrib.vertices[3*idx.vertex_index + 2];
                vertices[vertex] = glm::vec4(vx,vy,vz,1.0);
            }

            const glm::vec4  a = vertices[0];
            const glm::vec4  b = vertices[1];
            const glm::vec4  c = vertices[2];

            const glm::vec4  n = crossproduct(b-a,c-a);

            for (size_t vertex = 0; vertex < 3; ++vertex)
            {
                tinyobj::index_t idx = model->shapes[shape].mesh.indices[3*triangle + vertex];
                num_triangles_per_vertex[idx.vertex_index] += 1;
                vertex_normals[idx.vertex_index] += n;
                model->shapes[shape].mesh.indices[3*triangle + vertex].normal_index = idx.vertex_index;
            }
        }
    }

    model->attrib.normals.resize( 3*num_vertices );

    for (size_t i = 0; i < vertex_normals.size(); ++i)
    {
        glm::vec4 n = vertex_normals[i] / (float)num_triangles_per_vertex[i];
        n /= norm(n);
        model->attrib.normals[3*i + 0] = n.x;
        model->attrib.normals[3*i + 1] = n.y;
        model->attrib.normals[3*i + 2] = n.z;
    }
}

// Constrói triângulos para futura renderização a partir de um ObjModel.
void BuildTrianglesAndAddToVirtualScene(ObjModel* model)
{
    GLuint vertex_array_object_id;
    glGenVertexArrays(1, &vertex_array_object_id);
    glBindVertexArray(vertex_array_object_id);

    std::vector<GLuint> indices;
    std::vector<float>  model_coefficients;
    std::vector<float>  normal_coefficients;
    std::vector<float>  texture_coefficients;

    for (size_t shape = 0; shape < model->shapes.size(); ++shape)
    {
        size_t first_index = indices.size();
        size_t num_triangles = model->shapes[shape].mesh.num_face_vertices.size();

        const float minval = std::numeric_limits<float>::min();
        const float maxval = std::numeric_limits<float>::max();

        glm::vec3 bbox_min = glm::vec3(maxval,maxval,maxval);
        glm::vec3 bbox_max = glm::vec3(minval,minval,minval);

        for (size_t triangle = 0; triangle < num_triangles; ++triangle)
        {
            assert(model->shapes[shape].mesh.num_face_vertices[triangle] == 3);

            for (size_t vertex = 0; vertex < 3; ++vertex)
            {
                tinyobj::index_t idx = model->shapes[shape].mesh.indices[3*triangle + vertex];

                indices.push_back(first_index + 3*triangle + vertex);

                const float vx = model->attrib.vertices[3*idx.vertex_index + 0];
                const float vy = model->attrib.vertices[3*idx.vertex_index + 1];
                const float vz = model->attrib.vertices[3*idx.vertex_index + 2];
                //printf("tri %d vert %d = (%.2f, %.2f, %.2f)\n", (int)triangle, (int)vertex, vx, vy, vz);
                model_coefficients.push_back( vx ); // X
                model_coefficients.push_back( vy ); // Y
                model_coefficients.push_back( vz ); // Z
                model_coefficients.push_back( 1.0f ); // W

                bbox_min.x = std::min(bbox_min.x, vx);
                bbox_min.y = std::min(bbox_min.y, vy);
                bbox_min.z = std::min(bbox_min.z, vz);
                bbox_max.x = std::max(bbox_max.x, vx);
                bbox_max.y = std::max(bbox_max.y, vy);
                bbox_max.z = std::max(bbox_max.z, vz);

                // Inspecionando o código da tinyobjloader, o aluno Bernardo
                // Sulzbach (2017/1) apontou que a maneira correta de testar se
                // existem normais e coordenadas de textura no ObjModel é
                // comparando se o índice retornado é -1. Fazemos isso abaixo.

                if ( idx.normal_index != -1 )
                {
                    const float nx = model->attrib.normals[3*idx.normal_index + 0];
                    const float ny = model->attrib.normals[3*idx.normal_index + 1];
                    const float nz = model->attrib.normals[3*idx.normal_index + 2];
                    normal_coefficients.push_back( nx ); // X
                    normal_coefficients.push_back( ny ); // Y
                    normal_coefficients.push_back( nz ); // Z
                    normal_coefficients.push_back( 0.0f ); // W
                }

                if ( idx.texcoord_index != -1 )
                {
                    const float u = model->attrib.texcoords[2*idx.texcoord_index + 0];
                    const float v = model->attrib.texcoords[2*idx.texcoord_index + 1];
                    texture_coefficients.push_back( u );
                    texture_coefficients.push_back( v );
                }
            }
        }

        size_t last_index = indices.size() - 1;

        SceneObject theobject;
        theobject.name           = model->shapes[shape].name;
        theobject.first_index    = first_index; // Primeiro índice
        theobject.num_indices    = last_index - first_index + 1; // Número de indices
        theobject.rendering_mode = GL_TRIANGLES;       // Índices correspondem ao tipo de rasterização GL_TRIANGLES.
        theobject.vertex_array_object_id = vertex_array_object_id;

        theobject.bbox_min = bbox_min;
        theobject.bbox_max = bbox_max;

        g_VirtualScene[model->shapes[shape].name] = theobject;
    }

    GLuint VBO_model_coefficients_id;
    glGenBuffers(1, &VBO_model_coefficients_id);
    glBindBuffer(GL_ARRAY_BUFFER, VBO_model_coefficients_id);
    glBufferData(GL_ARRAY_BUFFER, model_coefficients.size() * sizeof(float), NULL, GL_STATIC_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, model_coefficients.size() * sizeof(float), model_coefficients.data());
    GLuint location = 0; // "(location = 0)" em "shader_vertex.glsl"
    GLint  number_of_dimensions = 4; // vec4 em "shader_vertex.glsl"
    glVertexAttribPointer(location, number_of_dimensions, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(location);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    if ( !normal_coefficients.empty() )
    {
        GLuint VBO_normal_coefficients_id;
        glGenBuffers(1, &VBO_normal_coefficients_id);
        glBindBuffer(GL_ARRAY_BUFFER, VBO_normal_coefficients_id);
        glBufferData(GL_ARRAY_BUFFER, normal_coefficients.size() * sizeof(float), NULL, GL_STATIC_DRAW);
        glBufferSubData(GL_ARRAY_BUFFER, 0, normal_coefficients.size() * sizeof(float), normal_coefficients.data());
        location = 1; // "(location = 1)" em "shader_vertex.glsl"
        number_of_dimensions = 4; // vec4 em "shader_vertex.glsl"
        glVertexAttribPointer(location, number_of_dimensions, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(location);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    if ( !texture_coefficients.empty() )
    {
        GLuint VBO_texture_coefficients_id;
        glGenBuffers(1, &VBO_texture_coefficients_id);
        glBindBuffer(GL_ARRAY_BUFFER, VBO_texture_coefficients_id);
        glBufferData(GL_ARRAY_BUFFER, texture_coefficients.size() * sizeof(float), NULL, GL_STATIC_DRAW);
        glBufferSubData(GL_ARRAY_BUFFER, 0, texture_coefficients.size() * sizeof(float), texture_coefficients.data());
        location = 2; // "(location = 1)" em "shader_vertex.glsl"
        number_of_dimensions = 2; // vec2 em "shader_vertex.glsl"
        glVertexAttribPointer(location, number_of_dimensions, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(location);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    GLuint indices_id;
    glGenBuffers(1, &indices_id);

    // "Ligamos" o buffer. Note que o tipo agora é GL_ELEMENT_ARRAY_BUFFER.
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices_id);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), NULL, GL_STATIC_DRAW);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, indices.size() * sizeof(GLuint), indices.data());
    // glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); // XXX Errado!
    //

    // "Desligamos" o VAO, evitando assim que operações posteriores venham a
    // alterar o mesmo. Isso evita bugs.
    glBindVertexArray(0);
}

// Carrega um Vertex Shader de um arquivo GLSL. Veja definição de LoadShader() abaixo.
GLuint LoadShader_Vertex(const char* filename)
{
    // Criamos um identificador (ID) para este shader, informando que o mesmo
    // será aplicado nos vértices.
    GLuint vertex_shader_id = glCreateShader(GL_VERTEX_SHADER);

    // Carregamos e compilamos o shader
    LoadShader(filename, vertex_shader_id);

    // Retorna o ID gerado acima
    return vertex_shader_id;
}

// Carrega um Fragment Shader de um arquivo GLSL . Veja definição de LoadShader() abaixo.
GLuint LoadShader_Fragment(const char* filename)
{
    // Criamos um identificador (ID) para este shader, informando que o mesmo
    // será aplicado nos fragmentos.
    GLuint fragment_shader_id = glCreateShader(GL_FRAGMENT_SHADER);

    // Carregamos e compilamos o shader
    LoadShader(filename, fragment_shader_id);

    // Retorna o ID gerado acima
    return fragment_shader_id;
}

// Função auxilar, utilizada pelas duas funções acima. Carrega código de GPU de
// um arquivo GLSL e faz sua compilação.
void LoadShader(const char* filename, GLuint shader_id)
{
    // Lemos o arquivo de texto indicado pela variável "filename"
    // e colocamos seu conteúdo em memória, apontado pela variável
    // "shader_string".
    std::ifstream file;
    try {
        file.exceptions(std::ifstream::failbit);
        file.open(filename);
    } catch ( std::exception& e ) {
        fprintf(stderr, "ERROR: Cannot open file \"%s\".\n", filename);
        std::exit(EXIT_FAILURE);
    }
    std::stringstream shader;
    shader << file.rdbuf();
    std::string str = shader.str();
    const GLchar* shader_string = str.c_str();
    const GLint   shader_string_length = static_cast<GLint>( str.length() );

    // Define o código do shader GLSL, contido na string "shader_string"
    glShaderSource(shader_id, 1, &shader_string, &shader_string_length);

    // Compila o código do shader GLSL (em tempo de execução)
    glCompileShader(shader_id);

    // Verificamos se ocorreu algum erro ou "warning" durante a compilação
    GLint compiled_ok;
    glGetShaderiv(shader_id, GL_COMPILE_STATUS, &compiled_ok);

    GLint log_length = 0;
    glGetShaderiv(shader_id, GL_INFO_LOG_LENGTH, &log_length);

    // Alocamos memória para guardar o log de compilação.
    // A chamada "new" em C++ é equivalente ao "malloc()" do C.
    GLchar* log = new GLchar[log_length];
    glGetShaderInfoLog(shader_id, log_length, &log_length, log);

    // Imprime no terminal qualquer erro ou "warning" de compilação
    if ( log_length != 0 )
    {
        std::string  output;

        if ( !compiled_ok )
        {
            output += "ERROR: OpenGL compilation of \"";
            output += filename;
            output += "\" failed.\n";
            output += "== Start of compilation log\n";
            output += log;
            output += "== End of compilation log\n";
        }
        else
        {
            output += "WARNING: OpenGL compilation of \"";
            output += filename;
            output += "\".\n";
            output += "== Start of compilation log\n";
            output += log;
            output += "== End of compilation log\n";
        }

        fprintf(stderr, "%s", output.c_str());
    }

    // A chamada "delete" em C++ é equivalente ao "free()" do C
    delete [] log;
}

// Esta função cria um programa de GPU, o qual contém obrigatoriamente um
// Vertex Shader e um Fragment Shader.
GLuint CreateGpuProgram(GLuint vertex_shader_id, GLuint fragment_shader_id)
{
    // Criamos um identificador (ID) para este programa de GPU
    GLuint program_id = glCreateProgram();

    // Definição dos dois shaders GLSL que devem ser executados pelo programa
    glAttachShader(program_id, vertex_shader_id);
    glAttachShader(program_id, fragment_shader_id);

    // Linkagem dos shaders acima ao programa
    glLinkProgram(program_id);

    // Verificamos se ocorreu algum erro durante a linkagem
    GLint linked_ok = GL_FALSE;
    glGetProgramiv(program_id, GL_LINK_STATUS, &linked_ok);

    // Imprime no terminal qualquer erro de linkagem
    if ( linked_ok == GL_FALSE )
    {
        GLint log_length = 0;
        glGetProgramiv(program_id, GL_INFO_LOG_LENGTH, &log_length);

        // Alocamos memória para guardar o log de compilação.
        // A chamada "new" em C++ é equivalente ao "malloc()" do C.
        GLchar* log = new GLchar[log_length];

        glGetProgramInfoLog(program_id, log_length, &log_length, log);

        std::string output;

        output += "ERROR: OpenGL linking of program failed.\n";
        output += "== Start of link log\n";
        output += log;
        output += "\n== End of link log\n";

        // A chamada "delete" em C++ é equivalente ao "free()" do C
        delete [] log;

        fprintf(stderr, "%s", output.c_str());
    }

    // Os "Shader Objects" podem ser marcados para deleção após serem linkados
    glDeleteShader(vertex_shader_id);
    glDeleteShader(fragment_shader_id);

    // Retornamos o ID gerado acima
    return program_id;
}

// Definição da função que será chamada sempre que a janela do sistema
// operacional for redimensionada, por consequência alterando o tamanho do
// "framebuffer" (região de memória onde são armazenados os pixels da imagem).
void FramebufferSizeCallback(GLFWwindow* window, int width, int height)
{
    // Indicamos que queremos renderizar em toda região do framebuffer. A
    // função "glViewport" define o mapeamento das "normalized device
    // coordinates" (NDC) para "pixel coordinates".  Essa é a operação de
    // "Screen Mapping" ou "Viewport Mapping" vista em aula ({+ViewportMapping2+}).
    glViewport(0, 0, width, height);

    // Atualizamos também a razão que define a proporção da janela (largura /
    // altura), a qual será utilizada na definição das matrizes de projeção,
    // tal que não ocorra distorções durante o processo de "Screen Mapping"
    // acima, quando NDC é mapeado para coordenadas de pixels. Veja slides 205-215 do documento Aula_09_Projecoes.pdf.
    //
    // O cast para float é necessário pois números inteiros são arredondados ao
    // serem divididos!
    g_ScreenRatio = (float)width / height;
}

// Função callback chamada sempre que o usuário aperta algum dos botões do mouse
void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
    {
        // Se o usuário pressionou o botão esquerdo do mouse
        glfwGetCursorPos(window, &g_LastCursorPosX, &g_LastCursorPosY);
        g_LeftMouseButtonPressed = true;
     // glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE)
    {
        // Quando o usuário soltar o botão esquerdo do mouse
        g_LeftMouseButtonPressed = false;
     // glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS)
    {
        // Se o usuário pressionou o botão direito do mouse
        glfwGetCursorPos(window, &g_LastCursorPosX, &g_LastCursorPosY);
        g_RightMouseButtonPressed = true;
    }
    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE)
    {
        // Quando o usuário soltar o botão direito do mouse
        g_RightMouseButtonPressed = false;
    }
    if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_PRESS)
    {
        // Se o usuário pressionou o botão do meio do mouse
        glfwGetCursorPos(window, &g_LastCursorPosX, &g_LastCursorPosY);
        g_MiddleMouseButtonPressed = true;
    }
    if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_RELEASE)
    {
        // Quando o usuário soltar o botão do meio do mouse
        g_MiddleMouseButtonPressed = false;
    }
}

// Função callback chamada sempre que o usuário movimentar o cursor do mouse em
// cima da janela OpenGL.
void CursorPosCallback(GLFWwindow* window, double xpos, double ypos)
{
    // Abaixo executamos o seguinte: caso o botão esquerdo do mouse esteja
    // pressionado, computamos quanto que o mouse se movimento desde o último
    // instante de tempo, e usamos esta movimentação para atualizar os
    // parâmetros que definem a posição da câmera dentro da cena virtual.
    // Assim, temos que o usuário consegue controlar a câmera.


    // renovemos a condicinoal para verificar o mouse esquerdo
    // assim temos a camera acompanhando os movimentos do mouse
    // Deslocamento do cursor do mouse em x e y de coordenadas de tela!
        float dx = xpos - g_LastCursorPosX;
        float dy = ypos - g_LastCursorPosY;

        // Atualizamos parâmetros da câmera com os deslocamentos
        g_CameraTheta -= SensibilidadeCamera*dx;
        g_CameraPhi   += SensibilidadeCamera*dy;

        // Em coordenadas esféricas, o ângulo phi deve ficar entre -pi/2 e +pi/2.
        float phimax = 3.141592f/2;
        float phimin = -phimax;

        if (g_CameraPhi > phimax)
            g_CameraPhi = phimax;

        if (g_CameraPhi < phimin)
            g_CameraPhi = phimin;

        // Atualizamos as variáveis globais para armazenar a posição atual do
        // cursor como sendo a última posição conhecida do cursor.
        g_LastCursorPosX = xpos;
        g_LastCursorPosY = ypos;



    if (g_RightMouseButtonPressed)
    {
        // Deslocamento do cursor do mouse em x e y de coordenadas de tela!
        float dx = xpos - g_LastCursorPosX;
        float dy = ypos - g_LastCursorPosY;

        // Atualizamos parâmetros da antebraço com os deslocamentos
        g_ForearmAngleZ -= 0.01f*dx;
        g_ForearmAngleX += 0.01f*dy;

        // Atualizamos as variáveis globais para armazenar a posição atual do
        // cursor como sendo a última posição conhecida do cursor.
        g_LastCursorPosX = xpos;
        g_LastCursorPosY = ypos;
    }

    if (g_MiddleMouseButtonPressed)
    {
        // Deslocamento do cursor do mouse em x e y de coordenadas de tela!
        float dx = xpos - g_LastCursorPosX;
        float dy = ypos - g_LastCursorPosY;

        // Atualizamos parâmetros da antebraço com os deslocamentos
        g_TorsoPositionX += 0.01f*dx;
        g_TorsoPositionY -= 0.01f*dy;

        // Atualizamos as variáveis globais para armazenar a posição atual do
        // cursor como sendo a última posição conhecida do cursor.
        g_LastCursorPosX = xpos;
        g_LastCursorPosY = ypos;
    }
}

// Função callback chamada sempre que o usuário movimenta a "rodinha" do mouse.
void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    // Atualizamos a distância da câmera para a origem utilizando a
    // movimentação da "rodinha", simulando um ZOOM.
    g_CameraDistance -= 0.1f*yoffset;

    // Uma câmera look-at nunca pode estar exatamente "em cima" do ponto para
    // onde ela está olhando, pois isto gera problemas de divisão por zero na
    // definição do sistema de coordenadas da câmera. Isto é, a variável abaixo
    // nunca pode ser zero. Versões anteriores deste código possuíam este bug,
    // o qual foi detectado pelo aluno Vinicius Fraga (2017/2).
    const float verysmallnumber = std::numeric_limits<float>::epsilon();
    if (g_CameraDistance < verysmallnumber)
        g_CameraDistance = verysmallnumber;
}

// Definição da função que será chamada sempre que o usuário pressionar alguma
// tecla do teclado. Veja http://www.glfw.org/docs/latest/input_guide.html#input_key
void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mod)
{
    // =====================
    // Não modifique este loop! Ele é utilizando para correção automatizada dos
    // laboratórios. Deve ser sempre o primeiro comando desta função KeyCallback().
    for (int i = 0; i < 10; ++i)
        if (key == GLFW_KEY_0 + i && action == GLFW_PRESS && mod == GLFW_MOD_SHIFT)
            std::exit(100 + i);
    // =====================

    // Se o usuário pressionar a tecla ESC, fechamos a janela.
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);

    // O código abaixo implementa a seguinte lógica:
    //   Se apertar tecla X       então g_AngleX += delta;
    //   Se apertar tecla shift+X então g_AngleX -= delta;
    //   Se apertar tecla Y       então g_AngleY += delta;
    //   Se apertar tecla shift+Y então g_AngleY -= delta;
    //   Se apertar tecla Z       então g_AngleZ += delta;
    //   Se apertar tecla shift+Z então g_AngleZ -= delta;

    float delta = 3.141592 / 16; // 22.5 graus, em radianos.

    if (key == GLFW_KEY_X && action == GLFW_PRESS)
    {
        g_AngleX += (mod & GLFW_MOD_SHIFT) ? -delta : delta;
    }

    if (key == GLFW_KEY_Y && action == GLFW_PRESS)
    {
        g_AngleY += (mod & GLFW_MOD_SHIFT) ? -delta : delta;
    }
    if (key == GLFW_KEY_Z && action == GLFW_PRESS)
    {
        g_AngleZ += (mod & GLFW_MOD_SHIFT) ? -delta : delta;
    }

    // Se o usuário apertar a tecla espaço, resetamos os ângulos de Euler para zero.
    if (key == GLFW_KEY_SPACE && action == GLFW_PRESS)
    {
        g_AngleX = 0.0f;
        g_AngleY = 0.0f;
        g_AngleZ = 0.0f;
        g_ForearmAngleX = 0.0f;
        g_ForearmAngleZ = 0.0f;
        g_TorsoPositionX = 0.0f;
        g_TorsoPositionY = 0.0f;
    }

    // Se o usuário apertar a tecla P, utilizamos projeção perspectiva.
    if (key == GLFW_KEY_P && action == GLFW_PRESS)
    {
        g_UsePerspectiveProjection = true;
    }

    // Se o usuário apertar a tecla O, utilizamos projeção ortográfica.
    if (key == GLFW_KEY_O && action == GLFW_PRESS)
    {
        g_UsePerspectiveProjection = false;
    }

    // Se o usuário apertar a tecla H, fazemos um "toggle" do texto informativo mostrado na tela.
    if (key == GLFW_KEY_H && action == GLFW_PRESS)
    {
        g_ShowInfoText = !g_ShowInfoText;
    }

    // Se o usuário apertar a tecla R, recarregamos os shaders dos arquivos "shader_fragment.glsl" e "shader_vertex.glsl".
    if (key == GLFW_KEY_R && action == GLFW_PRESS)
    {
        LoadShadersFromFiles();
        fprintf(stdout,"Shaders recarregados!\n");
        fflush(stdout);
    }
    if (key == GLFW_KEY_W)
    {
        if (action == GLFW_PRESS)
            tecla_W_pressionada = true;
        else if (action == GLFW_RELEASE)
            tecla_W_pressionada = false;
    }
    if (key == GLFW_KEY_S)
    {
        if (action == GLFW_PRESS)
            tecla_S_pressionada = true;
        else if (action == GLFW_RELEASE)
            tecla_S_pressionada = false;
    }
    if (key == GLFW_KEY_A)
    {
        if (action == GLFW_PRESS)
            tecla_A_pressionada = true;
        else if (action == GLFW_RELEASE)
            tecla_A_pressionada = false;
    }
    if (key == GLFW_KEY_D)
    {
        if (action == GLFW_PRESS)
            tecla_D_pressionada = true;
        else if (action == GLFW_RELEASE)
            tecla_D_pressionada = false;
    }
    if (key == GLFW_KEY_E)
    {
        if (action == GLFW_PRESS)
            tecla_E_pressionada = true;
        else if (action == GLFW_RELEASE)
            tecla_E_pressionada = false;
    }

    // basicamente um toggle pra montar e desmontar a bike
    if (key == GLFW_KEY_B)
    {
        if(action == GLFW_PRESS)
            if(tecla_B_pressionada)
                tecla_B_pressionada = false;
            else
                tecla_B_pressionada = true;
    }

    if (key == GLFW_KEY_F && action == GLFW_PRESS)
    {
        // Verifica se está em tela cheia
        GLFWmonitor* monitor = glfwGetWindowMonitor(window);

        if (!monitor) // Se não está em tela cheia, coloca em tela cheia
        {
            // Obtém o monitor primário
            monitor = glfwGetPrimaryMonitor();

            // Obtém as informações do modo de vídeo do monitor
            const GLFWvidmode* mode = glfwGetVideoMode(monitor);

            // Muda para tela cheia
            glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
        }
        else // Se está em tela cheia, volta para janela
        {
            // Define um tamanho padrão para a janela
            int width = 800;
            int height = 600;

            // Calcula a posição central na tela
            const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
            int xpos = (mode->width - width) / 2;
            int ypos = (mode->height - height) / 2;

            // Volta para modo janela
            glfwSetWindowMonitor(window, nullptr, xpos, ypos, width, height, 0);
        }

    }
            // Se estiver interagindo com o caixa, aceita input numérico
        if (g_InteractingWithCashier && action == GLFW_PRESS)
        {
            if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9)
            {
                g_InputTroco += (char)(key - GLFW_KEY_0 + '0');
                printf("Tecla pressionada: %c, Input atual: %s\n", (char)(key - GLFW_KEY_0 + '0'), g_InputTroco.c_str());
            }
            else if (key == GLFW_KEY_PERIOD || key == GLFW_KEY_KP_DECIMAL)
            {
                if (g_InputTroco.find('.') == std::string::npos) // Só permite um ponto decimal
                    g_InputTroco += '.';
            }
            else if (key == GLFW_KEY_BACKSPACE)
            {
                if (!g_InputTroco.empty())
                    g_InputTroco.pop_back();
            }
            else if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER)
            {
                // Verifica se o valor está correto
                float input_value = std::stof(g_InputTroco.empty() ? "0" : g_InputTroco);
                g_CorrectChange = g_PlayerMoney - g_TotalPurchaseValue;

                if (fabs(input_value - g_CorrectChange) < 0.01f) // Tolerância de 1 centavo
                {
                    printf("Troco correto!\n");
                    g_HasPaidPurchases = true;
                    g_PaymentCompleted = true;
                    g_InteractingWithCashier = false;
                    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                }
                else
                {
                    printf("Troco incorreto! Tente novamente.\n");
                    g_InputTroco = ""; // Limpa o input para nova tentativa
                }
            }
            else if (key == GLFW_KEY_ESCAPE)
            {
                // Cancela a interação
                g_InteractingWithCashier = false;
                g_InputTroco = "";
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            }
        }
}

// Definimos o callback para impressão de erros da GLFW no terminal
void ErrorCallback(int error, const char* description)
{
    fprintf(stderr, "ERROR: GLFW: %s\n", description);
}

// Esta função recebe um vértice com coordenadas de modelo p_model e passa o
// mesmo por todos os sistemas de coordenadas armazenados nas matrizes model,
// view, e projection; e escreve na tela as matrizes e pontos resultantes
// dessas transformações.
void TextRendering_ShowModelViewProjection(
    GLFWwindow* window,
    glm::mat4 projection,
    glm::mat4 view,
    glm::mat4 model,
    glm::vec4 p_model
)
{
    if ( !g_ShowInfoText )
        return;

    glm::vec4 p_world = model*p_model;
    glm::vec4 p_camera = view*p_world;
    glm::vec4 p_clip = projection*p_camera;
    glm::vec4 p_ndc = p_clip / p_clip.w;

    float pad = TextRendering_LineHeight(window);

    TextRendering_PrintString(window, " Model matrix             Model     In World Coords.", -1.0f, 1.0f-pad, 1.0f);
    TextRendering_PrintMatrixVectorProduct(window, model, p_model, -1.0f, 1.0f-2*pad, 1.0f);

    TextRendering_PrintString(window, "                                        |  ", -1.0f, 1.0f-6*pad, 1.0f);
    TextRendering_PrintString(window, "                            .-----------'  ", -1.0f, 1.0f-7*pad, 1.0f);
    TextRendering_PrintString(window, "                            V              ", -1.0f, 1.0f-8*pad, 1.0f);

    TextRendering_PrintString(window, " View matrix              World     In Camera Coords.", -1.0f, 1.0f-9*pad, 1.0f);
    TextRendering_PrintMatrixVectorProduct(window, view, p_world, -1.0f, 1.0f-10*pad, 1.0f);

    TextRendering_PrintString(window, "                                        |  ", -1.0f, 1.0f-14*pad, 1.0f);
    TextRendering_PrintString(window, "                            .-----------'  ", -1.0f, 1.0f-15*pad, 1.0f);
    TextRendering_PrintString(window, "                            V              ", -1.0f, 1.0f-16*pad, 1.0f);

    TextRendering_PrintString(window, " Projection matrix        Camera                    In NDC", -1.0f, 1.0f-17*pad, 1.0f);
    TextRendering_PrintMatrixVectorProductDivW(window, projection, p_camera, -1.0f, 1.0f-18*pad, 1.0f);

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    glm::vec2 a = glm::vec2(-1, -1);
    glm::vec2 b = glm::vec2(+1, +1);
    glm::vec2 p = glm::vec2( 0,  0);
    glm::vec2 q = glm::vec2(width, height);

    glm::mat4 viewport_mapping = Matrix(
        (q.x - p.x)/(b.x-a.x), 0.0f, 0.0f, (b.x*p.x - a.x*q.x)/(b.x-a.x),
        0.0f, (q.y - p.y)/(b.y-a.y), 0.0f, (b.y*p.y - a.y*q.y)/(b.y-a.y),
        0.0f , 0.0f , 1.0f , 0.0f ,
        0.0f , 0.0f , 0.0f , 1.0f
    );

    TextRendering_PrintString(window, "                                                       |  ", -1.0f, 1.0f-22*pad, 1.0f);
    TextRendering_PrintString(window, "                            .--------------------------'  ", -1.0f, 1.0f-23*pad, 1.0f);
    TextRendering_PrintString(window, "                            V                           ", -1.0f, 1.0f-24*pad, 1.0f);

    TextRendering_PrintString(window, " Viewport matrix           NDC      In Pixel Coords.", -1.0f, 1.0f-25*pad, 1.0f);
    TextRendering_PrintMatrixVectorProductMoreDigits(window, viewport_mapping, p_ndc, -1.0f, 1.0f-26*pad, 1.0f);
}

// Escrevemos na tela os ângulos de Euler definidos nas variáveis globais
// g_AngleX, g_AngleY, e g_AngleZ.
void TextRendering_ShowEulerAngles(GLFWwindow* window)
{
    if ( !g_ShowInfoText )
        return;

    float pad = TextRendering_LineHeight(window);

    char buffer[80];
    snprintf(buffer, 80, "Euler Angles rotation matrix = Z(%.2f)*Y(%.2f)*X(%.2f)\n", g_AngleZ, g_AngleY, g_AngleX);

    TextRendering_PrintString(window, buffer, -1.0f+pad/10, -1.0f+2*pad/10, 1.0f);
}

// Escrevemos na tela qual matriz de projeção está sendo utilizada.
void TextRendering_ShowProjection(GLFWwindow* window)
{
    if ( !g_ShowInfoText )
        return;

    float lineheight = TextRendering_LineHeight(window);
    float charwidth = TextRendering_CharWidth(window);

    if ( g_UsePerspectiveProjection )
        TextRendering_PrintString(window, "Perspective", 1.0f-13*charwidth, -1.0f+2*lineheight/10, 1.0f);
    else
        TextRendering_PrintString(window, "Orthographic", 1.0f-13*charwidth, -1.0f+2*lineheight/10, 1.0f);
}

// Escrevemos na tela o número de quadros renderizados por segundo (frames per
// second).
void TextRendering_ShowFramesPerSecond(GLFWwindow* window)
{
    if ( !g_ShowInfoText )
        return;

    // Variáveis estáticas (static) mantém seus valores entre chamadas
    // subsequentes da função!
    static float old_seconds = (float)glfwGetTime();
    static int   ellapsed_frames = 0;
    static char  buffer[20] = "?? fps";
    static int   numchars = 7;

    ellapsed_frames += 1;

    // Recuperamos o número de segundos que passou desde a execução do programa
    float seconds = (float)glfwGetTime();

    // Número de segundos desde o último cálculo do fps
    float ellapsed_seconds = seconds - old_seconds;

    if ( ellapsed_seconds > 1.0f )
    {
        numchars = snprintf(buffer, 20, "%.2f fps", ellapsed_frames / ellapsed_seconds);

        old_seconds = seconds;
        ellapsed_frames = 0;
    }

    float lineheight = TextRendering_LineHeight(window);
    float charwidth = TextRendering_CharWidth(window);

    TextRendering_PrintString(window, buffer, 1.0f-(numchars + 1)*charwidth, 1.0f-lineheight, 1.0f);
}

/// funções usadas para sortear itens e imprimi-los na tela

void SortearItens(int quantidade)
{
    // Limpa as listas anteriores
    itens_para_comprar.clear();
    itens_pegos.clear();

    // Cria uma cópia da lista original para não modificá-la
    std::vector<std::string> itens_disponiveis = todos_itens;

    // Sorteia 'quantidade' itens
    for(int i = 0; i < quantidade && !itens_disponiveis.empty(); ++i)
    {
        // Gera um índice aleatório
        int indice = rand() % itens_disponiveis.size();

        // Adiciona o item sorteado à lista de compras
        itens_para_comprar.push_back(itens_disponiveis[indice]);
        itens_pegos.push_back(false);  // Marca como não pego ainda

        // Remove o item sorteado da lista de disponíveis
        itens_disponiveis.erase(itens_disponiveis.begin() + indice);
    }
}

void DrawShoppingList(GLFWwindow* window)
{
    float char_width = TextRendering_CharWidth(window);
    float line_height = TextRendering_LineHeight(window);

    // Posição inicial da lista (canto superior direito)
    float x = 0.6f;
    float y = 0.8f;

    // Título da lista
    TextRendering_PrintString(window, "Lista de Compras:", x, y, 1.0f);

    // Imprime cada item com seu preço
    for (size_t i = 0; i < itens_para_comprar.size(); ++i)
    {
        std::string prefix = itens_pegos[i] ? "[x] " : "[ ] ";
        std::string item_text = prefix + itens_para_comprar[i];

        // Se o item foi pego, mostra seu preço
        if (itens_pegos[i])
        {
            char price_text[32];
            float item_price = g_ItemPrices[itens_para_comprar[i]];
            snprintf(price_text, sizeof(price_text), " - R$ %.2f", item_price);
            item_text += price_text;
        }

        TextRendering_PrintString(window, item_text,
                                  x, y - ((i+1) * line_height), 1.0f);
    }

    // Mostra o saldo do jogador
    char money_text[32];
    snprintf(money_text, sizeof(money_text), "Saldo: R$ %.2f", g_PlayerMoney);
    TextRendering_PrintString(window, money_text,
                              x, y - ((itens_para_comprar.size() + 1) * line_height), 1.0f);

    // Desenha o timer logo abaixo do saldo
    int minutos = (int)(tempo_restante / 60.0f);
    int segundos = (int)(tempo_restante) % 60;

    char buffer[32];
    snprintf(buffer, 32, "Tempo: %02d:%02d", minutos, segundos);

    TextRendering_PrintString(window, buffer,
                              x, y - ((itens_para_comprar.size() + 2) * line_height), 1.0f);

    // Se for game over, mostra a mensagem no centro
    if (game_over)
    {
        TextRendering_PrintString(window, "GAME OVER!", -0.2f, 0.0f, 2.0f);
    }
    else if (g_GameWon)
    {
        TextRendering_PrintString(window, "PARABENS! VOCE VENCEU!", -0.4f, 0.0f, 2.0f);
    }
}

/// Função usada para marcar item como "comprado" na lista

void MarcarItemComoPego(int item_id)
{
    std::string nome_item = id_para_nome[item_id];
    for (size_t i = 0; i < itens_para_comprar.size(); ++i)
    {
        if (itens_para_comprar[i] == nome_item)
        {
            itens_pegos[i] = true;
            printf("Item coletado: %s\n", nome_item.c_str());
            break;
        }
    }
}

/// #####################################################################################

// Função para debugging: imprime no terminal todas informações de um modelo
// geométrico carregado de um arquivo ".obj".
// Veja: https://github.com/syoyo/tinyobjloader/blob/22883def8db9ef1f3ffb9b404318e7dd25fdbb51/loader_example.cc#L98
void PrintObjModelInfo(ObjModel* model)
{
  const tinyobj::attrib_t                & attrib    = model->attrib;
  const std::vector<tinyobj::shape_t>    & shapes    = model->shapes;
  const std::vector<tinyobj::material_t> & materials = model->materials;

  printf("# of vertices  : %d\n", (int)(attrib.vertices.size() / 3));
  printf("# of normals   : %d\n", (int)(attrib.normals.size() / 3));
  printf("# of texcoords : %d\n", (int)(attrib.texcoords.size() / 2));
  printf("# of shapes    : %d\n", (int)shapes.size());
  printf("# of materials : %d\n", (int)materials.size());

  for (size_t v = 0; v < attrib.vertices.size() / 3; v++) {
    printf("  v[%ld] = (%f, %f, %f)\n", static_cast<long>(v),
           static_cast<const double>(attrib.vertices[3 * v + 0]),
           static_cast<const double>(attrib.vertices[3 * v + 1]),
           static_cast<const double>(attrib.vertices[3 * v + 2]));
  }

  for (size_t v = 0; v < attrib.normals.size() / 3; v++) {
    printf("  n[%ld] = (%f, %f, %f)\n", static_cast<long>(v),
           static_cast<const double>(attrib.normals[3 * v + 0]),
           static_cast<const double>(attrib.normals[3 * v + 1]),
           static_cast<const double>(attrib.normals[3 * v + 2]));
  }

  for (size_t v = 0; v < attrib.texcoords.size() / 2; v++) {
    printf("  uv[%ld] = (%f, %f)\n", static_cast<long>(v),
           static_cast<const double>(attrib.texcoords[2 * v + 0]),
           static_cast<const double>(attrib.texcoords[2 * v + 1]));
  }

  // For each shape
  for (size_t i = 0; i < shapes.size(); i++) {
    printf("shape[%ld].name = %s\n", static_cast<long>(i),
           shapes[i].name.c_str());
    printf("Size of shape[%ld].indices: %lu\n", static_cast<long>(i),
           static_cast<unsigned long>(shapes[i].mesh.indices.size()));

    size_t index_offset = 0;

    assert(shapes[i].mesh.num_face_vertices.size() ==
           shapes[i].mesh.material_ids.size());

    printf("shape[%ld].num_faces: %lu\n", static_cast<long>(i),
           static_cast<unsigned long>(shapes[i].mesh.num_face_vertices.size()));

    // For each face
    for (size_t f = 0; f < shapes[i].mesh.num_face_vertices.size(); f++) {
      size_t fnum = shapes[i].mesh.num_face_vertices[f];

      printf("  face[%ld].fnum = %ld\n", static_cast<long>(f),
             static_cast<unsigned long>(fnum));

      // For each vertex in the face
      for (size_t v = 0; v < fnum; v++) {
        tinyobj::index_t idx = shapes[i].mesh.indices[index_offset + v];
        printf("    face[%ld].v[%ld].idx = %d/%d/%d\n", static_cast<long>(f),
               static_cast<long>(v), idx.vertex_index, idx.normal_index,
               idx.texcoord_index);
      }

      printf("  face[%ld].material_id = %d\n", static_cast<long>(f),
             shapes[i].mesh.material_ids[f]);

      index_offset += fnum;
    }

    printf("shape[%ld].num_tags: %lu\n", static_cast<long>(i),
           static_cast<unsigned long>(shapes[i].mesh.tags.size()));
    for (size_t t = 0; t < shapes[i].mesh.tags.size(); t++) {
      printf("  tag[%ld] = %s ", static_cast<long>(t),
             shapes[i].mesh.tags[t].name.c_str());
      printf(" ints: [");
      for (size_t j = 0; j < shapes[i].mesh.tags[t].intValues.size(); ++j) {
        printf("%ld", static_cast<long>(shapes[i].mesh.tags[t].intValues[j]));
        if (j < (shapes[i].mesh.tags[t].intValues.size() - 1)) {
          printf(", ");
        }
      }
      printf("]");

      printf(" floats: [");
      for (size_t j = 0; j < shapes[i].mesh.tags[t].floatValues.size(); ++j) {
        printf("%f", static_cast<const double>(
                         shapes[i].mesh.tags[t].floatValues[j]));
        if (j < (shapes[i].mesh.tags[t].floatValues.size() - 1)) {
          printf(", ");
        }
      }
      printf("]");

      printf(" strings: [");
      for (size_t j = 0; j < shapes[i].mesh.tags[t].stringValues.size(); ++j) {
        printf("%s", shapes[i].mesh.tags[t].stringValues[j].c_str());
        if (j < (shapes[i].mesh.tags[t].stringValues.size() - 1)) {
          printf(", ");
        }
      }
      printf("]");
      printf("\n");
    }
  }

  for (size_t i = 0; i < materials.size(); i++) {
    printf("material[%ld].name = %s\n", static_cast<long>(i),
           materials[i].name.c_str());
    printf("  material.Ka = (%f, %f ,%f)\n",
           static_cast<const double>(materials[i].ambient[0]),
           static_cast<const double>(materials[i].ambient[1]),
           static_cast<const double>(materials[i].ambient[2]));
    printf("  material.Kd = (%f, %f ,%f)\n",
           static_cast<const double>(materials[i].diffuse[0]),
           static_cast<const double>(materials[i].diffuse[1]),
           static_cast<const double>(materials[i].diffuse[2]));
    printf("  material.Ks = (%f, %f ,%f)\n",
           static_cast<const double>(materials[i].specular[0]),
           static_cast<const double>(materials[i].specular[1]),
           static_cast<const double>(materials[i].specular[2]));
    printf("  material.Tr = (%f, %f ,%f)\n",
           static_cast<const double>(materials[i].transmittance[0]),
           static_cast<const double>(materials[i].transmittance[1]),
           static_cast<const double>(materials[i].transmittance[2]));
    printf("  material.Ke = (%f, %f ,%f)\n",
           static_cast<const double>(materials[i].emission[0]),
           static_cast<const double>(materials[i].emission[1]),
           static_cast<const double>(materials[i].emission[2]));
    printf("  material.Ns = %f\n",
           static_cast<const double>(materials[i].shininess));
    printf("  material.Ni = %f\n", static_cast<const double>(materials[i].ior));
    printf("  material.dissolve = %f\n",
           static_cast<const double>(materials[i].dissolve));
    printf("  material.illum = %d\n", materials[i].illum);
    printf("  material.map_Ka = %s\n", materials[i].ambient_texname.c_str());
    printf("  material.map_Kd = %s\n", materials[i].diffuse_texname.c_str());
    printf("  material.map_Ks = %s\n", materials[i].specular_texname.c_str());
    printf("  material.map_Ns = %s\n",
           materials[i].specular_highlight_texname.c_str());
    printf("  material.map_bump = %s\n", materials[i].bump_texname.c_str());
    printf("  material.map_d = %s\n", materials[i].alpha_texname.c_str());
    printf("  material.disp = %s\n", materials[i].displacement_texname.c_str());
    printf("  <<PBR>>\n");
    printf("  material.Pr     = %f\n", materials[i].roughness);
    printf("  material.Pm     = %f\n", materials[i].metallic);
    printf("  material.Ps     = %f\n", materials[i].sheen);
    printf("  material.Pc     = %f\n", materials[i].clearcoat_thickness);
    printf("  material.Pcr    = %f\n", materials[i].clearcoat_thickness);
    printf("  material.aniso  = %f\n", materials[i].anisotropy);
    printf("  material.anisor = %f\n", materials[i].anisotropy_rotation);
    printf("  material.map_Ke = %s\n", materials[i].emissive_texname.c_str());
    printf("  material.map_Pr = %s\n", materials[i].roughness_texname.c_str());
    printf("  material.map_Pm = %s\n", materials[i].metallic_texname.c_str());
    printf("  material.map_Ps = %s\n", materials[i].sheen_texname.c_str());
    printf("  material.norm   = %s\n", materials[i].normal_texname.c_str());
    std::map<std::string, std::string>::const_iterator it(
        materials[i].unknown_parameter.begin());
    std::map<std::string, std::string>::const_iterator itEnd(
        materials[i].unknown_parameter.end());

    for (; it != itEnd; it++) {
      printf("  material.%s = %s\n", it->first.c_str(), it->second.c_str());
    }
    printf("\n");
  }
}

// set makeprg=cd\ ..\ &&\ make\ run\ >/dev/null
// vim: set spell spelllang=pt_br :

