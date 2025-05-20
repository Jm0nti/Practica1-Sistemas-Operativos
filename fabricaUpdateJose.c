#include <sys/wait.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

int main(int argc, char *argv[]) {
    
    // Verificación de entrada
    if (argc != 2) {
        perror("Error: Debe proporcionar N.\n");
        return 1;
    }

    // Verificacion de que N es digito
    // Con esta verificcacion se asegura que sea positivo

    for (int i = 0; argv[1][i] != '\0'; i++) {
        if (!isdigit(argv[1][i])) {
            perror("Error: N debe ser un número entero.\n");
            return 2;
        }
    }

    // Verificacion de que N es par
    if (atoi(argv[1]) % 2 != 0) {
        perror("Error: N debe ser un número par.\n");
        return 4;
    }

    // Descriptores de lectura y escritura tuberia anonima
    int fdTuberia[2];

    if (pipe(fdTuberia)<0) {
        perror("Fallo de pipe al crear tuberia.\n");
        return 5;
    } else {
        printf("Tuberia creada correctamente.\n");
    }

    // Creacion de proceso productor
    pid_t productor = fork();

    if (productor == 0) { // Hijo: productor
        
        close(fdTuberia[1]); // Cerrar el extremo de escritura
        
        //Datos que se van a leer desde la tuberia
        int N;
        
        read(fdTuberia[0], &N, sizeof(N)); // Leer N del padre desde la tuberia
        
        close(fdTuberia[0]); // Cerrar el extremo de lectura

        // Definición tamaño de la memoria compartida (cinta)
        const int TAMANOMEMORIA = sizeof(char)*2;

        // Nombre de la memoria compartida
        const char *nombreMemoria = "/CINTA";
        
        // descriptor de archivo del área de memoria compartida
        int fdMemoria;
        
        //puntero al area de memoria compartida
        char *ptr;

        // Se abre la memoria compartida
        fdMemoria = shm_open(nombreMemoria, O_RDWR, 0666);
        if (fdMemoria == -1) {
            perror("Error: No se pudo abrir la memoria compartida.\n");
            return 5;
        }

        // Mapear la memoria compartida
        ptr = mmap(0, TAMANOMEMORIA, PROT_READ | PROT_WRITE, MAP_SHARED, fdMemoria, 0); //DEBO REVISAR LAS FLAGS
        if (ptr == MAP_FAILED) {
            perror("Error: No se pudo mapear la memoria compartida.\n");
            return 7;
        }
        
        char *productos[3] = {"AB", "AC", "BC"}; // Productos a producir
        
        srand(time(NULL)); // Inicializar la semilla para la generación de números aleatorios
        
        for (int i = 0; i < N; i++) {
            int indice = rand() % 3; // Generar un índice aleatorio entre 0 y 2
            printf("Producido %s\n", productos[indice]);
            
            //-------------------------------------------
            //ACÁ DEBE IR LA ESCRITURA EN MEMORIA COMPARTIDA
            //escribiendo uno por uno el producto a medida que la memoria se desocupe por los robots
            //-------------------------------------------
            
            sleep(1);
        }

        //--------------------------------------------
        // ACÁ ESCRIBO SOLO UN DATO EN LA MEMORIA COMPARTIDA COMO PRUEBA

        sprintf(ptr, "%s", "PP"); // Escribir en la memoria compartida
        
        return(0); // Termina el proceso hijo
        
    } else if (productor > 0) { // Padre: consumidor
        
        // Definición tamaño de la memoria compartida (cinta)
        const int TAMANOMEMORIA = sizeof(char)*2;
        
        // Nombre de la memoria compartida
        const char *nombreMemoria = "/CINTA";
        
        // Descriptor de archivo del área de memoria compartida
        int fdMemoria;
        
        //puntero al area de memoria compartida
        void *ptr;
        
        //Eliminar la memoria compartida si ya existe
        shm_unlink(nombreMemoria);
        
        // Crear la memoria compartida
        fdMemoria = shm_open(nombreMemoria, O_CREAT | O_RDWR, 0666);
        if (fdMemoria == -1) {
            perror("Error: No se pudo crear la memoria compartida.\n");
            return 6;
        }
        
        // Configurar el tamaño de la memoria compartida
        if (ftruncate(fdMemoria, TAMANOMEMORIA) == -1) {
            perror("Error: No se pudo configurar el tamaño de la memoria compartida.\n");
            return 7;
        }
        
        // Mapear la memoria compartida
        
        ptr = mmap(0, TAMANOMEMORIA, PROT_READ | PROT_WRITE, MAP_SHARED, fdMemoria, 0);
        if (ptr == MAP_FAILED) {
            perror("Error: No se pudo mapear la memoria compartida.\n");
            return 8;
        }
        
        //Inicialización de N a partir de la entrada
        int N = atoi(argv[1]);
        
        //Escribir en la tuberia
        
        close(fdTuberia[0]); // Cerrar el extremo de lectura
        write(fdTuberia[1],&N,sizeof(N)); // Enviar N al hijo
        
        close(fdTuberia[1]); // Cerrar el extremo de escritura
        
        wait(NULL); // Espera al hijo
        
    } else {
        perror("Error: No se pudo crear el productor.\n");
        return 10;
    }
    
    return 0;
}
