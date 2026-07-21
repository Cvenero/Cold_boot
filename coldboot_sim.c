/*
 * El programa NO interactua con hardware real. Simula el proceso de:
 *   1) Cargar una imagen (formato PGM P5, escala de grises) en un buffer
 *      que representa el contenido de la SDRAM de la victima.
 *   2) Simular el "congelamiento" y corte de energia, aplicando un modelo
 *      de degradacion de bits (bit-flip hacia cero) en funcion de la
 *      temperatura del chip y el tiempo de decaimiento (decay time),
 *      replicando la tendencia reportada en la Fig. 4 del articulo:
 *        - A temperaturas cercanas a la ambiente (0 a 33.6 C) la tasa de
 *          recuperacion cae a niveles cercanos al minimo (~59.7%).
 *        - A -20 C y -30 C la tasa de recuperacion se mantiene por encima
 *          del 99% durante varios segundos de decaimiento.
 *   3) Calcular la tasa de recuperacion bit a bit comparando la imagen
 *      recuperada contra la original, y guardar la imagen degradada.
 *
 * Compilacion:
 *   gcc -O2 -Wall -o coldboot_sim coldboot_sim.c -lm
 *
 * Uso:
 *   ./coldboot_sim <temperatura_C> <decay_segundos> [imagen_entrada.pgm] [imagen_salida.pgm]
 *
 *   Se genera una imagen sintetica de prueba.
 *
 * Ejemplos:
 *   ./coldboot_sim -30 5
 *   ./coldboot_sim -20 10 original.pgm recuperada.pgm
 *   ./coldboot_sim --tabla 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define DEFAULT_WIDTH  256
#define DEFAULT_HEIGHT 256
#define MAX_GRAY       255

typedef struct {
    int width;
    int height;
    unsigned char *data; /* buffer que representa el contenido de la RAM */
} Imagen;

/* ---------------------------------------------------------------------
 * Generacion de una imagen sintetica de prueba.
 * --------------------------------------------------------------------- */
static Imagen generar_imagen_sintetica(int w, int h) {
    Imagen img;
    img.width = w;
    img.height = h;
    img.data = malloc((size_t)w * h);
    if (!img.data) {
        fprintf(stderr, "Error: no se pudo reservar memoria para la imagen.\n");
        exit(EXIT_FAILURE);
    }

    int cx = w / 2, cy = h / 2;
    int radio = (w < h ? w : h) / 3;

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int dx = x - cx, dy = y - cy;
            double dist = sqrt((double)(dx * dx + dy * dy));
            unsigned char valor;
            if (dist < radio) {
                /* circulo central: degradado radial (simula un rostro) */
                double t = dist / radio;
                valor = (unsigned char)(60 + t * 150);
            } else {
                /* fondo: patron de franjas para tener variedad de bits */
                valor = (unsigned char)((x ^ y) & 0xFF);
            }
            img.data[y * w + x] = valor;
        }
    }
    return img;
}

/* ---------------------------------------------------------------------
 * Lectura de una imagen PGM (formato P5, binario, escala de grises).
 * --------------------------------------------------------------------- */
static int leer_pgm(const char *ruta, Imagen *img) {
    FILE *f = fopen(ruta, "rb");
    if (!f) return -1;

    char magic[3] = {0};
    if (fscanf(f, "%2s", magic) != 1 || strcmp(magic, "P5") != 0) {
        fprintf(stderr, "Error: '%s' no es un archivo PGM P5 valido.\n", ruta);
        fclose(f);
        return -1;
    }

    int c = fgetc(f);
    /* saltar comentarios y espacios */
    while (c == '#' || c == ' ' || c == '\n' || c == '\t' || c == '\r') {
        if (c == '#') { while ((c = fgetc(f)) != '\n' && c != EOF); }
        c = fgetc(f);
    }
    ungetc(c, f);

    int w, h, maxval;
    if (fscanf(f, "%d %d %d", &w, &h, &maxval) != 3) {
        fprintf(stderr, "Error: cabecera PGM invalida en '%s'.\n", ruta);
        fclose(f);
        return -1;
    }
    fgetc(f); /* un unico caracter de espacio en blanco tras el maxval */

    img->width = w;
    img->height = h;
    img->data = malloc((size_t)w * h);
    if (!img->data) {
        fclose(f);
        return -1;
    }

    size_t leidos = fread(img->data, 1, (size_t)w * h, f);
    fclose(f);
    if (leidos != (size_t)w * h) {
        fprintf(stderr, "Advertencia: el archivo '%s' contenia menos bytes de los esperados.\n", ruta);
    }
    return 0;
}

/* ---------------------------------------------------------------------
 * Escritura de una imagen PGM (formato P5).
 * --------------------------------------------------------------------- */
static int escribir_pgm(const char *ruta, const Imagen *img) {
    FILE *f = fopen(ruta, "wb");
    if (!f) return -1;
    fprintf(f, "P5\n%d %d\n%d\n", img->width, img->height, MAX_GRAY);
    fwrite(img->data, 1, (size_t)img->width * img->height, f);
    fclose(f);
    return 0;
}

/* ---------------------------------------------------------------------
 * Modelo de degradacion de la RAM (remanencia).
 *
 * Se modela la probabilidad de que un bit se "apague" (pase a 0) en
 * funcion de la temperatura del chip y del tiempo de decaimiento,
 * calibrado para aproximar las tendencias reportadas en la Fig. 4 del
 * articulo:
 *
 *   - Temperatura ambiente / chip (0 a 33.6 C): degradacion muy rapida,
 *     la recuperacion cae hacia el piso empirico de 59.7% en pocos
 *     segundos.
 *   - -20 C: recuperacion > 99% a 1s, cayendo a ~85.5% a los 10s.
 *   - -30 C: recuperacion se mantiene ~99% independientemente del
 *     tiempo de decaimiento (dentro del rango estudiado).
 *
 * p_apagado(T, t) = p_max(T) * (1 - exp(-t / tau(T)))
 *
 *   p_max(T)  -> proporcion maxima de bits que terminan degradados
 *   tau(T)    -> constante de tiempo: mayor tau => degradacion mas lenta
 * --------------------------------------------------------------------- */
static double probabilidad_apagado(double temp_c, double decay_s) {
    double p;

    if (temp_c >= -15.0) {
        /* 0 C, -10 C y temperaturas de chip (~33.6 C): degradacion agresiva.
         * Satura rapido dejando un piso de recuperacion cercano al 59.7%
         * (minimo empirico reportado en el articulo). */
        double p_max = 0.80, tau = 0.3;
        p = p_max * (1.0 - exp(-decay_s / tau));
    } else if (temp_c >= -25.0) {
        /* rango -20 C: ~99.2% de recuperacion a 1s, cayendo a ~85.5% a 10s,
         * siguiendo una ley potencial calibrada con esos dos puntos. */
        double k = 0.016, n = 1.258;
        p = k * pow(decay_s, n);
        if (p > 0.9) p = 0.9;
    } else {
        /* -30 C o menor: degradacion muy lenta, recuperacion ~99% sostenida
         * independientemente del tiempo de decaimiento estudiado. */
        double p_max = 0.01, tau = 40.0;
        p = p_max * (1.0 - exp(-decay_s / tau));
    }

    if (p < 0.0) p = 0.0;
    if (p > 1.0) p = 1.0;
    return p;
}

/* ---------------------------------------------------------------------
 * Aplica el modelo de decaimiento bit a bit sobre una copia de la imagen
 * original, simulando el volcado (dump) de la RAM tras el ataque.
 * --------------------------------------------------------------------- */
static Imagen simular_decaimiento(const Imagen *original, double temp_c, double decay_s,
                                   unsigned long *bits_error, unsigned long *bits_total) {
    Imagen recuperada;
    recuperada.width = original->width;
    recuperada.height = original->height;
    size_t n = (size_t)original->width * original->height;
    recuperada.data = malloc(n);
    if (!recuperada.data) {
        fprintf(stderr, "Error: no se pudo reservar memoria para la imagen recuperada.\n");
        exit(EXIT_FAILURE);
    }

    double p_apagado = probabilidad_apagado(temp_c, decay_s);

    unsigned long errores = 0;
    unsigned long total_bits = n * 8;

    for (size_t i = 0; i < n; i++) {
        unsigned char byte_original = original->data[i];
        unsigned char byte_recuperado = 0;
        for (int b = 0; b < 8; b++) {
            int bit = (byte_original >> b) & 1;
            if (bit == 1) {
                double r = (double)rand() / (double)RAND_MAX;
                if (r < p_apagado) {
                    bit = 0;       /* el bit se degrado (remanencia -> 0) */
                    errores++;
                }
            }
            byte_recuperado |= (bit << b);
        }
        recuperada.data[i] = byte_recuperado;
    }

    if (bits_error) *bits_error = errores;
    if (bits_total)  *bits_total = total_bits;

    return recuperada;
}

/* ---------------------------------------------------------------------
 * Reproduce una tabla: tasa de recuperacion para distintas combinaciones 
 * de temperatura y tiempo dedecaimiento.
 * --------------------------------------------------------------------- */
static void tabla_temperatura_tiempo(void) {
    double temperaturas[] = {0.0, -10.0, -20.0, -30.0};
    double tiempos[]      = {1.0, 5.0, 10.0, 30.0, 60.0, 300.0};

    Imagen base = generar_imagen_sintetica(DEFAULT_WIDTH, DEFAULT_HEIGHT);

    printf("Tabla de tasa de recuperacion (%%) segun temperatura y tiempo de decaimiento\n");
    printf("%-12s", "Temp \\ t(s)");
    for (size_t j = 0; j < sizeof(tiempos)/sizeof(tiempos[0]); j++) {
        printf("%10.0f", tiempos[j]);
    }
    printf("\n");

    for (size_t i = 0; i < sizeof(temperaturas)/sizeof(temperaturas[0]); i++) {
        printf("%-12.1f", temperaturas[i]);
        for (size_t j = 0; j < sizeof(tiempos)/sizeof(tiempos[0]); j++) {
            unsigned long errores, total_bits;
            Imagen rec = simular_decaimiento(&base, temperaturas[i], tiempos[j], &errores, &total_bits);
            double recuperacion = 100.0 * (1.0 - (double)errores / (double)total_bits);
            printf("%10.2f", recuperacion);
            free(rec.data);
        }
        printf("\n");
    }

    free(base.data);
}

static void imprimir_uso(const char *prog) {
    fprintf(stderr,
        "Uso:\n"
        "  %s <temperatura_C> <decay_segundos> [entrada.pgm] [salida.pgm]\n"
        "  %s --tabla\n"
        "\n"
        "Si no se indica 'entrada.pgm', se genera una imagen sintetica de prueba.\n"
        "Si no se indica 'salida.pgm', se usa 'recuperada.pgm' por defecto.\n",
        prog, prog);
}

int main(int argc, char *argv[]) {
    srand((unsigned int)time(NULL));

    if (argc >= 2 && strcmp(argv[1], "--tabla") == 0) {
        tabla_temperatura_tiempo();
        return 0;
    }

    if (argc < 3) {
        imprimir_uso(argv[0]);
        return EXIT_FAILURE;
    }

    double temp_c  = atof(argv[1]);
    double decay_s = atof(argv[2]);
    const char *ruta_entrada = (argc >= 4) ? argv[3] : NULL;
    const char *ruta_salida  = (argc >= 5) ? argv[4] : "recuperada.pgm";

    Imagen original;
    if (ruta_entrada) {
        if (leer_pgm(ruta_entrada, &original) != 0) {
            fprintf(stderr, "No se pudo leer '%s'. Se usara una imagen sintetica.\n", ruta_entrada);
            original = generar_imagen_sintetica(DEFAULT_WIDTH, DEFAULT_HEIGHT);
        }
    } else {
        original = generar_imagen_sintetica(DEFAULT_WIDTH, DEFAULT_HEIGHT);
        escribir_pgm("original.pgm", &original);
        printf("Imagen sintetica generada y guardada en 'original.pgm'\n");
    }

    printf("Simulando ataque de arranque en frio:\n");
    printf("  Temperatura del chip : %.1f C\n", temp_c);
    printf("  Tiempo de decaimiento: %.1f s\n", decay_s);

    unsigned long bits_error = 0, bits_total = 0;
    Imagen recuperada = simular_decaimiento(&original, temp_c, decay_s, &bits_error, &bits_total);

    double tasa_recuperacion = 100.0 * (1.0 - (double)bits_error / (double)bits_total);

    if (escribir_pgm(ruta_salida, &recuperada) != 0) {
        fprintf(stderr, "Error al escribir '%s'\n", ruta_salida);
    } else {
        printf("Imagen recuperada guardada en '%s'\n", ruta_salida);
    }

    printf("\nResultados:\n");
    printf("  Bits totales   : %lu\n", bits_total);
    printf("  Bits erroneos  : %lu\n", bits_error);
    printf("  Tasa de recuperacion: %.3f%%\n", tasa_recuperacion);

    free(original.data);
    free(recuperada.data);
    return 0;
}