#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include "config.h"
#include "customer.h"
#include "supermarketcheckout.h"
#include "queue.h"
#include <math.h>


//Supermarket enter control
static pthread_mutex_t acmutex= PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t accond = PTHREAD_COND_INITIALIZER;
//Queue control
static pthread_mutex_t * queuemutex; //Lock on queues
static pthread_cond_t * queuecond; //Condition variable on queues
static pthread_cond_t * smcond; //Used to wake up a cashier when there is some one in queue

static pthread_mutex_t * smexitmutex; //Mutex on the variable "smexit" to control the close of a queue (0=Director is not closing the queue, 1=Director is closing the queue)

//DirectorOK 
static pthread_mutex_t okmutex = PTHREAD_MUTEX_INITIALIZER; //Lock on queues
static pthread_cond_t okcond = PTHREAD_COND_INITIALIZER; //Condition variable on queues

//DirectorMutex
static pthread_mutex_t DirectorMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t DirectorCond = PTHREAD_COND_INITIALIZER;

//Used to syncronized the variable qslength for the queues length (Used by director to open/close the supermarket checkouts)
static pthread_mutex_t * qslengthmutex;
static pthread_cond_t * qslengthcond;

//Lock and condition variable to syncronize the update by the cashiers
static pthread_mutex_t updatelock = PTHREAD_MUTEX_INITIALIZER; //Lock on queues
static pthread_cond_t updatecond = PTHREAD_COND_INITIALIZER; //Condition variable on queues
static pthread_mutex_t * upvarlock;


static config * cf; //Program configuration
static queue ** qs; //Queues
static int * qslength; //Array that contains queue's length
static int * smexit;
static int * updatevariable; //Used to update the director from cashier! 
static int exitok=0; 
static int customerexit=0;
static int exitbroadcast=0;
static long t; //TIME

volatile sig_atomic_t sighup=0;
volatile sig_atomic_t sigquit=0;

void CreateQueueManagement();
void QueueFree();


static void handler (int signum) {

if (signum==1) sighup=1;
if (signum==3) sigquit=1;

printf("Ricevuto segnale %d\n",signum);
}


void * clockT(void *arg) {
    int id=(int) (intptr_t) arg;
    int exittime=0;
    while(1){
        struct timespec t={(cf->directornews/1000),((cf->directornews%1000)*1000000)};
        nanosleep(&t,NULL); //Sleeping randomtime mseconds
        pthread_mutex_lock(&smexitmutex[id]);
        if (smexit[id]==1) exittime=1;
        pthread_mutex_unlock(&smexitmutex[id]);
        printf("%d: EXITTIME %d\n",id,exittime);
        fflush(stdout);
        if (exittime!=1 && sigquit==0 ) {  
            printf("%d UPDATINGGGGGGGGGGGGGGGGGGGGGGGGGGGG \n",id);
            fflush(stdout);
            pthread_mutex_lock(&qslengthmutex[id]);
            pthread_mutex_lock(&queuemutex[id]);
            qslength[id]=queuelength(qs[id],id);
            pthread_mutex_unlock(&queuemutex[id]);
            pthread_mutex_unlock(&qslengthmutex[id]);

            pthread_mutex_lock(&updatelock);
            pthread_mutex_lock(&upvarlock[id]);
            updatevariable[id]=1;
            pthread_mutex_unlock(&upvarlock[id]);
            pthread_cond_signal(&updatecond);
            pthread_mutex_unlock(&updatelock);
        }
        else { 
            printf("CLOCK %d CLOSED",id); 
            if (sigquit==1) {
                pthread_mutex_lock(&updatelock);
                pthread_cond_signal(&updatecond);
                pthread_mutex_unlock(&updatelock);
            }
            pthread_exit(NULL); 
        }
    }
    
    return NULL;
}



void * customerT (void *arg) {

    time_t          s,s2,s3,s4;  // Seconds
    struct timespec spec;
    struct timespec spec2;
    struct timespec spec3;
    struct timespec spec4;
    long time1,time2,time3,time4;

    clock_gettime(CLOCK_REALTIME, &spec); //Timestamp when customer enters the supermarket
    time1 = (spec.tv_sec)*1000 + (spec.tv_nsec) / 1000000;


    customer * cs = malloc(sizeof(customer)); //Customer data
    setupcs(cs,(int) (intptr_t) (arg)); //Set up struct
    int id = cs->id; //ID CUSTOMER
    unsigned int seed=cs->id+t; //Creating seed
    long randomtime; //Random time to buy products
    int nqueue; //Queue chosen
    int check; //If chosen a valid queue
    int changequeue=0; //If the queue has been closed and need to change queue
    int change=0; //Controls if the customer is switching supermarket checkout or not
    printf("Customer %d: joined the supermarket\n",id);
    fflush(stdout);

    //Customer's buy time
    cs->nproducts=rand_r(&seed)%(cf->P); //Random number of products: 0<nproducts<=P
    if ((cs->nproducts)!=0){
        while ((randomtime = rand_r(&seed) % (cf->T))<10); //Random number of buy time
        struct timespec t={(randomtime/1000),((randomtime%1000)*1000000)};
        nanosleep(&t,NULL); //Sleeping randomtime mseconds
    
        do{
            changequeue=0; //Reset change queue value
            check=0; 
            do {
                nqueue=rand_r(&seed) % (cf->K); //Random queue number
                pthread_mutex_lock(&queuemutex[nqueue]);
                if (qs[nqueue]->queueopen!=0) check=1; //Check if the queue is open
                pthread_mutex_unlock(&queuemutex[nqueue]);
                if(sigquit==1) break;
            }while(check==0);
            if (sigquit!=1){
                pthread_mutex_lock(&queuemutex[nqueue]);
                if (change==0) printf("Customer %d: Entra in cassa --> %d\n",id,nqueue); //First time that the cs is in a queue
                else { printf("Customer %d: CAMBIA E VA NELLA CASSAAAAAAAAAAAAAAAAAAAAAAAA --> %d\n",id,nqueue); cs->queuechecked++;}//Not the first time that the cs is in a queue
                fflush(stdout);

                clock_gettime(CLOCK_REALTIME, &spec3); //timestamp for calculating time waited in queue
                time3 = (spec3.tv_sec)*1000 + (spec3.tv_nsec) / 1000000;
                if((joinqueue(&qs[nqueue],&cs,nqueue))==-1){ //Joining the queue chosen
                    fprintf(stderr, "malloc failed\n");
                    exit(EXIT_FAILURE); 
                } 
                pthread_cond_signal(&smcond[nqueue]); //Signal to the queue to advice that a new customer is in queue
                while ((cs->queuedone)==0 && changequeue==0){ //While the customer hasnt paid or needs to change queue 'cause it has been closed
                    pthread_cond_wait(&queuecond[nqueue],&queuemutex[nqueue]);
                    if (qs[nqueue]->queueopen==0) changequeue=1; //If the customer has been waken and he hasn't done the queue --> sm closed and changes the queue
                }
                //DA IMPLEMENTARE: SE SIGQUIT SETTATO -> THREAD EXIT 
                pthread_mutex_unlock(&queuemutex[nqueue]);
                change++;
            }
            else break;
        }while(cs->queuedone==0);   
        clock_gettime(CLOCK_REALTIME, &spec4); //Get timestamp when customer has paid
        time4 = (spec4.tv_sec)*1000 + (spec4.tv_nsec) / 1000000;
    }
    pthread_mutex_lock(&acmutex); 
    customerexit++; //Number of customer that is waiting director permition
    pthread_mutex_unlock(&acmutex);
    pthread_mutex_lock(&okmutex); 
    exitok=0; 
    pthread_mutex_lock(&DirectorMutex);
    pthread_cond_signal(&DirectorCond); //Signal to the director that someone wants to exit
    pthread_mutex_unlock(&DirectorMutex);
    while(exitok!=1) { pthread_cond_wait(&okcond,&okmutex);} //While director doesnt allow the exit --> wait
    printf("Customer %d: leaved the supermarket\n",id);
    fflush(stdout);
    pthread_mutex_unlock(&okmutex); 
    
    clock_gettime(CLOCK_REALTIME, &spec2); //Timestamp of the exit from the supermarket
    time2 = (spec2.tv_sec)*1000 + (spec2.tv_nsec) / 1000000;

    cs->time=time2-time1; //Time passed in the supermarket
    cs->timeq=time4-time3; //Time passed in queue
    //printcs(*cs);
    return NULL; 
}

void * smcheckout(void *arg) {

    time_t          s,s2;  // Seconds
    struct timespec spec;
    struct timespec spec2;
    long time1,time2;

    clock_gettime(CLOCK_REALTIME, &spec); //Timestamp of the smcheckout open
    time1 = (spec.tv_sec)*1000 + (spec.tv_nsec) / 1000000;

    supermarketcheckout * smdata=((supermarketcheckout*)arg);
    int id = smdata->id-1;
    unsigned int seed=smdata->id+t; //Creating seed
    long randomtime;
    int exittime=0; //If the cashier has to close the smcheckout

    pthread_mutex_lock(&queuemutex[id]);
    qs[id]->queueopen=1;
    pthread_mutex_unlock(&queuemutex[id]);
    
    pthread_t clock; 
    if (pthread_create(&clock,NULL,clockT,(void*) (intptr_t) (id))!=0) {
        fprintf(stderr,"clock %d: thread creation, failed!",id);
        exit(EXIT_FAILURE);
    }
    
    printf("----------------------SUPERMARKET CHECKOUT %d OPENED!--------------------\n",id); fflush(stdout);
    while(1){
    pthread_mutex_lock(&queuemutex[id]);
    while(queuelength(qs[id],id)==0) { 
        if (sigquit==1) goto cassaexit;
        printf("Cashier %d: Waiting customers\n",id); 
        fflush(stdout); 
        pthread_cond_wait(&smcond[id],&queuemutex[id]);
        if (exitbroadcast==1) goto cassaexit;
    } //Wait until the queue is empty
    printQueue(qs[id],id); 
    customer * qcs=removecustomer(&qs[id],id); //Serves the customer that is the first in the queue    
    printf("Cashier %d: Serving customer: %d\n",id,qcs->id);
    fflush(stdout);
    pthread_mutex_unlock(&queuemutex[id]);
    //Number of time to scan the product and let the customer pay
    while ((randomtime = rand_r(&seed) % 80)<20); //Random number of time interval: 20-80

    randomtime=randomtime+(cf->S*qcs->nproducts);
    struct timespec t={(randomtime/1000),((randomtime%1000)*1000000)};
    nanosleep(&t,NULL); //Sleeping randomtime mseconds

    //Signal to the customer that the cashier has done
    printf("Customer %d has paid!\n",qcs->id);
    fflush(stdout);
    smdata->nproducts+=qcs->nproducts;
    smdata->ncustomers++;
    //printf("%f",((smdata->servicetime*smdata->ncustomers)+randomtime)/smdata->ncustomers++);
    //printf("%ld \n",randomtime);
    smdata->servicetime=smdata->servicetime+((randomtime-smdata->servicetime)/smdata->ncustomers);
    printf("DATI CASSA %d: ", id); printsm(*smdata); fflush(stdout);

    qcs->queuedone=1;
    pthread_mutex_lock(&queuemutex[id]);
    pthread_cond_signal(&queuecond[id]);
    pthread_mutex_lock(&smexitmutex[id]);
    if (smexit[id]==1) exittime=1;
    pthread_mutex_unlock(&smexitmutex[id]);
    if (exittime==1 || sigquit==1) { 
    cassaexit:    
        qs[id]->queueopen=0;
        pthread_mutex_unlock(&queuemutex[id]);
        if (pthread_join(clock,NULL) == -1 ) {
            fprintf(stderr,"DirectorSMControl: thread join, failed!");
        }
        printf("--------------- SUPERMARKET CHECKOUT %d CLOSED ------------------\n",(id));
        fflush(stdout);
        pthread_mutex_lock(&smexitmutex[id]);
        smexit[id]=0;
        pthread_mutex_unlock(&smexitmutex[id]);

        pthread_mutex_lock(&queuemutex[id]);
        printQueue(qs[id],id);
        pthread_cond_broadcast(&queuecond[id]);
        resetQueue(&qs[id],id);
        printQueue(qs[id],id);
        pthread_mutex_unlock(&queuemutex[id]);

        clock_gettime(CLOCK_REALTIME, &spec2);
        time2 = (spec2.tv_sec)*1000 + (spec2.tv_nsec) / 1000000;
        smdata->time+=time2-time1;
        smdata->nclosure++;
        pthread_exit(NULL);
    }
    pthread_mutex_unlock(&queuemutex[id]);
    }
    // smdata->nclosure++;
    // clock_gettime(CLOCK_REALTIME, &spec2);
    // time2 = (spec2.tv_sec)*1000 + (spec2.tv_nsec) / 1000000;
    // smdata->time+=time2-time1;

    return NULL;
}


void * DirectorSMcontrol(void *arg) {

    pthread_t * smchecks;
    supermarketcheckout * smdata = ((supermarketcheckout*)arg);
    int smopen=cf->smopen; //smcheckouts opened

    smchecks = malloc(cf->K*sizeof(pthread_t)); //Creating smopen threads
    if (!smchecks){
        fprintf(stderr, "malloc fallita\n");
	    exit(EXIT_FAILURE);
    }

    //Starting smopen smcheckouts!
    for (int i=0;i<cf->smopen;i++){
        pthread_mutex_lock(&queuemutex[i]);
            qs[i]->queueopen=1;
        pthread_mutex_unlock(&queuemutex[i]);
        if (pthread_create(&smchecks[i],NULL,smcheckout,&((supermarketcheckout*)arg)[i])!=0) {
            fprintf(stderr,"supermarketcheckout %d: thread creation, failed!",i);
            exit(EXIT_FAILURE);
        }
    }
    
    int check;
    int check1;
    int check2;
    int index;
    int counter;
    int counter1;
    int queueopen;
    int j;
    int closeoropen=0;

    while(sigquit!=1 && sighup!=1){
        check1=0;
        check2=0;
        pthread_mutex_lock(&updatelock);
        while(check1<cf->S1 && check2==0 && sigquit!=1 && sighup!=1){
            pthread_cond_wait(&updatecond,&updatelock);
            if (sigquit!=1 && sighup!=1) {
                check=0;
                queueopen=0;
                counter1=0;
                for (int i=0;i<cf->K;i++) {
                    pthread_mutex_lock(&queuemutex[i]);
                    if (qs[i]->queueopen==1) queueopen++;
                    pthread_mutex_unlock(&queuemutex[i]);
                    pthread_mutex_lock(&upvarlock[i]);
                    if (updatevariable[i]==1) counter1++;
                    pthread_mutex_unlock(&upvarlock[i]);
                }
                printf("QUEUEOPEN=%d , COUNTER=%d\n",queueopen,counter1); fflush(stdout);   
                if (queueopen==counter1) check++;

                if (check!=0){
                    for (int i=0;i<cf->K;i++){
                    pthread_mutex_lock(&qslengthmutex[i]);
                    pthread_mutex_lock(&queuemutex[i]);
                    printf("LUNGHEZZA CASSA %d = %d\n",i,qslength[i]);
                    fflush(stdout);
                    if(qslength[i]<=1 && qs[i]->queueopen==1) check1++; //Counting the number of queue that has at most 1 customer
                    pthread_mutex_unlock(&queuemutex[i]);
                    if (qslength[i]>=cf->S2) check2++; //Checking if there is a queue with more then cf->S2 customers
                    pthread_mutex_unlock(&qslengthmutex[i]);
                    }
                }
                            
            }
        }
        pthread_mutex_unlock(&updatelock);
        if (sigquit!=1 && sighup!=1)
        {
            for (int i=0;i<cf->K;i++) {
            pthread_mutex_lock(&upvarlock[i]);
            updatevariable[i]=0;
            pthread_mutex_unlock(&upvarlock[i]);
            }

            counter=0;
            j=0;
            if (check1>0 && smopen>1 && closeoropen==0){
                fflush(stdout);
                while(counter<(check1-cf->S1+1) && j<cf->K){
                    pthread_mutex_lock(&qslengthmutex[j]);
                    pthread_mutex_lock(&queuemutex[j]);
                    if(qslength[j]<=1 && qs[j]->queueopen==1) {
                        pthread_mutex_lock(&smexitmutex[j]);
                        smexit[j]=1;
                        pthread_mutex_unlock(&smexitmutex[j]);
                        counter++;
                        smopen--;
                    }
                    pthread_mutex_unlock(&queuemutex[j]);
                    pthread_mutex_unlock(&qslengthmutex[j]);
                    j++;
                }
            }
            //TODO: VARIABILE PER VERIFICARE SE SI E' CHIUSO DEF IL THREAD O NO
            if (check2>0 && smopen<cf->K && closeoropen==1) {
                index=-1;
                for (int i=0;i<cf->K;i++){
                    pthread_mutex_lock(&queuemutex[i]);
                    printf("%d \n",qs[i]->queueopen);
                    pthread_mutex_lock(&smexitmutex[i]);
                    if(qs[i]->queueopen==0 && index==-1 && smexit[i]!=1) { index=i; }
                    pthread_mutex_unlock(&smexitmutex[i]);
                    pthread_mutex_unlock(&queuemutex[i]);
                }
                if (index!=-1) {
                    printf("INDEXXXXXXXXX:%d\n",index); fflush(stdout);
                    pthread_mutex_lock(&queuemutex[index]);
                    qs[index]->queueopen=1;
                    pthread_mutex_unlock(&queuemutex[index]);
                    if (pthread_create(&smchecks[index],NULL,smcheckout,&((supermarketcheckout*)arg)[index])!=0) {
                        fprintf(stderr,"supermarketcheckout %d: thread creation, failed!",index);
                    exit(EXIT_FAILURE);
                    }
                    smopen++;
                } 
            }
            if(closeoropen==0) closeoropen=1; else closeoropen=0;
        }
    }

    for (int i=0;i<cf->K;i++){
        pthread_mutex_lock(&queuemutex[i]);
        if (qs[i]->queueopen==1){
            if (pthread_join(smchecks[i],NULL) == -1 ) {
                fprintf(stderr,"SMcheckout: thread join, failed!");
            }
        }
        pthread_mutex_unlock(&smexitmutex[i]);
    }

    return NULL;
}

void * DirectorCustomersControl (void *arg){
   
    pthread_t * cs;
    int customerscreated; //Number of all customers created from the start of the supermarket
    int activecustomers;//Number of active customers in the supermarket
    int i,j;
    int mallocsize;
    
    customerscreated=activecustomers=mallocsize=cf->C;

    //Creating C customers
    cs = malloc(mallocsize*sizeof(pthread_t)); //index 0-49
    if (!cs){
        fprintf(stderr, "malloc fallita\n");
	    exit(EXIT_FAILURE);
    }

    for (i=0;i<cf->C;i++) {
        if (pthread_create(&cs[i],NULL,customerT,(void*) (intptr_t) i)!=0) {
            fprintf(stderr,"customerT %d: thread creation, failed!",i);
            exit(EXIT_FAILURE);
        }
    }

    //Da implementare: caso di SIGHUP
    while(sigquit!=1 && activecustomers!=0){
        pthread_mutex_lock(&DirectorMutex); 
        while (activecustomers!=0) { 
           pthread_mutex_lock(&acmutex);
           if(customerexit==0) {pthread_mutex_unlock(&acmutex); pthread_cond_wait(&DirectorCond,&DirectorMutex); pthread_mutex_lock(&acmutex);} //Get waken when a customer wants to get out of the supermarket 
           customerexit--;
           pthread_mutex_unlock(&acmutex);
           //Say to the customer that can exit
           pthread_mutex_lock(&okmutex); 
           exitok=1;
           pthread_cond_signal(&okcond);
           pthread_mutex_unlock(&okmutex);
           activecustomers--;
           printf("%d",activecustomers);
           if (activecustomers==0) {
               for (int i=0;i<cf->K;i++) {
                    pthread_mutex_lock(&queuemutex[i]);
                    exitbroadcast=1;
                    pthread_cond_signal(&smcond[i]);
                    pthread_mutex_unlock(&queuemutex[i]);
               }   
           }
           if(activecustomers==(cf->C-cf->E)) break;
        }
        pthread_mutex_unlock(&DirectorMutex);

        //If number of customers is equal to C-E ==> wake up E customers and let them enter the supermarket
        if (activecustomers==(cf->C-cf->E) && sighup!=1 && sigquit!=1) { 
            mallocsize+=cf->E;
            if((cs=realloc(cs,mallocsize*sizeof(pthread_t)))==NULL) {
                fprintf(stderr,"1° realloc failed");
                exit(EXIT_FAILURE);
            }
            j=customerscreated;
            printf("MALLOCSIZE:%d\n",mallocsize);
            for(i=0;i<cf->E;i++){
                if (pthread_create(&cs[j+i],NULL,customerT,(void*) (intptr_t) i+j)!=0) {
                    fprintf(stderr,"customerT %d: thread creation, failed!",j+i);
                    exit(EXIT_FAILURE);
                }
                customerscreated++;
                activecustomers++;
             }  
        }
    }
    for (i=0;i<mallocsize;i++){
        if (pthread_join(cs[i],NULL) == -1 ) {
            fprintf(stderr,"DirectorSMControl: thread join, failed!");
        }
    }
        
    free(cs);
    return NULL;
}


void * directorT(void *arg) {

    
    //Creating subthread of director that manages the supermarket checkouts
    pthread_t DirectorSM;
    void *csdata;
    
    if (pthread_create(&DirectorSM,NULL,DirectorSMcontrol, (void*) arg)) {
        fprintf(stderr,"DirectorSMcontrol: thread creation, failed!");
        exit(EXIT_FAILURE);
    }

    //Creating subthread of director that manages the customers entry and exit
    pthread_t DirectorCustomers;
    if (pthread_create(&DirectorCustomers,NULL,DirectorCustomersControl,NULL)) {
        fprintf(stderr,"DirectorCustomersControl: thread creation, failed!");
        exit(EXIT_FAILURE);
    }

    
    //JOINS
    if (pthread_join(DirectorSM,NULL) == -1 ) {
            fprintf(stderr,"DirectorSMControl: thread join, failed!");
    }


    if (pthread_join(DirectorCustomers,NULL) == -1 ) {
            fprintf(stderr,"DirectorSMControl: thread join, failed!");
    }
    //JOIN'S END

    //Free heap memory used!
    return NULL;
    //return (void*) csdata;
}

int main(int argc, char const *argv[])
{
    supermarketcheckout * smdata;
    struct sigaction s;
    t=time(NULL);

    if (argc!=2) {
        fprintf(stderr,"usa: %s test\n",argv[0]);
        exit(EXIT_FAILURE);
    }

    if (strcmp(argv[1],"test.txt")==0){
        //Parameter's configure
        if ((cf=test(argv[1]))==NULL){
            exit(EXIT_FAILURE);
        }
    }
    else {
        fprintf(stderr,"%s: The argoument needs to be -> test\n",argv[0]);
        exit(EXIT_FAILURE);
    }

    CreateQueueManagement(); //Create queues and mutex/condition vars to control them with threads.

    //Creating the "container" for cashier
    
    smdata = malloc(cf->K*sizeof(supermarketcheckout));
    for (int i=0;i<cf->K;i++){
        setupsm(&smdata[i],i);
    }

    if (!smdata ){
        fprintf(stderr, "malloc fallita\n");
	    exit(EXIT_FAILURE);
    }

    memset(&s,0,sizeof(s));
    s.sa_handler=handler;
    if (sigaction(SIGHUP,&s,NULL)==-1) {
        fprintf(stderr,"Handler error");
    }
    if (sigaction(SIGQUIT,&s,NULL)==-1) {
        fprintf(stderr,"Handler error");
    }

    //Creating director thread
    pthread_t director;

    if (pthread_create(&director,NULL,directorT,(void*)smdata)!=0) {
        fprintf(stderr,"Director thread creation, failed!");
            exit(EXIT_FAILURE);
    }

    if (pthread_join(director,NULL) == -1 ) {
            fprintf(stderr,"Director thread join, failed!");
        }
    
    //for (int i=0;i<cf->K;i++) printsm(smdata[i]);


    free(smdata); QueueFree();
    return 0;
}


void CreateQueueManagement(){

    //Creating queues for the K supermarket checkouts
    if ((qs=(queue**) malloc((cf->K)*sizeof(queue*)))==NULL) {
        fprintf(stderr, "malloc failed\n");
        exit(EXIT_FAILURE);  
    }
    for (int i=0;i<cf->K; i++) {qs[i]=createqueues(i);}
   

    //Creating the mutex for the queues
    if((queuemutex=malloc(cf->K*sizeof(pthread_mutex_t)))==NULL){
        fprintf(stderr, "malloc failed\n");
        exit(EXIT_FAILURE);  
    }
    for (int i=0;i<cf->K;i++){
        if (pthread_mutex_init(&queuemutex[i], NULL) != 0) {
            fprintf(stderr, "pthread_mutex_init queue failed\n");
            exit(EXIT_FAILURE);                   
        }
    }

    //Creating the condition variable for the K queues
    if((queuecond=malloc(cf->K*sizeof(pthread_cond_t)))==NULL){
        fprintf(stderr, "malloc failed\n");
        exit(EXIT_FAILURE); 
    }
    for (int i=0;i<cf->K;i++){
        if (pthread_cond_init(&queuecond[i], NULL) != 0) {
            fprintf(stderr, "pthread_cond_init queue failed\n");
            exit(EXIT_FAILURE);                   
        }
    }

    if((smcond=malloc(cf->K*sizeof(pthread_cond_t)))==NULL){
        fprintf(stderr, "malloc failed\n");
        exit(EXIT_FAILURE); 
    }
    for (int i=0;i<cf->K;i++){
        if (pthread_cond_init(&smcond[i], NULL) != 0) {
            fprintf(stderr, "pthread_cond_init queue failed\n");
            exit(EXIT_FAILURE);                   
        }
    }


    //Creating the mutex for the queues
    if((qslengthmutex=malloc(cf->K*sizeof(pthread_mutex_t)))==NULL){
        fprintf(stderr, "malloc failed\n");
        exit(EXIT_FAILURE);  
    }
    for (int i=0;i<cf->K;i++){
        if (pthread_mutex_init(&qslengthmutex[i], NULL) != 0) {
            fprintf(stderr, "pthread_mutex_init queue failed\n");
            exit(EXIT_FAILURE);                   
        }
    }

    if((qslengthcond=malloc(cf->K*sizeof(pthread_cond_t)))==NULL){
        fprintf(stderr, "malloc failed\n");
        exit(EXIT_FAILURE); 
    }
    for (int i=0;i<cf->K;i++){
        if (pthread_cond_init(&qslengthcond[i], NULL) != 0) {
            fprintf(stderr, "pthread_cond_init queue failed\n");
            exit(EXIT_FAILURE);                   
        }
    }

    if ((qslength=malloc((cf->K)*sizeof(int)))==NULL) {
        fprintf(stderr, "malloc failed\n");
        exit(EXIT_FAILURE);  
    }
    for (int i=0;i<cf->K;i++) qslength[i]=queuelength(qs[i],i);


    if((smexitmutex=malloc(cf->K*sizeof(pthread_mutex_t)))==NULL){
        fprintf(stderr, "malloc failed\n");
        exit(EXIT_FAILURE);  
    }
    for (int i=0;i<cf->K;i++){
        if (pthread_mutex_init(&smexitmutex[i], NULL) != 0) {
            fprintf(stderr, "pthread_mutex_init queue failed\n");
            exit(EXIT_FAILURE);                   
        }
    }

    if ((smexit=malloc((cf->K)*sizeof(int)))==NULL) {
        fprintf(stderr, "malloc failed\n");
        exit(EXIT_FAILURE);  
    }
    for (int i=0;i<cf->K;i++) smexit[i]=0;

    if ((updatevariable=malloc((cf->K)*sizeof(int)))==NULL) {
        fprintf(stderr, "malloc failed\n");
        exit(EXIT_FAILURE);  
    }
    for (int i=0;i<cf->K;i++) updatevariable[i]=0;


    if((upvarlock=malloc(cf->K*sizeof(pthread_mutex_t)))==NULL){
        fprintf(stderr, "malloc failed\n");
        exit(EXIT_FAILURE);  
    }
    for (int i=0;i<cf->K;i++){
        if (pthread_mutex_init(&upvarlock[i], NULL) != 0) {
            fprintf(stderr, "pthread_mutex_init upvarlock failed\n");
            exit(EXIT_FAILURE);                   
        }
    }

}


void QueueFree(){

    free(qs); free(queuemutex); free(queuecond); free(smcond); free(qslengthmutex);
    free(qslengthcond); free(qslength); free(smexitmutex); free(smexit); free(updatevariable);
    free(upvarlock);
}











