#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/mman.h>

int main(){

    const int TAMANOMEMORIA = sizeof(char)*2; // tamaño de la memoria compartida
    const char *nombreMemoria = "/CINTA"; // nombre de la memoria compartida
    int fdMemoria; // descriptor de archivo del área de memoria compartida
    char *ptr; // puntero al área de memoria compartida

    // Se abre la memoria compartida
    fdMemoria = shm_open(nombreMemoria, O_RDWR, 0666);
    if (fdMemoria == -1) {
        perror("Error: No se pudo abrir la memoria compartida.\n");
        return 1;
    }

    // Mapear la memoria compartida
    ptr = mmap(0, TAMANOMEMORIA, PROT_READ, MAP_SHARED, fdMemoria, 0);
    if (ptr == MAP_FAILED) {
        perror("Error: No se pudo mapear la memoria compartida.\n");
        return 7;
    }

    // Leer de la memoria compartida
    printf("Robot empaqueta productos: %s\n",(char *)ptr);

    // Eliminar el objeto de memoria compartida
    shm_unlink(nombreMemoria);
    return 0;
}