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
#include <errno.h>

int main(int argc, char *argv[]) {

    //-------------------------------------
    // Validación del N
    //-------------------------------------

    // Verificación entrada de N
    if (argc != 2) {
        perror("Error: Debe proporcionar N.\n");
        return 1;
    }

    // Verificación de que N es digito y positivo
    for (int i = 0; argv[1][i] != '\0'; i++) {
        if (!isdigit(argv[1][i])) {
            perror("Error: N debe ser un número entero y positivo.\n");
            return 2;
        }
    }

    // Verificación de que N es par
    if (atoi(argv[1]) % 2 != 0) {
        perror("Error: N debe ser un número par.\n");
        return 3;
    }

    printf("padre: N validado correctamente.\n");

    // Descriptores de lectura y escritura tubería anonima
    int fdTuberia[2];
    if (pipe(fdTuberia) == -1) {
        perror("Fallo de pipe al crear tubería.\n");
        return 4;
    } else {
        printf("padre: Tuberia a productor creada correctamente.\n");
    }

    //----------------------------------------------------
    // Creación de memoria compartida
    //----------------------------------------------------

    // Definiciones de memoria
    const char *path = "/CINTA";
    const int DATA_SIZE = sizeof(char)*2;

    // Descriptores de archivo del area de memoria compartida
    int fdMemoria;
    // Puntero al area de memoria compartida
    void *ptr;
    // Eliminar la memoria compartida si ya existe
    if (shm_unlink(path) == -1) {
        perror("Error: No se pudo eliminar la memoria compartida.\n");
        return 19;
    }
    
    // Crear la memoria compartida
    fdMemoria = shm_open(path, O_CREAT | O_RDWR, 0666);
    if (fdMemoria == -1) {
            perror("Error: No se pudo crear la memoria compartida.\n");
            return 5;
        }
        
    // Configurar el tamaño de la memoria compartida
    if (ftruncate(fdMemoria, DATA_SIZE) == -1) {
            perror("Error: No se pudo configurar el tamaño de la memoria compartida.\n");
            return 6;
        }
        
    // Mapear la memoria compartida
    ptr = mmap(0, DATA_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fdMemoria, 0);
    if (ptr == MAP_FAILED) {
            perror("Error: No se pudo mapear la memoria compartida.\n");
            return 7;
        }
    
    //-------------------------------------
    // Creación proceso productor
    //-------------------------------------

    pid_t productor = fork();

    if (productor == 0) { // HIJO: PRODUCTOR

        //-------------------------------------
        // Semáforos
        //-------------------------------------

        //Nombres semáforos
        const char *nomsemprod = "/SEMPROD";
        const char *nomsemr1 = "/SEMR1";
        const char *nomsemr2 = "/SEMR2";
        const char *nomsemr3 = "/SEMR3";
        
        // Asegurarse de que los semáforos no existan previamente (para un inicio limpio)
        int unlinkSemProd = sem_unlink(nomsemprod);
        int unlinkSemR1 = sem_unlink(nomsemr1);
        int unlinkSemR2 = sem_unlink(nomsemr2);
        int unlinkSemR3 = sem_unlink(nomsemr3);

        if ((unlinkSemProd == -1 && errno != ENOENT) ||
            (unlinkSemR1 == -1 && errno != ENOENT) ||
            (unlinkSemR2 == -1 && errno != ENOENT) ||
            (unlinkSemR3 == -1 && errno != ENOENT)) {
            perror("Error: No se pudo eliminar uno o más semáforos.\n");
            return 21;
        }

        // Crear semáforos
        sem_t *semprod = sem_open(nomsemprod, O_CREAT, 0666, 1);
        sem_t *semr1 = sem_open(nomsemr1, O_CREAT, 0666, 0);
        sem_t *semr2 = sem_open(nomsemr2, O_CREAT, 0666, 0);
        sem_t *semr3 = sem_open(nomsemr3, O_CREAT, 0666, 0);
        
        // Verificar que los semáforos se hayan creado correctamente
        if (semprod == SEM_FAILED || semr1 == SEM_FAILED || semr2 == SEM_FAILED || semr3 == SEM_FAILED) {
            perror("Falla sem_open en productor");
            return 8;
        }
        
        srand(time(NULL)); // Inicializar la semilla para la generación de números aleatorios
        
        // Lista de strings a producir
        const char *productos[] = {"AB", "AC", "BC"};
        
        //-------------------------------------------
        // Recibimiento de N desde el proceso padre
        //-------------------------------------------

        printf("hijo: Esperando a recibir N desde fabrica\n");

        // Cerrar extremo de escritura
        if(close(fdTuberia[1]) == -1) {
            perror("Fallo close(fdTuberia[1]) en productor");
            return 20;
        }
        // Leer N desde la tubería
        int N;
        
        if (read(fdTuberia[0], &N, sizeof(N)) == -1) {
            perror("Fallo read(fdTuberia[0]) en productor");
            return 29;
        }

        // Cerrar extremo de lectura
        close(fdTuberia[0]);
        
        printf("Productor activo...\n");

    // Producir los strings de la lista
    for (int i = 0; i <N; i++) {
        int indice = rand() % 3;
        
        if(sem_wait(semprod) == -1) { // Esperar a que el espacio de memoria compartida esté libre (productor puede producir)
            perror("Falla sem_wait en productor");
            return 22;
        }
        
        // Copiar el string actual a la memoria compartida
        memcpy(ptr, productos[indice], DATA_SIZE);
        
        // Usamos %.*s para imprimir exactamente DATA_SIZE caracteres desde ptr,
        // ya que ptr no necesariamente estará terminado en null si DATA_SIZE
        // es exactamente la longitud del string sin el terminador ('\0').
        printf("Producido: %.*s\n", DATA_SIZE, (char*)ptr);

        //"Descomentar la siguiente linea para mejorar la visualización de la ejecución
        sleep(1); // Simular trabajo

        if (productos[indice] == "AB") { // Avisar a robot 1 que hay un nuevo producto para él
            if (sem_post(semr1) == -1) {
                perror("Falla sem_post(semr1) en productor");
                return 25;
            }
        } else if (productos[indice] == "AC") { // Avisar a robot 2 que hay un nuevo producto para él
            if (sem_post(semr2) == -1) {
                perror("Falla sem_post(semr2) en productor");
                return 26;
            }
        } else if (productos[indice] == "BC") { // Avisar a robot 3 que hay un nuevo producto para él
            if (sem_post(semr3) == -1) {
                perror("Falla sem_post(semr3) en productor");
                return 27;
            }
        }
        
    }
    
    if(sem_wait(semprod) == -1) { // Esperar a que el espacio de memoria compartida esté libre (productor puede producir)
            perror("Falla sem_wait en productor");
            return 23;
        }
    
    memcpy(ptr,"ZZ", DATA_SIZE); // Escribir el string de terminación
    
    printf("Producción terminada (ZZ)\n");
    
    if (sem_post(semr1) == -1) { // Avisar a robot 1 que hay un nuevo dato
        perror("Falla sem_post(semr1) en productor");
        return 30;
    }

    if(sem_wait(semprod) == -1) { // Esperar a que todos lean la señal de terminación
            perror("Falla sem_wait en productor");
            return 24;
        } 
    
    // Cierre y desvinculación de recursos

    // "Desmapear" el area de memoria
    if (munmap(ptr, DATA_SIZE) == -1) {
        perror("Falla munmap() en productor");
        return 9;
    }

    // Cerrar el descriptor de archivo de la memoria compartida
    if (close(fdMemoria) == -1) {
        perror("Falla close(fd) en productor");
        return 10;
    }

    // Cerrar los semáforos
    int semCloseProd = sem_close(semprod);
    int semCloseR1 = sem_close(semr1);
    int semCloseR2 = sem_close(semr2);
    int semCloseR3 = sem_close(semr3);

    if (semCloseProd == -1 || semCloseR1 == -1 || semCloseR2 == -1 || semCloseR3 == -1) {
        perror("Falla sem_close en productor");
        return 28;
    }

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
        int unlink1 = unlink(tuberiaRobot1);
        int unlink2 = unlink(tuberiaRobot2);
        int unlink3 = unlink(tuberiaRobot3);
        if ((unlink1 == -1 && errno != ENOENT) ||
            (unlink2 == -1 && errno != ENOENT) ||
            (unlink3 == -1 && errno != ENOENT)) {
            perror("Error: No se pudo eliminar una o más tuberías para los robots.\n");
            return 47;
        }

        if(mkfifo(tuberiaRobot1, 0666) == -1){
            perror("Error: No se pudo crear la tubería para el robot 1.\n");
            return 11;
        }

        if(mkfifo(tuberiaRobot2, 0666) == -1){
            perror("Error: No se pudo crear la tubería para el robot 2.\n");
            return 12;
        }

        if(mkfifo(tuberiaRobot3, 0666) == -1){
            perror("Error: No se pudo crear la tubería para el robot 3.\n");
            return 13;
        }

        printf("padre: Esperando a robots para enviarles N\n");
        
        int fdr1 = open(tuberiaRobot1, O_WRONLY);
        int fdr2 = open(tuberiaRobot2, O_WRONLY);
        int fdr3 = open(tuberiaRobot3, O_WRONLY);

        if (fdr1 == -1 || fdr2 == -1 || fdr3 == -1) {
            perror("Error: No se pudo abrir la tubería para algún robot.\n");
            return 14;
        }

        //--------------------------------------------------
        // Envio de N a los robots a través de sus tuberías
        //--------------------------------------------------

        if (write(fdr1, &N, sizeof(N)) == -1) { // Enviar N al robot 1
            perror("Error: No se pudo escribir en la tubería para el robot 1.\n");
            return 37;
        }
        if (close(fdr1) == -1) {
            perror("Error: No se pudo cerrar la tubería para el robot 1.\n");
            return 38;
        }
        if (write(fdr2, &N, sizeof(N)) == -1) { // Enviar N al robot 2
            perror("Error: No se pudo escribir en la tubería para el robot 2.\n");
            return 39;
        }
        if (close(fdr2) == -1) {
            perror("Error: No se pudo cerrar la tubería para el robot 2.\n");
            return 40;
        }
        if (write(fdr3, &N, sizeof(N)) == -1) { // Enviar N al robot 3
            perror("Error: No se pudo escribir en la tubería para el robot 3.\n");
            return 41;
        }
        if (close(fdr3) == -1) {
            perror("Error: No se pudo cerrar la tubería para el robot 3.\n");
            return 42;
        }

        printf("Enviado N a robots: %d\n", N);

        //---------------------------------------------------------------
        // Envio de N al proceso hijo (productor) a través de la tubería
        //---------------------------------------------------------------

        //Escribir en la tubería para hijo (productor)
        if (close(fdTuberia[0]) == -1) { // Cerrar el extremo de lectura
            perror("Error: No se pudo cerrar el extremo de lectura de la tubería para el productor.\n");
            return 43;
        }
        if (write(fdTuberia[1], &N, sizeof(N)) == -1) { // Enviar N al hijo
            perror("Error: No se pudo escribir en la tubería para el productor.\n");
            return 44;
        }
        printf("Enviado N a productor: %d\n", N);
        if (close(fdTuberia[1]) == -1) { // Cerrar el extremo de escritura
            perror("Error: No se pudo cerrar el extremo de escritura de la tubería para el productor.\n");
            return 45;
        }

        //-------------------------------------------
        // Desúes de terminar la producción
        //-------------------------------------------
        
        // Recibir la cantidad de productos (cp) de los robots
        int cp1, cp2, cp3;
        fdr1 = open(tuberiaRobot1, O_RDONLY);
        if (fdr1 == -1) {
            perror("Error: No se pudo abrir la tubería para el robot 1.\n");
            return 15;
        }
        
        if(read(fdr1, &cp1, sizeof(cp1)) == -1) { // Recibir cp del robot 1
            perror("Error: No se pudo leer la tubería para el robot 1.\n");
            return 31;
        }

        if (close(fdr1) == -1) {
            perror("Error: No se pudo cerrar la tubería para el robot 1.\n");
            return 32;
        }

        fdr2 = open(tuberiaRobot2, O_RDONLY);
        if (fdr2 == -1) {
            perror("Error: No se pudo abrir la tubería para el robot 2.\n");
            return 16;
        }
        
        if (read(fdr2, &cp2, sizeof(cp2)) == -1) { // Recibir cp del robot 2
            perror("Error: No se pudo leer la tubería para el robot 2.\n");
            return 33;
        }
        
        if (close(fdr2) == -1) {
            perror("Error: No se pudo cerrar la tubería para el robot 2.\n");
            return 34;
        }

        fdr3 = open(tuberiaRobot3, O_RDONLY);
        if (fdr3 == -1) {
            perror("Error: No se pudo abrir la tubería para el robot 3.\n");
            return 17;
        }
        
        if (read(fdr3, &cp3, sizeof(cp3)) == -1) { // Recibir cp del robot 3
            perror("Error: No se pudo leer la tubería para el robot 3.\n");
            return 35;
        }

        if (close(fdr3) == -1) {
            perror("Error: No se pudo cerrar la tubería para el robot 3.\n");
            return 36;
        }

        printf("Robot 1: Productos empacados: %d\n", cp1);
        printf("Robot 2: Productos empacados: %d\n", cp2);
        printf("Robot 3: Productos empacados: %d\n", cp3);
        printf("Total productos empacados: %d\n", cp1 + cp2 + cp3);
        
        if (wait(NULL) == -1) { //Esperar a que el hijo termine
            perror("Error: Falla en wait al esperar al hijo.\n");
            return 46;
        }
    } else {

        perror("Error: No se pudo crear el proceso hijo (productor).\n");
        return 18;
    
    }

    return 0;
}
