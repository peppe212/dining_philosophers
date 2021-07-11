//Written by Giuseppe Muschetta

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stddef.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>

#define MAX_TURNI 5
#define DECIMODISECONDO 100000000
#define CENTESIMODISECONDO 10000000

//macro to handle system calls errors:
#define syscall(a,b)            \
        if( (a) == -1 ){        \
            perror(b);          \
            exit(EXIT_FAILURE); \
        }


//possibile state in which a philopher might be
enum{
    //he wants to eat, he can start eating only if he has access to both the sticks
    HA_FAME,
    //he's eating (both the sticks are hold)
    STA_MANGIANDO,
    //he's meditating, hence the stick are no longer held
    STA_MEDITANDO
};

//Sync Variables:
//
//inizializzo il mutex per l'array condiviso
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

//inizializzo la variabile di condizione
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

//dichiaro la riorsa condivisa
int *philosopher;

int *ptr;
int thread_count;

//usefull in the signal handler
volatile sig_atomic_t stop = 0;


//it will be used as a timer:
void myNanoSleep(long nanosecs){
    struct timespec ts;
    ts.tv_sec=0;
    ts.tv_nsec = nanosecs;
    nanosleep(&ts,NULL);
}


void *threadFunction(void *arg){
    int i = *(int*)arg;
    int pasti = MAX_TURNI;
    while(!stop && pasti > 0) {

        pthread_mutex_lock(&mutex); //LOCK ACQUIRE
        while (((philosopher[i] == HA_FAME) && (philosopher[(i - 1) % thread_count] == STA_MANGIANDO)) ||
               ((philosopher[i] == HA_FAME) && (philosopher[(i + 1) % thread_count] == STA_MANGIANDO)))  {
            printf("Il F. %d e' in attesa delle bacchette..!\n",i+1);
            pthread_cond_wait(&cond, &mutex);
        }
        //a questo punto puo' iniziare a mangiare
        printf("Il F. %d ha finalmente preso ambedue le bacchete e puo' iniziare a mangiare\n",i+1);
        philosopher[i] = STA_MANGIANDO;
        pthread_mutex_unlock(&mutex); //LOCK RELEASE


        //faccio trascorrere del tempo (quello impiegato dal filosofo i-esimo per mangiare)
        myNanoSleep(DECIMODISECONDO);

        pthread_mutex_lock(&mutex); //LOCK ACQUIRE
        printf("Il F. %d ha finito di mangiare, ergo posa le bacchette e inizia a meditare\n",i+1);
        philosopher[i] = STA_MEDITANDO; //ergo lascio le bacchette, posso fare una broadcast
        pthread_cond_signal(&cond);
        pthread_mutex_unlock(&mutex); //LOCK RELEASE

        if(stop)
            break;

        //faccio trascorrere del tempo (quello impiegato per meditare)
        myNanoSleep(CENTESIMODISECONDO);

        pthread_mutex_lock(&mutex); //LOCK ACQUIRE
        printf("Il F. %d ha finito di meditare ed e' piu' affamato che mai! ora vuole solo le bacchette!\n",i+1);
        philosopher[i] = HA_FAME;
        pthread_mutex_unlock(&mutex); //LOCK RELEASE

        pasti--;
        printf("Al F. %d restano ancora %d pasti \n",i+1,pasti);

    }//end_while

    //just in case...
    pthread_mutex_unlock(&mutex);
    //to avoid calling the stub
    pthread_exit(NULL);
}


int *sharedResource(int size){
    //ad ogni filosofo corrisponde un thread
    //la risorsa condivisa sara' un array di stati in cui puo' trovarsi ciuscun filosofo
    int *shared = (int*)malloc(size*sizeof(int));
    if(shared == NULL){
        fprintf(stderr,"malloc failed allocating heap memory %d\n",__LINE__);
        exit(EXIT_FAILURE);
    }
    //init array:
    int i;
    for(i=0;i<size;i++){
        shared[i] = HA_FAME;
    }
    return shared;
}


void signalHandler(int signum){
    write(STDOUT_FILENO,"terminazione con segnale  ",27);
    switch(signum){
        case SIGINT:
            write(STDOUT_FILENO,"SIGINT arrived...",18);
            stop = 1;
            break;
        case SIGSTOP:
            write(STDOUT_FILENO,"SIGSTOP arrived...",19);
            stop = 1;
            break;
        default:
            break;
    }
}

//it will be called by atexit
void cleanup(){
    printf("distruggo il mutex\n");
    pthread_mutex_destroy(&mutex);
    printf("distruggo la variabile di condizione\n");
    pthread_cond_destroy(&cond);
    printf("dealloco l'array ptr\n");
    free(ptr);
    printf("dealloco la risorsa condivisa\n");
    free(philosopher);
    printf("Terminazione corretta..!\n");
    fflush(stdin);
}


int main(int argc, char *argv[]) {
    //quando il main termina, faremo cleanup!
    atexit(cleanup);
    //checking argument:
    if(argc < 2){
        //non ho argomenti, ergo setto thread_count a default
        thread_count = 5;
    }
    else{
        //ho almeno un argomento
         long arg = strtol(argv[1],NULL,10);
         if(arg < 6){
             thread_count = 5;
         }
         else{
             thread_count = (int)arg;
         }
    }
    printf("A tavola si siedono %d filosofi affamati!\n\n",thread_count);

    //handling signals: SIGINT, SIGSTOP
    //pressing [Ctrl + c] or [Ctrl + \] will start the right closing phase
    struct sigaction sa;
    memset(&sa,0,sizeof(sa));
    sa.sa_handler = SIG_IGN;
    syscall( (sigaction(SIGPIPE,&sa,NULL)),"ignore SIGPIPE")

    sa.sa_handler = signalHandler;
    sigaction(SIGINT,&sa,NULL);
    sigaction(SIGSTOP,&sa,NULL);
    //end signal handling

    //allochiamo la risorsa condivisa
    philosopher = sharedResource(thread_count);

    //creo un array di threads per semplificarmi la vita con la successiva join
    pthread_t thread[thread_count];

    //alloco un array ptr per passare l'argomento con piu' precisione
    ptr = (int*)calloc(thread_count,sizeof(*ptr));
    if(ptr == NULL){
        fprintf(stdout,"calloc failed at %d line\n",__LINE__);
        exit(EXIT_FAILURE);
    }

    //ad ogni filosofo corrisponde un thread:
    int i;
    for(i=0;i<thread_count;++i) {
        //questo puntatore serve per non far sovrascrivere velocemente i nella funzione del thread
        ptr[i] = i;
        pthread_create(&thread[i], NULL, threadFunction, &ptr[i]);
    }

    //ora possiamo fare una join su tutti i threads:
    for (int j = 0; j < thread_count; ++j) {
        pthread_join(thread[j],NULL);
    }

    fprintf(stdout,"\nOperazioni di cleanup:\n");
    //partira' la cleanup
    return EXIT_SUCCESS;
}
