#include <glad/glad.h>                         // Carga las funciones modernas de OpenGL.
#include <GLFW/glfw3.h>                        // Crea la ventana y permite leer el teclado.
#include <glm/glm.hpp>                         // Permite usar vectores vec3 y operaciones matemáticas.
#include <glm/gtc/matrix_transform.hpp>        // Permite crear cámara con lookAt y perspectiva.
#include <glm/gtc/type_ptr.hpp>                // Permite enviar matrices/vectores GLM a OpenGL.
#include <algorithm>                           // Permite usar std::clamp para limitar valores.
#include <cmath>                               // Permite usar funciones matemáticas.
#include <fstream>                             // Permite leer los archivos .glsl.
#include <iostream>                            // Permite imprimir errores en consola.
#include <sstream>                             // Permite convertir archivos en cadenas de texto.
#include <string>                              // Permite manejar textos y rutas.
#include <vector>                              // Permite almacenar vértices, índices y bloques.

const unsigned int SCR_WIDTH = 1280;           // Ancho inicial de la ventana.
const unsigned int SCR_HEIGHT = 720;           // Alto inicial de la ventana.
int gWidth = SCR_WIDTH;                        // Ancho actual usado para la proyección.
int gHeight = SCR_HEIGHT;                      // Alto actual usado para la proyección.

const float ROOM_MIN_X = -5.0f;                // Pared izquierda del cuarto.
const float ROOM_MAX_X =  5.0f;                // Pared derecha del cuarto.
const float ROOM_MIN_Y = -3.2f;                // Piso visual del cuarto.
const float ROOM_MAX_Y =  3.3f;                // Techo visual del cuarto.
const float VISIBLE_MIN_Y = -2.62f;             // Límite inferior jugable para que padel y esfera nunca salgan de la cámara.
const float VISIBLE_MAX_Y =  2.62f;             // Límite superior jugable para que padel y esfera siempre permanezcan visibles.
const float ROOM_MIN_Z = -7.0f;                // Pared del fondo del cuarto, ubicada detrás de la pared de bloques.
const float ROOM_MAX_Z =  2.4f;                // Límite frontal del cuarto, cercano al jugador y a la cámara.
const float BLOCK_WALL_Z = -6.5f;             // Plano central donde se colocan los objetos gráficos bloque.
const float BLOCK_SIZE_X = 1.8f;              // Ancho grande de cada objeto gráfico bloque para que se vea casi tres veces más grande que antes.
const float BLOCK_SIZE_Y = 1.30f;              // Alto grande de cada objeto gráfico bloque para llenar desde piso hasta techo.
const float BLOCK_SIZE_Z = 1.00f;              // Profundidad del objeto gráfico bloque para que se perciba claramente en 3D.
const float BLOCK_FRONT_Z = BLOCK_WALL_Z + BLOCK_SIZE_Z * 0.5f; // Cara frontal de la pared de bloques, usada como límite físico.
const float MIN_TRAJECTORY_Z_RATIO = 0.42f;    // Mínimo componente Z de la dirección para evitar rebotes infinitos sólo en piso/techo.
const float MAX_TRAJECTORY_Y_RATIO = 0.68f;    // Máximo componente Y de la dirección para que la esfera no se vuelva casi vertical.

const int INITIAL_LIVES = 5;                   // Vidas iniciales.
const glm::vec3 INITIAL_BALL_POSITION(0.05f, -2.05f, 1.10f); // Posición inicial de la bola.
const glm::vec3 INITIAL_BALL_DIRECTION(0.6f, 1.00f, -2.65f); // Dirección inicial de la bola.
const glm::vec3 INITIAL_PADDLE_POSITION(0.0f, -2.35f, 1.65f); // Posición inicial del padel.

struct Vertex {                                // Vértice para modo retenido.
    glm::vec3 position;                         // Posición local del vértice.
    glm::vec3 normal;                           // Normal local para iluminación Phong.
};                                             // Fin de Vertex.

struct Mesh {                                  // Malla almacenada en GPU.
    GLuint VAO = 0;                             // Vertex Array Object.
    GLuint VBO = 0;                             // Vertex Buffer Object.
    GLuint EBO = 0;                             // Element Buffer Object.
    GLsizei indexCount = 0;                     // Número de índices a dibujar.
};                                             // Fin de Mesh.

struct Transform {                             // Transformaciones de un objeto gráfico.
    glm::vec3 translation = glm::vec3(0.0f);    // Traslación T.
    glm::vec3 rotation = glm::vec3(0.0f);       // Rotación Euler Rx, Ry, Rz.
    glm::vec3 scale = glm::vec3(1.0f);          // Escalamiento S.
    int useAxisAngle = 0;                       // 1 si se usa rotación eje-ángulo.
    glm::vec3 axis = glm::vec3(0, 1, 0);        // Eje para rotación de la esfera.
    float axisAngle = 0.0f;                     // Ángulo de rotación de la esfera.
};                                             // Fin de Transform.

struct Material {                              // Material iluminado.
    glm::vec3 color = glm::vec3(1.0f);          // Color base del objeto gráfico.
    float alpha = 1.0f;                         // Transparencia.
    float shininess = 64.0f;                    // Brillo especular.
    float attenuationScale = 0.06f;             // Control de atenuación por distancia.
    float emissive = 0.0f;                      // Luz propia visual del objeto.
    glm::vec3 accentColor = glm::vec3(1.0f);      // Color secundario usado para degradados y bordes neón.
    int gradientMode = 0;                         // 1 activa degradado especial en el fragment shader.
    int ballMode = 0;
};                                             // Fin de Material.

struct Block {                                 // Objeto gráfico bloque.
    glm::vec3 position;                         // Posición en la pared.
    glm::vec3 size;                             // Escala del bloque.
    glm::vec3 color;                            // Color del bloque.
    bool alive = true;                          // Indica si todavía colisiona.
    bool breaking = false;                      // Indica si desaparece gradualmente.
    float breakTimer = 0.0f;                    // Tiempo restante de desaparición.
};                                             // Fin de Block.

struct Ball {                                  // Esfera del juego.
    glm::vec3 position = INITIAL_BALL_POSITION;             // Posición inicial cerca del padel para iniciar el recorrido hacia los bloques.
    glm::vec3 velocity = glm::normalize(glm::vec3(0.55f, 1.00f, -2.65f)); // Dirección inicial con avance fuerte en Z hacia la pared de bloques.
    float radius = 0.25f;                       // Radio de la esfera.
    float speed = 3.35f;                        // Rapidez constante de la esfera, sin cambiar la fuerza al chocar con el padel.
    glm::vec3 spinAxis = glm::vec3(0, 1, 0);    // Eje de rotación visual.
    float spinAngle = 0.0f;                     // Ángulo acumulado.
};                                             // Fin de Ball.

struct Paddle {                                // Padel controlado por teclado.
    glm::vec3 position = INITIAL_PADDLE_POSITION; // Posición inicial del padel en el frente del cuarto, ajustada para que el nuevo padel más ancho no se pierda abajo.
    glm::vec3 size = glm::vec3(2.00f, 0.80f, 0.55f);  // Tamaño físico del padel, ahora más largo y más ancho para que sea más visible y cómodo de controlar.
    float speed = 4.25f;                         // Velocidad de movimiento para alcanzar también la zona superior del cuarto.
};                                             // Fin de Paddle.

struct Game {                                  // Estado completo del juego.
    Ball ball;                                  // Esfera.
    Paddle paddle;                              // Padel.
    std::vector<Block> blocks;                  // Pared de objetos gráficos bloque.
    int lives = INITIAL_LIVES;                              // Vidas.
    bool victory = false;                       // Estado de victoria.
    bool gameOver = false;                      // Estado de derrota.
    bool resetKeyHeld = false;                  // Tecla de reinicio presionada.
};                                              // Fin de Game.

std::string readFile(const std::string& path) {                    // Lee un archivo de texto.
    std::ifstream file(path);                                      // Abre el archivo.
    if (!file.is_open()) { std::cerr << "No se pudo abrir: " << path << "\n"; return ""; } // Valida apertura.
    std::stringstream ss;                                          // Crea buffer de texto.
    ss << file.rdbuf();                                            // Copia contenido al buffer.
    return ss.str();                                               // Devuelve texto completo.
}                                                                  // Fin de readFile.

GLuint compileShader(GLenum type, const std::string& src, const std::string& name) { // Compila un shader.
    GLuint shader = glCreateShader(type);                          // Crea shader en GPU.
    const char* code = src.c_str();                                 // Obtiene puntero al código.
    glShaderSource(shader, 1, &code, nullptr);                      // Envía código al shader.
    glCompileShader(shader);                                       // Compila el shader.
    GLint ok = GL_FALSE;                                            // Variable de estado.
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);                  // Consulta estado.
    if (!ok) {                                                      // Si hay error.
        GLint len = 0;                                              // Tamaño del log.
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);            // Consulta log.
        std::string log(std::max(len, 1), '\0');                    // Reserva log.
        glGetShaderInfoLog(shader, len, nullptr, &log[0]);          // Obtiene log.
        std::cerr << "Error en " << name << ":\n" << log << "\n";  // Imprime error.
        glDeleteShader(shader);                                     // Borra shader inválido.
        return 0;                                                   // Falla.
    }                                                              // Fin de validación.
    return shader;                                                  // Devuelve shader compilado.
}                                                                  // Fin de compileShader.

GLuint createProgram(const std::string& vsPath, const std::string& fsPath) {   // Crea programa shader.
    GLuint vs = compileShader(GL_VERTEX_SHADER, readFile(vsPath), vsPath);     // Compila vertex shader.
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, readFile(fsPath), fsPath);   // Compila fragment shader.
    if (!vs || !fs) return 0;                                                  // Valida compilación.
    GLuint program = glCreateProgram();                                        // Crea programa.
    glAttachShader(program, vs);                                               // Adjunta vertex shader.
    glAttachShader(program, fs);                                               // Adjunta fragment shader.
    glLinkProgram(program);                                                    // Enlaza programa.
    glDeleteShader(vs);                                                        // Elimina shader temporal.
    glDeleteShader(fs);                                                        // Elimina shader temporal.
    GLint ok = GL_FALSE;                                                       // Estado del enlace.
    glGetProgramiv(program, GL_LINK_STATUS, &ok);                              // Consulta enlace.
    if (!ok) {                                                                 // Si falla.
        GLint len = 0;                                                         // Tamaño del log.
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);                     // Consulta log.
        std::string log(std::max(len, 1), '\0');                               // Reserva log.
        glGetProgramInfoLog(program, len, nullptr, &log[0]);                   // Obtiene log.
        std::cerr << "Error enlazando shaders:\n" << log << "\n";             // Imprime error.
        glDeleteProgram(program);                                              // Borra programa.
        return 0;                                                              // Falla.
    }                                                                          // Fin de validación.
    return program;                                                            // Devuelve programa.
}                                                                              // Fin de createProgram.

void framebufferSizeCallback(GLFWwindow*, int w, int h) {     // Se ejecuta cuando cambia el tamaño de la ventana.
    gWidth = std::max(w, 1);                                  // Actualiza ancho.
    gHeight = std::max(h, 1);                                 // Actualiza alto.
    glViewport(0, 0, gWidth, gHeight);                        // Ajusta área de dibujo.
}                                                            // Fin del callback.

void uploadMesh(Mesh& mesh, const std::vector<Vertex>& v, const std::vector<unsigned int>& i) { // Sube malla a GPU.
    mesh.indexCount = static_cast<GLsizei>(i.size());         // Guarda número de índices.
    glGenVertexArrays(1, &mesh.VAO);                          // Crea VAO.
    glGenBuffers(1, &mesh.VBO);                               // Crea VBO.
    glGenBuffers(1, &mesh.EBO);                               // Crea EBO.
    glBindVertexArray(mesh.VAO);                              // Activa VAO.
    glBindBuffer(GL_ARRAY_BUFFER, mesh.VBO);                   // Activa VBO.
    glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(Vertex), v.data(), GL_STATIC_DRAW); // Copia vértices a GPU una sola vez.
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.EBO);           // Activa EBO.
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, i.size() * sizeof(unsigned int), i.data(), GL_STATIC_DRAW); // Copia índices.
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position)); // Atributo posición.
    glEnableVertexAttribArray(0);                              // Habilita posición.
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));   // Atributo normal.
    glEnableVertexAttribArray(1);                              // Habilita normal.
    glBindVertexArray(0);                                      // Desactiva VAO.
}                                                             // Fin de uploadMesh.

Mesh createCubeMesh() {                                                // Crea un cubo unitario reutilizable.
    Mesh mesh;                                                         // Contenedor de malla.
    std::vector<Vertex> v = {                                          // Vértices con normales por cara.
        {{-0.5f,-0.5f, 0.5f},{0,0,1}}, {{ 0.5f,-0.5f, 0.5f},{0,0,1}}, {{ 0.5f, 0.5f, 0.5f},{0,0,1}}, {{-0.5f, 0.5f, 0.5f},{0,0,1}}, // Frente.
        {{ 0.5f,-0.5f,-0.5f},{0,0,-1}},{{-0.5f,-0.5f,-0.5f},{0,0,-1}},{{-0.5f, 0.5f,-0.5f},{0,0,-1}},{{ 0.5f, 0.5f,-0.5f},{0,0,-1}}, // Fondo.
        {{-0.5f,-0.5f,-0.5f},{-1,0,0}},{{-0.5f,-0.5f, 0.5f},{-1,0,0}},{{-0.5f, 0.5f, 0.5f},{-1,0,0}},{{-0.5f, 0.5f,-0.5f},{-1,0,0}}, // Izquierda.
        {{ 0.5f,-0.5f, 0.5f},{1,0,0}}, {{ 0.5f,-0.5f,-0.5f},{1,0,0}}, {{ 0.5f, 0.5f,-0.5f},{1,0,0}}, {{ 0.5f, 0.5f, 0.5f},{1,0,0}},  // Derecha.
        {{-0.5f, 0.5f, 0.5f},{0,1,0}}, {{ 0.5f, 0.5f, 0.5f},{0,1,0}}, {{ 0.5f, 0.5f,-0.5f},{0,1,0}}, {{-0.5f, 0.5f,-0.5f},{0,1,0}},  // Arriba.
        {{-0.5f,-0.5f,-0.5f},{0,-1,0}},{{ 0.5f,-0.5f,-0.5f},{0,-1,0}},{{ 0.5f,-0.5f, 0.5f},{0,-1,0}},{{-0.5f,-0.5f, 0.5f},{0,-1,0}}  // Abajo.
    };                                                                  // Fin de vértices.
    std::vector<unsigned int> i = {                                      // Índices para 12 triángulos.
        0,1,2,0,2,3, 4,5,6,4,6,7, 8,9,10,8,10,11, 12,13,14,12,14,15, 16,17,18,16,18,19, 20,21,22,20,22,23
    };                                                                  // Fin de índices.
    uploadMesh(mesh, v, i);                                             // Sube cubo a GPU.
    return mesh;                                                        // Devuelve cubo.
}                                                                       // Fin de createCubeMesh.

Mesh createSphereMesh(int stacks, int sectors) {                        // Crea esfera triangulada.
    Mesh mesh;                                                          // Contenedor de malla.
    std::vector<Vertex> v;                                              // Lista de vértices.
    std::vector<unsigned int> idx;                                      // Lista de índices.
    const float PI = 3.14159265359f;                                    // Valor de pi.
    for (int s = 0; s <= stacks; ++s) {                                 // Recorre latitudes.
        float stackAngle = PI * 0.5f - float(s) * PI / float(stacks);   // Ángulo vertical.
        float xy = std::cos(stackAngle);                                // Radio del anillo.
        float z = std::sin(stackAngle);                                 // Altura del anillo.
        for (int k = 0; k <= sectors; ++k) {                            // Recorre longitudes.
            float sectorAngle = float(k) * 2.0f * PI / float(sectors);  // Ángulo horizontal.
            glm::vec3 p(xy * std::cos(sectorAngle), xy * std::sin(sectorAngle), z); // Punto unitario.
            v.push_back({p, glm::normalize(p)});                        // Guarda posición y normal.
        }                                                               // Fin de longitudes.
    }                                                                   // Fin de latitudes.
    for (int s = 0; s < stacks; ++s) {                                  // Recorre franjas.
        int k1 = s * (sectors + 1);                                      // Inicio de franja actual.
        int k2 = k1 + sectors + 1;                                      // Inicio de franja siguiente.
        for (int k = 0; k < sectors; ++k, ++k1, ++k2) {                 // Recorre sectores.
            if (s != 0) { idx.push_back(k1); idx.push_back(k2); idx.push_back(k1 + 1); } // Triángulo superior.
            if (s != stacks - 1) { idx.push_back(k1 + 1); idx.push_back(k2); idx.push_back(k2 + 1); } // Triángulo inferior.
        }                                                               // Fin de sectores.
    }                                                                   // Fin de franjas.
    uploadMesh(mesh, v, idx);                                           // Sube esfera a GPU.
    return mesh;                                                        // Devuelve esfera.
}                                                                       // Fin de createSphereMesh.

void deleteMesh(Mesh& m) {                                               // Libera memoria de GPU.
    glDeleteVertexArrays(1, &m.VAO);                                      // Elimina VAO.
    glDeleteBuffers(1, &m.VBO);                                           // Elimina VBO.
    glDeleteBuffers(1, &m.EBO);                                           // Elimina EBO.
}                                                                        // Fin de deleteMesh.

void setTransform(GLuint p, const Transform& t) {                         // Envía transformaciones al shader de vértices.
    glUniform3fv(glGetUniformLocation(p, "uTranslation"), 1, glm::value_ptr(t.translation)); // Envía traslación.
    glUniform3fv(glGetUniformLocation(p, "uRotation"), 1, glm::value_ptr(t.rotation));       // Envía rotación.
    glUniform3fv(glGetUniformLocation(p, "uScale"), 1, glm::value_ptr(t.scale));             // Envía escala.
    glUniform1i(glGetUniformLocation(p, "uUseAxisAngle"), t.useAxisAngle);                   // Envía modo de rotación.
    glUniform3fv(glGetUniformLocation(p, "uAxis"), 1, glm::value_ptr(t.axis));                // Envía eje.
    glUniform1f(glGetUniformLocation(p, "uAxisAngle"), t.axisAngle);                         // Envía ángulo.
}                                                                        // Fin de setTransform.

void setMaterial(GLuint p, const Material& m) {                           // Envía material al shader de fragmentos.
    glUniform3fv(glGetUniformLocation(p, "uBaseColor"), 1, glm::value_ptr(m.color));          // Envía color.
    glUniform1f(glGetUniformLocation(p, "uAlpha"), m.alpha);                                  // Envía alfa.
    glUniform1f(glGetUniformLocation(p, "uShininess"), m.shininess);                          // Envía brillo.
    glUniform1f(glGetUniformLocation(p, "uAttenuationScale"), m.attenuationScale);            // Envía atenuación.
    glUniform1f(glGetUniformLocation(p, "uEmissive"), m.emissive);                            // Envía emisión.
    glUniform3fv(glGetUniformLocation(p, "uAccentColor"), 1, glm::value_ptr(m.accentColor));     // Envía color de acento para degradados neón.
    glUniform1i(glGetUniformLocation(p, "uGradientMode"), m.gradientMode);                       // Envía modo de degradado para bloques.
    glUniform1i(glGetUniformLocation(p, "uBallMode"), m.ballMode);                             // Envía modo de degradado para bola.
}                                                                        // Fin de setMaterial.

void drawMesh(const Mesh& m) {                                            // Dibuja una malla retenida.
    glBindVertexArray(m.VAO);                                             // Activa VAO.
    glDrawElements(GL_TRIANGLES, m.indexCount, GL_UNSIGNED_INT, nullptr);  // Dibuja con índices del EBO.
    glBindVertexArray(0);                                                 // Desactiva VAO.
}                                                                        // Fin de drawMesh.

void drawObject(GLuint p, const Mesh& mesh, const Transform& t, const Material& m) { // Dibuja un objeto gráfico.
    setTransform(p, t);                                                   // Envía T, R y S.
    setMaterial(p, m);                                                    // Envía material.
    drawMesh(mesh);                                                       // Dibuja la malla.
}                                                                        // Fin de drawObject.

bool sphereAabb(const glm::vec3& c, float r, const glm::vec3& bc, const glm::vec3& bs, glm::vec3& n) { // Colisión esfera-caja.
    glm::vec3 half = bs * 0.5f;                                           // Mitad de caja.
    glm::vec3 closest = glm::clamp(c, bc - half, bc + half);              // Punto más cercano.
    glm::vec3 d = c - closest;                                            // Vector caja-esfera.
    float d2 = glm::dot(d, d);                                            // Distancia cuadrada.
    if (d2 > r * r) return false;                                         // Sin intersección.
    if (d2 > 0.00001f) { n = glm::normalize(d); return true; }            // Normal por punto cercano.
    glm::vec3 local = c - bc;                                             // Posición local.
    glm::vec3 pen = half - glm::abs(local);                               // Penetración por eje.
    if (pen.x < pen.y && pen.x < pen.z) n = glm::vec3(local.x >= 0 ? 1 : -1, 0, 0); // Normal X.
    else if (pen.y < pen.z) n = glm::vec3(0, local.y >= 0 ? 1 : -1, 0);   // Normal Y.
    else n = glm::vec3(0, 0, local.z >= 0 ? 1 : -1);                      // Normal Z.
    return true;                                                          // Hay choque.
}                                                                        // Fin de sphereAabb.

glm::vec3 reflectKeepSpeed(const glm::vec3& v, const glm::vec3& n, float speed) { // Refleja velocidad.
    glm::vec3 r = v - 2.0f * glm::dot(v, n) * n;                          // Fórmula de reflexión real.
    if (glm::length(r) < 0.0001f) r = -v;                                  // Evita vector nulo.
    return glm::normalize(r) * speed;                                      // Conserva la misma fuerza.
}                                                                        // Fin de reflectKeepSpeed.

void stabilizeBallVelocity(Ball& b, int preferredZSign = 0) {              // Corrige trayectorias que se vuelven casi verticales.
    glm::vec3 d = glm::normalize(b.velocity);                              // Convierte la velocidad en dirección unitaria.
    float zSign = preferredZSign != 0 ? float(preferredZSign) : (d.z < 0.0f ? -1.0f : 1.0f); // Elige el sentido Z que debe conservarse.
    if (std::abs(d.z) < MIN_TRAJECTORY_Z_RATIO) {                          // Detecta si la bola casi no avanza hacia padel o bloques.
        d.z = zSign * MIN_TRAJECTORY_Z_RATIO;                              // Fuerza un avance mínimo en profundidad.
    }                                                                       // Termina corrección de Z.
    if (std::abs(d.y) > MAX_TRAJECTORY_Y_RATIO) {                          // Detecta una trayectoria demasiado vertical.
        d.y = (d.y > 0.0f ? 1.0f : -1.0f) * MAX_TRAJECTORY_Y_RATIO;         // Limita el componente vertical para evitar rebote infinito piso-techo.
    }                                                                       // Termina corrección de Y.
    if (glm::length(d) < 0.0001f) {                                         // Valida que la dirección no sea nula.
        d = glm::vec3(0.45f, 0.35f, -1.0f);                                 // Usa una dirección segura si algo degenerado ocurre.
    }                                                                       // Termina validación.
    b.velocity = glm::normalize(d) * b.speed;                               // Reaplica rapidez constante sin cambiar la fuerza de la bola.
}                                                                           // Fin de stabilizeBallVelocity.

//bool blockWallStillVisible(const Game& g) {                                 // Indica si todavía existe la pared de bloques.
//    for (const Block& block : g.blocks) {                                    // Recorre todos los objetos gráficos bloque.
//        if (block.alive || block.breaking) return true;                      // Devuelve verdadero si algún bloque existe o desaparece gradualmente.
//    }                                                                       // Termina recorrido.
//    return false;                                                           // Devuelve falso si ya no queda pared.
//}                                                                           // Fin de blockWallStillVisible.

void createBlocks(Game& g) {                                               // Crea la pared completa de objetos gráficos bloque.
    g.blocks.clear();                                                       // Borra cualquier bloque anterior antes de construir una nueva partida.
    const int rows = 5;                                                     // Número de filas grandes para cubrir casi todo el alto del cuarto.
    const int cols = 5;                                                     // Número de columnas grandes para cubrir casi todo el ancho del cuarto.
    const float gapX = 0.2f;                                                // Separación horizontal mínima para que la pared quede casi cerrada.
    const float gapY = 0.2f;                                                // Separación vertical mínima para cubrir desde piso hasta techo.
    const float totalWidth = cols * BLOCK_SIZE_X + (cols - 1) * gapX;        // Ancho total ocupado por la pared de bloques.
    const float totalHeight = rows * BLOCK_SIZE_Y + (rows - 1) * gapY;       // Alto total ocupado por la pared de bloques.
    const float startX = -totalWidth * 0.5f + BLOCK_SIZE_X * 0.5f;           // Posición X del primer bloque para centrar la pared.
    const float startY = -totalHeight * 0.5f + BLOCK_SIZE_Y * 0.5f;          // Posición Y del primer bloque para cubrir de piso a techo.
    glm::vec3 colors[5] = {                                                 // Paleta de colores neón por fila.
        {1.0f,0.10f,0.12f},                                                 // Fila roja inferior con saturación alta.
        {1.0f,0.62f,0.08f},                                                 // Fila amarilla neón.
        {0.08f,1.00f,0.35f},                                                // Fila verde neón.
        {0.08f,0.55f,1.0f},                                                 // Fila azul/cian intenso.
        {0.92f,0.12f,1.00f}                                                 // Fila magenta superior.
    };                                                                      // Fin de la paleta.
    for (int r = 0; r < rows; ++r) {                                        // Recorre cada fila de la pared.
        for (int c = 0; c < cols; ++c) {                                    // Recorre cada columna de la pared.
            float x = startX + c * (BLOCK_SIZE_X + gapX);                   // Calcula la posición X del objeto gráfico bloque.
            float y = startY + r * (BLOCK_SIZE_Y + gapY);                   // Calcula la posición Y del objeto gráfico bloque.
            glm::vec3 position(x, y, BLOCK_WALL_Z);                         // Define la posición mundial del objeto gráfico bloque.
            glm::vec3 size(BLOCK_SIZE_X, BLOCK_SIZE_Y, BLOCK_SIZE_Z);       // Define el tamaño grande del objeto gráfico bloque.
            g.blocks.push_back({position, size, colors[r], true, false, 0.0f}); // Agrega el objeto gráfico bloque a la lista del juego.
        }                                                                   // Termina una columna.
    }                                                                       // Termina una fila.
}                                                                           // Fin de createBlocks.

void resetBall(Game& g) {                                                   // Reinicia la esfera.
    g.ball.position = INITIAL_BALL_POSITION;                        // Reinicia la esfera cerca del padel.
    g.ball.velocity = glm::normalize(INITIAL_BALL_DIRECTION) * g.ball.speed; // Reinicia con trayectoria real hacia la pared de bloques.
    g.ball.spinAngle = 0.0f;                                                // Reinicia rotación.
}                                                                           // Fin de resetBall.

void resetGame(Game& g) {                                                   // Reinicia la partida.
    g.lives = INITIAL_LIVES;                                                // Reinicia vidas.
    g.victory = false;                                                       // Quita victoria.
    g.gameOver = false;                                                      // Quita derrota.
    g.paddle.position = INITIAL_PADDLE_POSITION;                             // Reinicia el padel
    resetBall(g);                                                           // Reinicia esfera.
    createBlocks(g);                                                        // Reconstruye pared de bloques.
}                                                                           // Fin de resetGame.

void processInput(GLFWwindow* w, Game& g, float dt) {                       // Procesa teclado.
    if (glfwGetKey(w, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(w, true); // ESC cierra.
    
    bool resetPressed = glfwGetKey(w, GLFW_KEY_R) == GLFW_PRESS;             // Guarda tecla de reinicio presionada.
    if (resetPressed && !g.resetKeyHeld){
        resetGame(g);
    }                                                                     // R reinicia.
    g.resetKeyHeld = resetPressed;

    if (g.victory || g.gameOver) return;                                    // No mueve si terminó.
    float d = g.paddle.speed * dt;                                          // Distancia del padel.
    if (glfwGetKey(w, GLFW_KEY_LEFT) == GLFW_PRESS)  g.paddle.position.x -= d; // Flecha izquierda.
    if (glfwGetKey(w, GLFW_KEY_RIGHT) == GLFW_PRESS) g.paddle.position.x += d; // Flecha derecha.
    if (glfwGetKey(w, GLFW_KEY_UP) == GLFW_PRESS)    g.paddle.position.y += d; // Flecha arriba.
    if (glfwGetKey(w, GLFW_KEY_DOWN) == GLFW_PRESS)  g.paddle.position.y -= d; // Flecha abajo.
    g.paddle.position.x = std::clamp(g.paddle.position.x, ROOM_MIN_X + g.paddle.size.x * 0.5f, ROOM_MAX_X - g.paddle.size.x * 0.5f); // Limita X.
    g.paddle.position.y = std::clamp(g.paddle.position.y, ROOM_MIN_Y + g.paddle.size.y * 0.5f, ROOM_MAX_Y - g.paddle.size.y * 0.5f); // Limita Y del padel.
}                                                                           // Fin de processInput.

void updateSpin(Ball& b, float dt) {                                        // Actualiza rotación de la esfera.
    glm::vec3 dir = glm::normalize(b.velocity);                             // Dirección de trayectoria.
    glm::vec3 axis = glm::cross(glm::vec3(0,1,0), dir);                     // Eje perpendicular a trayectoria.
    if (glm::length(axis) < 0.001f) axis = glm::cross(glm::vec3(1,0,0), dir); // Evita eje nulo.
    b.spinAxis = glm::normalize(axis);                                      // Guarda eje normalizado.
    b.spinAngle += (b.speed / b.radius) * dt;                               // Gira en sentido de trayectoria.
}                                                                           // Fin de updateSpin.

void updateGame(Game& g, float dt) {                                        // Actualiza toda la física del juego en cada fotograma.
    if (g.victory || g.gameOver) return;                                    // Detiene la física si la partida ya terminó.
    Ball& b = g.ball;                                                       // Obtiene una referencia corta a la esfera.
    b.position += b.velocity * dt;                                          // Integra la posición usando velocidad por tiempo.
    if (b.position.x - b.radius < ROOM_MIN_X) {                             // Verifica choque con pared izquierda.
        b.position.x = ROOM_MIN_X + b.radius;                               // Coloca la esfera dentro del cuarto.
        b.velocity = reflectKeepSpeed(b.velocity, glm::vec3(1,0,0), b.speed); // Refleja con normal hacia la derecha.
        stabilizeBallVelocity(b);                                           // Evita que la trayectoria quede atrapada verticalmente.
    }                                                                       // Fin de choque con pared izquierda.
    if (b.position.x + b.radius > ROOM_MAX_X) {                             // Verifica choque con pared derecha.
        b.position.x = ROOM_MAX_X - b.radius;                               // Coloca la esfera dentro del cuarto.
        b.velocity = reflectKeepSpeed(b.velocity, glm::vec3(-1,0,0), b.speed); // Refleja con normal hacia la izquierda.
        stabilizeBallVelocity(b);                                           // Mantiene un avance mínimo hacia padel o bloques.
    }                                                                       // Fin de choque con pared derecha.
    if (b.position.y - b.radius < ROOM_MIN_Y) {                          // Verifica choque con el límite inferior visible.
        b.position.y = ROOM_MIN_Y + b.radius;                             // Coloca la esfera dentro del área visible inferior.
        b.velocity = reflectKeepSpeed(b.velocity, glm::vec3(0,1,0), b.speed); // Refleja con normal hacia arriba.
        stabilizeBallVelocity(b);                                           // Corrige el caso donde sólo rebota entre piso y techo.
    }                                                                       // Fin de choque con piso.
    if (b.position.y + b.radius > ROOM_MAX_Y) {                          // Verifica choque con el límite superior visible.
        b.position.y = ROOM_MAX_Y - b.radius;                             // Coloca la esfera dentro del área visible superior.
        b.velocity = reflectKeepSpeed(b.velocity, glm::vec3(0,-1,0), b.speed); // Refleja con normal hacia abajo.
        stabilizeBallVelocity(b);                                           // Corrige el caso donde sólo rebota entre techo y piso.
    }                                                                       // Fin de choque con techo.
    if (b.position.z - b.radius < ROOM_MIN_Z) {                             // Verifica choque con el fondo real del cuarto.
        b.position.z = ROOM_MIN_Z + b.radius;                               // Coloca la esfera delante del fondo.
        b.velocity = reflectKeepSpeed(b.velocity, glm::vec3(0,0,1), b.speed); // Refleja hacia el jugador.
        stabilizeBallVelocity(b, 1);                                        // Fuerza regreso hacia el padel.
    }                                                                       // Fin de choque con fondo.
    if (b.position.z + b.radius > ROOM_MAX_Z) {                             // Verifica si la esfera pasó el frente del cuarto.
        g.lives--;                                                          // Resta una vida porque el jugador no alcanzó la bola.
        if (g.lives <= 0) {                                                 // Revisa si ya no quedan vidas.
            g.gameOver = true;                                              // Activa estado de derrota.
        } else {                                                            // Si todavía quedan vidas.
            resetBall(g);                                                   // Reinicia la esfera para continuar la partida.
        }                                                                   // Fin de validación de vidas.
        return;                                                             // Termina este fotograma para evitar colisiones falsas después de perder vida.
    }                                                                       // Fin del control frontal.
    glm::vec3 n;                                                            // Normal que devuelve la colisión esfera-caja.
    if (b.velocity.z > 0.0f && sphereAabb(b.position, b.radius, g.paddle.position, g.paddle.size, n)) { // Evalúa choque con padel sólo cuando la bola viene hacia el jugador.
        float rx = (b.position.x - g.paddle.position.x) / (g.paddle.size.x * 0.5f); // Calcula impacto relativo horizontal sobre el padel.
        float ry = (b.position.y - g.paddle.position.y) / (g.paddle.size.y * 0.5f); // Calcula impacto relativo vertical sobre el padel.
        glm::vec3 padNormal = glm::normalize(glm::vec3(rx * 0.70f, ry * 0.55f, 1.0f)); // Crea una normal inclinada para rebote real según zona de impacto.
        b.velocity = reflectKeepSpeed(b.velocity, padNormal, b.speed);       // Rebota con la misma fuerza y dirección dependiente del impacto.
        b.position.z = g.paddle.position.z - g.paddle.size.z * 0.5f - b.radius - 0.02f; // Saca la bola al lado de juego, no dentro del padel.
        stabilizeBallVelocity(b, -1);                                       // Fuerza trayectoria de regreso hacia la pared de bloques.
    }                                                                       // Fin de choque con padel.
    bool hitBlock = false;                                                   // Indica si ya se rompió un objeto gráfico bloque en este fotograma.
    for (Block& block : g.blocks) {                                          // Recorre todos los objetos gráficos bloque.
        if (block.breaking) {                                                // Revisa si el bloque está en animación de desaparición.
            block.breakTimer -= dt;                                          // Resta tiempo a los 2 segundos de reducción.
            if (block.breakTimer <= 0.0f) block.breaking = false;            // Termina la animación cuando llega a cero.
            continue;                                                       // Los bloques rompiéndose ya no colisionan.
        }                                                                    // Fin de bloque en desaparición.
        if (!block.alive) continue;                                          // Ignora objetos gráficos bloque ya destruidos.
        if (sphereAabb(b.position, b.radius, block.position, block.size, n)) { // Evalúa choque esfera contra objeto gráfico bloque.
            b.velocity = reflectKeepSpeed(b.velocity, n, b.speed);           // Refleja con la normal real del punto de choque.
            if (n.z > 0.25f) b.position.z = block.position.z + block.size.z * 0.5f + b.radius + 0.02f; // Corrige salida por la cara frontal.
            block.alive = false;                                             // El objeto gráfico bloque deja de ser sólido inmediatamente.
            block.breaking = true;                                           // Activa la desaparición visual gradual.
            block.breakTimer = 2.0f;                                         // Configura dos segundos de reducción hasta desaparecer.
            hitBlock = true;                                                 // Marca que hubo impacto con un bloque.
            stabilizeBallVelocity(b);                                        // Evita que el rebote quede sin avance en profundidad.
            break;                                                          // Rompe sólo un bloque por fotograma para evitar dobles impactos irreales.
        }                                                                    // Fin de choque con bloque.
    }                                                                        // Fin de recorrido de bloques.
    //if (!hitBlock && blockWallStillVisible(g) && b.position.z - b.radius < BLOCK_FRONT_Z) { // Evita que la bola pase detrás de la pared de bloques.
    //    b.position.z = BLOCK_FRONT_Z + b.radius + 0.02f;                     // Coloca la bola delante de la cara frontal de los bloques.
    //    b.velocity = reflectKeepSpeed(b.velocity, glm::vec3(0,0,1), b.speed); // Rebota contra el plano frontal de la pared.
    //    stabilizeBallVelocity(b, 1);                                        // Fuerza regreso hacia el padel.
    //}                                                                        // Fin del muro invisible anti-paso detrás de bloques.
    bool visible = false;                                                    // Variable para saber si quedan objetos gráficos visibles.
    for (const Block& block : g.blocks) {                                    // Recorre todos los bloques.
        if (block.alive || block.breaking) visible = true;                   // Marca visible si está sólido o desapareciendo.
    }                                                                        // Fin de revisión de bloques visibles.
    if (!visible) g.victory = true;                                          // Activa victoria cuando ya no queda ningún bloque visible.
    b.velocity = glm::normalize(b.velocity) * b.speed;                       // Asegura rapidez constante final.
    updateSpin(b, dt);                                                       // Rota visualmente la esfera en sentido de su trayectoria.
}                                                                            // Fin de updateGame.

void title(GLFWwindow* w, const Game& g) {                                    // Actualiza título informativo.
    int left = 0;                                                             // Contador de bloques activos.
    for (const Block& b : g.blocks) if (b.alive) left++;                      // Cuenta bloques restantes.
    std::string t = "Arkanoid 3D - Vidas: " + std::to_string(g.lives) + " - Objetos gráficos: " + std::to_string(left); // Título normal.
    if (g.victory) t = "ARKANOID 3D - VICTORIA - presiona R";                // Título victoria.
    if (g.gameOver) t = "ARKANOID 3D - GAME OVER - presiona R";              // Título derrota.
    glfwSetWindowTitle(w, t.c_str());                                        // Cambia título.
}                                                                            // Fin de title.

void renderRoom(GLuint p, const Mesh& cube) {
    const float wallThickness = 0.05f;
    const float edgeThickness = 0.04f;

    // Dimensiones calculadas a partir de los límites físicos del cuarto.
    const float roomWidth  = ROOM_MAX_X - ROOM_MIN_X;
    const float roomHeight = ROOM_MAX_Y - ROOM_MIN_Y;
    const float roomDepth  = ROOM_MAX_Z - ROOM_MIN_Z;

    // Centro real del cuarto.
    const float centerX = (ROOM_MIN_X + ROOM_MAX_X) * 0.5f;
    const float centerY = (ROOM_MIN_Y + ROOM_MAX_Y) * 0.5f;
    const float centerZ = (ROOM_MIN_Z + ROOM_MAX_Z) * 0.5f;

    Material wall{
        {0.025f, 0.065f, 0.145f},
        0.30f,
        48.0f,
        0.055f,
        0.01f
    };

    // Piso
    Transform floor;
    floor.translation = glm::vec3(
        centerX,
        ROOM_MIN_Y - wallThickness * 0.5f,
        centerZ
    );
    floor.scale = glm::vec3(
        roomWidth,
        wallThickness,
        roomDepth
    );
    drawObject(p, cube, floor, wall);

    // Techo
    Transform ceil;
    ceil.translation = glm::vec3(
        centerX,
        ROOM_MAX_Y + wallThickness * 0.5f,
        centerZ
    );
    ceil.scale = glm::vec3(
        roomWidth,
        wallThickness,
        roomDepth
    );
    drawObject(p, cube, ceil, wall);

    // Pared izquierda
    Transform leftWall;
    leftWall.translation = glm::vec3(
        ROOM_MIN_X - wallThickness * 0.5f,
        centerY,
        centerZ
    );
    leftWall.scale = glm::vec3(
        wallThickness,
        roomHeight,
        roomDepth
    );
    drawObject(p, cube, leftWall, wall);

    // Pared derecha
    Transform rightWall;
    rightWall.translation = glm::vec3(
        ROOM_MAX_X + wallThickness * 0.5f,
        centerY,
        centerZ
    );
    rightWall.scale = glm::vec3(
        wallThickness,
        roomHeight,
        roomDepth
    );
    drawObject(p, cube, rightWall, wall);

    // Pared del fondo
    Transform backWall;
    backWall.translation = glm::vec3(
        centerX,
        centerY,
        ROOM_MIN_Z - wallThickness * 0.5f
    );
    backWall.scale = glm::vec3(
        roomWidth,
        roomHeight,
        wallThickness
    );
    drawObject(p, cube, backWall, wall);

    Material neonMagenta{
        {1.0f, 0.05f, 0.95f},
        0.98f,
        160.0f,
        0.045f,
        0.25f
    };

    Material gridMat{
        {0.00f, 0.55f, 1.00f},  // Azul de la cuadrícula
        1.00f,                   // Transparencia
        100.0f,                  // Brillo especular
        0.020f,                  // Atenuación
        0.25f,                    // Emisión para que sea visible
    };

    float xs[2] = {ROOM_MIN_X, ROOM_MAX_X};
    float ys[2] = {ROOM_MIN_Y, ROOM_MAX_Y};
    float zs[2] = {ROOM_MIN_Z, ROOM_MAX_Z};

    const float gridStep = 1.75f;       // Distancia entre líneas
    const float gridThickness = 0.024f; // Grosor de las líneas
    const float surfaceOffset = 0.0012f; // Evita z-fighting con las paredes

    int floorCols = static_cast<int>(roomWidth / gridStep);
    int floorDepth = static_cast<int>(roomDepth / gridStep);

    // Líneas del piso paralelas al eje Z
    for (float x = ROOM_MIN_X + gridStep; x < ROOM_MAX_X; x += gridStep) {

        Transform line;
        line.translation = glm::vec3(
            x,
            ROOM_MIN_Y + surfaceOffset,
            centerZ
        );

        line.scale = glm::vec3(
            gridThickness,
            0.002f,
            roomDepth
        );

        drawObject(p, cube, line, gridMat);
    }

    // Líneas del piso paralelas al eje X
    for (float z = ROOM_MIN_Z + gridStep; z < ROOM_MAX_Z; z += gridStep) {

        Transform line;
        line.translation = glm::vec3(
            centerX,
            ROOM_MIN_Y + surfaceOffset,
            z
        );

        line.scale = glm::vec3(
            roomWidth,
            0.015f,
            gridThickness
        );

        drawObject(p, cube, line, gridMat);
    }

    int sideRows = static_cast<int>(roomHeight / gridStep);
    int sideDepth = static_cast<int>(roomDepth / gridStep);

    // Líneas horizontales profundas de las paredes laterales
    for (float y = ROOM_MIN_Y + gridStep; y < ROOM_MAX_Y; y += gridStep) {

        Transform leftLine;
        leftLine.translation = glm::vec3(
            ROOM_MIN_X + surfaceOffset,
            y,
            centerZ
        );

        leftLine.scale = glm::vec3(
            0.002f,
            gridThickness,
            roomDepth
        );

        drawObject(p, cube, leftLine, gridMat);

        Transform rightLine;
        rightLine.translation = glm::vec3(
            ROOM_MAX_X - surfaceOffset,
            y,
            centerZ
        );

        rightLine.scale = glm::vec3(
            0.002f,
            gridThickness,
            roomDepth
        );

        drawObject(p, cube, rightLine, gridMat);
    }

    // Líneas verticales de las paredes laterales
    for (float z = ROOM_MIN_Z + gridStep; z < ROOM_MAX_Z; z += gridStep) {

        Transform leftLine;
        leftLine.translation = glm::vec3(
            ROOM_MIN_X + surfaceOffset,
            centerY,
            z
        );

        leftLine.scale = glm::vec3(
            0.02f,
            roomHeight,
            gridThickness
        );

        drawObject(p, cube, leftLine, gridMat);

        Transform rightLine;
        rightLine.translation = glm::vec3(
            ROOM_MAX_X - surfaceOffset,
            centerY,
            z
        );

        rightLine.scale = glm::vec3(
            0.02f,
            roomHeight,
            gridThickness
        );

        drawObject(p, cube, rightLine, gridMat);
    }

    // Número de divisiones para la pared del fondo
    int backCols = static_cast<int>(roomWidth / gridStep);
    int backRows = static_cast<int>(roomHeight / gridStep);

    // Líneas verticales de la pared del fondo
    for (float x = ROOM_MIN_X + gridStep; x < ROOM_MAX_X; x += gridStep) {

        Transform line;
        line.translation = glm::vec3(
            x,
            centerY,
            ROOM_MIN_Z + surfaceOffset
        );

        line.scale = glm::vec3(
            gridThickness,
            roomHeight,
            0.01f
        );

        drawObject(p, cube, line, gridMat);
    }

    // Líneas horizontales de la pared del fondo
    for (float y = ROOM_MIN_Y + gridStep; y < ROOM_MAX_Y; y += gridStep) {

        Transform line;
        line.translation = glm::vec3(
            centerX,
            y,
            ROOM_MIN_Z + surfaceOffset
        );

        line.scale = glm::vec3(
            roomWidth,
            gridThickness,
            0.002f
        );

        drawObject(p, cube, line, gridMat);
    }

    int ceilCols = static_cast<int>(roomWidth / gridStep);
    int ceilDepth = static_cast<int>(roomDepth / gridStep);

    // Líneas del techo paralelas al eje Z
    for (int i = 1; i < ceilCols; ++i) {
        float x = ROOM_MIN_X + i * gridStep;

        Transform line;
        line.translation = glm::vec3(
            x,
            ROOM_MAX_Y - surfaceOffset,
            centerZ
        );

        line.scale = glm::vec3(
            gridThickness,
            0.002f,
            roomDepth
        );

        drawObject(p, cube, line, gridMat);
    }

    // Líneas del techo paralelas al eje X
    for (int k = 1; k < ceilDepth; ++k) {
        float z = ROOM_MIN_Z + k * gridStep;

        Transform line;
        line.translation = glm::vec3(
            centerX,
            ROOM_MAX_Y - surfaceOffset,
            z
        );

        line.scale = glm::vec3(
            roomWidth,
            0.03f,
            gridThickness
        );

        drawObject(p, cube, line, gridMat);
    }

    // Aristas paralelas al eje X.
    for (int yi = 0; yi < 2; ++yi) {
        for (int zi = 0; zi < 2; ++zi) {
            Transform edge;

            edge.translation = glm::vec3(
                centerX,
                ys[yi],
                zs[zi]
            );

            edge.scale = glm::vec3(
                roomWidth + edgeThickness,
                edgeThickness,
                edgeThickness
            );

            drawObject(
                p,
                cube,
                edge,
                neonMagenta
            );
        }
    }

    // Aristas paralelas al eje Y.
    for (int xi = 0; xi < 2; ++xi) {
        for (int zi = 0; zi < 2; ++zi) {
            Transform edge;

            edge.translation = glm::vec3(
                xs[xi],
                centerY,
                zs[zi]
            );

            edge.scale = glm::vec3(
                edgeThickness,
                roomHeight + edgeThickness,
                edgeThickness
            );

            drawObject(p, cube, edge, neonMagenta);
        }
    }

    // Aristas paralelas al eje Z.
    for (int xi = 0; xi < 2; ++xi) {
        for (int yi = 0; yi < 2; ++yi) {
            Transform edge;

            edge.translation = glm::vec3(
                xs[xi],
                ys[yi],
                centerZ
            );

            edge.scale = glm::vec3(
                edgeThickness,
                edgeThickness,
                roomDepth + edgeThickness
            );

            drawObject(
                p,
                cube,
                edge,
                neonMagenta
            );
        }
    }
}                                                                           // Fin de renderRoom.

void renderBlocks(GLuint p, const Mesh& cube, const Game& g) {                // Renderiza la pared de objetos gráficos bloque con aspecto neón y más profundidad visual.
    for (const Block& b : g.blocks) {                                         // Recorre cada objeto gráfico bloque de la pared.
        if (!b.alive && !b.breaking) continue;                                // Omite el objeto gráfico cuando ya desapareció por completo.
        float f = b.alive ? 1.0f : std::clamp(b.breakTimer / 2.0f, 0.0f, 1.0f); // Calcula el factor de reducción gradual durante dos segundos.
        glm::vec3 accent = glm::min(b.color + glm::vec3(0.55f), glm::vec3(1.0f)); // Calcula un color de acento más claro para el brillo neón.
        Transform body;                                                       // Crea la transformación del cuerpo principal del objeto gráfico bloque.
        body.translation = b.position;                                        // Coloca el bloque en su posición dentro de la pared.
        body.scale = b.size * f;                                              // Reduce el bloque suavemente cuando está desapareciendo.
        Material bodyMat;                                                     // Crea el material principal con degradado.
        bodyMat.color = b.color;                                              // Asigna el color base de la fila del bloque.
        bodyMat.alpha = 1.0f;                                                    // Usa el factor de desaparición como transparencia.
        bodyMat.shininess = 180.0f;                                           // Usa alto brillo especular para aspecto plástico/cristal neón.
        bodyMat.attenuationScale = 0.015f;                                    // Mantiene iluminación sensible a la distancia de la esfera.
        bodyMat.emissive = 0.06f * f;                                         // Agrega una emisión leve para que el bloque no se vea plano.
        bodyMat.accentColor = b.color;                                         // Envía el color secundario al shader para el degradado.
        bodyMat.gradientMode = 0;                                             // Activa el modo de degradado especial en el fragment shader.
        drawObject(p, cube, body, bodyMat);                                   // Dibuja el cuerpo principal del objeto gráfico bloque.

        //Transform frontPanel;                                                 // Crea una placa frontal más pequeña para simular bisel 3D.
        //frontPanel.translation = b.position + glm::vec3(0.0f, 0.0f, b.size.z * 0.515f * f); // Ubica la placa en la cara frontal del bloque.
        //frontPanel.scale = glm::vec3(b.size.x * 0.78f * f, b.size.y * 0.58f * f, 0.035f * f); // Reduce la placa para dejar bordes visibles.
        //Material panelMat;                                                    // Crea material para la placa frontal brillante.
        //panelMat.color = glm::mix(b.color, accent, 0.48f);                    // Mezcla color base con acento para crear degradado visual.
        //panelMat.alpha = 0.72f * f;                                           // Hace la placa ligeramente translúcida.
        //panelMat.shininess = 220.0f;                                          // Aumenta brillo para reflejo fuerte.
        //panelMat.attenuationScale = 0.038f;                                   // Aplica atenuación por distancia de la bola.
        //panelMat.emissive = 0.04f * f;                                        // Agrega emisión para efecto neón.
        //panelMat.accentColor = glm::vec3(1.0f);                               // Usa blanco como acento especular del centro.
        //panelMat.gradientMode = 1;                                            // Activa degradado también en la placa frontal.
        //drawObject(p, cube, frontPanel, panelMat);                            // Dibuja la placa frontal del bloque.

        Material neonEdge;                                                    // Crea material para bordes neón físicos del bloque.
        neonEdge.color = accent;                                              // Usa color claro como borde luminoso.
        neonEdge.alpha = 0.65f * f;                                           // Mantiene los bordes visibles durante la desaparición.
        neonEdge.shininess = 220.0f;                                          // Usa brillo máximo para bordes intensos.
        neonEdge.attenuationScale = 0.015f;                                   // Permite que la bola ilumine el borde según distancia.
        neonEdge.emissive = 0.08f * f;                                        // Hace que los bordes tengan luz propia neón.
        neonEdge.accentColor = accent;                               // Define blanco como acento del borde.
        neonEdge.gradientMode = 0;                                            // Desactiva degradado porque el borde ya es una pieza luminosa.

        //Transform topEdge;                                                    // Crea borde superior del bloque.
        //topEdge.translation = b.position + glm::vec3(0.0f, b.size.y * 0.49f * f, b.size.z * 0.55f * f); // Ubica borde superior en la cara frontal.
        //topEdge.scale = glm::vec3(b.size.x * 0.88f * f, 0.045f * f, 0.075f * f); // Escala el borde superior como barra delgada.
        //drawObject(p, cube, topEdge, neonEdge);                               // Dibuja borde superior.
//
        //Transform bottomEdge;                                                 // Crea borde inferior del bloque.
        //bottomEdge.translation = b.position + glm::vec3(0.0f, -b.size.y * 0.49f * f, b.size.z * 0.55f * f); // Ubica borde inferior en la cara frontal.
        //bottomEdge.scale = glm::vec3(b.size.x * 0.88f * f, 0.045f * f, 0.075f * f); // Escala el borde inferior como barra delgada.
        //drawObject(p, cube, bottomEdge, neonEdge);                            // Dibuja borde inferior.
//
        //Transform leftEdge;                                                   // Crea borde izquierdo del bloque.
        //leftEdge.translation = b.position + glm::vec3(-b.size.x * 0.49f * f, 0.0f, b.size.z * 0.55f * f); // Ubica borde izquierdo en la cara frontal.
        //leftEdge.scale = glm::vec3(0.050f * f, b.size.y * 0.78f * f, 0.075f * f); // Escala el borde izquierdo como barra vertical.
        //drawObject(p, cube, leftEdge, neonEdge);                              // Dibuja borde izquierdo.
//
        //Transform rightEdge;                                                  // Crea borde derecho del bloque.
        //rightEdge.translation = b.position + glm::vec3(b.size.x * 0.49f * f, 0.0f, b.size.z * 0.55f * f); // Ubica borde derecho en la cara frontal.
        //rightEdge.scale = glm::vec3(0.050f * f, b.size.y * 0.78f * f, 0.075f * f); // Escala el borde derecho como barra vertical.
        //drawObject(p, cube, rightEdge, neonEdge);                             // Dibuja borde derecho.

        //Material frame;                                                       // Crea material para contorno de alambre final.
        //frame.color = glm::vec3(0.92f, 0.96f, 1.0f);                                        // Usa blanco para remarcar aristas de volumen.
        //frame.alpha = 0.35f * f;                                              // Baja opacidad para no tapar el degradado.
        //frame.shininess = 280.0f;                                             // Usa brillo alto en el contorno.
        //frame.attenuationScale = 0.030f;                                      // Mantiene atenuación por distancia.
        //frame.emissive = 0.02f * f;                                           // Da emisión al contorno.
        //frame.accentColor = glm::vec3(1.0f);                                  // Define acento blanco.
        //frame.gradientMode = 0;                                               // Desactiva degradado del contorno.
        //glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);                            // Cambia a modo alambre para mostrar la estructura 3D.
        //drawObject(p, cube, body, frame);                                     // Dibuja el marco del modelo sobre el bloque.
        //glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);                            // Regresa a modo relleno normal.
    }                                                                         // Termina recorrido de objetos gráficos bloque.
}                                                                             // Fin de renderBlocks.

//void renderPaddle(GLuint p, const Mesh& cube, const Game& g) {                 // Renderiza el padel rediseñado con extremos verdes y centro transparente.
//    Material glass;                                                           // Crea el material del centro transparente del padel.
//    glass.color = glm::vec3(0.00f, 0.78f, 0.86f);                              // Usa un tono cian verdoso para que el centro se vea como vidrio tecnológico.
//    glass.alpha = 0.30f;                                                       // Hace el centro transparente para que no se vea sólido ni pesado.
//    glass.shininess = 220.0f;                                                  // Aumenta el brillo especular para que el centro parezca cristal o acrílico.
//    glass.attenuationScale = 0.040f;                                           // Mantiene la iluminación de la esfera con caída suave por distancia.
//    glass.emissive = 0.02f;                                                    // Agrega una emisión leve para que el centro no desaparezca en zonas oscuras.
//    glass.accentColor = glm::vec3(0.0f, 1.0f, 0.75f);                           // Define un color de acento verde-cian para el material transparente.
//    glass.gradientMode = 0;                                                    // Desactiva el degradado de bloques porque el padel usa un material simple.
//
//    Material neonGreen;                                                        // Crea el material verde neón para los extremos y el marco del padel.
//    neonGreen.color = glm::vec3(0.00f, 1.00f, 0.35f);                           // Usa verde intenso en lugar de rosado para cumplir el rediseño solicitado.
//    neonGreen.alpha = 1.00f;                                                   // Mantiene los extremos completamente opacos para que funcionen como tapas visibles.
//    neonGreen.shininess = 240.0f;                                              // Da alto brillo a las tapas verdes para reforzar el estilo neón.
//    neonGreen.attenuationScale = 0.040f;                                       // Aplica la misma atenuación por distancia que al resto del padel.
//    neonGreen.emissive = 0.12f;                                                // Agrega emisión fuerte para que los extremos verdes destaquen en el cuarto oscuro.
//    neonGreen.accentColor = glm::vec3(0.45f, 1.0f, 0.70f);                      // Define un verde más claro como color secundario del material.
//    neonGreen.gradientMode = 0;                                                // Desactiva degradado porque las tapas son piezas sólidas.
//
//    Transform centerPanel;                                                     // Crea la transformación del panel central transparente.
//    centerPanel.translation = g.paddle.position;                               // Coloca el centro exactamente en la posición física del padel.
//    centerPanel.scale = glm::vec3(g.paddle.size.x * 0.86f, g.paddle.size.y, g.paddle.size.z); // Hace el centro más largo y ancho, dejando espacio para las tapas verdes.
//    glDepthMask(GL_FALSE);                                                     // Evita que el vidrio transparente oculte incorrectamente la bola dibujada después.
//    drawObject(p, cube, centerPanel, glass);                                   // Dibuja el centro transparente del padel.
//    glDepthMask(GL_TRUE);                                                      // Reactiva la escritura de profundidad para las piezas opacas siguientes.
//
//    Transform leftCap;                                                         // Crea la transformación del extremo izquierdo del padel.
//    leftCap.translation = g.paddle.position + glm::vec3(-g.paddle.size.x * 0.50f, 0.0f, 0.0f); // Ubica la tapa izquierda en el borde izquierdo.
//    leftCap.scale = glm::vec3(0.24f, g.paddle.size.y * 1.12f, g.paddle.size.z * 1.12f); // Hace la tapa izquierda más gruesa que antes para verse robusta.
//    drawObject(p, cube, leftCap, neonGreen);                                   // Dibuja el extremo izquierdo en verde neón.
//
//    Transform rightCap;                                                        // Crea la transformación del extremo derecho del padel.
//    rightCap.translation = g.paddle.position + glm::vec3(g.paddle.size.x * 0.50f, 0.0f, 0.0f); // Ubica la tapa derecha en el borde derecho.
//    rightCap.scale = glm::vec3(0.24f, g.paddle.size.y * 1.12f, g.paddle.size.z * 1.12f); // Hace la tapa derecha igual de robusta que la izquierda.
//    drawObject(p, cube, rightCap, neonGreen);                                  // Dibuja el extremo derecho en verde neón.
//
//    Transform topFrame;                                                        // Crea la barra superior del marco verde del padel.
//    topFrame.translation = g.paddle.position + glm::vec3(0.0f, g.paddle.size.y * 0.56f, 0.0f); // Ubica el borde superior del marco.
//    topFrame.scale = glm::vec3(g.paddle.size.x * 0.94f, 0.045f, g.paddle.size.z * 1.08f); // Hace una barra delgada que marca el límite superior sin crear líneas celestes.
//    drawObject(p, cube, topFrame, neonGreen);                                  // Dibuja la barra superior verde.
//
//    Transform bottomFrame;                                                     // Crea la barra inferior del marco verde del padel.
//    bottomFrame.translation = g.paddle.position + glm::vec3(0.0f, -g.paddle.size.y * 0.56f, 0.0f); // Ubica el borde inferior del marco.
//    bottomFrame.scale = glm::vec3(g.paddle.size.x * 0.94f, 0.045f, g.paddle.size.z * 1.08f); // Hace una barra inferior delgada para cerrar el marco.
//    drawObject(p, cube, bottomFrame, neonGreen);                               // Dibuja la barra inferior verde.
//
//    //Transform frontGlassEdge;                                                  // Crea un borde frontal muy delgado para remarcar profundidad 3D.
//    //frontGlassEdge.translation = g.paddle.position + glm::vec3(0.0f, 0.0f, g.paddle.size.z * 0.56f); // Ubica el borde en la cara frontal mirando a la cámara.
//    //frontGlassEdge.scale = glm::vec3(g.paddle.size.x * 0.82f, g.paddle.size.y * 0.92f, 0.030f); // Crea una placa frontal delgada, no una línea horizontal.
//    //glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);                                 // Cambia a modo alambre para mostrar sólo el contorno del centro transparente.
//    //drawObject(p, cube, frontGlassEdge, neonGreen);                            // Dibuja el contorno frontal en verde neón.
//    //glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);                                 // Regresa a modo relleno para el resto de objetos gráficos.
//}                                                                            // Fin de renderPaddle.

void renderPaddle(GLuint p, const Mesh& cube, const Game& g) {
    const float sideThickness = 0.22f;    // grosor lateral
    const float topThickness  = 0.07f;    // grosor arriba/abajo

    Material shell;
    shell.color = glm::vec3(0.35f, 0.18f, 1.00f);   // morado azulado
    shell.alpha = 0.95f;
    shell.shininess = 260.0f;
    shell.attenuationScale = 0.030f;
    shell.emissive = 0.04f;
    shell.accentColor = shell.color;
    shell.gradientMode = 0;

    Material glass;
    glass.color = glm::vec3(0.55f, 0.75f, 1.00f);   // azul claro tipo cristal
    glass.alpha = 0.18f;                            // transparencia del centro
    glass.shininess = 280.0f;
    glass.attenuationScale = 0.028f;
    glass.emissive = 0.015f;
    glass.accentColor = glass.color;
    glass.gradientMode = 0;

    // Barra lateral izquierda
    Transform leftBar;
    leftBar.translation = g.paddle.position + glm::vec3(-(g.paddle.size.x - sideThickness) * 0.5f, 0.0f, 0.0f);
    leftBar.scale = glm::vec3(sideThickness, g.paddle.size.y, g.paddle.size.z);
    drawObject(p, cube, leftBar, shell);

    // Barra lateral derecha
    Transform rightBar;
    rightBar.translation = g.paddle.position + glm::vec3((g.paddle.size.x - sideThickness) * 0.5f, 0.0f, 0.0f);
    rightBar.scale = glm::vec3(sideThickness, g.paddle.size.y, g.paddle.size.z);
    drawObject(p, cube, rightBar, shell);

    // Barra superior
    Transform topBar;
    topBar.translation = g.paddle.position + glm::vec3(0.0f, (g.paddle.size.y - topThickness) * 0.5f, 0.0f);
    topBar.scale = glm::vec3(g.paddle.size.x - 2.0f * sideThickness, topThickness, g.paddle.size.z);
    drawObject(p, cube, topBar, shell);

    // Barra inferior
    Transform bottomBar;
    bottomBar.translation = g.paddle.position + glm::vec3(0.0f, -(g.paddle.size.y - topThickness) * 0.5f, 0.0f);
    bottomBar.scale = glm::vec3(g.paddle.size.x - 2.0f * sideThickness, topThickness, g.paddle.size.z);
    drawObject(p, cube, bottomBar, shell);

    // Centro transparente
    Transform centerGlass;
    centerGlass.translation = g.paddle.position;
    centerGlass.scale = glm::vec3(
        g.paddle.size.x - 2.0f * sideThickness,
        g.paddle.size.y - 2.0f * topThickness,
        g.paddle.size.z * 0.78f
    );

    glDepthMask(GL_FALSE);
    drawObject(p, cube, centerGlass, glass);
    glDepthMask(GL_TRUE);
}

void renderBall(GLuint p, const Mesh& sphere, const Game& g) {                 // Renderiza esfera/luz.
    Transform t;                                                              // Transformación de esfera.
    t.translation=g.ball.position;                                            // Traslación.
    t.scale=glm::vec3(g.ball.radius);                                         // Escala por radio.
    t.useAxisAngle=1;                                                         // Activa rotación eje-ángulo.
    t.axis=g.ball.spinAxis;                                                   // Eje de trayectoria.
    t.axisAngle=g.ball.spinAngle;                                             // Ángulo acumulado.
    Material m{{0.08f,0.90f,1.0f},1.0f,256.0f,0.05f,1.15f};                 // Material emisivo.
    m.ballMode=1;                                                             // Modo de degradado para bola.
    drawObject(p,sphere,t,m);                                                 // Dibuja esfera.
}                                                                             // Fin de renderBall.

std::vector<std::string> glyph(char ch) {                                // Devuelve una letra en matriz 5x7 para dibujar texto sin librerías externas.
    switch (ch) {                                                           // Selecciona el patrón según el carácter.
        case 'A': return {"01110","10001","10001","11111","10001","10001","10001"}; // Letra A.
        case 'C': return {"01111","10000","10000","10000","10000","10000","01111"}; // Letra C.
        case 'E': return {"11111","10000","10000","11110","10000","10000","11111"}; // Letra E.
        case 'G': return {"01111","10000","10000","10111","10001","10001","01110"}; // Letra G.
        case 'I': return {"11111","00100","00100","00100","00100","00100","11111"}; // Letra I.
        case 'M': return {"10001","11011","10101","10101","10001","10001","10001"}; // Letra M.
        case 'N': return {"10001","11001","10101","10011","10001","10001","10001"}; // Letra N.
        case 'O': return {"01110","10001","10001","10001","10001","10001","01110"}; // Letra O.
        case 'P': return {"11110","10001","10001","11110","10000","10000","10000"}; // Letra P.
        case 'R': return {"11110","10001","10001","11110","10100","10010","10001"}; // Letra R.
        case 'S': return {"01111","10000","10000","01110","00001","00001","11110"}; // Letra S.
        case 'T': return {"11111","00100","00100","00100","00100","00100","00100"}; // Letra T.
        case 'V': return {"10001","10001","10001","10001","10001","01010","00100"}; // Letra V.
        default:  return {"00000","00000","00000","00000","00000","00000","00000"}; // Espacio u otro carácter.
    }                                                                       // Fin de selección.
}                                                                           // Fin de glyph.

void renderBitmapText(GLuint p, const Mesh& cube, const std::string& text, glm::vec3 center, float pixel, const Material& mat) { // Dibuja texto con cubos pequeños.
    const float charWidth = pixel * 6.0f;                                   // Calcula ancho por carácter incluyendo separación.
    const float totalWidth = charWidth * float(text.size());                // Calcula ancho total de la frase.
    glm::vec3 start = center + glm::vec3(-totalWidth * 0.5f, pixel * 3.0f, 0.0f); // Calcula esquina inicial para centrar el texto.
    for (size_t i = 0; i < text.size(); ++i) {                              // Recorre cada carácter de la frase.
        std::vector<std::string> g = glyph(text[i]);                        // Obtiene patrón 5x7 del carácter.
        for (int row = 0; row < 7; ++row) {                                 // Recorre filas del patrón.
            for (int col = 0; col < 5; ++col) {                             // Recorre columnas del patrón.
                if (g[row][col] != '1') continue;                           // Omite celdas apagadas.
                Transform t;                                                // Crea transformación para un pixel de texto.
                t.translation = start + glm::vec3(float(i) * charWidth + float(col) * pixel, -float(row) * pixel, 0.0f); // Ubica pixel de texto.
                t.scale = glm::vec3(pixel * 0.82f, pixel * 0.82f, 0.035f);   // Escala el pixel como cubo delgado.
                drawObject(p, cube, t, mat);                                // Dibuja el pixel encendido.
            }                                                               // Termina columnas.
        }                                                                   // Termina filas.
    }                                                                       // Termina caracteres.
}                                                                           // Fin de renderBitmapText.

void renderBitmapChar(GLuint p, const Mesh& cube, char ch, glm::vec3 pos, float pixel, const Material& mat) {
    std::vector<std::string> g = glyph(ch);

    for (int row = 0; row < 7; ++row) {
        for (int col = 0; col < 5; ++col) {
            if (g[row][col] != '1') continue;

            Transform t;
            t.translation = pos + glm::vec3(float(col) * pixel, -float(row) * pixel, 0.0f);
            t.scale = glm::vec3(pixel * 0.82f, pixel * 0.82f, 0.035f);
            drawObject(p, cube, t, mat);
        }
    }
}

void renderNeonWord(GLuint p,
                    const Mesh& cube,
                    const std::string& word,
                    glm::vec3 start,
                    float pixel,
                    const std::vector<Material>& mats) {
    float charStep = pixel * 6.0f; // ancho por letra + espacio

    for (size_t i = 0; i < word.size(); ++i) {
        renderBitmapChar(
            p,
            cube,
            word[i],
            start + glm::vec3(float(i) * charStep, 0.0f, 0.0f),
            pixel,
            mats[i]
        );
    }
}

//void renderFinalPanel(GLuint p, const Mesh& cube, const Game& g) {             // Renderiza mensaje claro de victoria o derrota.
//    if (!g.victory && !g.gameOver) return;                                  // Sólo dibuja si el juego terminó.
//    glm::vec3 panelColor = g.victory ? glm::vec3(0.02f,0.85f,0.35f) : glm::vec3(1.0f,0.06f,0.08f); // Selecciona verde para victoria o rojo para derrota.
//    Material panelMat{panelColor, 0.78f, 160.0f, 0.05f, 0.30f};             // Crea material translúcido para el panel.
//    Transform panel;                                                        // Crea transformación del panel.
//    panel.translation = glm::vec3(0.0f, 0.20f, 0.55f);                      // Ubica panel delante de la escena para que sea visible.
//    panel.scale = glm::vec3(6.25f, 1.75f, 0.08f);                           // Escala panel amplio para contener el mensaje.
//    drawObject(p, cube, panel, panelMat);                                   // Dibuja el panel.
//    Material textMat{{0.0f,0.95f,1.0f},1.0f,180.0f,0.045f,0.95f};           // Material cian neón para el texto.
//    std::string line1 = g.victory ? "VICTORIA" : "GAME OVER";              // Define primera línea del mensaje.
//    std::string line2 = "PRESIONA R";                                       // Define segunda línea indicando cómo reiniciar.
//    renderBitmapText(p, cube, line1, glm::vec3(0.0f, 0.60f, 0.64f), 0.105f, textMat); // Dibuja primera línea.
//    renderBitmapText(p, cube, line2, glm::vec3(0.0f, -0.08f, 0.64f), 0.080f, textMat); // Dibuja instrucción de reinicio.
//}                                                                         // Fin de renderFinalPanel.

void renderFinalPanel(GLuint p, const Mesh& cube, const Game& g) {
    if (!g.victory && !g.gameOver) return;

    // Fondo oscuro del panel
    Material panelMat;
    panelMat.color = glm::vec3(0.01f, 0.01f, 0.02f);
    panelMat.alpha = 1.0f;
    panelMat.shininess = 80.0f;
    panelMat.attenuationScale = 0.03f;
    panelMat.emissive = 0.02f;
    panelMat.accentColor = panelMat.color;
    panelMat.gradientMode = 0;

    Transform panel;
    panel.translation = glm::vec3(0.0f, 0.20f, 1.90f);
    panel.scale = glm::vec3(5.0f, 3.00f, 0.10f);
    drawObject(p, cube, panel, panelMat);

    // Materiales neón por color
    Material neonBlue;
    neonBlue.color = glm::vec3(0.0f, 0.0f, 1.00f);
    neonBlue.alpha = 1.0f;
    neonBlue.shininess = 260.0f;
    neonBlue.attenuationScale = 0.02f;
    neonBlue.emissive = 1.10f;
    neonBlue.accentColor = neonBlue.color;
    neonBlue.gradientMode = 0;

    Material neonCyan;
    neonCyan.color = glm::vec3(0.00f, 1.00f, 0.90f);
    neonCyan.alpha = 1.0f;
    neonCyan.shininess = 260.0f;
    neonCyan.attenuationScale = 0.02f;
    neonCyan.emissive = 1.10f;
    neonCyan.accentColor = neonCyan.color;
    neonCyan.gradientMode = 0;

    Material neonRed;
    neonRed.color = glm::vec3(1.00f, 0.0f, 0.0f);
    neonRed.alpha = 1.0f;
    neonRed.shininess = 260.0f;
    neonRed.attenuationScale = 0.02f;
    neonRed.emissive = 1.10f;
    neonRed.accentColor = neonRed.color;
    neonRed.gradientMode = 0;

    Material neonGreen;
    neonGreen.color = glm::vec3(0.0f, 1.00f, 0.0f);
    neonGreen.alpha = 1.0f;
    neonGreen.shininess = 260.0f;
    neonGreen.attenuationScale = 0.02f;
    neonGreen.emissive = 1.0f;
    neonGreen.accentColor = neonGreen.color;
    neonGreen.gradientMode = 0;

    // Material suave para el texto secundario
    Material smallText;
    smallText.color = glm::vec3(0.92f, 0.92f, 0.95f);
    smallText.alpha = 1.0f;
    smallText.shininess = 180.0f;
    smallText.attenuationScale = 0.03f;
    smallText.emissive = 0.20f;
    smallText.accentColor = smallText.color;
    smallText.gradientMode = 0;

    if (g.gameOver) {
        std::vector<Material> topRow = {neonBlue, neonCyan, neonRed, neonGreen};   // G A M E
        std::vector<Material> bottomRow = {neonBlue, neonCyan, neonRed, neonGreen}; // O V E R

        float pixel = 0.09f;
        float charStep = pixel * 6.0f;
        float wordWidth = 4.0f * charStep;

        glm::vec3 topStart(-wordWidth * 0.5f, 1.0f, 1.98f);
        glm::vec3 bottomStart(-wordWidth * 0.5f, 0.1f, 1.98f);

        renderNeonWord(p, cube, "GAME", topStart, pixel, topRow);
        renderNeonWord(p, cube, "OVER", bottomStart, pixel, bottomRow);

        renderBitmapText(
            p,
            cube,
            "PRESIONA R",
            glm::vec3(0.0f, -0.85f, 1.98f),
            0.050f,
            smallText
        );
    }
    else if (g.victory) {
        // Si quieres, aquí puedes dejar otro estilo para victoria
        Material victoryText;
        victoryText.color = glm::vec3(0.0f, 1.00f, 0.0f);
        victoryText.alpha = 1.0f;
        victoryText.shininess = 250.0f;
        victoryText.attenuationScale = 0.02f;
        victoryText.emissive = 0.85f;
        victoryText.accentColor = victoryText.color;
        victoryText.gradientMode = 0;

        renderBitmapText(
            p,
            cube,
            "VICTORIA",
            glm::vec3(0.0f, 0.45f, 0.98f),
            0.085f,
            victoryText
        );

        renderBitmapText(
            p,
            cube,
            "PRESIONA R",
            glm::vec3(0.0f, -0.45f, 0.98f),
            0.055f,
            smallText
        );
    }
}

int main() {                                                                  // Función principal.
    if (!glfwInit()) { std::cerr << "No se pudo inicializar GLFW.\n"; return -1; } // Inicializa GLFW.
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);                             // Solicita OpenGL 3.
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);                             // Solicita OpenGL 3.3.
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);             // Usa perfil moderno.
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Arkanoid 3D", nullptr, nullptr); // Crea ventana.
    if (!window) { std::cerr << "No se pudo crear ventana.\n"; glfwTerminate(); return -1; } // Valida ventana.
    glfwMakeContextCurrent(window);                                            // Activa contexto.
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);           // Asigna callback.
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) { std::cerr << "No se pudo cargar GLAD.\n"; return -1; } // Carga GLAD.
    glEnable(GL_DEPTH_TEST);                                                   // Activa profundidad 3D.
    glEnable(GL_BLEND);                                                        // Activa transparencia.
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);                         // Define mezcla alfa.
    glEnable(GL_CULL_FACE);                                                    // Activa culling.
    GLuint program = createProgram("vertex_shader.glsl", "fragment_shader.glsl"); // Carga shaders externos.
    if (!program) { glfwTerminate(); return -1; }                              // Valida programa.
    Mesh cube = createCubeMesh();                                              // Crea cubo retenido.
    Mesh sphere = createSphereMesh(32, 48);                                    // Crea esfera retenida.
    Game game;                                                                 // Crea estado de juego.
    resetGame(game);                                                           // Inicializa partida.
    float last = (float)glfwGetTime();                                         // Tiempo anterior.
    float titleTimer = 0.0f;                                                   // Temporizador de título.
    while (!glfwWindowShouldClose(window)) {                                   // Bucle principal.
        float now = (float)glfwGetTime();                                      // Tiempo actual.
        float dt = std::clamp(now - last, 0.0f, 0.033f);                       // Delta time.
        last = now;                                                            // Actualiza tiempo.
        titleTimer += dt;                                                      // Acumula título.
        processInput(window, game, dt);                                        // Procesa teclado.
        updateGame(game, dt);                                                  // Actualiza juego.
        if (titleTimer > 0.18f) { title(window, game); titleTimer = 0.0f; }    // Actualiza título.
        glClearColor(0.005f, 0.008f, 0.030f, 1.0f);                            // Fondo oscuro.
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);                    // Limpia pantalla.
        glUseProgram(program);                                                 // Usa shaders.
        glm::vec3 cameraPos(0.0f, -0.05f, 8.60f);                                // Aleja la cámara para mantener visibles el padel y la esfera en todos los límites.
        glm::mat4 view = glm::lookAt(cameraPos, glm::vec3(0.0f, 0.0f, -3.30f), glm::vec3(0,1,0)); // Orienta la cámara al centro del cuarto conservando profundidad 3D.
        glm::mat4 proj = glm::perspective(glm::radians(58.0f), (float)gWidth/(float)gHeight, 0.1f, 100.0f); // Aumenta el campo de visión para que nada se pierda arriba o abajo.
        glUniformMatrix4fv(glGetUniformLocation(program,"uView"),1,GL_FALSE,glm::value_ptr(view)); // Envía vista.
        glUniformMatrix4fv(glGetUniformLocation(program,"uProjection"),1,GL_FALSE,glm::value_ptr(proj)); // Envía proyección.
        glUniform3fv(glGetUniformLocation(program,"uLightPos"),1,glm::value_ptr(game.ball.position)); // La luz es la bola.
        glUniform3fv(glGetUniformLocation(program,"uViewPos"),1,glm::value_ptr(cameraPos)); // Envía cámara.
        glUniform3f(glGetUniformLocation(program,"uAmbientLight"),0.10f,0.11f,0.15f); // Luz ambiental leve para que el cuarto siempre conserve sus márgenes visibles.
        glUniform3f(glGetUniformLocation(program,"uDiffuseLight"),2.10f,2.20f,2.40f);  // Luz difusa.
        glUniform3f(glGetUniformLocation(program,"uSpecularLight"),2.5f,2.65f,2.85f); // Luz especular.
        glDepthMask(GL_FALSE);                                                // No escribe profundidad para cuarto transparente.
        renderRoom(program, cube);                                            // Dibuja cuarto.
        glDepthMask(GL_TRUE);                                                 // Reactiva profundidad.
        renderBlocks(program, cube, game);                                    // Dibuja bloques.
        renderPaddle(program, cube, game);                                    // Dibuja padel.
        renderBall(program, sphere, game);                                    // Dibuja bola.
        renderFinalPanel(program, cube, game);                                // Dibuja victoria/derrota.
        glfwSwapBuffers(window);                                              // Presenta imagen.
        glfwPollEvents();                                                     // Procesa eventos.
    }                                                                         // Fin del bucle.
    deleteMesh(cube);                                                         // Libera cubo.
    deleteMesh(sphere);                                                       // Libera esfera.
    glDeleteProgram(program);                                                 // Libera shaders.
    glfwTerminate();                                                          // Cierra GLFW.
    return 0;                                                                 // Fin correcto.
}                                                                             // Fin de main.
