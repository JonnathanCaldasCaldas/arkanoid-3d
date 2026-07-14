#version 330 core

/*
    El vertex shader recibe posiciones y normales locales desde el VAO. Con los
    uniforms de transformación arma la matriz de modelo, calcula la posición en
    mundo y transforma la normal con la inversa transpuesta. Esa normal corregida
    es importante porque varios objetos usan escala no uniforme.
*/
layout (location = 0) in vec3 aPosicion;
layout (location = 1) in vec3 aNormal;

uniform mat4 uVista;
uniform mat4 uProyeccion;
uniform vec3 uTraslacion;
uniform vec3 uRotacion;
uniform vec3 uEscala;
uniform int uUsarEjeAngulo;
uniform vec3 uEje;
uniform float uAnguloEje;

out vec3 vPosicionFragmento;
out vec3 vNormal;
out vec3 vPosicionLocal;
out vec3 vNormalLocal;

mat4 matrizTraslacion(vec3 t) {
    mat4 M = mat4(1.0);
    M[3] = vec4(t, 1.0);
    return M;
}

mat4 matrizEscala(vec3 s) {
    mat4 M = mat4(1.0);
    M[0][0] = s.x;
    M[1][1] = s.y;
    M[2][2] = s.z;
    return M;
}

mat4 rotarX(float a) {
    float c = cos(a);
    float s = sin(a);
    return mat4(1.0,0.0,0.0,0.0, 0.0,c,s,0.0, 0.0,-s,c,0.0, 0.0,0.0,0.0,1.0);
}

mat4 rotarY(float a) {
    float c = cos(a);
    float s = sin(a);
    return mat4(c,0.0,-s,0.0, 0.0,1.0,0.0,0.0, s,0.0,c,0.0, 0.0,0.0,0.0,1.0);
}

mat4 rotarZ(float a) {
    float c = cos(a);
    float s = sin(a);
    return mat4(c,s,0.0,0.0, -s,c,0.0,0.0, 0.0,0.0,1.0,0.0, 0.0,0.0,0.0,1.0);
}

mat4 matrizEjeAngulo(vec3 eje, float angulo) {
    vec3 a = normalize(eje);
    float c = cos(angulo);
    float s = sin(angulo);
    float oc = 1.0 - c;
    return mat4(oc*a.x*a.x+c,     oc*a.x*a.y+a.z*s, oc*a.x*a.z-a.y*s, 0.0,
                oc*a.x*a.y-a.z*s, oc*a.y*a.y+c,     oc*a.y*a.z+a.x*s, 0.0,
                oc*a.x*a.z+a.y*s, oc*a.y*a.z-a.x*s, oc*a.z*a.z+c,     0.0,
                0.0,               0.0,               0.0,             1.0);
}

void main() {
    mat4 T = matrizTraslacion(uTraslacion);
    mat4 S = matrizEscala(uEscala);
    mat4 R = rotarZ(uRotacion.z) * rotarY(uRotacion.y) * rotarX(uRotacion.x);
    if (uUsarEjeAngulo == 1) R = matrizEjeAngulo(uEje, uAnguloEje);
    mat4 model = T * R * S;
    vec4 posicionMundo = model * vec4(aPosicion, 1.0);
    vPosicionFragmento = posicionMundo.xyz;
    vNormal = normalize(mat3(transpose(inverse(model))) * aNormal);
    vPosicionLocal = aPosicion;
    vNormalLocal = aNormal;
    gl_Position = uProyeccion * uVista * posicionMundo;
}
