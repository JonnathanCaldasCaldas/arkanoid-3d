#version 330 core

/*
    El fragment shader aplica una iluminación tipo Phong sencilla. La posición de
    luz viene desde C++ y coincide con la posición de la bola, por eso la escena
    reacciona como si la esfera fuera una fuente luminosa móvil. También se usan
    modos de material para el degradado de bloques y la marca visual de la bola.
*/
in vec3 vPosicionFragmento;
in vec3 vNormal;
in vec3 vPosicionLocal;
in vec3 vNormalLocal;
out vec4 FragColor;

uniform vec3 uPosicionLuz;
uniform vec3 uPosicionVista;
uniform vec3 uColorBase;
uniform vec3 uLuzAmbiente;
uniform vec3 uLuzDifusa;
uniform vec3 uLuzEspecular;
uniform float uBrilloEspecular;
uniform float uFactorAtenuacion;
uniform float uAlfa;
uniform float uEmision;
uniform vec3 uColorAcento;
uniform int uModoDegradado;
uniform int uModoBola;

void main() {
    vec3 n = normalize(vNormal);
    vec3 l = normalize(uPosicionLuz - vPosicionFragmento);
    vec3 v = normalize(uPosicionVista - vPosicionFragmento);
    vec3 r = normalize(2.0 * max(dot(l, n), 0.0) * n - l);
    float distancia = length(uPosicionLuz - vPosicionFragmento);
    float atenuacion = 1.0 / max(0.18, distancia * distancia * uFactorAtenuacion);
    float difuso = max(dot(n, l), 0.0);
    float especularIntensidad = 0.0;
    if (difuso > 0.0) especularIntensidad = pow(max(dot(r, v), 0.0), uBrilloEspecular);
    vec3 colorObjeto = uColorBase;
    if (uModoDegradado == 1) {
        float altura = clamp(vPosicionLocal.y + 0.5, 0.0, 1.0);
        float frente = clamp(vPosicionLocal.z + 0.5, 0.0, 1.0);
        vec3 colorOscuro = uColorBase * 0.38;
        vec3 colorMedio = mix(uColorBase, uColorAcento, 0.28);
        colorObjeto = mix(colorOscuro, colorMedio, altura);
        colorObjeto = mix(colorObjeto, uColorAcento, frente * 0.32);
        vec3 cara = abs(normalize(vNormalLocal));
        float bordeXY = max(smoothstep(0.40, 0.50, abs(vPosicionLocal.x)), smoothstep(0.40, 0.50, abs(vPosicionLocal.y)));
        float bordeYZ = max(smoothstep(0.40, 0.50, abs(vPosicionLocal.y)), smoothstep(0.40, 0.50, abs(vPosicionLocal.z)));
        float bordeXZ = max(smoothstep(0.40, 0.50, abs(vPosicionLocal.x)), smoothstep(0.40, 0.50, abs(vPosicionLocal.z)));
        float borde = cara.z > 0.5 ? bordeXY : (cara.x > 0.5 ? bordeYZ : bordeXZ);
        colorObjeto += uColorAcento * borde * 0.28;
    }

    if (uModoBola == 1){
        vec3 local = normalize(vPosicionLocal);

        float franja = 1.0 - smoothstep(
            0.10,
            0.20,
            abs(local.y)
        );

        vec3 colorFranja = vec3(1.0, 0.10, 0.18);

        colorObjeto = mix(
            colorObjeto,
            colorFranja,
            franja
        );

        float marca = smoothstep(
            0.82,
            0.98,
            local.x
        );

        colorObjeto = mix(
            colorObjeto,
            vec3(1.0),
            marca * 0.55
        );
    }
    vec3 ambiente = uLuzAmbiente * colorObjeto;
    vec3 difusa = uLuzDifusa * colorObjeto * difuso * atenuacion;
    vec3 especular = uLuzEspecular * especularIntensidad * atenuacion;
    vec3 emision = colorObjeto * uEmision;
    vec3 color = ambiente + difusa + especular + emision;
    color = color / (color + vec3(1.0));
    FragColor = vec4(color, uAlfa);
}
