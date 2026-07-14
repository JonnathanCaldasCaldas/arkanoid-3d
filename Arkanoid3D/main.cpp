#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

const unsigned int ANCHO_VENTANA = 1280;
const unsigned int ALTO_VENTANA = 720;
int anchoVentanaActual = ANCHO_VENTANA;
int altoVentanaActual = ALTO_VENTANA;

const float CUARTO_MIN_X = -5.0f;
const float CUARTO_MAX_X =  5.0f;
const float CUARTO_MIN_Y = -3.2f;
const float CUARTO_MAX_Y =  3.3f;
const float LIMITE_VISIBLE_MIN_Y = -2.62f;
const float LIMITE_VISIBLE_MAX_Y =  2.62f;
const float CUARTO_MIN_Z = -7.0f;
const float CUARTO_MAX_Z =  2.4f;
const float Z_PARED_BLOQUES = -6.5f;
const float TAMANO_BLOQUE_X = 1.8f;
const float TAMANO_BLOQUE_Y = 1.30f;
const float TAMANO_BLOQUE_Z = 1.00f;
const float Z_FRENTE_BLOQUES = Z_PARED_BLOQUES + TAMANO_BLOQUE_Z * 0.5f;
const float RELACION_MINIMA_TRAYECTORIA_Z = 0.42f;
const float RELACION_MAXIMA_TRAYECTORIA_Y = 0.68f;

const int VIDAS_INICIALES = 5;
const glm::vec3 POSICION_INICIAL_BOLA(0.05f, -2.05f, 1.10f);
const glm::vec3 DIRECCION_INICIAL_BOLA(0.6f, 1.00f, -2.65f);
const glm::vec3 POSICION_INICIAL_PADEL(0.0f, -2.35f, 1.65f);

/*
    El juego usa modo retenido porque la geometría casi no cambia durante la
    ejecución. Una malla guarda vértices, normales e índices, y esos datos se
    envían una sola vez a la GPU con VAO, VBO y EBO. En cada fotograma solo se
    reutiliza esa malla con otra transformación o material, que es el enfoque
    esperado en OpenGL moderno.
*/
struct Vertice {
    glm::vec3 posicion;
    glm::vec3 normal;
};

struct Malla {
    GLuint VAO = 0;
    GLuint VBO = 0;
    GLuint EBO = 0;
    GLsizei cantidadIndices = 0;
};

struct Transformacion {
    glm::vec3 traslacion = glm::vec3(0.0f);
    glm::vec3 rotacion = glm::vec3(0.0f);
    glm::vec3 escala = glm::vec3(1.0f);
    int usarEjeAngulo = 0;
    glm::vec3 eje = glm::vec3(0, 1, 0);
    float anguloEje = 0.0f;
};

struct Material {
    glm::vec3 color = glm::vec3(1.0f);
    float alfa = 1.0f;
    float brilloEspecular = 64.0f;
    float factorAtenuacion = 0.06f;
    float emision = 0.0f;
    glm::vec3 colorAcento = glm::vec3(1.0f);
    int modoDegradado = 0;
    int modoBola = 0;
};

struct Bloque {
    glm::vec3 posicion;
    glm::vec3 tamano;
    glm::vec3 color;
    bool activo = true;
    bool rompiendose = false;
    float temporizadorRuptura = 0.0f;
};

struct Bola {
    glm::vec3 posicion = POSICION_INICIAL_BOLA;
    glm::vec3 velocidad = glm::normalize(glm::vec3(0.55f, 1.00f, -2.65f));
    float radio = 0.25f;
    float rapidez = 3.35f;
    glm::vec3 ejeGiro = glm::vec3(0, 1, 0);
    float anguloGiro = 0.0f;
};

struct Padel {
    glm::vec3 posicion = POSICION_INICIAL_PADEL;
    glm::vec3 tamano = glm::vec3(2.00f, 0.80f, 0.55f);
    float rapidez = 4.25f;
};

struct Juego {
    Bola bola;
    Padel padel;
    std::vector<Bloque> bloques;
    int vidas = VIDAS_INICIALES;
    bool victoria = false;
    bool juegoTerminado = false;
    bool teclaReinicioPresionada = false;
};

std::string leerArchivo(const std::string& ruta) {
    std::ifstream archivo(ruta);
    if (!archivo.is_open()) { std::cerr << "No se pudo abrir: " << ruta << "\n"; return ""; }
    std::stringstream ss;
    ss << archivo.rdbuf();
    return ss.str();
}

GLuint compilarShader(GLenum tipo, const std::string& codigoFuente, const std::string& nombre) {
    GLuint sombreador = glCreateShader(tipo);
    const char* codigo = codigoFuente.c_str();
    glShaderSource(sombreador, 1, &codigo, nullptr);
    glCompileShader(sombreador);
    GLint correcto = GL_FALSE;
    glGetShaderiv(sombreador, GL_COMPILE_STATUS, &correcto);
    if (!correcto) {
        GLint longitud = 0;
        glGetShaderiv(sombreador, GL_INFO_LOG_LENGTH, &longitud);
        std::string registro(std::max(longitud, 1), '\0');
        glGetShaderInfoLog(sombreador, longitud, nullptr, &registro[0]);
        std::cerr << "Error en " << nombre << ":\n" << registro << "\n";
        glDeleteShader(sombreador);
        return 0;
    }
    return sombreador;
}

GLuint crearProgramaShader(const std::string& rutaVS, const std::string& rutaFS) {
    GLuint shaderVertices = compilarShader(GL_VERTEX_SHADER, leerArchivo(rutaVS), rutaVS);
    GLuint shaderFragmentos = compilarShader(GL_FRAGMENT_SHADER, leerArchivo(rutaFS), rutaFS);
    if (!shaderVertices || !shaderFragmentos) return 0;
    GLuint programa = glCreateProgram();
    glAttachShader(programa, shaderVertices);
    glAttachShader(programa, shaderFragmentos);
    glLinkProgram(programa);
    glDeleteShader(shaderVertices);
    glDeleteShader(shaderFragmentos);
    GLint correcto = GL_FALSE;
    glGetProgramiv(programa, GL_LINK_STATUS, &correcto);
    if (!correcto) {
        GLint longitud = 0;
        glGetProgramiv(programa, GL_INFO_LOG_LENGTH, &longitud);
        std::string registro(std::max(longitud, 1), '\0');
        glGetProgramInfoLog(programa, longitud, nullptr, &registro[0]);
        std::cerr << "Error enlazando shaders:\n" << registro << "\n";
        glDeleteProgram(programa);
        return 0;
    }
    return programa;
}

void actualizarTamanoVentana(GLFWwindow*, int w, int h) {
    anchoVentanaActual = std::max(w, 1);
    altoVentanaActual = std::max(h, 1);
    glViewport(0, 0, anchoVentanaActual, altoVentanaActual);
}

/*
    Aquí se sube la geometría a la GPU. El VBO contiene los vértices, el EBO
    conserva el orden de los triángulos y el VAO recuerda como interpretar los
    atributos de posición y normal. Después de esta función, dibujar la malla es
    mucho más barato porque no se vuelve a copiar toda la geometría.
*/
void subirMallaAGPU(Malla& malla, const std::vector<Vertice>& v, const std::vector<unsigned int>& i) {
    malla.cantidadIndices = static_cast<GLsizei>(i.size());
    glGenVertexArrays(1, &malla.VAO);
    glGenBuffers(1, &malla.VBO);
    glGenBuffers(1, &malla.EBO);
    glBindVertexArray(malla.VAO);
    glBindBuffer(GL_ARRAY_BUFFER, malla.VBO);
    glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(Vertice), v.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, malla.EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, i.size() * sizeof(unsigned int), i.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertice), (void*)offsetof(Vertice, posicion));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertice), (void*)offsetof(Vertice, normal));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}

/*
    El cubo se define con vértices separados por cara. Aunque varias esquinas
    ocupan la misma posición, cada cara necesita una normal propia para que la
    iluminación se vea plana y no suavizada. Esta misma malla se reutiliza para
    bloques, paredes, marcos, texto y piezas del padel.
*/
Malla crearMallaCubo() {
    Malla malla;
    std::vector<Vertice> v = {
        {{-0.5f,-0.5f, 0.5f},{0,0,1}}, {{ 0.5f,-0.5f, 0.5f},{0,0,1}}, {{ 0.5f, 0.5f, 0.5f},{0,0,1}}, {{-0.5f, 0.5f, 0.5f},{0,0,1}},
        {{ 0.5f,-0.5f,-0.5f},{0,0,-1}},{{-0.5f,-0.5f,-0.5f},{0,0,-1}},{{-0.5f, 0.5f,-0.5f},{0,0,-1}},{{ 0.5f, 0.5f,-0.5f},{0,0,-1}},
        {{-0.5f,-0.5f,-0.5f},{-1,0,0}},{{-0.5f,-0.5f, 0.5f},{-1,0,0}},{{-0.5f, 0.5f, 0.5f},{-1,0,0}},{{-0.5f, 0.5f,-0.5f},{-1,0,0}},
        {{ 0.5f,-0.5f, 0.5f},{1,0,0}}, {{ 0.5f,-0.5f,-0.5f},{1,0,0}}, {{ 0.5f, 0.5f,-0.5f},{1,0,0}}, {{ 0.5f, 0.5f, 0.5f},{1,0,0}},
        {{-0.5f, 0.5f, 0.5f},{0,1,0}}, {{ 0.5f, 0.5f, 0.5f},{0,1,0}}, {{ 0.5f, 0.5f,-0.5f},{0,1,0}}, {{-0.5f, 0.5f,-0.5f},{0,1,0}},
        {{-0.5f,-0.5f,-0.5f},{0,-1,0}},{{ 0.5f,-0.5f,-0.5f},{0,-1,0}},{{ 0.5f,-0.5f, 0.5f},{0,-1,0}},{{-0.5f,-0.5f, 0.5f},{0,-1,0}}
    };
    std::vector<unsigned int> i = {
        0,1,2,0,2,3, 4,5,6,4,6,7, 8,9,10,8,10,11, 12,13,14,12,14,15, 16,17,18,16,18,19, 20,21,22,20,22,23
    };
    subirMallaAGPU(malla, v, i);
    return malla;
}

/*
    La esfera se genera por anillos de latitud y longitud. Cada punto nace sobre
    una esfera unitaria, por eso la normal se obtiene normalizando la posición
    local. Luego los índices unen los anillos en triángulos para formar la bola.
*/
Malla crearMallaEsfera(int pilas, int sectores) {
    Malla malla;
    std::vector<Vertice> v;
    std::vector<unsigned int> indices;
    const float PI = 3.14159265359f;
    for (int s = 0; s <= pilas; ++s) {
        float anguloPila = PI * 0.5f - float(s) * PI / float(pilas);
        float radioAnillo = std::cos(anguloPila);
        float z = std::sin(anguloPila);
        for (int k = 0; k <= sectores; ++k) {
            float anguloSector = float(k) * 2.0f * PI / float(sectores);
            glm::vec3 p(radioAnillo * std::cos(anguloSector), radioAnillo * std::sin(anguloSector), z);
            v.push_back({p, glm::normalize(p)});
        }
    }
    for (int s = 0; s < pilas; ++s) {
        int k1 = s * (sectores + 1);
        int k2 = k1 + sectores + 1;
        for (int k = 0; k < sectores; ++k, ++k1, ++k2) {
            if (s != 0) { indices.push_back(k1); indices.push_back(k2); indices.push_back(k1 + 1); }
            if (s != pilas - 1) { indices.push_back(k1 + 1); indices.push_back(k2); indices.push_back(k2 + 1); }
        }
    }
    subirMallaAGPU(malla, v, indices);
    return malla;
}

void eliminarMalla(Malla& m) {
    glDeleteVertexArrays(1, &m.VAO);
    glDeleteBuffers(1, &m.VBO);
    glDeleteBuffers(1, &m.EBO);
}

void enviarTransformacion(GLuint p, const Transformacion& t) {
    glUniform3fv(glGetUniformLocation(p, "uTraslacion"), 1, glm::value_ptr(t.traslacion));
    glUniform3fv(glGetUniformLocation(p, "uRotacion"), 1, glm::value_ptr(t.rotacion));
    glUniform3fv(glGetUniformLocation(p, "uEscala"), 1, glm::value_ptr(t.escala));
    glUniform1i(glGetUniformLocation(p, "uUsarEjeAngulo"), t.usarEjeAngulo);
    glUniform3fv(glGetUniformLocation(p, "uEje"), 1, glm::value_ptr(t.eje));
    glUniform1f(glGetUniformLocation(p, "uAnguloEje"), t.anguloEje);
}

void enviarMaterial(GLuint p, const Material& m) {
    glUniform3fv(glGetUniformLocation(p, "uColorBase"), 1, glm::value_ptr(m.color));
    glUniform1f(glGetUniformLocation(p, "uAlfa"), m.alfa);
    glUniform1f(glGetUniformLocation(p, "uBrilloEspecular"), m.brilloEspecular);
    glUniform1f(glGetUniformLocation(p, "uFactorAtenuacion"), m.factorAtenuacion);
    glUniform1f(glGetUniformLocation(p, "uEmision"), m.emision);
    glUniform3fv(glGetUniformLocation(p, "uColorAcento"), 1, glm::value_ptr(m.colorAcento));
    glUniform1i(glGetUniformLocation(p, "uModoDegradado"), m.modoDegradado);
    glUniform1i(glGetUniformLocation(p, "uModoBola"), m.modoBola);
}

void dibujarMalla(const Malla& m) {
    glBindVertexArray(m.VAO);
    glDrawElements(GL_TRIANGLES, m.cantidadIndices, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

void dibujarObjetoGrafico(GLuint p, const Malla& malla, const Transformacion& t, const Material& m) {
    enviarTransformacion(p, t);
    enviarMaterial(p, m);
    dibujarMalla(malla);
}

/*
    La colisión esfera-caja se calcula buscando el punto de la caja más cercano
    al centro de la esfera. Si ese punto queda dentro del radio, existe choque.
    Cuando el centro está dentro de la caja, se usa el eje de menor penetracion
    para elegir una normal estable y sacar la bola por la cara correcta.
*/
bool colisionEsferaCaja(const glm::vec3& c, float r, const glm::vec3& bc, const glm::vec3& bs, glm::vec3& n) {
    glm::vec3 mitad = bs * 0.5f;
    glm::vec3 puntoCercano = glm::clamp(c, bc - mitad, bc + mitad);
    glm::vec3 d = c - puntoCercano;
    float d2 = glm::dot(d, d);
    if (d2 > r * r) return false;
    if (d2 > 0.00001f) { n = glm::normalize(d); return true; }
    glm::vec3 local = c - bc;
    glm::vec3 penetracion = mitad - glm::abs(local);
    if (penetracion.x < penetracion.y && penetracion.x < penetracion.z) n = glm::vec3(local.x >= 0 ? 1 : -1, 0, 0);
    else if (penetracion.y < penetracion.z) n = glm::vec3(0, local.y >= 0 ? 1 : -1, 0);
    else n = glm::vec3(0, 0, local.z >= 0 ? 1 : -1);
    return true;
}

/*
    La reflexión conserva la rapidez de la bola. Primero se aplica la fórmula
    vectorial v - 2(v.n)n para cambiar la dirección según la normal del choque;
    despues se normaliza el resultado y se multiplica por la rapidez original.
*/
glm::vec3 reflejarManteniendoRapidez(const glm::vec3& v, const glm::vec3& n, float rapidez) {
    glm::vec3 r = v - 2.0f * glm::dot(v, n) * n;
    if (glm::length(r) < 0.0001f) r = -v;
    return glm::normalize(r) * rapidez;
}

/*
    Esta corrección evita que la bola quede atrapada en rebotes casi verticales.
    Se fuerza una componente mínima en Z para que avance hacia bloques o padel,
    y se limita Y para que no pase demásiado tiempo rebotando entre piso y techo.
*/
void estabilizarVelocidadBola(Bola& b, int signoZPreferido = 0) {
    glm::vec3 d = glm::normalize(b.velocidad);
    float signoZ = signoZPreferido != 0 ? float(signoZPreferido) : (d.z < 0.0f ? -1.0f : 1.0f);
    if (std::abs(d.z) < RELACION_MINIMA_TRAYECTORIA_Z) {
        d.z = signoZ * RELACION_MINIMA_TRAYECTORIA_Z;
    }
    if (std::abs(d.y) > RELACION_MAXIMA_TRAYECTORIA_Y) {
        d.y = (d.y > 0.0f ? 1.0f : -1.0f) * RELACION_MAXIMA_TRAYECTORIA_Y;
    }
    if (glm::length(d) < 0.0001f) {
        d = glm::vec3(0.45f, 0.35f, -1.0f);
    }
    b.velocidad = glm::normalize(d) * b.rapidez;
}

/*
    Los bloques se crean como una grilla de filas y columnas centrada en la pared
    del fondo. El cálculo del ancho y alto total permite repartirlos de forma
    uniforme, y la paleta por fila ayuda a leer visualmente el avance del juego.
*/
void crearBloques(Juego& g) {
    g.bloques.clear();
    const int filas = 5;
    const int columnas = 5;
    const float separacionX = 0.2f;
    const float separacionY = 0.2f;
    const float anchoTotal = columnas * TAMANO_BLOQUE_X + (columnas - 1) * separacionX;
    const float altoTotal = filas * TAMANO_BLOQUE_Y + (filas - 1) * separacionY;
    const float inicioX = -anchoTotal * 0.5f + TAMANO_BLOQUE_X * 0.5f;
    const float inicioY = -altoTotal * 0.5f + TAMANO_BLOQUE_Y * 0.5f;
    glm::vec3 colores[5] = {
        {1.0f,0.10f,0.12f},
        {1.0f,0.62f,0.08f},
        {0.08f,1.00f,0.35f},
        {0.08f,0.55f,1.0f},
        {0.92f,0.12f,1.00f}
    };
    for (int r = 0; r < filas; ++r) {
        for (int c = 0; c < columnas; ++c) {
            float x = inicioX + c * (TAMANO_BLOQUE_X + separacionX);
            float y = inicioY + r * (TAMANO_BLOQUE_Y + separacionY);
            glm::vec3 posicion(x, y, Z_PARED_BLOQUES);
            glm::vec3 tamano(TAMANO_BLOQUE_X, TAMANO_BLOQUE_Y, TAMANO_BLOQUE_Z);
            g.bloques.push_back({posicion, tamano, colores[r], true, false, 0.0f});
        }
    }
}

void reiniciarBola(Juego& g) {
    g.bola.posicion = POSICION_INICIAL_BOLA;
    g.bola.velocidad = glm::normalize(DIRECCION_INICIAL_BOLA) * g.bola.rapidez;
    g.bola.anguloGiro = 0.0f;
}

void reiniciarJuego(Juego& g) {
    g.vidas = VIDAS_INICIALES;
    g.victoria = false;
    g.juegoTerminado = false;
    g.padel.posicion = POSICION_INICIAL_PADEL;
    reiniciarBola(g);
    crearBloques(g);
}

/*
    La entrada del teclado solo modifica el estádo del juego. Las flechas mueven
    el padel usando delta time y luego se limita su posición dentro del cuarto.
    La tecla R se controla con una bandera para reiniciar una sola vez por pulso.
*/
void procesarEntrada(GLFWwindow* w, Juego& g, float dt) {
    if (glfwGetKey(w, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(w, true);

    bool reinicioPresionado = glfwGetKey(w, GLFW_KEY_R) == GLFW_PRESS;
    if (reinicioPresionado && !g.teclaReinicioPresionada){
        reiniciarJuego(g);
    }
    g.teclaReinicioPresionada = reinicioPresionado;

    if (g.victoria || g.juegoTerminado) return;
    float d = g.padel.rapidez * dt;
    if (glfwGetKey(w, GLFW_KEY_LEFT) == GLFW_PRESS)  g.padel.posicion.x -= d;
    if (glfwGetKey(w, GLFW_KEY_RIGHT) == GLFW_PRESS) g.padel.posicion.x += d;
    if (glfwGetKey(w, GLFW_KEY_UP) == GLFW_PRESS)    g.padel.posicion.y += d;
    if (glfwGetKey(w, GLFW_KEY_DOWN) == GLFW_PRESS)  g.padel.posicion.y -= d;
    g.padel.posicion.x = std::clamp(g.padel.posicion.x, CUARTO_MIN_X + g.padel.tamano.x * 0.5f, CUARTO_MAX_X - g.padel.tamano.x * 0.5f);
    g.padel.posicion.y = std::clamp(g.padel.posicion.y, CUARTO_MIN_Y + g.padel.tamano.y * 0.5f, CUARTO_MAX_Y - g.padel.tamano.y * 0.5f);
}

void actualizarRotacionBola(Bola& b, float dt) {
    glm::vec3 direccion = glm::normalize(b.velocidad);
    glm::vec3 eje = glm::cross(glm::vec3(0,1,0), direccion);
    if (glm::length(eje) < 0.001f) eje = glm::cross(glm::vec3(1,0,0), direccion);
    b.ejeGiro = glm::normalize(eje);
    b.anguloGiro += (b.rapidez / b.radio) * dt;
}

/*
    Esta función concentra la física de cada fotograma. Se integra la posición
    de la bola, se revisan colisiónes con paredes, padel y bloques, y cada choque
    produce una normal usada para reflejar la trayectoria. Cuando un bloque es
    golpeado deja de colisiónar inmediatamente, pero se queda reduciéndose durante
    un tiempo corto para que la destrucción sea visible.
*/
void actualizarJuego(Juego& g, float dt) {
    if (g.victoria || g.juegoTerminado) return;
    Bola& b = g.bola;
    b.posicion += b.velocidad * dt;
    if (b.posicion.x - b.radio < CUARTO_MIN_X) {
        b.posicion.x = CUARTO_MIN_X + b.radio;
        b.velocidad = reflejarManteniendoRapidez(b.velocidad, glm::vec3(1,0,0), b.rapidez);
        estabilizarVelocidadBola(b);
    }
    if (b.posicion.x + b.radio > CUARTO_MAX_X) {
        b.posicion.x = CUARTO_MAX_X - b.radio;
        b.velocidad = reflejarManteniendoRapidez(b.velocidad, glm::vec3(-1,0,0), b.rapidez);
        estabilizarVelocidadBola(b);
    }
    if (b.posicion.y - b.radio < CUARTO_MIN_Y) {
        b.posicion.y = CUARTO_MIN_Y + b.radio;
        b.velocidad = reflejarManteniendoRapidez(b.velocidad, glm::vec3(0,1,0), b.rapidez);
        estabilizarVelocidadBola(b);
    }
    if (b.posicion.y + b.radio > CUARTO_MAX_Y) {
        b.posicion.y = CUARTO_MAX_Y - b.radio;
        b.velocidad = reflejarManteniendoRapidez(b.velocidad, glm::vec3(0,-1,0), b.rapidez);
        estabilizarVelocidadBola(b);
    }
    if (b.posicion.z - b.radio < CUARTO_MIN_Z) {
        b.posicion.z = CUARTO_MIN_Z + b.radio;
        b.velocidad = reflejarManteniendoRapidez(b.velocidad, glm::vec3(0,0,1), b.rapidez);
        estabilizarVelocidadBola(b, 1);
    }
    if (b.posicion.z + b.radio > CUARTO_MAX_Z) {
        g.vidas--;
        if (g.vidas <= 0) {
            g.juegoTerminado = true;
        } else {
            reiniciarBola(g);
        }
        return;
    }
    glm::vec3 n;
    if (b.velocidad.z > 0.0f && colisionEsferaCaja(b.posicion, b.radio, g.padel.posicion, g.padel.tamano, n)) {
        float rx = (b.posicion.x - g.padel.posicion.x) / (g.padel.tamano.x * 0.5f);
        float ry = (b.posicion.y - g.padel.posicion.y) / (g.padel.tamano.y * 0.5f);
        glm::vec3 padNormal = glm::normalize(glm::vec3(rx * 0.70f, ry * 0.55f, 1.0f));
        b.velocidad = reflejarManteniendoRapidez(b.velocidad, padNormal, b.rapidez);
        b.posicion.z = g.padel.posicion.z - g.padel.tamano.z * 0.5f - b.radio - 0.02f;
        estabilizarVelocidadBola(b, -1);
    }
    bool bloqueGolpeado = false;
    for (Bloque& bloque : g.bloques) {
        if (bloque.rompiendose) {
            bloque.temporizadorRuptura -= dt;
            if (bloque.temporizadorRuptura <= 0.0f) bloque.rompiendose = false;
            continue;
        }
        if (!bloque.activo) continue;
        if (colisionEsferaCaja(b.posicion, b.radio, bloque.posicion, bloque.tamano, n)) {
            b.velocidad = reflejarManteniendoRapidez(b.velocidad, n, b.rapidez);
            if (n.z > 0.25f) b.posicion.z = bloque.posicion.z + bloque.tamano.z * 0.5f + b.radio + 0.02f;
            bloque.activo = false;
            bloque.rompiendose = true;
            bloque.temporizadorRuptura = 2.0f;
            bloqueGolpeado = true;
            estabilizarVelocidadBola(b);
            break;
        }
    }

    bool hayBloquesVisibles = false;
    for (const Bloque& bloque : g.bloques) {
        if (bloque.activo || bloque.rompiendose) hayBloquesVisibles = true;
    }
    if (!hayBloquesVisibles) g.victoria = true;
    b.velocidad = glm::normalize(b.velocidad) * b.rapidez;
    actualizarRotacionBola(b, dt);
}

void actualizarTitulo(GLFWwindow* w, const Juego& g) {
    int restantes = 0;
    for (const Bloque& b : g.bloques) if (b.activo) restantes++;
    std::string t = "Arkanoid 3D - Vidas: " + std::to_string(g.vidas) + " - Objetos gráficos: " + std::to_string(restantes);
    if (g.victoria) t = "ARKANOID 3D - VICTORIA - presiona R";
    if (g.juegoTerminado) t = "ARKANOID 3D - GAME OVER - presiona R";
    glfwSetWindowTitle(w, t.c_str());
}

/*
    El cuarto se construye con la misma malla de cubo: paredes delgadas, líneas
    de cuadrícula y aristas neón. La cuadrícula da profundidad y los marcos dejan
    claros los límites reales donde la bola y el padel pueden moverse.
*/
void renderizarCuarto(GLuint p, const Malla& cubo) {
    const float grosorPared = 0.05f;
    const float grosorArista = 0.04f;

    const float anchoCuarto  = CUARTO_MAX_X - CUARTO_MIN_X;
    const float altoCuarto = CUARTO_MAX_Y - CUARTO_MIN_Y;
    const float profundidadCuarto  = CUARTO_MAX_Z - CUARTO_MIN_Z;

    const float centroX = (CUARTO_MIN_X + CUARTO_MAX_X) * 0.5f;
    const float centroY = (CUARTO_MIN_Y + CUARTO_MAX_Y) * 0.5f;
    const float centroZ = (CUARTO_MIN_Z + CUARTO_MAX_Z) * 0.5f;

    Material pared{
        {0.025f, 0.065f, 0.145f},
        0.30f,
        48.0f,
        0.055f,
        0.01f
    };

    Transformacion piso;
    piso.traslacion = glm::vec3(
        centroX,
        CUARTO_MIN_Y - grosorPared * 0.5f,
        centroZ
    );
    piso.escala = glm::vec3(
        anchoCuarto,
        grosorPared,
        profundidadCuarto
    );
    dibujarObjetoGrafico(p, cubo, piso, pared);

    Transformacion techo;
    techo.traslacion = glm::vec3(
        centroX,
        CUARTO_MAX_Y + grosorPared * 0.5f,
        centroZ
    );
    techo.escala = glm::vec3(
        anchoCuarto,
        grosorPared,
        profundidadCuarto
    );
    dibujarObjetoGrafico(p, cubo, techo, pared);

    Transformacion paredIzquierda;
    paredIzquierda.traslacion = glm::vec3(
        CUARTO_MIN_X - grosorPared * 0.5f,
        centroY,
        centroZ
    );
    paredIzquierda.escala = glm::vec3(
        grosorPared,
        altoCuarto,
        profundidadCuarto
    );
    dibujarObjetoGrafico(p, cubo, paredIzquierda, pared);

    Transformacion paredDerecha;
    paredDerecha.traslacion = glm::vec3(
        CUARTO_MAX_X + grosorPared * 0.5f,
        centroY,
        centroZ
    );
    paredDerecha.escala = glm::vec3(
        grosorPared,
        altoCuarto,
        profundidadCuarto
    );
    dibujarObjetoGrafico(p, cubo, paredDerecha, pared);

    Transformacion paredFondo;
    paredFondo.traslacion = glm::vec3(
        centroX,
        centroY,
        CUARTO_MIN_Z - grosorPared * 0.5f
    );
    paredFondo.escala = glm::vec3(
        anchoCuarto,
        altoCuarto,
        grosorPared
    );
    dibujarObjetoGrafico(p, cubo, paredFondo, pared);

    Material neonMagenta{
        {1.0f, 0.05f, 0.95f},
        0.98f,
        160.0f,
        0.045f,
        0.25f
    };

    Material materialCuadricula{
        {0.00f, 0.55f, 1.00f},
        1.00f,
        100.0f,
        0.020f,
        0.25f,
    };

    float limitesX[2] = {CUARTO_MIN_X, CUARTO_MAX_X};
    float limitesY[2] = {CUARTO_MIN_Y, CUARTO_MAX_Y};
    float limitesZ[2] = {CUARTO_MIN_Z, CUARTO_MAX_Z};

    const float pasoCuadricula = 1.75f;
    const float grosorCuadricula = 0.024f;
    const float desfaseSuperficie = 0.0012f;

    int columnasPiso = static_cast<int>(anchoCuarto / pasoCuadricula);
    int profundidadPiso = static_cast<int>(profundidadCuarto / pasoCuadricula);

    for (float x = CUARTO_MIN_X + pasoCuadricula; x < CUARTO_MAX_X; x += pasoCuadricula) {

        Transformacion linea;
        linea.traslacion = glm::vec3(
            x,
            CUARTO_MIN_Y + desfaseSuperficie,
            centroZ
        );

        linea.escala = glm::vec3(
            grosorCuadricula,
            0.002f,
            profundidadCuarto
        );

        dibujarObjetoGrafico(p, cubo, linea, materialCuadricula);
    }

    for (float z = CUARTO_MIN_Z + pasoCuadricula; z < CUARTO_MAX_Z; z += pasoCuadricula) {

        Transformacion linea;
        linea.traslacion = glm::vec3(
            centroX,
            CUARTO_MIN_Y + desfaseSuperficie,
            z
        );

        linea.escala = glm::vec3(
            anchoCuarto,
            0.015f,
            grosorCuadricula
        );

        dibujarObjetoGrafico(p, cubo, linea, materialCuadricula);
    }

    int filasLaterales = static_cast<int>(altoCuarto / pasoCuadricula);
    int profundidadLateral = static_cast<int>(profundidadCuarto / pasoCuadricula);

    for (float y = CUARTO_MIN_Y + pasoCuadricula; y < CUARTO_MAX_Y; y += pasoCuadricula) {

        Transformacion lineaIzquierda;
        lineaIzquierda.traslacion = glm::vec3(
            CUARTO_MIN_X + desfaseSuperficie,
            y,
            centroZ
        );

        lineaIzquierda.escala = glm::vec3(
            0.002f,
            grosorCuadricula,
            profundidadCuarto
        );

        dibujarObjetoGrafico(p, cubo, lineaIzquierda, materialCuadricula);

        Transformacion lineaDerecha;
        lineaDerecha.traslacion = glm::vec3(
            CUARTO_MAX_X - desfaseSuperficie,
            y,
            centroZ
        );

        lineaDerecha.escala = glm::vec3(
            0.002f,
            grosorCuadricula,
            profundidadCuarto
        );

        dibujarObjetoGrafico(p, cubo, lineaDerecha, materialCuadricula);
    }

    for (float z = CUARTO_MIN_Z + pasoCuadricula; z < CUARTO_MAX_Z; z += pasoCuadricula) {

        Transformacion lineaIzquierda;
        lineaIzquierda.traslacion = glm::vec3(
            CUARTO_MIN_X + desfaseSuperficie,
            centroY,
            z
        );

        lineaIzquierda.escala = glm::vec3(
            0.02f,
            altoCuarto,
            grosorCuadricula
        );

        dibujarObjetoGrafico(p, cubo, lineaIzquierda, materialCuadricula);

        Transformacion lineaDerecha;
        lineaDerecha.traslacion = glm::vec3(
            CUARTO_MAX_X - desfaseSuperficie,
            centroY,
            z
        );

        lineaDerecha.escala = glm::vec3(
            0.02f,
            altoCuarto,
            grosorCuadricula
        );

        dibujarObjetoGrafico(p, cubo, lineaDerecha, materialCuadricula);
    }

    int columnasFondo = static_cast<int>(anchoCuarto / pasoCuadricula);
    int filasFondo = static_cast<int>(altoCuarto / pasoCuadricula);

    for (float x = CUARTO_MIN_X + pasoCuadricula; x < CUARTO_MAX_X; x += pasoCuadricula) {

        Transformacion linea;
        linea.traslacion = glm::vec3(
            x,
            centroY,
            CUARTO_MIN_Z + desfaseSuperficie
        );

        linea.escala = glm::vec3(
            grosorCuadricula,
            altoCuarto,
            0.01f
        );

        dibujarObjetoGrafico(p, cubo, linea, materialCuadricula);
    }

    for (float y = CUARTO_MIN_Y + pasoCuadricula; y < CUARTO_MAX_Y; y += pasoCuadricula) {

        Transformacion linea;
        linea.traslacion = glm::vec3(
            centroX,
            y,
            CUARTO_MIN_Z + desfaseSuperficie
        );

        linea.escala = glm::vec3(
            anchoCuarto,
            grosorCuadricula,
            0.002f
        );

        dibujarObjetoGrafico(p, cubo, linea, materialCuadricula);
    }

    int columnasTecho = static_cast<int>(anchoCuarto / pasoCuadricula);
    int profundidadTecho = static_cast<int>(profundidadCuarto / pasoCuadricula);

    for (int i = 1; i < columnasTecho; ++i) {
        float x = CUARTO_MIN_X + i * pasoCuadricula;

        Transformacion linea;
        linea.traslacion = glm::vec3(
            x,
            CUARTO_MAX_Y - desfaseSuperficie,
            centroZ
        );

        linea.escala = glm::vec3(
            grosorCuadricula,
            0.002f,
            profundidadCuarto
        );

        dibujarObjetoGrafico(p, cubo, linea, materialCuadricula);
    }

    for (int k = 1; k < profundidadTecho; ++k) {
        float z = CUARTO_MIN_Z + k * pasoCuadricula;

        Transformacion linea;
        linea.traslacion = glm::vec3(
            centroX,
            CUARTO_MAX_Y - desfaseSuperficie,
            z
        );

        linea.escala = glm::vec3(
            anchoCuarto,
            0.03f,
            grosorCuadricula
        );

        dibujarObjetoGrafico(p, cubo, linea, materialCuadricula);
    }

    for (int yi = 0; yi < 2; ++yi) {
        for (int zi = 0; zi < 2; ++zi) {
            Transformacion arista;

            arista.traslacion = glm::vec3(
                centroX,
                limitesY[yi],
                limitesZ[zi]
            );

            arista.escala = glm::vec3(
                anchoCuarto + grosorArista,
                grosorArista,
                grosorArista
            );

            dibujarObjetoGrafico(
                p,
                cubo,
                arista,
                neonMagenta
            );
        }
    }

    for (int xi = 0; xi < 2; ++xi) {
        for (int zi = 0; zi < 2; ++zi) {
            Transformacion arista;

            arista.traslacion = glm::vec3(
                limitesX[xi],
                centroY,
                limitesZ[zi]
            );

            arista.escala = glm::vec3(
                grosorArista,
                altoCuarto + grosorArista,
                grosorArista
            );

            dibujarObjetoGrafico(p, cubo, arista, neonMagenta);
        }
    }

    for (int xi = 0; xi < 2; ++xi) {
        for (int yi = 0; yi < 2; ++yi) {
            Transformacion arista;

            arista.traslacion = glm::vec3(
                limitesX[xi],
                limitesY[yi],
                centroZ
            );

            arista.escala = glm::vec3(
                grosorArista,
                grosorArista,
                profundidadCuarto + grosorArista
            );

            dibujarObjetoGrafico(
                p,
                cubo,
                arista,
                neonMagenta
            );
        }
    }
}

/*
    Durante la ruptura, el bloque ya no cuenta como sólido, pero sigue visible
    por unos segúndos. El temporizador se usa como factor de escala y emision;
    asi el bloque se encoge gradualmente en lugar de desaparecer de golpe.
*/
void renderizarBloques(GLuint p, const Malla& cubo, const Juego& g) {
    for (const Bloque& b : g.bloques) {
        if (!b.activo && !b.rompiendose) continue;
        float f = b.activo ? 1.0f : std::clamp(b.temporizadorRuptura / 2.0f, 0.0f, 1.0f);
        glm::vec3 acento = glm::min(b.color + glm::vec3(0.55f), glm::vec3(1.0f));
        Transformacion cuerpo;
        cuerpo.traslacion = b.posicion;
        cuerpo.escala = b.tamano * f;
        Material materialCuerpo;
        materialCuerpo.color = b.color;
        materialCuerpo.alfa = 1.0f;
        materialCuerpo.brilloEspecular = 180.0f;
        materialCuerpo.factorAtenuacion = 0.015f;
        materialCuerpo.emision = 0.06f * f;
        materialCuerpo.colorAcento = b.color;
        materialCuerpo.modoDegradado = 0;
        dibujarObjetoGrafico(p, cubo, cuerpo, materialCuerpo);

        Material bordeNeon;
        bordeNeon.color = acento;
        bordeNeon.alfa = 0.65f * f;
        bordeNeon.brilloEspecular = 220.0f;
        bordeNeon.factorAtenuacion = 0.015f;
        bordeNeon.emision = 0.08f * f;
        bordeNeon.colorAcento = acento;
        bordeNeon.modoDegradado = 0;

    }
}

/*
    El padel visual se arma con barras y un centro transparente. Todas las piezas
    usan cubos transformados, pero la caja física sigue siendo una sola; por eso
    se ve como un marco con vidrio sin complicar la detección de colisiónes.
*/
void renderizarPadel(GLuint p, const Malla& cubo, const Juego& g) {
    const float grosorLateral = 0.22f;
    const float grosorSuperior  = 0.07f;

    Material marco;
    marco.color = glm::vec3(0.35f, 0.18f, 1.00f);
    marco.alfa = 0.95f;
    marco.brilloEspecular = 260.0f;
    marco.factorAtenuacion = 0.030f;
    marco.emision = 0.04f;
    marco.colorAcento = marco.color;
    marco.modoDegradado = 0;

    Material vidrio;
    vidrio.color = glm::vec3(0.55f, 0.75f, 1.00f);
    vidrio.alfa = 0.18f;
    vidrio.brilloEspecular = 280.0f;
    vidrio.factorAtenuacion = 0.028f;
    vidrio.emision = 0.015f;
    vidrio.colorAcento = vidrio.color;
    vidrio.modoDegradado = 0;

    Transformacion barraIzquierda;
    barraIzquierda.traslacion = g.padel.posicion + glm::vec3(-(g.padel.tamano.x - grosorLateral) * 0.5f, 0.0f, 0.0f);
    barraIzquierda.escala = glm::vec3(grosorLateral, g.padel.tamano.y, g.padel.tamano.z);
    dibujarObjetoGrafico(p, cubo, barraIzquierda, marco);

    Transformacion barraDerecha;
    barraDerecha.traslacion = g.padel.posicion + glm::vec3((g.padel.tamano.x - grosorLateral) * 0.5f, 0.0f, 0.0f);
    barraDerecha.escala = glm::vec3(grosorLateral, g.padel.tamano.y, g.padel.tamano.z);
    dibujarObjetoGrafico(p, cubo, barraDerecha, marco);

    Transformacion barraSuperior;
    barraSuperior.traslacion = g.padel.posicion + glm::vec3(0.0f, (g.padel.tamano.y - grosorSuperior) * 0.5f, 0.0f);
    barraSuperior.escala = glm::vec3(g.padel.tamano.x - 2.0f * grosorLateral, grosorSuperior, g.padel.tamano.z);
    dibujarObjetoGrafico(p, cubo, barraSuperior, marco);

    Transformacion barraInferior;
    barraInferior.traslacion = g.padel.posicion + glm::vec3(0.0f, -(g.padel.tamano.y - grosorSuperior) * 0.5f, 0.0f);
    barraInferior.escala = glm::vec3(g.padel.tamano.x - 2.0f * grosorLateral, grosorSuperior, g.padel.tamano.z);
    dibujarObjetoGrafico(p, cubo, barraInferior, marco);

    Transformacion vidrioCentral;
    vidrioCentral.traslacion = g.padel.posicion;
    vidrioCentral.escala = glm::vec3(
        g.padel.tamano.x - 2.0f * grosorLateral,
        g.padel.tamano.y - 2.0f * grosorSuperior,
        g.padel.tamano.z * 0.78f
    );

    glDepthMask(GL_FALSE);
    dibujarObjetoGrafico(p, cubo, vidrioCentral, vidrio);
    glDepthMask(GL_TRUE);
}

/*
    La bola funcióna como objeto y como fuente de luz. Se dibuja con material
    emisivo y, en el bucle principal, su posición se envía al shader como
    uPosicionLuz para que ilumine bloques, paredes y padel desde su ubicacion.
*/
void renderizarBola(GLuint p, const Malla& esfera, const Juego& g) {
    Transformacion t;
    t.traslacion=g.bola.posicion;
    t.escala=glm::vec3(g.bola.radio);
    t.usarEjeAngulo=1;
    t.eje=g.bola.ejeGiro;
    t.anguloEje=g.bola.anguloGiro;
    Material m{{0.08f,0.90f,1.0f},1.0f,256.0f,0.05f,1.15f};
    m.modoBola=1;
    dibujarObjetoGrafico(p,esfera,t,m);
}

std::vector<std::string> glifo(char ch) {
    switch (ch) {
        case 'A': return {"01110","10001","10001","11111","10001","10001","10001"};
        case 'C': return {"01111","10000","10000","10000","10000","10000","01111"};
        case 'E': return {"11111","10000","10000","11110","10000","10000","11111"};
        case 'G': return {"01111","10000","10000","10111","10001","10001","01110"};
        case 'I': return {"11111","00100","00100","00100","00100","00100","11111"};
        case 'M': return {"10001","11011","10101","10101","10001","10001","10001"};
        case 'N': return {"10001","11001","10101","10011","10001","10001","10001"};
        case 'O': return {"01110","10001","10001","10001","10001","10001","01110"};
        case 'P': return {"11110","10001","10001","11110","10000","10000","10000"};
        case 'R': return {"11110","10001","10001","11110","10100","10010","10001"};
        case 'S': return {"01111","10000","10000","01110","00001","00001","11110"};
        case 'T': return {"11111","00100","00100","00100","00100","00100","00100"};
        case 'V': return {"10001","10001","10001","10001","10001","01010","00100"};
        default:  return {"00000","00000","00000","00000","00000","00000","00000"};
    }
}

void renderizarTextoMapaBits(GLuint p, const Malla& cubo, const std::string& texto, glm::vec3 centro, float pixel, const Material& material) {
    const float anchoCaracter = pixel * 6.0f;
    const float anchoTotal = anchoCaracter * float(texto.size());
    glm::vec3 inicio = centro + glm::vec3(-anchoTotal * 0.5f, pixel * 3.0f, 0.0f);
    for (size_t i = 0; i < texto.size(); ++i) {
        std::vector<std::string> g = glifo(texto[i]);
        for (int fila = 0; fila < 7; ++fila) {
            for (int columna = 0; columna < 5; ++columna) {
                if (g[fila][columna] != '1') continue;
                Transformacion t;
                t.traslacion = inicio + glm::vec3(float(i) * anchoCaracter + float(columna) * pixel, -float(fila) * pixel, 0.0f);
                t.escala = glm::vec3(pixel * 0.82f, pixel * 0.82f, 0.035f);
                dibujarObjetoGrafico(p, cubo, t, material);
            }
        }
    }
}

void renderizarCaracterMapaBits(GLuint p, const Malla& cubo, char ch, glm::vec3 pos, float pixel, const Material& material) {
    std::vector<std::string> g = glifo(ch);

    for (int fila = 0; fila < 7; ++fila) {
        for (int columna = 0; columna < 5; ++columna) {
            if (g[fila][columna] != '1') continue;

            Transformacion t;
            t.traslacion = pos + glm::vec3(float(columna) * pixel, -float(fila) * pixel, 0.0f);
            t.escala = glm::vec3(pixel * 0.82f, pixel * 0.82f, 0.035f);
            dibujarObjetoGrafico(p, cubo, t, material);
        }
    }
}

void renderizarPalabraNeon(GLuint p,
                    const Malla& cubo,
                    const std::string& palabra,
                    glm::vec3 inicio,
                    float pixel,
                    const std::vector<Material>& mats) {
    float pasoCaracter = pixel * 6.0f;

    for (size_t i = 0; i < palabra.size(); ++i) {
        renderizarCaracterMapaBits(
            p,
            cubo,
            palabra[i],
            inicio + glm::vec3(float(i) * pasoCaracter, 0.0f, 0.0f),
            pixel,
            mats[i]
        );
    }
}

/*
    El panel final tambien se dibuja con cubos pequenos. Esto evita depender de
    una librería de texto y mantiene victoria, derrota y mensaje de reinicio
    dentro del mismo sistema de mallas, materiales y transformaciónes.
*/
void renderizarPanelFinal(GLuint p, const Malla& cubo, const Juego& g) {
    if (!g.victoria && !g.juegoTerminado) return;

    Material materialPanel;
    materialPanel.color = glm::vec3(0.01f, 0.01f, 0.02f);
    materialPanel.alfa = 1.0f;
    materialPanel.brilloEspecular = 80.0f;
    materialPanel.factorAtenuacion = 0.03f;
    materialPanel.emision = 0.02f;
    materialPanel.colorAcento = materialPanel.color;
    materialPanel.modoDegradado = 0;

    Transformacion panel;
    panel.traslacion = glm::vec3(0.0f, 0.20f, 1.90f);
    panel.escala = glm::vec3(5.0f, 3.00f, 0.10f);
    dibujarObjetoGrafico(p, cubo, panel, materialPanel);

    Material neonAzul;
    neonAzul.color = glm::vec3(0.0f, 0.0f, 1.00f);
    neonAzul.alfa = 1.0f;
    neonAzul.brilloEspecular = 260.0f;
    neonAzul.factorAtenuacion = 0.02f;
    neonAzul.emision = 1.10f;
    neonAzul.colorAcento = neonAzul.color;
    neonAzul.modoDegradado = 0;

    Material neonCian;
    neonCian.color = glm::vec3(0.00f, 1.00f, 0.90f);
    neonCian.alfa = 1.0f;
    neonCian.brilloEspecular = 260.0f;
    neonCian.factorAtenuacion = 0.02f;
    neonCian.emision = 1.10f;
    neonCian.colorAcento = neonCian.color;
    neonCian.modoDegradado = 0;

    Material neonRojo;
    neonRojo.color = glm::vec3(1.00f, 0.0f, 0.0f);
    neonRojo.alfa = 1.0f;
    neonRojo.brilloEspecular = 260.0f;
    neonRojo.factorAtenuacion = 0.02f;
    neonRojo.emision = 1.10f;
    neonRojo.colorAcento = neonRojo.color;
    neonRojo.modoDegradado = 0;

    Material neonVerde;
    neonVerde.color = glm::vec3(0.0f, 1.00f, 0.0f);
    neonVerde.alfa = 1.0f;
    neonVerde.brilloEspecular = 260.0f;
    neonVerde.factorAtenuacion = 0.02f;
    neonVerde.emision = 1.0f;
    neonVerde.colorAcento = neonVerde.color;
    neonVerde.modoDegradado = 0;

    Material textoPequeno;
    textoPequeno.color = glm::vec3(0.92f, 0.92f, 0.95f);
    textoPequeno.alfa = 1.0f;
    textoPequeno.brilloEspecular = 180.0f;
    textoPequeno.factorAtenuacion = 0.03f;
    textoPequeno.emision = 0.20f;
    textoPequeno.colorAcento = textoPequeno.color;
    textoPequeno.modoDegradado = 0;

    if (g.juegoTerminado) {
        std::vector<Material> filaSuperior = {neonAzul, neonCian, neonRojo, neonVerde};
        std::vector<Material> filaInferior = {neonAzul, neonCian, neonRojo, neonVerde};

        float pixel = 0.09f;
        float pasoCaracter = pixel * 6.0f;
        float anchoPalabra = 4.0f * pasoCaracter;

        glm::vec3 inicioSuperior(-anchoPalabra * 0.5f, 1.0f, 1.98f);
        glm::vec3 inicioInferior(-anchoPalabra * 0.5f, 0.1f, 1.98f);

        renderizarPalabraNeon(p, cubo, "GAME", inicioSuperior, pixel, filaSuperior);
        renderizarPalabraNeon(p, cubo, "OVER", inicioInferior, pixel, filaInferior);

        renderizarTextoMapaBits(
            p,
            cubo,
            "PRESIONA R",
            glm::vec3(0.0f, -0.85f, 1.98f),
            0.050f,
            textoPequeno
        );
    }
    else if (g.victoria) {

        Material textoVictoria;
        textoVictoria.color = glm::vec3(0.0f, 1.00f, 0.0f);
        textoVictoria.alfa = 1.0f;
        textoVictoria.brilloEspecular = 250.0f;
        textoVictoria.factorAtenuacion = 0.02f;
        textoVictoria.emision = 0.85f;
        textoVictoria.colorAcento = textoVictoria.color;
        textoVictoria.modoDegradado = 0;

        renderizarTextoMapaBits(
            p,
            cubo,
            "VICTORIA",
            glm::vec3(0.0f, 0.45f, 0.98f),
            0.085f,
            textoVictoria
        );

        renderizarTextoMapaBits(
            p,
            cubo,
            "PRESIONA R",
            glm::vec3(0.0f, -0.45f, 0.98f),
            0.055f,
            textoPequeno
        );
    }
}

/*
    En main se prepara GLFW, GLAD, los shaders y las mallas retenidas. Dentro del
    bucle principal se calcula delta time, se procesa entrada, se actualiza la
    física y se renderiza la escena. La cámara se define con glm::lookAt para
    mirar al cuarto, y glm::perspective crea la proyección 3D que recibe el
    vertex shader mediante uniforms.
*/
int main() {
    if (!glfwInit()) { std::cerr << "No se pudo inicializar GLFW.\n"; return -1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* ventana = glfwCreateWindow(ANCHO_VENTANA, ALTO_VENTANA, "Arkanoid 3D", nullptr, nullptr);
    if (!ventana) { std::cerr << "No se pudo crear ventana.\n"; glfwTerminate(); return -1; }
    glfwMakeContextCurrent(ventana);
    glfwSetFramebufferSizeCallback(ventana, actualizarTamanoVentana);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) { std::cerr << "No se pudo cargar GLAD.\n"; return -1; }
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_CULL_FACE);
    GLuint programa = crearProgramaShader("vertex_shader.glsl", "fragment_shader.glsl");
    if (!programa) { glfwTerminate(); return -1; }
    Malla cubo = crearMallaCubo();
    Malla esfera = crearMallaEsfera(32, 48);
    Juego juego;
    reiniciarJuego(juego);
    float tiempoAnterior = (float)glfwGetTime();
    float temporizadorTitulo = 0.0f;
    while (!glfwWindowShouldClose(ventana)) {
        float tiempoActual = (float)glfwGetTime();
        float dt = std::clamp(tiempoActual - tiempoAnterior, 0.0f, 0.033f);
        tiempoAnterior = tiempoActual;
        temporizadorTitulo += dt;
        procesarEntrada(ventana, juego, dt);
        actualizarJuego(juego, dt);
        if (temporizadorTitulo > 0.18f) { actualizarTitulo(ventana, juego); temporizadorTitulo = 0.0f; }
        glClearColor(0.005f, 0.008f, 0.030f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(programa);
        glm::vec3 posicionCamara(0.0f, -0.05f, 8.60f);
        glm::mat4 vista = glm::lookAt(posicionCamara, glm::vec3(0.0f, 0.0f, -3.30f), glm::vec3(0,1,0));
        glm::mat4 proyeccion = glm::perspective(glm::radians(58.0f), (float)anchoVentanaActual/(float)altoVentanaActual, 0.1f, 100.0f);
        glUniformMatrix4fv(glGetUniformLocation(programa,"uVista"),1,GL_FALSE,glm::value_ptr(vista));
        glUniformMatrix4fv(glGetUniformLocation(programa,"uProyeccion"),1,GL_FALSE,glm::value_ptr(proyeccion));
        glUniform3fv(glGetUniformLocation(programa,"uPosicionLuz"),1,glm::value_ptr(juego.bola.posicion));
        glUniform3fv(glGetUniformLocation(programa,"uPosicionVista"),1,glm::value_ptr(posicionCamara));
        glUniform3f(glGetUniformLocation(programa,"uLuzAmbiente"),0.10f,0.11f,0.15f);
        glUniform3f(glGetUniformLocation(programa,"uLuzDifusa"),2.10f,2.20f,2.40f);
        glUniform3f(glGetUniformLocation(programa,"uLuzEspecular"),2.5f,2.65f,2.85f);
        glDepthMask(GL_FALSE);
        renderizarCuarto(programa, cubo);
        glDepthMask(GL_TRUE);
        renderizarBloques(programa, cubo, juego);
        renderizarPadel(programa, cubo, juego);
        renderizarBola(programa, esfera, juego);
        renderizarPanelFinal(programa, cubo, juego);
        glfwSwapBuffers(ventana);
        glfwPollEvents();
    }
    eliminarMalla(cubo);
    eliminarMalla(esfera);
    glDeleteProgram(programa);
    glfwTerminate();
    return 0;
}
