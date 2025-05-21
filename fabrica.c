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
#include <semaphore.h>

int main(int argc, char *argv[]) {

    // Verificacion entrada de N
    if (argc != 2) {
        perror("Error: Debe proporcionar N.\n");
        return 1;
    }

    // Verificacion de que N es digito y positivo
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

    printf("N validado correctamente.\n");

    // Descriptores de lectura y escritura tuberia anonima
    int fdTuberia[2];
    if (pipe(fdTuberia) < 0) {
        perror("Fallo de pipe al crear tuberia.\n");
        return 5;
    } else {
        printf("Tuberia creada correctamente.\n");
    }

    //-----------------------------------------------------
    // Creación de memoria compartida
    //-----------------------------------------------------

    // Definiciones de memoria
    const char *path = "/CINTA";
    const int DATA_SIZE = sizeof(char)*2;

    // Descriptores de archivo del area de memoria compartida
    int fdMemoria;
    // Puntero al area de memoria compartida
    void *ptr;
    // Eliminar la memoria compartida si ya existe
    shm_unlink(path);
    
    // Crear la memoria compartida
    fdMemoria = shm_open(path, O_CREAT | O_RDWR, 0666);
    if (fdMemoria == -1) {
            perror("Error: No se pudo crear la memoria compartida.\n");
            return 6;
        }
        
    // Configurar el tamaño de la memoria compartida
    if (ftruncate(fdMemoria, DATA_SIZE) == -1) {
            perror("Error: No se pudo configurar el tamaño de la memoria compartida.\n");
            return 7;
        }
        
    // Mapear la memoria compartida
    ptr = mmap(0, DATA_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fdMemoria, 0);
    if (ptr == MAP_FAILED) {
            perror("Error: No se pudo mapear la memoria compartida.\n");
            return 8;
        }
        
    // Crear proceso productor
    pid_t productor = fork();

    if (productor == 0) { // HIJO: PRODUCTOR

        //Nombres Semaforos
        const char *nomsemprod = "/SEMPROD";
        const char *nomsemr1 = "/SEMR1";
        const char *nomsemr2 = "/SEMR2";
        const char *nomsemr3 = "/SEMR3";
        
        // Asegurarse de que los semaforos no existan previamente (para un inicio limpio)
        sem_unlink(nomsemprod);
        sem_unlink(nomsemr1);
        sem_unlink(nomsemr2);
        sem_unlink(nomsemr3);

        // Crear semaforos
        sem_t *semprod = sem_open(nomsemprod, O_CREAT, 0666, 1);
        sem_t *semr1 = sem_open(nomsemr1, O_CREAT, 0666, 0);
        sem_t *semr2 = sem_open(nomsemr2, O_CREAT, 0666, 0);
        sem_t *semr3 = sem_open(nomsemr3, O_CREAT, 0666, 0);
        
        // Verificar que los semaforos se hayan creado correctamente
        if (semprod == SEM_FAILED || semr1 == SEM_FAILED || semr2 == SEM_FAILED || semr3 == SEM_FAILED) {
            perror("Falla sem_open en productor");
            return 1;
        }
        
        srand(time(NULL)); // Inicializar la semilla para la generación de números aleatorios
        
        // Lista de strings a producir
        const char *productos[] = {"AB", "AC", "BC"};
        
        // Cerrar extremo de escritura
        close(fdTuberia[1]);
        // Leer N desde la tuberia
        int N;
        read(fdTuberia[0], &N, sizeof(N));
        // Cerrar extremo de lectura
        close(fdTuberia[0]);
        
        printf("Productor activo...\n");

    // Producir los strings de la lista
    for (int i = 0; i <N; i++) {
        int indice = rand() % 3;
        sem_wait(semprod); // Esperar a que el slot este libre (productor puede producir)
        
        // Copiar el string actual a la memoria compartida
        memcpy(ptr, productos[indice], DATA_SIZE);
        
        // Usamos %.*s para imprimir exactamente SHM_DATA_SIZE caracteres desde ptr,
        // ya que ptr no necesariamente estara terminado en null si SHM_DATA_SIZE
        // es exactamente la longitud del string sin el terminador.
        printf("Producido: %.*s\n", DATA_SIZE, (char*)ptr);
        
        sleep(1); // Simular trabajo

        if (productos[indice] == "AB") {
            sem_post(semr1); // Avisar al consumidor que hay un nuevo dato
        } else if (productos[indice] == "AC") {
            sem_post(semr2); // Avisar al consumidor que hay un nuevo dato
        } else if (productos[indice] == "BC") {
            sem_post(semr3); // Avisar al consumidor que hay un nuevo dato
        }
        
    }
    
    sem_wait(semprod); // Esperar a que el slot este libre (productor puede producir)
    
    memcpy(ptr,"ZZ", DATA_SIZE); // Escribir el string de terminacion
    
    printf("Producción terminada (ZZ)\n");
    
    sem_post(semr1); // Avisar a robot 1 que hay un nuevo dato
    sem_wait(semprod); // Esperar a que todos lean la señal de terminacion

    // "Desmapear" el area de memoria
    if (munmap(ptr, DATA_SIZE)<0) {
        perror("Falla munmap() en productor");
        // Continuar para cerrar y desvincular recursos
    }

    // Cerrar el descriptor de archivo de la memoria compartida
    if (close(fdMemoria)<0) {
        perror("Falla close(fd) en productor");
    }

    // Cerrar los semaforos
    sem_close(semprod);
    sem_close(semr1);
    sem_close(semr2);
    sem_close(semr3);

    printf("Productor terminado.\n");

    } else if (productor > 0) { // PROCESO PADRE

        //Inicialización de N a partir de la entrada
        int N = atoi(argv[1]);

        //----------------------------------------------------
        // Tuberias para los robots
        //----------------------------------------------------

        const char *tuberiaRobot1 = "/tmp/tuberiaRobot1";
        const char *tuberiaRobot2 = "/tmp/tuberiaRobot2";
        const char *tuberiaRobot3 = "/tmp/tuberiaRobot3";

        //eliminar si existen
        unlink(tuberiaRobot1);
        unlink(tuberiaRobot2);
        unlink(tuberiaRobot3);

        if(mkfifo(tuberiaRobot1, 0666) == -1){
            perror("Error: No se pudo crear la tuberia para el robot 1.\n");
            return 9;
        }

        if(mkfifo(tuberiaRobot2, 0666) == -1){
            perror("Error: No se pudo crear la tuberia para el robot 2.\n");
            return 10;
        }

        if(mkfifo(tuberiaRobot3, 0666) == -1){
            perror("Error: No se pudo crear la tuberia para el robot 3.\n");
            return 11;
        }

        int fdr1 = open(tuberiaRobot1, O_WRONLY);
        int fdr2 = open(tuberiaRobot2, O_WRONLY);
        int fdr3 = open(tuberiaRobot3, O_WRONLY);

        if (fdr1 == -1 || fdr2 == -1 || fdr3 == -1) {
            perror("Error: No se pudo abrir la tuberia para el robot.\n");
            return 12;
        }

        write(fdr1, &N, sizeof(N)); // Enviar N al robot 1
        close(fdr1);
        write(fdr2, &N, sizeof(N)); // Enviar N al robot 2
        close(fdr2);
        write(fdr3, &N, sizeof(N)); // Enviar N al robot 3
        close(fdr3);

        //Escribir en la tuberia para hijo (productor)
        close(fdTuberia[0]); // Cerrar el extremo de lectura
        write(fdTuberia[1],&N,sizeof(N)); // Enviar N al hijo
        printf("Enviado N a productor: %d\n", N);
        close(fdTuberia[1]); // Cerrar el extremo de escritura

        //------------------------------------------------
        // De acá para abajo debe ir el codigo que abre y lee de las tuberias (se necesita semaforos para coordinar)
        //------------------------------------------------
        
        wait(NULL); // Espera al hijo
    }

    return 0;
}
