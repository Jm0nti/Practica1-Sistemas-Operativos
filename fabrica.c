#include <sys/wait.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <time.h>

int main(int argc, char *argv[]) {

    int N;
    char *productos[3] = {"AB", "AC", "BC"};
    // indice para obtener producto aleatorio
    int indice;
    // Inicializamos producidos con un tamaño de N
    char **producidos = malloc(sizeof(char*) * N);
    

    // Verificación de entrada
    if (argc != 2) {
        perror("Error: Debe proporcionar N.\n");
        return 1;
    }

    // Verificacion de que N es digito
    if (!isdigit(argv[1][0])) {
        perror("Error: N debe ser un número entero.\n");
        return 2;
    }

    N = atoi(argv[1]);

    // Verificacion de que N es positivo
    if (N <= 0) {
        perror("Error: N debe ser un número entero positivo.\n");
        return 3;
    }

    // Verificacion de que N es par
    if (N % 2 != 0) {
        perror("Error: N debe ser un número par.\n");
        return 4;
    }

    // Descriptores de lectura y escritura tuberia anonima
    int fd[2];

    if (pipe(fd)<0) {
        perror("Fallo de pipe al crear tuberia.\n");
        return 1;
    } else {
        printf("Tuberia creada correctamente.\n");
    }

    // Creacion de proceso productor
    pid_t productor = fork();

    if (productor < 0) {
        perror("Error: No se pudo crear el proceso.\n");
        return 5;
    }

    if (productor == 0) { // Hijo: productor
        srand(time(NULL));

        // Reasignamos memoria para anadir ZZ al final
        char **temp = realloc(producidos, sizeof(char*) * (N + 1)); // +1 para "ZZ"
        if (temp == NULL) {
            perror("Error: No se pudo reasignar memoria.");
            free(producidos); // Liberar memoria
            return 1;
        }
        producidos = temp;

        for (int i = 0; i < N; i++) {
            indice = rand() % 3;
            printf("Produciendo %s\n", productos[indice]);
            producidos[i] = productos[indice]; // Guardar string
            sleep(1);
        }

        producidos[N] = "ZZ"; // Agregar ZZ como último
        
        // Verificacion asignacion de memoria a producidos
        if (producidos == NULL) {
            perror("Error: No se pudo asignar memoria inicial a producidos.\n");
            return 1;
        }

        // Debugging: Imprimir contenido de cinta
        printf("Contenido de la cinta:\n");
        for (int i = 0; i <= N; i++) {
            printf("- %s\n", producidos[i]);
        }

        // Enviar producidos a la tuberia
        write(fd[1], producidos, sizeof(char*) * (N + 1));

        free(producidos); // liberar memoria
        exit(0);
        
    } else { // Padre: consumidor
        // Leer de la tuberia
        char **cinta = malloc(sizeof(char*) * (N + 1));
        if (cinta == NULL) {
            perror("Error: No se pudo asignar memoria a cinta.\n");
            return 1;
        } 
        read(fd[0], cinta, sizeof(char*) * (N + 1));   
        // Debugging: Imprimir contenido de cinta desde padre
        printf("Contenido de la cinta desde el padre:\n");
        for (int i = 0; i <= N; i++) {
            printf("- %s\n", cinta[i]);
        }
        wait(NULL); // Espera al hijo
    }

    // Cerrar descriptores de archivos tuberia
    close(fd[1]); 
    close(fd[0]); 
    
    return 0;
}
