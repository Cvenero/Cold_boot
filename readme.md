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
