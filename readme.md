# Simulador de Ataque de Arranque en Frío (Cold Boot Attack)

## Descripción

Este programa simula el efecto de un ataque de arranque en frío sobre una memoria SDRAM, basado en el modelo experimental descrito por Won et al. (2020). El simulador no requiere hardware real; opera sobre imágenes en escala de grises (formato PGM P5) y aplica un modelo de degradación de bits dependiente de la temperatura y el tiempo de decaimiento. Permite calcular la tasa de recuperación de los datos y generar la imagen resultante.

## Dependencias

- Compilador GCC (versión 4.8 o superior).
- Biblioteca matemática estándar (enlazada con `-lm`).
- Sistema operativo Linux (o cualquier sistema POSIX con GCC).

## Compilación

Ejecute el siguiente comando en la terminal:

```bash
gcc -O2 -Wall -o coldboot_sim coldboot_sim.c -lm
```

## Ejemplos

### Ejemplo 1: Ataque exitoso a -30 °C durante 5 segundos

```bash
./coldboot_sim -30 5
```

**Salida esperada:**

```text
Imagen sintetica generada y guardada en 'original.pgm'
Simulando ataque de arranque en frio:
  Temperatura del chip : -30.0 C
  Tiempo de decaimiento: 5.0 s
Imagen recuperada guardada en 'recuperada.pgm'

Resultados:
  Bits totales   : 524288
  Bits erroneos  : 300
  Tasa de recuperacion: 99.943%
```

### Ejemplo 2: Ataque sin refrigeración (temperatura ambiente a 33.6 °C)

**Comando:**

```bash
./coldboot_sim 33.6 5
```

**Salida esperada:**

```text
Imagen sintetica generada y guardada en 'original.pgm'
Simulando ataque de arranque en frio:
  Temperatura del chip : 33.6 C
  Tiempo de decaimiento: 5.0 s
Imagen recuperada guardada en 'recuperada.pgm'

Resultados:
  Bits totales   : 524288
  Bits erroneos  : 213040
  Tasa de recuperacion: 59.366%
```

### Ejemplo 3: Degradación intermedia a -20 °C durante 10 segundos

**Comando:**

```bash
./coldboot_sim -20 10
```

**Salida esperada:**

```text
Imagen sintetica generada y guardada en 'original.pgm'
Simulando ataque de arranque en frio:
  Temperatura del chip : -20.0 C
  Tiempo de decaimiento: 10.0 s
Imagen recuperada guardada en 'recuperada.pgm'

Resultados:
  Bits totales   : 524288
  Bits erroneos  : 77233
  Tasa de recuperacion: 85.269%
```
