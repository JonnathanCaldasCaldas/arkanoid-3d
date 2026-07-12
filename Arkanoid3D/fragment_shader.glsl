#version 330 core                                                        // Usa GLSL 3.30, compatible con OpenGL 3.3 en perfil Core.

in vec3 vFragPos;                                                        // Entrada interpolada: posicion del fragmento en coordenadas de mundo.
in vec3 vNormal;                                                         // Entrada interpolada: normal del fragmento en espacio mundo para iluminacion.
in vec3 vLocalPos;                                                       // Entrada interpolada: posicion local del fragmento dentro de la geometria original.
in vec3 vLocalNormal;                                                    // Entrada interpolada: normal local usada para reconocer que cara del cubo se pinta.
out vec4 FragColor;                                                      // Salida obligatoria del fragment shader: color RGBA que se escribe en el framebuffer.

uniform vec3 uLightPos;                                                  // Uniforme: posicion de la fuente de luz en espacio mundo, aqui asociada a la esfera.
uniform vec3 uViewPos;                                                   // Uniforme: posicion de la camara en espacio mundo para calcular reflejo especular.
uniform vec3 uBaseColor;                                                 // Uniforme: color base del material u objeto que se esta rasterizando.
uniform vec3 uAmbientLight;                                              // Uniforme: intensidad/color de la luz ambiental, presente aunque no haya luz directa.
uniform vec3 uDiffuseLight;                                              // Uniforme: intensidad/color de la luz difusa que depende del angulo luz-normal.
uniform vec3 uSpecularLight;                                             // Uniforme: intensidad/color de la luz especular, el brillo tipo reflejo.
uniform float uShininess;                                                // Uniforme: exponente de brillo de Phong; valores altos hacen reflejos mas pequenos.
uniform float uAttenuationScale;                                         // Uniforme: escala la perdida de intensidad luminica con la distancia al fragmento.
uniform float uAlpha;                                                    // Uniforme: canal alfa del fragmento, usado para transparencia si el blending esta activo.
uniform float uEmissive;                                                 // Uniforme: cantidad de luz propia del material, independiente de la fuente de luz.
uniform vec3 uAccentColor;                                               // Uniforme: color secundario usado en degradados, bordes y efecto visual neon.
uniform int uGradientMode;                                               // Uniforme bandera: activa un sombreado especial de degradado para bloques.
uniform int uBallMode;                                                   // Uniforme

void main() {                                                            // Punto de entrada del fragment shader; se ejecuta por cada fragmento rasterizado.
    vec3 n = normalize(vNormal);                                         // Normaliza la normal interpolada para que tenga longitud 1 antes de usar productos punto.
    vec3 l = normalize(uLightPos - vFragPos);                            // Vector direccion hacia la luz: desde el fragmento hasta la posicion de la luz.
    vec3 v = normalize(uViewPos - vFragPos);                             // Vector direccion hacia el observador: desde el fragmento hasta la camara.
    vec3 r = normalize(2.0 * max(dot(l, n), 0.0) * n - l);               // Vector reflejado de Phong usando r = 2(l.n)n - l para el calculo especular.
    float dist = length(uLightPos - vFragPos);                           // Distancia euclidiana entre el fragmento y la fuente de luz en espacio mundo.
    float attenuation = 1.0 / max(0.18, dist * dist * uAttenuationScale); // Atenuacion cuadratica: reduce luz con distancia y evita division por valores muy pequenos.
    float diff = max(dot(n, l), 0.0);                                    // Termino difuso Lambertiano: mide cuanto apunta la normal hacia la luz.
    float spec = 0.0;                                                    // Inicializa la intensidad especular en cero por si el fragmento no recibe luz frontal.
    if (diff > 0.0) spec = pow(max(dot(r, v), 0.0), uShininess);          // Si hay luz difusa, calcula brillo especular Phong segun alineacion reflejo-vista.
    vec3 objectColor = uBaseColor;                                       // Comienza el color del material con el color base enviado desde la aplicacion.
    if (uGradientMode == 1) {                                             // Si esta activo, aplica un material visual especial para bloques con degradado.
        float high = clamp(vLocalPos.y + 0.5, 0.0, 1.0);                 // Factor vertical local: convierte Y de aprox. [-0.5,0.5] a rango [0,1].
        float front = clamp(vLocalPos.z + 0.5, 0.0, 1.0);                // Factor de profundidad local: aumenta hacia la cara frontal del bloque.
        vec3 darkColor = uBaseColor * 0.38;                              // Genera una version oscura del color base para simular sombra/material volumetrico.
        vec3 midColor = mix(uBaseColor, uAccentColor, 0.28);             // Mezcla color base y acento para crear un tono intermedio del degradado.
        objectColor = mix(darkColor, midColor, high);                    // Interpola verticalmente entre color oscuro y medio usando la altura local.
        objectColor = mix(objectColor, uAccentColor, front * 0.32);      // Anade influencia del color de acento hacia el frente para dar sensacion de volumen.
        vec3 face = abs(normalize(vLocalNormal));                        // Obtiene la direccion dominante de la normal local, ignorando el signo de la cara.
        float edgeXY = max(smoothstep(0.40, 0.50, abs(vLocalPos.x)), smoothstep(0.40, 0.50, abs(vLocalPos.y))); // Mascara de borde para caras orientadas en Z.
        float edgeYZ = max(smoothstep(0.40, 0.50, abs(vLocalPos.y)), smoothstep(0.40, 0.50, abs(vLocalPos.z))); // Mascara de borde para caras orientadas en X.
        float edgeXZ = max(smoothstep(0.40, 0.50, abs(vLocalPos.x)), smoothstep(0.40, 0.50, abs(vLocalPos.z))); // Mascara de borde para caras orientadas en Y.
        float edge = face.z > 0.5 ? edgeXY : (face.x > 0.5 ? edgeYZ : edgeXZ); // Elige la mascara correcta segun la cara dominante del cubo.
        objectColor += uAccentColor * edge * 0.28;                       // Suma brillo de borde para sugerir bisel/neon sin modificar la geometria real.
    }  
    
    if (uBallMode == 1){
        vec3 local = normalize(vLocalPos);

        //Franja alrededor de la esfera
        float stripe = 1.0 - smoothstep(
            0.10,
            0.20,
            abs(local.y)
        );

        vec3 stripeColor = vec3(1.0, 0.10, 0.18);

        objectColor = mix(
            objectColor,
            stripeColor,
            stripe
        );

        // Pequeña marca adicional para reconocer mejor el giro.
        float mark = smoothstep(
            0.82,
            0.98,
            local.x
        );

        objectColor = mix(
            objectColor,
            vec3(1.0),
            mark * 0.55
        );
    }                                                                   // Termina el bloque de color procedural para el modo degradado.
    vec3 ambient = uAmbientLight * objectColor;                          // Componente ambiental: ilumina el objeto de forma constante en todas las direcciones.
    vec3 diffuse = uDiffuseLight * objectColor * diff * attenuation;      // Componente difusa: color del material modulado por Lambert y atenuacion.
    vec3 specular = uSpecularLight * spec * attenuation;                  // Componente especular: reflejo blanco/color de luz modulado por distancia.
    vec3 emissive = objectColor * uEmissive;                             // Componente emisiva: color propio que no depende de normal, luz ni camara.
    vec3 color = ambient + diffuse + specular + emissive;                 // Suma las contribuciones del modelo de iluminacion local.
    color = color / (color + vec3(1.0));                                  // Aplica tone mapping simple de Reinhard para comprimir valores HDR a rango visible.
    FragColor = vec4(color, uAlpha);                                     // Escribe el color final con transparencia alfa hacia el framebuffer.
}                                                                         // Termina el fragment shader.
