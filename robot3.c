#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <semaphore.h>
#include <errno.h>

int main() {
    // Definiciones de memoria
    const char *path = "/CINTA";
    const int DATA_SIZE = sizeof(char)*2;

    //Nombre de los semáforos
    const char *nomsemprod = "/SEMPROD";
    const char *nomsemr3 = "/SEMR3";
    const char *nomsemr2 = "/SEMR2";

    // Abrir semáforos 
    sem_t *semprod = sem_open(nomsemprod, O_CREAT, 0666, 0);
    sem_t *semr3 = sem_open(nomsemr3, O_CREAT, 0666, 0);
    sem_t *semr2 = sem_open(nomsemr2, O_CREAT, 0666, 0);

    if (semprod == SEM_FAILED || semr3 == SEM_FAILED || semr2 == SEM_FAILED) {
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
        sem_close(semr3);
        sem_close(semr2);
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
        sem_close(semr3);
        sem_close(semr2);
        return 3;
    }
    // Buffer local para leer el string +1 para el caracter nulo
    char buffer[DATA_SIZE + 1];
    
    //---------------------
    //Tuberia a fabrica
    //---------------------

    const char *tuberiaRobot3 = "/tmp/tuberiaRobot3";

    int fdr3 = open(tuberiaRobot3, O_RDONLY);
    if (fdr3 == -1) {
        perror("Error: No se pudo abrir la tuberia para el robot 3.\n");
        munmap(ptr, DATA_SIZE);
        close(fd);
        sem_close(semprod);
        sem_close(semr3);
        sem_close(semr2);
        return 4;
    }

    printf("Robot 3: Esperando a recibir N...\n");

    // Leer N desde la tuberia
    int N;
    if (read(fdr3, &N, sizeof(N)) == -1) {
        perror("Error: No se pudo leer N desde la tuberia para el robot 3.\n");
        close(fdr3);
        munmap(ptr, DATA_SIZE);
        close(fd);
        sem_close(semprod);
        sem_close(semr3);
        sem_close(semr2);
        return 5;
    }
    
    if (close(fdr3) == -1) {
        perror("Error: No se pudo cerrar la tuberia para el robot 3 tras leer N.\n");
        munmap(ptr, DATA_SIZE);
        close(fd);
        sem_close(semprod);
        sem_close(semr3);
        sem_close(semr2);
        return 6;
    }
    
    printf("Robot 3: Recibido N = %d\n", N);
    
    printf("Robot 3 activo...\n");
    //------------------------
    // Robot 3 empaca SOLO BC
    //------------------------

    int cp = 0; // Contador de productos empacados por el robot 3

    for (int i = 0; i < N; i++) {
        if (sem_wait(semr3) == -1) { // Esperar a que el productor haya escrito algo
            perror("Falla sem_wait(semr3) en robot");
            munmap(ptr, DATA_SIZE);
            close(fd);
            sem_close(semprod);
            sem_close(semr3);
            sem_close(semr2);
            return 7;
        }

        buffer[DATA_SIZE] = '\0'; // Añadir terminador nulo para imprimir como string
        // Copiar los datos de la memoria compartida al buffer local
        memcpy(buffer, ptr, DATA_SIZE);

        if (strcmp(buffer, "ZZ") == 0) { //Si se finaliza la producción
           
            printf("Producción terminada (ZZ)\n");

            if (sem_post(semprod) == -1) { // Avisar al productor que ya todos leyeron la señal de terminación
                perror("Falla sem_post(semprod) en robot");
                munmap(ptr, DATA_SIZE);
                close(fd);
                sem_close(semprod);
                sem_close(semr3);
                sem_close(semr2);
                return 8;
            }

            break; // Terminar si se recibe el string de terminación
        
        }else{ //Si los productos son BC, actúa
            
            printf("Empacado: %s\n", buffer);
    
            cp++;

            memset(ptr, 0, DATA_SIZE);  // Limpiar buffer de memoria compartida

            //sleep(1); //"Descomentar la linea para mejorar la visualización de la ejecución
    
            if (sem_post(semprod) == -1) { // Avisar al productor que el espacio de memoria esta libre
                perror("Falla sem_post(semprod) en robot");
                munmap(ptr, DATA_SIZE);
                close(fd);
                sem_close(semprod);
                sem_close(semr3);
                sem_close(semr2);
                return 9;
            }
        }

    }

    // Enviar cp a fabrica.c
    fdr3 = open(tuberiaRobot3, O_WRONLY);
    if (fdr3 == -1) {
        perror("Error: No se pudo abrir la tuberia para el robot 3.\n");
        munmap(ptr, DATA_SIZE);
        close(fd);
        sem_close(semprod);
        sem_close(semr3);
        sem_close(semr2);
        return 10;
    }

    if (write(fdr3, &cp, sizeof(cp)) == -1) { // Enviar cp al proceso padre (fabrica)
        perror("Error: No se pudo escribir cp en la tuberia para el robot 3.\n");
        close(fdr3);
        munmap(ptr, DATA_SIZE);
        close(fd);
        sem_close(semprod);
        sem_close(semr3);
        sem_close(semr2);
        return 11;
    }
    if (close(fdr3) == -1) {
        perror("Error: No se pudo cerrar la tuberia para el robot 3 tras escribir cp.\n");
        munmap(ptr, DATA_SIZE);
        close(fd);
        sem_close(semprod);
        sem_close(semr3);
        sem_close(semr2);
        return 12;
    }
    printf("Robot 3: Productos empacados: %d\n", cp);

    // "Desmapear" el área de memoria
    if (munmap(ptr, DATA_SIZE) == -1) {
        perror("Falla munmap() en robot");
        close(fd);
        sem_close(semprod);
        sem_close(semr3);
        sem_close(semr2);
        return 13;
    }

    // Cerrar el descriptor de archivo
    if (close(fd) == -1) {
        perror("Falla close(fd) en robot");
        sem_close(semprod);
        sem_close(semr3);
        sem_close(semr2);
        return 14;
    }

    // Cerrar los semáforos
    if (sem_close(semprod) == -1) {
        perror("Falla sem_close(semprod) en robot");
        return 15;
    }
    if (sem_close(semr3) == -1) {
        perror("Falla sem_close(semr3) en robot");
        return 16;
    }
    if (sem_close(semr2) == -1) {
        perror("Falla sem_close(semr2) en robot");
        return 17;
    }

    printf("Robot 3 termina.\n");
    return 0;
}
