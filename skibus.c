#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/wait.h>

#define SHARED_VAR_NAME "/shared_var"
#define SHARED_SEM_NAME "/shared_sem"

#define ERROR_CODE 1
#define WENT_FINE 0
#define FILE_ERROR 2
#define SEM_ERROR 3
#define SM_ERROR 4

typedef struct input {
    int L; //number of skiers
    int Z; //number of bus stops
    int K; //capacity of the bus
    int TL; //max skier wait time
    int TB; // max drive time
} input_S;

struct shared_var{
    int process_number;
    int curstop;
    int capacity;
    int waiting;
}*shared_variables;
struct shared_sem{
    sem_t *writing;
    sem_t *skier_ready;
}*shared_semaphores;

FILE* logfile;
int shm_fd;

int input_control(input_S* input_data, char** arguments, int ammount){
    int lborders[] = {0,0,9,-1,-1};
    int uborders[] = {20000,11,101,10001,1001};
    int values[] = {input_data->L,input_data->Z,input_data->K,input_data->TL,input_data->TB};
    if(ammount != 6){
        fprintf(stderr, "The wrong ammount of input parameters were given.");
        return ERROR_CODE;
    }
    for(int argument = 1; argument < ammount; argument++){
        int index = argument -1;
        char* errorconversion = NULL;
        int arg_value = strtol(arguments[argument],&errorconversion,10);
        if(*errorconversion!=0||lborders[index]>arg_value||uborders[index]<arg_value){
            fprintf(stderr, "A wrong parameter was given.");
            return ERROR_CODE;
        }else {
            values[index] = arg_value;
        }
    }
    input_data->L = values[0];
    input_data->Z = values[1];
    input_data->K = values[2];
    input_data->TL = values[3];
    input_data->TB = values[4];
    return WENT_FINE;
}
void initialize_shared() {
    int shm_fd;

    // Create shared memory for shared_var
    shm_fd = shm_open(SHARED_VAR_NAME, O_CREAT | O_RDWR, 0666);
    ftruncate(shm_fd, sizeof(struct shared_var));
    shared_variables = (struct shared_var *) mmap(NULL, sizeof(struct shared_var), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

    // Initialize shared_var struct
    shared_variables->process_number = 0;
    shared_variables->curstop = 0;
    shared_variables->capacity = 0;
    shared_variables->waiting = 0;

    // Create and initialize semaphores
    shared_semaphores = (struct shared_sem *) mmap(NULL, sizeof(struct shared_sem), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    shared_semaphores->writing = (sem_t *) malloc(sizeof(sem_t));
    shared_semaphores->skier_ready = (sem_t *) malloc(sizeof(sem_t));

    sem_init(shared_semaphores->writing, 0, 1);
    sem_init(shared_semaphores->skier_ready, 0, 1);
}
void bus_process(input_S* params){
    shared_variables->capacity=params->K;
    sem_wait(shared_semaphores->writing);
    shared_variables->process_number++;
    fprintf(logfile, "%d: BUS: started\n", shared_variables->process_number);
    fflush(logfile);
    sem_post(shared_semaphores->writing);
    for(int stop = 1; stop < params->Z+1;stop++){
        usleep(rand() % (params->TB+1));
        sem_wait(shared_semaphores->writing);
        shared_variables->process_number++;
        fprintf(logfile, "%d: BUS: arrived to %d\n", shared_variables->process_number, stop);
        fflush(logfile);
        sem_post(shared_semaphores->writing);
        shared_variables->curstop=stop;
    }
    sem_wait(shared_semaphores->writing);
    shared_variables->process_number++;
    fprintf(logfile, "%d: BUS: arrived to final", shared_variables->process_number);
    fflush(logfile);
    sem_post(shared_semaphores->writing);
}
void skier_process(int number, input_S* params, int busstop){
    sem_wait(shared_semaphores->writing);
    shared_variables->process_number++;
    fprintf(logfile, "%d: L %d: started\n", shared_variables->process_number, number);
    fflush(logfile);
    sem_post(shared_semaphores->writing);
    usleep(rand() % (params->TL+1));
    sem_wait(shared_semaphores->writing);
    shared_variables->waiting++;
    shared_variables->process_number++;
    fprintf(logfile, "%d: L %d: arrived to %d\n", shared_variables->process_number, number, busstop);
    sem_post(shared_semaphores->writing);
}
void cleanup(){
    // Close semaphores
    sem_close(shared_semaphores->writing);
    sem_close(shared_semaphores->skier_ready);

    // Unlink semaphores
    sem_unlink(SHARED_SEM_NAME "_proc_starting");
    sem_unlink(SHARED_SEM_NAME "_skier_ready");

    // Remove shared memory mapping
    munmap(shared_variables, sizeof(struct shared_var));

    // Close shared memory file descriptor
    close(shm_fd);

    // Unlink shared memory
    shm_unlink(SHARED_VAR_NAME);
}
int main (int argc,char** argv){
    int returncode = WENT_FINE;
    input_S input_data;
    returncode = input_control(&input_data,argv, argc);

    if((logfile = fopen("proj2.out","w"))==NULL){
        fprintf(stderr, "An error ocurred whilst accesing the output file.");
        exit(1);
    }

    initialize_shared();
    setbuf(logfile, NULL);

    int main_proc_n = fork();
    if(main_proc_n == 0) {
        for (int skier=1; skier < input_data.L+1; skier++) {
            int busstop = rand() % (input_data.Z + 1);
            int skier_proc_n = fork();
            if (skier_proc_n == 0) {
                skier_process(skier, &input_data, busstop);
                exit(0);
            }else if (skier_proc_n == -1){
                fprintf(stderr,"Failed to create the skier process.");
                cleanup();
                exit(1);
            }
        }
        int bus_proc_n = fork();
        if(bus_proc_n == 0){
            bus_process(&input_data);
            exit(0);
        }else if(bus_proc_n == -1){
            fprintf(stderr,"Failed to create the bus process.");
            exit(1);
        }
        exit(0);
    }else{
        int status;
        while (wait(&status) > 0){
        }
        printf("Done.");
        cleanup();
        fclose(logfile);
    }
    return returncode;
}