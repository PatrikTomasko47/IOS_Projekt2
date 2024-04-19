#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

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
int *count, *proc_id, *skier_id;

struct shared_stuff{
    sem_t *ready_skier;
    sem_t *incr_anumber;
    int process_number;
}shared_mem;
shared_mem shared;

int input_control(input_S* input_data, char** arguments, int ammount){
    int lborders[] = {0,0,9,-1,-1};
    int uborders[] = {20000,11,101,10001,1001};
    int values[] = {input_data->L,input_data->Z,input_data->K,input_data->TL,input_data->TB};
    if(ammount != 6){
        fprintf(stderr, "The wrong ammount of input parameters were given.");
        exit(1);
    }
    for(int argument = 1; argument < ammount; argument++){
        int index = argument -1;
        char* errorconversion = NULL;
        int arg_value = strtol(arguments[argument],&errorconversion,10);
        if(*errorconversion!=0||lborders[index]>arg_value||uborders[index]<arg_value){
            fprintf(stderr, "A wrong parameter was given.");
            exit(1);
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
setshared(){
    if(((shared = mmap(NULL, sizeof(shared_mem), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0)) == NULL)
    ||(sem_init(shared.ready_skier,1,1)==-1)
    ||(sem_init(shared.incr_anumber,1,1)==-1)){
        fprintf(stderr, "There was an error initialising the semaphores.");
        exit (1);
        return SEM_ERROR;
    }
    return WENT_FINE;
}
void skier_process(int number, input_S* params){
    sem_wait(incr_anumber);
    shared.processnumber+=1;
    fprintf(logfile, "%d: L %d: started", );
    sem_post(incr_anumber);
    fflush(logfile);
    usleep(rand() % (params->TL+1));
    sem_wait(sem_skier_id);
    fprintf(logfile, "%d: L %d: started", );
    sem_post(sem_skier_id);
}
int main (int argc,char** argv){
    input_S input_data;
    input_control(&input_data,argv, argc);
    printf("%d %d %d %d %d", input_data.L, input_data.Z, input_data.K, input_data.TL, input_data.TB);
    return controll_code;

    if((logfile = fopen("proj2.out","w"))==NULL){
        fprintf(stderr, "An error ocurred whilst accesing the output file.");
        exit(1);
        return FILE_ERROR;
    }

    setshared();
    setbuf(logfile, NULL);

    pid_t main_proc_n, skier_proc_n;
    *proc_id=1;
    *skier_id=0;
    *count=0;

    main_proc_n = fork();
    if(shared.proc_id_proc == 0){
        for(int skier; skier<input_data.L;skier++){
            skier_proc_n = fork();
            if(skier_proc_n == 0){
                skier_process(skier, &input_data);
                exit(0);
            }
        }
    }else{
        fprintf(stderr,"An error occured when creating the processes.");
    }
}