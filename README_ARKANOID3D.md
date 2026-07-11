# Arkanoid 3D - OpenGL moderno en modo retenido

Esta versión corrige los límites visuales del padel y de la esfera, mejora el aspecto de los objetos gráficos bloque y conserva la estructura solicitada:

- `main.cpp`
- `vertex_shader.glsl`
- `fragment_shader.glsl`

## Correcciones principales

1. El padel ya no se pierde al subir o bajar.
2. La esfera se mantiene dentro de un rango visible para el jugador.
3. La cámara fue alejada y se amplió el campo de visión para ver mejor el cuarto completo.
4. Los objetos gráficos bloque ahora usan degradado, bordes neón, placa frontal y contorno 3D.
5. La pared de bloques sigue bloqueando la esfera para que no pase detrás.
6. Las transformaciones se calculan en el shader de vértices.
7. La iluminación principal sigue siendo la esfera.

## Controles

- Flecha izquierda: mover padel a la izquierda.
- Flecha derecha: mover padel a la derecha.
- Flecha arriba: subir padel dentro del límite visible.
- Flecha abajo: bajar padel dentro del límite visible.
- R: reiniciar partida.
- ESC: cerrar.

## Compilación

Ejecutar desde la carpeta principal del proyecto:

```bat
COMPILAR_ARKANOID3D.bat
```

Luego ejecutar:

```bat
EJECUTAR_ARKANOID3D.bat
```


ACTUALIZACION DEL PADEL
- Padel más largo y más ancho.
- Extremos verdes neón.
- Centro transparente.
- Se quitaron las líneas celestes horizontales del diseño anterior.
