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

//GENERAL CLOCK mutex and cond
static pthread_mutex_t GCmutex = PTHREAD_MUTEX_INITIALIZER; 
static pthread_cond_t GCcond = PTHREAD_COND_INITIALIZER; 


static config * cf; //Program configuration
static queue ** qs; //Queues
static int * qslength; //Array that contains queue's length
static int * smexit;
static int * updatevariable; //Used to update the director from cashier! 
static int exitok=0; 

void CreateQueueManagement();
void QueueFree();

void * GeneralClockT(void * arg){
    int check;
    int queueopen;
    int counter;
    while(1){
        check=0;
        pthread_mutex_lock(&GCmutex);
        while (check==0) {
            counter=0;
            queueopen=0;
            pthread_cond_wait(&GCcond,&GCmutex);    
            printf("AAAAAAAAAAAAAAAAAAAAA\n"); fflush(stdout);
            for (int i=0;i<cf->K;i++) {
                pthread_mutex_lock(&queuemutex[i]);
                if (qs[i]->queueopen==1) queueopen++;
                pthread_mutex_unlock(&queuemutex[i]);
                pthread_mutex_lock(&upvarlock[i]);
                if (updatevariable[i]==1) counter++;
                pthread_mutex_unlock(&upvarlock[i]);
            }
            printf("QUEUEOPEN=%d , COUNTER=%d\n",queueopen,counter); fflush(stdout);   
            if (queueopen==counter) check++;
        }

        for (int i=0;i<cf->K;i++) {
        pthread_mutex_lock(&upvarlock[i]);
        updatevariable[i]=0;
        pthread_mutex_unlock(&upvarlock[i]);
        }

        pthread_mutex_unlock(&GCmutex);
        printf("SANDING QUEUE LENGTH");
        fflush(stdout);
        pthread_mutex_lock(&updatelock);
        pthread_cond_signal(&updatecond);
        pthread_mutex_unlock(&updatelock);
        
    }
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
        if (exittime!=1) {  
        printf("%d UPDATINGGGGGGGGGGGGGGGGGGGGGGGGGGGG \n",id);
        fflush(stdout);
        pthread_mutex_lock(&qslengthmutex[id]);
        pthread_mutex_lock(&queuemutex[id]);
        qslength[id]=queuelength(qs[id]);
        pthread_mutex_unlock(&queuemutex[id]);
        pthread_mutex_unlock(&qslengthmutex[id]);

        pthread_mutex_lock(&GCmutex);
        pthread_mutex_lock(&upvarlock[id]);
        updatevariable[id]=1;
        pthread_mutex_unlock(&upvarlock[id]);
        pthread_cond_signal(&GCcond);
        pthread_mutex_unlock(&GCmutex);
        }
        else { printf("CLOCK %d CLOSED",id); pthread_exit(NULL); }
    }
    
    return NULL;
}



void * customerT (void *arg) {

    customer * cs = (customer*) arg;
    int id = cs->id; //ID CUSTOMER
    unsigned int seed=cs->id; //Creating seed
    long randomtime=0;
    int nqueue;
    int check=-1;
    int changequeue=0;
    int change=0;
    printf("Customer %d: joined the supermarket\n",id);
    fflush(stdout);

    //Customer's buy time
    cs->nproducts=rand_r(&seed)%(cf->P); //Random number of products: 0<nproducts<=P
    if ((cs->nproducts)!=0){
        while ((randomtime = rand_r(&seed) % (cf->T))<10); //Random number of buy time
        struct timespec t={(randomtime/1000),((randomtime%1000)*1000000)};
        nanosleep(&t,NULL); //Sleeping randomtime mseconds

        do{
        //Customer join the queue and waits until has done
            changequeue=0;
            do {
                nqueue=rand_r(&seed) % (cf->K);
                pthread_mutex_lock(&queuemutex[nqueue]);
                if (qs[nqueue]->queueopen!=0) check=0;
                pthread_mutex_unlock(&queuemutex[nqueue]);
            }while(check==-1);
            pthread_mutex_lock(&queuemutex[nqueue]);
            if (change==0) printf("Customer %d: Entra in cassa --> %d\n",id,nqueue);
            else printf("Customer %d: CAMBIA E VA NELLA CASSAAAAAAAAAAAAAAAAAAAAAAAA --> %d\n",id,nqueue);
            fflush(stdout);
            if((joinqueue(&qs[nqueue],&cs,nqueue))==-1){ //After sleep, the costumer joins a queue to pay
                fprintf(stderr, "malloc failed\n");
                exit(EXIT_FAILURE); 
            } 
            pthread_cond_signal(&smcond[nqueue]);
            while ((cs->queuedone)==0 && changequeue==0){
                pthread_cond_wait(&queuecond[nqueue],&queuemutex[nqueue]);
                if (qs[nqueue]->queueopen==0) changequeue=1;
            }
            //DA IMPLEMENTARE: SE SIGQUIT SETTATO -> THREAD EXIT 
            pthread_mutex_unlock(&queuemutex[nqueue]);
            change++;
        }while(cs->queuedone==0);
    }
    cs->time=randomtime+cs->timeq;
    pthread_mutex_lock(&okmutex); 
    exitok=2;
    pthread_mutex_lock(&DirectorMutex);
    pthread_cond_signal(&DirectorCond); 
    pthread_mutex_unlock(&DirectorMutex);
    while(exitok!=1) pthread_cond_wait(&okcond,&okmutex);
    printf("Customer %d: leaved the supermarket\n",id);
    fflush(stdout);
    exitok=0;
    pthread_mutex_unlock(&okmutex); 


    return NULL; 
}

void * smcheckout(void *arg) {

    int id = (((supermarketcheckout*)arg)->id)-1;
    unsigned int seed=((customer*)arg)->id; //Creating seed
    long randomtime;
    int exittime=0;

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
    while(queuelength(qs[id])==0) { printf("Cashier %d: Waiting customers\n",id); fflush(stdout); pthread_cond_wait(&smcond[id],&queuemutex[id]);} //Wait until the queue is empty
    printQueue(qs[id],id); 
    customer * qcs=removecustomer(&qs[id]); //Serves the customer that is the first in the queue    
    printf("Cashier %d: Serving customer: %d\n",id,qcs->id);
    fflush(stdout);
    pthread_mutex_unlock(&queuemutex[id]);
    //Number of time to scan the product and let the customer pay
    while ((randomtime = rand_r(&seed) % 80)<20); //Random number of time interval: 20-80
    randomtime=randomtime*cf->S;
    struct timespec t={(randomtime/1000),((randomtime%1000)*1000000)};
    nanosleep(&t,NULL); //Sleeping randomtime mseconds

    //Signal to the customer that the cashier has done
    printf("Customer %d has paid!\n",qcs->id);
    fflush(stdout);
    qcs->queuedone=1;
    qcs->timeq=randomtime;
    pthread_mutex_lock(&queuemutex[id]);
    pthread_cond_signal(&queuecond[id]);
    pthread_mutex_lock(&smexitmutex[id]);
    if (smexit[id]==1) exittime=1;
    pthread_mutex_unlock(&smexitmutex[id]);
    if (exittime==1) { 
        qs[id]->queueopen=0;
        pthread_mutex_unlock(&queuemutex[id]);
        if (pthread_join(clock,NULL) == -1 ) {
            fprintf(stderr,"DirectorSMControl: thread join, failed!");
        }
        printf("---------------------------- SUPERMARKET CHECKOUT %d CLOSED ------------------------\n",(id));
        fflush(stdout);
        pthread_mutex_lock(&smexitmutex[id]);
        smexit[id]=0;
        pthread_mutex_unlock(&smexitmutex[id]);

        pthread_mutex_lock(&queuemutex[id]);
        printQueue(qs[id],id);
        pthread_cond_broadcast(&queuecond[id]);
        pthread_mutex_unlock(&queuemutex[id]);
        pthread_exit(NULL);
    }
    pthread_mutex_unlock(&queuemutex[id]);

    }

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
    

    int check1;
    int check2;
    int index;
    int counter;
    int j;
    int closeoropen=0;

    while(1){
        check1=0;
        check2=0;
        pthread_mutex_lock(&updatelock);
        while(check1<cf->S1 && check2==0){
            pthread_cond_wait(&updatecond,&updatelock);
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
        pthread_mutex_unlock(&updatelock);
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

void * DirectorCustomersControl (void *arg){
   
    pthread_t * cs;
    int customerscreated; //Number of all customers created from the start of the supermarket
    int activecustomers;//Number of active customers in the supermarket
    int i,j;
    int mallocsize;
    
    customerscreated=activecustomers=mallocsize=cf->C;

    //Creating C customers
    cs = malloc(mallocsize*sizeof(pthread_t)); //index 0-49
    customer * csdata = malloc(mallocsize*sizeof(customer)); //index 0-49
    if (!cs || !csdata){
        fprintf(stderr, "malloc fallita\n");
	    exit(EXIT_FAILURE);
    }

    for (i=0;i<cf->C;i++) {
        setupcs(&csdata[i],i);
        if (pthread_create(&cs[i],NULL,customerT,&csdata[i])!=0) {
            fprintf(stderr,"customerT %d: thread creation, failed!",i);
            exit(EXIT_FAILURE);
        }
    }

    //Da implementare: caso di SIGHUP
    while(1){
        pthread_mutex_lock(&DirectorMutex); 
        while (activecustomers!=(cf->C-cf->E)) { pthread_cond_wait(&DirectorCond,&DirectorMutex); //Get waken when a customer wants to get out of the supermarket 
           //Say to the customer that can exit
           pthread_mutex_lock(&okmutex); 
           exitok=1;
           pthread_cond_signal(&okcond);
           pthread_mutex_unlock(&okmutex);
           activecustomers--;
           fflush(stdout);
        }
        pthread_mutex_unlock(&DirectorMutex);

        //If number of customers is equal to C-E ==> wake up E customers and let them enter the supermarket
        if (activecustomers==(cf->C-cf->E)) { 
            mallocsize+=cf->E;
            if((cs=realloc(cs,mallocsize*sizeof(pthread_t)))==NULL) {
                fprintf(stderr,"1° realloc failed");
                exit(EXIT_FAILURE);
            }
            if((csdata=realloc(csdata,mallocsize*sizeof(customer)))==NULL){
                fprintf(stderr,"2° realloc failed");
                exit(EXIT_FAILURE);
            }
            j=customerscreated;
            for(i=0;i<cf->E;i++){
                setupcs(&csdata[j+i],j+i);
                // printcs(csdata[j+i]);
                if (pthread_create(&cs[j+i],NULL,customerT,(void*) &csdata[j+i])!=0) {
                    fprintf(stderr,"customerT %d: thread creation, failed!",j+i);
                    exit(EXIT_FAILURE);
                }
                customerscreated++;
                activecustomers++;
             }  

        }
    }
    free(cs); return (void *) csdata;
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


    if (pthread_join(DirectorCustomers,&csdata) == -1 ) {
            fprintf(stderr,"DirectorSMControl: thread join, failed!");
    }
    //JOIN'S END

    //Free heap memory used!
    return csdata;
    //return (void*) csdata;
}

int main(int argc, char const *argv[])
{
    supermarketcheckout * smdata;
    void *csdata;
   
    if (argc!=2) {
        fprintf(stderr,"usa: %s test\n",argv[0]);
        exit(EXIT_FAILURE);
    }

    if (strcmp(argv[1],"test")==0){
        //Parameter's configure
        if ((cf=test())==NULL){
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

    //Creating director thread
    pthread_t director;
    pthread_t GC;

    if (pthread_create(&director,NULL,directorT,(void*)smdata)!=0) {
        fprintf(stderr,"Director thread creation, failed!");
            exit(EXIT_FAILURE);
    }

    if (pthread_create(&GC,NULL,GeneralClockT,NULL)!=0) {
        fprintf(stderr,"GeneralClockT thread creation, failed!");
            exit(EXIT_FAILURE);
    }

    if (pthread_join(director,&csdata) == -1 ) {
            fprintf(stderr,"Director thread join, failed!");
        }
    
    if (pthread_join(GC,NULL) == -1 ) {
            fprintf(stderr,"GeneralClockT thread join, failed!");
        }

    //for (int i=0;i<cf->numcustomers;i++) printcs((customer) (((customer*)csdata)[i]));
    // // for (int i=0;i<cf->K;i++) printsm(smdata[i]);


    free(smdata); free(csdata); QueueFree();
    // return 0;
}


void CreateQueueManagement(){

    //Creating queues for the K supermarket checkouts
    if ((qs=(queue**) malloc((cf->K)*sizeof(queue*)))==NULL) {
        fprintf(stderr, "malloc failed\n");
        exit(EXIT_FAILURE);  
    }
    for (int i=0;i<cf->K; i++) qs[i]=createqueues(i); 

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
    for (int i=0;i<cf->K;i++) qslength[i]=queuelength(qs[i]);


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











