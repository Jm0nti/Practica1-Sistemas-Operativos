#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <semaphore.h>

int main() {
    const char *path = "/CINTA";
    const int DATA_SIZE = sizeof(char)*2; // Debe ser el mismo tamano que en el productor
    const char *nomsemprod = "/SEMPROD";
    const char *nomsemr3 = "/SEMR3";
    const char *nomsemr2 = "/SEMR2";

    //const int num_items_to_consume = 3; // HACER TUBERIA PARA LEER N DE fabrica.c
    /* ------- NO ENTIENDO ESA LINEA 15 (la de arriba), CREO QUE SOBRA  (?) ----------*/

    // Abrir semaforos 
    sem_t *semprod = sem_open(nomsemprod, O_CREAT, 0666, 0);
    sem_t *semr3 = sem_open(nomsemr3, O_CREAT, 0666, 0);
    sem_t *semr2 = sem_open(nomsemr2, O_CREAT, 0666, 0);

    if (semprod == SEM_FAILED || semr3 == SEM_FAILED || semr2 == SEM_FAILED) {
        perror("Falla sem_open en consumidor");
        return 1;
    }

    // El consumidor NO hace shm_unlink() al inicio.

    // Abrir el area de memoria compartida 
    // Usamos O_RDWR porque el consumidor va a leer y luego "eliminar" (escribir)
    int fd = shm_open(path, O_RDWR, 0666);
    if (fd < 0) {
        perror("Falla shm_open() en consumidor");
        sem_close(semprod);
        sem_close(semr3);
        sem_close(semr2);
        return 1;
    }

    // Mapear la memoria compartida
    // Usamos PROT_READ | PROT_WRITE porque leeremos y luego limpiaremos la memoria
    void *ptr = mmap(
        NULL,
        DATA_SIZE,
        PROT_READ | PROT_WRITE, // Consumidor lee y escribe para limpiar
        MAP_SHARED,
        fd,
        0
    );

    if (ptr == MAP_FAILED) {
        perror("Falla mmap() en consumidor");
        close(fd);
        sem_close(semprod);
        sem_close(semr3);
        sem_close(semr2);
        return 3;
    }

    printf("Consumidor activo...\n");

    // Buffer local para leer el string +1 para el caracter nulo
    char buffer[DATA_SIZE + 1];

    //---------------------
    //Tuberia a fabrica
    //---------------------

    const char *tuberiaRobot3 = "/tmp/tuberiaRobot3";

    int fdr3 = open(tuberiaRobot3, O_RDONLY);
    if (fdr3 == -1) {
        perror("Error: No se pudo abrir la tuberia para el robot 3.\n");
        return 12;
    }

    // Leer N desde la tuberia
    int N;
    read(fdr3, &N, sizeof(N));
    
    close(fdr3);

    printf("Robot 3: Recibido N = %d\n", N);

    //--------------------
    // Robot 1 empaca SOLO BC
    //--------------------

    int cp = 0; // Contador de productos empacados por el robot 1

    for (int i = 0; i < N; i++) {
        sem_wait(semr3); // Esperar a que el productor haya escrito algo

        buffer[DATA_SIZE] = '\0'; // Anadir terminador nulo para imprimir como string
        // Copiar los datos de la memoria compartida al buffer local
        memcpy(buffer, ptr, DATA_SIZE);

        if (strcmp(buffer, "ZZ") == 0) { //Si se finaliza la producción
           
            printf("Producción terminada (ZZ)\n");

            sem_post(semprod);

            break; // Terminar si se recibe el string de terminacion
        
        }else{ //Si los productos son BC actua
            
            printf("Leido: %s\n", buffer);
    
            cp++;
            // "Eliminar" el contenido de la memoria compartida escribiendo ceros (o lo que desees)
            // Esto es para cumplir con el requisito de "eliminarlo del espacio de memoria".
            memset(ptr, 0, DATA_SIZE); 
            // printf("Memoria compartida limpiada.\n"); // Para depuracion

            sleep(1); // Simular trabajo
    
            sem_post(semprod); // Avisar al productor que el slot esta libre

        }

    }

    // Enviar cp a fabrica.c
    fdr3 = open(tuberiaRobot3, O_WRONLY);
    if (fdr3 == -1) {
        perror("Error: No se pudo abrir la tuberia para el robot 1.\n");
        return 12;
    }

    
    write(fdr3, &cp, sizeof(cp)); // Enviar cp al robot 1
    close(fdr3); 
    printf("Robot 1: Productos empacados: %d\n", cp);


    // "Desmapear" el area de memoria
    if (munmap(ptr, DATA_SIZE) == -1) {
        perror("Falla munmap() en consumidor");
    }

    // Cerrar el descriptor de archivo
    if (close(fd) == -1) {
        perror("Falla close(fd) en consumidor");
    }

    // Cerrar los semaforos
    sem_close(semprod);
    sem_close(semr3);
    sem_close(semr2);

    printf("Consumidor terminado.\n");
    return 0;
}