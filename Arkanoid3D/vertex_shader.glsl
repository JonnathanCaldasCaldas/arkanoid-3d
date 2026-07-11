#version 330 core                                                        // Usa GLSL 3.30, compatible con OpenGL 3.3 en perfil Core.

layout (location = 0) in vec3 aPosition;                                 // Atributo de vertice 0: posicion local del vertice recibida desde el VAO/VBO.
layout (location = 1) in vec3 aNormal;                                   // Atributo de vertice 1: vector normal local usado para iluminacion por fragmento.

uniform mat4 uView;                                                       // Uniforme matriz View: transforma coordenadas de mundo al espacio de camara.
uniform mat4 uProjection;                                                 // Uniforme matriz Projection: aplica perspectiva y lleva de camara a clip space.
uniform vec3 uTranslation;                                                // Uniforme de traslacion: desplazamiento del modelo en coordenadas de mundo.
uniform vec3 uRotation;                                                   // Uniforme de rotacion Euler: angulos en radianes para rotar en X, Y y Z.
uniform vec3 uScale;                                                      // Uniforme de escala: factores de escalamiento no uniforme en los ejes X, Y y Z.
uniform int uUseAxisAngle;                                                // Uniforme bandera: decide si se reemplaza la rotacion Euler por rotacion eje-angulo.
uniform vec3 uAxis;                                                       // Uniforme eje: direccion del eje de rotacion para la formula eje-angulo.
uniform float uAxisAngle;                                                 // Uniforme angulo: cantidad de giro en radianes alrededor del eje indicado.

out vec3 vFragPos;                                                        // Salida al fragment shader: posicion del vertice interpolada en espacio mundo.
out vec3 vNormal;                                                         // Salida al fragment shader: normal transformada e interpolada para calculo de luz.
out vec3 vLocalPos;                                                       // Salida al fragment shader: posicion original local para efectos por geometria/cara.
out vec3 vLocalNormal;                                                    // Salida al fragment shader: normal original local para identificar la cara del bloque.

mat4 translationMatrix(vec3 t) {                                          // Funcion auxiliar que construye una matriz de traslacion 4x4 homogenea.
    mat4 M = mat4(1.0);                                                   // Crea una matriz identidad: deja vertices sin cambios antes de insertar la traslacion.
    M[3] = vec4(t, 1.0);                                                  // En GLSL las matrices son por columnas; la columna 3 guarda tx, ty, tz y w=1.
    return M;                                                            // Devuelve la matriz T para mover el objeto en el espacio de mundo.
}                                                                         // Termina la funcion translationMatrix.

mat4 scaleMatrix(vec3 s) {                                                // Funcion auxiliar que construye una matriz de escala 4x4.
    mat4 M = mat4(1.0);                                                   // Inicia con identidad para conservar los terminos que no escalan.
    M[0][0] = s.x;                                                        // Coloca el factor de escala sobre el eje X en la diagonal principal.
    M[1][1] = s.y;                                                        // Coloca el factor de escala sobre el eje Y en la diagonal principal.
    M[2][2] = s.z;                                                        // Coloca el factor de escala sobre el eje Z en la diagonal principal.
    return M;                                                            // Devuelve la matriz S para cambiar el tamano del modelo.
}                                                                         // Termina la funcion scaleMatrix.

mat4 rotateX(float a) {                                                   // Funcion auxiliar que crea una matriz de rotacion alrededor del eje X.
    float c = cos(a);                                                     // Calcula coseno del angulo; controla la componente conservada durante el giro.
    float s = sin(a);                                                     // Calcula seno del angulo; controla el intercambio entre los ejes Y y Z.
    return mat4(1.0,0.0,0.0,0.0, 0.0,c,s,0.0, 0.0,-s,c,0.0, 0.0,0.0,0.0,1.0); // Devuelve Rx en orden de columnas, como espera GLSL.
}                                                                         // Termina la funcion rotateX.

mat4 rotateY(float a) {                                                   // Funcion auxiliar que crea una matriz de rotacion alrededor del eje Y.
    float c = cos(a);                                                     // Calcula coseno del angulo para la matriz de giro.
    float s = sin(a);                                                     // Calcula seno del angulo para mezclar coordenadas X y Z.
    return mat4(c,0.0,-s,0.0, 0.0,1.0,0.0,0.0, s,0.0,c,0.0, 0.0,0.0,0.0,1.0); // Devuelve Ry en formato column-major propio de OpenGL/GLSL.
}                                                                         // Termina la funcion rotateY.

mat4 rotateZ(float a) {                                                   // Funcion auxiliar que crea una matriz de rotacion alrededor del eje Z.
    float c = cos(a);                                                     // Calcula coseno del angulo para las componentes X e Y.
    float s = sin(a);                                                     // Calcula seno del angulo para rotar en el plano XY.
    return mat4(c,s,0.0,0.0, -s,c,0.0,0.0, 0.0,0.0,1.0,0.0, 0.0,0.0,0.0,1.0); // Devuelve Rz manteniendo Z constante y girando X/Y.
}                                                                         // Termina la funcion rotateZ.

mat4 axisAngleMatrix(vec3 axis, float angle) {                            // Funcion auxiliar que construye una rotacion eje-angulo con la formula de Rodrigues.
    vec3 a = normalize(axis);                                             // Normaliza el eje para que represente solo direccion y no cambie la escala.
    float c = cos(angle);                                                 // Calcula coseno del angulo para los terminos principales de la matriz.
    float s = sin(angle);                                                 // Calcula seno del angulo para los terminos cruzados de la rotacion.
    float oc = 1.0 - c;                                                   // Calcula 1-coseno, valor usado por la formula eje-angulo.
    return mat4(oc*a.x*a.x+c,     oc*a.x*a.y+a.z*s, oc*a.x*a.z-a.y*s, 0.0, // Columna 0: define como el eje X local contribuye al X/Y/Z rotado.
                oc*a.x*a.y-a.z*s, oc*a.y*a.y+c,     oc*a.y*a.z+a.x*s, 0.0, // Columna 1: define como el eje Y local contribuye al X/Y/Z rotado.
                oc*a.x*a.z+a.y*s, oc*a.y*a.z-a.x*s, oc*a.z*a.z+c,     0.0, // Columna 2: define como el eje Z local contribuye al X/Y/Z rotado.
                0.0,               0.0,               0.0,             1.0); // Columna 3: mantiene coordenadas homogeneas sin traslacion.
}                                                                         // Termina la funcion axisAngleMatrix.

void main() {                                                             // Punto de entrada del vertex shader; se ejecuta una vez por cada vertice.
    mat4 T = translationMatrix(uTranslation);                             // Calcula la matriz de traslacion del modelo usando el uniforme enviado por CPU.
    mat4 S = scaleMatrix(uScale);                                         // Calcula la matriz de escala para deformar o ajustar el tamano del objeto.
    mat4 R = rotateZ(uRotation.z) * rotateY(uRotation.y) * rotateX(uRotation.x); // Compone rotaciones Euler; por multiplicacion, X se aplica primero, luego Y y Z.
    if (uUseAxisAngle == 1) R = axisAngleMatrix(uAxis, uAxisAngle);        // Si la bandera esta activa, usa rotacion eje-angulo en lugar de Euler.
    mat4 model = T * R * S;                                               // Forma la matriz Model; transforma de espacio local a espacio mundo.
    vec4 worldPosition = model * vec4(aPosition, 1.0);                    // Convierte la posicion local a homogenea y la lleva a coordenadas de mundo.
    vFragPos = worldPosition.xyz;                                         // Pasa la posicion mundial al fragment shader para calcular luz/distancias.
    vNormal = normalize(mat3(transpose(inverse(model))) * aNormal);        // Usa la matriz normal: inversa transpuesta corrige normales con escala no uniforme.
    vLocalPos = aPosition;                                                // Conserva posicion local sin transformar para patrones, degradados o seleccion por cara.
    vLocalNormal = aNormal;                                               // Conserva normal local sin transformar para saber la orientacion original de la cara.
    gl_Position = uProjection * uView * worldPosition;                    // Calcula clip space: Projection * View * Model * posicion, salida obligatoria del shader.
}                                                                         // Termina el vertex shader.
