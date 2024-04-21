/**
 * @file skibus.c
 * @author Patrik Tomasko (xtomasp00)
 * @date 21.4.2024
 * @brief 2nd project of IOS
 */

//including all the necessary libraries
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/wait.h>

//defining constants representing the names of shared structures
#define SHARED_VAR_NAME "shared_var_mem"
#define SHARED_SEM_NAME "shared_sem_mem"

//defining return codes, representing all the error that can occur
#define ERROR_INPUT 1
#define WENT_FINE 0
#define ERROR_SEM 1
#define ERROR_SM 1

/**
 * @brief Struct input stores all the input parameters that the user has provided, the input parameters are checked and inserted into the struct in input_control function
 * @see input_control()
 */
typedef struct input {
    int L; //number of skiers
    int Z; //number of bus stops
    int K; //capacity of the bus
    int TL; //max skier wait time
    int TB; // max drive time
} input_S;

/**
 * @brief Struct shared_var stores all the variables that are used to ensure a correct execution of the code regarding the interprocess communication, the shared memory is initialized in the initialize_shared function
 * @see initialize_shared()
 */
struct shared_var{
    int process_number;
    int curstop;
    int boardedammount;
    int waiting;
    int buswaiters[10];
    int alltheskiers;
}*shared_variables;
/**
 * @brief Struct shared_var stores all the semaphores used for syncronisation among the processes and mutex, the shared memory is initialized in the initialize_shared function
 * @see initialize_shared()
 */
struct shared_sem{
    sem_t *writing;
    sem_t *boarding;
    sem_t *arrivedfinal;
}*shared_semaphores;

//logfile represents the output file proj2.out
FILE* logfile;
//shared_mem variable is later on used to store the shared memory that will hold the shared variable
int shared_mem;

/**
 * @brief Function input_controll checks wether the input parameters are correct and in case they are, stores them in the input struct
 * @param input_data pointer to the input struct that will hold all the input parameters
 * @param arguments pointer to the input parameters (argv)
 * @param ammount the ammount of input parameters (argc)
 * @return returns 0 or 1 as a constant defined in the beginning of the file, 0 representing that the allocation went fine, 1 representing an error
 */
int input_control(input_S* input_data, char** arguments, int ammount){
    //initialising lists containing the upper and lover borders
    int lborders[] = {0,0,9,-1,-1};
    int uborders[] = {20000,11,101,10001,1001};
    int values[] = {0,0,0,0,0};
    //checking wether the use entered the correct ammount of input parameters
    if(ammount != 6){
        fprintf(stderr, "The wrong ammount of input parameters were given.");
        return ERROR_INPUT;
    }
    for(int argument = 1; argument < ammount; argument++){
        int index = argument -1;
        char* errorconversion = NULL;
        //converting the input parameters to integers
        int arg_value = strtol(arguments[argument],&errorconversion,10);
        //checking wether the conversion was done correctly, and checking wether the input file is in the correct range
        if(*errorconversion!=0||lborders[index]>arg_value||uborders[index]<arg_value){
            fprintf(stderr, "A wrong parameter was given.");
            return ERROR_INPUT;
        }else {
            values[index] = arg_value;
        }
    }
    //assigning the input parameters to their respective variables in the input struct
    input_data->L = values[0];
    input_data->Z = values[1];
    input_data->K = values[2];
    input_data->TL = values[3];
    input_data->TB = values[4];
    return WENT_FINE;
}
/**
 * @brief Function initialize_shared creates the shared memory, initializes all the shared variable and initializes/opens the semaphores
 */
void initialize_shared() {
    //creating the shared memory for the shared semaphores
    shared_semaphores = (struct shared_sem *) mmap(NULL, sizeof(struct shared_sem), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    //initializing the semaphor writing, which is unnamed, it's used for making sure the processes don't distrupt each other during the writing to the logfile
    shared_semaphores->writing = (sem_t *) malloc(sizeof(sem_t));
    sem_init(shared_semaphores->writing, 1, 1);

    //initializing the named semaphors boarding and arrivedfinal, which make sure the processes skier and bus are able to communicate among each other
    shared_semaphores->boarding = sem_open(SHARED_SEM_NAME "sem_boarding", O_CREAT, 0666, 0);
    shared_semaphores->arrivedfinal = sem_open(SHARED_SEM_NAME "sem_arrivedfinal", O_CREAT, 0666, 0);

    //creating the shared memory for the shared variables
    shared_mem = shm_open(SHARED_VAR_NAME, O_CREAT | O_RDWR, 0666);
    ftruncate(shared_mem, sizeof(struct shared_var));
    shared_variables = (struct shared_var *) mmap(NULL, sizeof(struct shared_var), PROT_READ | PROT_WRITE, MAP_SHARED, shared_mem, 0);

    //initializing the variables inside the shared_var struct
    shared_variables->alltheskiers=0;
    shared_variables->process_number = 0;
    shared_variables->curstop = 0;
    shared_variables->boardedammount = 0;
    shared_variables->waiting = 0;
    for (int stop = 0; stop < 10; stop++) {
        shared_variables->buswaiters[stop] = 0;
    }
}
/**
 * @brief Function bus_process represents the process skibus that picks up the skiers and delivers them to the cable car
 * @param params struct holding the input parameters, in the case of the skibus process, it needs to know it's own capacity and the maximum time between bus stops
 */
void bus_process(input_S* params){
    //bus starting
    sem_wait(shared_semaphores->writing);
    shared_variables->process_number++;
    fprintf(logfile, "%d: BUS: started\n", shared_variables->process_number);
    fflush(logfile);
    sem_post(shared_semaphores->writing);
    //while there are still people waiting or not all the people have arrived to the bus stop, repeat the trips across the bus stops
    while(shared_variables->waiting>0||shared_variables->alltheskiers!=params->L) {
        //itterating through the bus stops
        for (int stop = 1; stop < params->Z + 1; stop++) {
            //waiting - traveling between the stops, time is defined by the input param TB
            usleep(rand() % (params->TB + 1));
            //arriving to a bus stop
            sem_wait(shared_semaphores->writing);
            shared_variables->process_number++;
            fprintf(logfile, "%d: BUS: arrived to %d\n", shared_variables->process_number, stop);
            fflush(logfile);
            sem_post(shared_semaphores->writing);
            //setting the variable curstop to the currenct busstop to inform the skiers wether the bus is on their stop
            shared_variables->curstop = stop;
            //boarding the skiers
            sem_post(shared_semaphores->boarding);
            //waiting until the current stop has no more people or the bus is full
            while (shared_variables->boardedammount != params->K &&
                   shared_variables->buswaiters[stop - 1] > 0) { usleep(100); }
            sem_wait(shared_semaphores->boarding);
            //changing the curstop variable to 0 representing that it is traveling
            shared_variables->curstop = 0;
            sem_wait(shared_semaphores->writing);
            shared_variables->process_number++;
            fprintf(logfile, "%d: BUS: leaving %d\n", shared_variables->process_number, stop);
            fflush(logfile);
            sem_post(shared_semaphores->writing);
        }
        //arriving to the cable car
        sem_wait(shared_semaphores->writing);
        shared_variables->process_number++;
        fprintf(logfile, "%d: BUS: arrived to final\n", shared_variables->process_number);
        fflush(logfile);
        sem_post(shared_semaphores->writing);
        //informing the boarded skiers, the bus has arrived to the cable car, via the arrivedfinal semaphore
        sem_post(shared_semaphores->arrivedfinal);
        //waiting for the people to leave the bus
        while (shared_variables->boardedammount != 0) { usleep(100); }
        sem_wait(shared_semaphores->arrivedfinal);
        //leaving the cable car and ending or repeating the process
        sem_wait(shared_semaphores->writing);
        shared_variables->process_number++;
        fprintf(logfile, "%d: BUS: leaving final\n", shared_variables->process_number);
        fflush(logfile);
        sem_post(shared_semaphores->writing);
    }
    //in case all the skiers have been delivered (no skiers are waiting), the ending message is written to the file
    sem_wait(shared_semaphores->writing);
    shared_variables->process_number++;
    fprintf(logfile, "%d: BUS: finish\n", shared_variables->process_number);
    fflush(logfile);
    sem_post(shared_semaphores->writing);
}
/**
 * @brief Function skier_process, represents the process of the skier, it describes his behavior and interaction with other processes
 * @param number this is the numbe of the skier, each one has his own unique number, that is the showen in the log file
 * @param params struct holding the input parameters, in the case of the skier process, it needs to know the maximum time of having a breakfast
 * @param busstop represents the bus stop at which the skier will be waiting
 */
void skier_process(int number, input_S* params, int busstop){
    //setting the variable boarded to 0 meaning the skier is not yet on the bus
    int boarded = 0;
    //writing to the output file, that the skier has started
    sem_wait(shared_semaphores->writing);
    shared_variables->process_number++;
    fprintf(logfile, "%d: L %d: started\n", shared_variables->process_number, number);
    fflush(logfile);
    sem_post(shared_semaphores->writing);
    //skier waiting, aka having breakfast
    usleep(rand() % (params->TL+1));
    //skier arriving to his bus stop
    sem_wait(shared_semaphores->writing);
    //adding himself to the number of people waiting on his bus stup
    shared_variables->buswaiters[busstop-1]++;
    //adding himself to the number of waiting skiers
    shared_variables->waiting++;
    shared_variables->process_number++;
    //adding himself to the ammount of skiers after breakfast
    shared_variables->alltheskiers++;
    fprintf(logfile, "%d: L %d: arrived to %d\n", shared_variables->process_number, number, busstop);
    sem_post(shared_semaphores->writing);
    //while the skier is not yet boarded, he keeps waiting for the boarding semaphore and attempting to get on
    while(boarded != 1) {
        //cooldown after an unsuccessful attempt to get on the bus
        usleep(rand() % (201));
        //waiting for the boarding semaphore, through which the bus informs that it has arrived to a bus stop
        sem_wait(shared_semaphores->boarding);
        //checking wether the skier is at the bus stop at which the bus is and if there is still space left in the bus
        if(shared_variables->curstop == busstop && shared_variables->boardedammount != params->K){
            //skier getting boarded
            boarded = 1;
            //skier no longer waiting
            shared_variables->waiting--;
            //adding himself to the ammount of people on board, in order to know wether the bus is full
            shared_variables->boardedammount++;
            //skier no longer waiting on that specific bus stop
            shared_variables->buswaiters[busstop-1]--;
            //writing the information that he has boarded to the output file
            sem_wait(shared_semaphores->writing);
            shared_variables->process_number++;
            fprintf(logfile, "%d: L %d: boarding\n", shared_variables->process_number, number);
            fflush(logfile);
            sem_post(shared_semaphores->writing);
        }
        sem_post(shared_semaphores->boarding);
    }
    //waiting for the arrival to the cable car
    sem_wait(shared_semaphores->arrivedfinal);
    sem_wait(shared_semaphores->writing);
    shared_variables->boardedammount--;
    shared_variables->process_number++;
    fprintf(logfile, "%d: L %d: going to ski\n", shared_variables->process_number, number);
    fflush(logfile);
    sem_post(shared_semaphores->writing);
    sem_post(shared_semaphores->arrivedfinal);
}
/**
 * @brief Function cleanup frees all the shared memory and closes and unlinks the semaphores
 */
void cleanup(){
    //closing the unnamed semaphore
    sem_close(shared_semaphores->writing);
    //unlinking the named semaphores
    sem_unlink(SHARED_SEM_NAME "sem_boarding");
    sem_unlink(SHARED_SEM_NAME "sem_arrivedfinal");
    //removing the mapping of the shared memory
    munmap(shared_variables, sizeof(struct shared_var));
    // closing the shared memory descriptor
    close(shared_mem);
    //unlinking the shared memory
    shm_unlink(SHARED_VAR_NAME);
}
/**
 * @brief Function main creates the main process and all the subprocesses, checks the input parameters and returns the return number
 * @param argc number of parameters given by the user
 * @param argv list of the parameters given by the user
 * @return 0 if the code went fine 1 if there was an error
 */
int main (int argc,char** argv){
    //creating the variables to store the input parameters
    input_S input_data;
    //checking wether the input parameters were ok
    if(input_control(&input_data,argv, argc)==1){
        exit(1);
    }
    //accessing the output log file, in case there is a problem the program exits with 1
    if((logfile = fopen("proj2.out","w"))==NULL){
        fprintf(stderr, "An error ocurred whilst accesing the output file.");
        exit(1);
    }
    //initializing the shared variables and sems and setting the output file
    initialize_shared();
    setbuf(logfile, NULL);
    //creating the main process
    int main_proc_n = fork();
    if(main_proc_n == 0) {
        //creating all the skier processes
        for (int skier = 1; skier < input_data.L + 1; skier++) {
            //giving the skiers a random bus
            int busstop = (rand() % input_data.Z) + 1;
            int skier_proc_n = fork();
            if (skier_proc_n == 0) {
                skier_process(skier, &input_data, busstop);
                exit(0);
            } else if (skier_proc_n == -1) {
                //if there was a problem creating the skier process inform about this and exit with 1
                fprintf(stderr, "Failed to create the skier process.");
                cleanup();
                exit(1);
            }
        }
        //creating the bus process
        int bus_proc_n = fork();
        if (bus_proc_n == 0) {
            bus_process(&input_data);
            exit(0);
        } else if (bus_proc_n == -1) {
            //if there was a problem creating the bus process inform about this and exit with 1
            fprintf(stderr, "Failed to create the bus process.");
            exit(1);
        }
        exit(0);
    }else if(main_proc_n==-1){
        //if there was a problem creating the main process inform about this and exit with 1
        fprintf(stderr, "Failed to create the main process.");
        cleanup();
        exit(1);
    }else{
        int status;
        //waiting for the processes to finish
        while (wait(&status) > 0){
        }
        cleanup();
        fclose(logfile);
        exit(1);
    }
    return WENT_FINE;
}