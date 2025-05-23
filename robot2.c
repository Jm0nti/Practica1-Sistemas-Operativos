#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <semaphore.h>

int main() {
    // Definiciones de memoria
    const char *path = "/CINTA";
    const int DATA_SIZE = sizeof(char)*2; 

    //Nombre de los semáforos
    const char *nomsemprod = "/SEMPROD";
    const char *nomsemr1 = "/SEMR1";
    const char *nomsemr2 = "/SEMR2";
    const char *nomsemr3 = "/SEMR3";

    // Abrir semáforos 
    sem_t *semprod = sem_open(nomsemprod, O_CREAT, 0666, 0);
    sem_t *semr1 = sem_open(nomsemr1, O_CREAT, 0666, 0);
    sem_t *semr2 = sem_open(nomsemr2, O_CREAT, 0666, 0);
    sem_t *semr3 = sem_open(nomsemr3, O_CREAT, 0666, 0);

    if (semprod == SEM_FAILED || semr1 == SEM_FAILED || semr2 == SEM_FAILED || semr3 == SEM_FAILED) {
        perror("Falla sem_open en robot");
        return 1;
    }

    // Nota: el robot NO ejecuta shm_unlink().

    // Abrir el área de memoria compartida 
    // Usamos O_RDWR porque el robot va a leer y luego "eliminar" (escribir)
    int fd = shm_open(path, O_RDWR, 0666);
    if (fd < 0) {
        perror("Falla shm_open() en robot");
        sem_close(semprod);
        sem_close(semr1);
        sem_close(semr2);
        sem_close(semr3);
        return 2;
    }

    // Mapear la memoria compartida
    // Usamos PROT_READ | PROT_WRITE porque leeremos y luego limpiaremos la memoria
    void *ptr = mmap(
        NULL,
        DATA_SIZE,
        PROT_READ | PROT_WRITE, // Robot lee y escribe para limpiar
        MAP_SHARED,
        fd,
        0
    );

    if (ptr == MAP_FAILED) {
        perror("Falla mmap() en robot");
        close(fd);
        sem_close(semprod);
        sem_close(semr1);
        sem_close(semr2);
        sem_close(semr3);
        return 3;
    }

    // Buffer local para leer el string +1 para el caracter nulo
    char buffer[DATA_SIZE + 1];

    //---------------------
    //Tuberia a fabrica
    //---------------------

    const char *tuberiaRobot2 = "/tmp/tuberiaRobot2";
    
    int fdr2 = open(tuberiaRobot2, O_RDONLY);
    if (fdr2 == -1) {
        perror("Error: No se pudo abrir la tuberia para el robot 2.\n");
        return 4;
    }
    
    // Leer N desde la tuberia
    int N;
    read(fdr2, &N, sizeof(N));
    
    close(fdr2);
    
    printf("Robot 2: Recibido N = %d\n", N);
    
    printf("Robot 2 activo...\n");
    
    //------------------------
    // Robot 2 empaca SOLO AC
    //------------------------

    int cp = 0; // Contador de productos empacados por el robot 2

    for (int i = 0; i < N; i++) {
        sem_wait(semr2); // Esperar a que el productor haya escrito algo

        buffer[DATA_SIZE] = '\0'; // Añadir terminador nulo para imprimir como string
        // Copiar los datos de la memoria compartida al buffer local
        memcpy(buffer, ptr, DATA_SIZE);

        if (strcmp(buffer, "ZZ") == 0) { //Si se finaliza la producción
           
            printf("Producción terminada (ZZ)\n");

            // Avisar al robot 3 que hay un nuevo dato, no se elimina la información (robot 3 lee la señal de terminación)
            sem_post(semr3);

            break; // Terminar si se recibe el string de terminación
        
        }else { //Si los productos son AC, actúa
            
            printf("Empacado: %s\n", buffer);
    
            cp++;

            memset(ptr, 0, DATA_SIZE); // Limpiar buffer de memoria compartida

            sleep(1); 
    
            sem_post(semprod); // Avisar al productor que el espacio de memoria esta libre

        }

    }

    // Enviar cp a fabrica.c
    fdr2 = open(tuberiaRobot2, O_WRONLY);
    if (fdr2 == -1) {
        perror("Error: No se pudo abrir la tuberia para el robot 2.\n");
        return 5;
    }
    
    write(fdr2, &cp, sizeof(cp)); // Enviar cp al proceso padre (fabrica)
    close(fdr2); 
    printf("Robot 2: Productos empacados: %d\n", cp);

    // "Desmapear" el área de memoria
    if (munmap(ptr, DATA_SIZE) == -1) {
        perror("Falla munmap() en robot");
        return 6;
    }

    // Cerrar el descriptor de archivo
    if (close(fd) == -1) {
        perror("Falla close(fd) en robot");
        return 7;
    }

    // Cerrar los semáforos
    sem_close(semprod);
    sem_close(semr1);
    sem_close(semr2);

    printf("Robot 2 termina.\n");
    return 0;
}