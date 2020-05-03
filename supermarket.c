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
static pthread_mutex_t * queuemutex;
static pthread_cond_t * queuecond;
static pthread_cond_t * smcond;

static config * cf; //Program configuration
static int activecustomers=0;//Number of active customers in the supermarket
static queue ** qs; //Queues

void spawnthreads(customer * csdata, supermarketcheckout * smdata);
void CreateQueueManagement();

void * customerT (void *arg) {

    int id = ((customer *)arg)->id; //ID CUSTOMER
    unsigned int seed=((customer*)arg)->id; //Creating seed
    long randomtime;

    pthread_mutex_lock(&acmutex); 
    //Testing if the number of customers in the supermarket is == then C
    while (activecustomers==cf->C){
        printf("Customer %d: is waiting to enter the supermarket\n",id); //Wait if the condition is true
        fflush(stdout);
        pthread_cond_wait(&accond,&acmutex);
    }
    activecustomers++; //Customer enters the supermarket
    printf("Customer %d: joined the supermarket --> %d\n",id,activecustomers);
    fflush(stdout);
    pthread_mutex_unlock(&acmutex);

    //Customer's buy time
    ((customer *)arg)->nproducts=rand_r(&seed)%(cf->P); //Random number of products: 0<nproducts<=P
    while ((randomtime = rand_r(&seed) % (cf->T))<10); //Random number of buy time
    struct timespec t={(randomtime/1000),((randomtime%1000)*1000000)};
    nanosleep(&t,&t); //Sleeping randomtime mseconds
    int nqueue=rand_r(&seed) % (cf->K);
    
    //Customer join the queue and waits until has done
    pthread_mutex_lock(&queuemutex[nqueue]);
    printf("Customer %d: Entra in cassa --> %d\n",id,nqueue);
    fflush(stdout);
    if((joinqueue(&qs[nqueue],((customer *)arg)))==-1){ //After sleep, the costumer joins a queue to pay
        fprintf(stderr, "malloc failed\n");
        exit(EXIT_FAILURE); 
    } 
    pthread_cond_signal(&smcond[nqueue]);
    while ((((customer *)arg)->queuedone)==0) pthread_cond_wait(&queuecond[nqueue],&queuemutex[nqueue]);
    pthread_mutex_unlock(&queuemutex[nqueue]);
    
    //After have paid the costumer leaves the supermarket
    pthread_mutex_lock(&acmutex);
    activecustomers--;
    printf("Customer %d: leaved the supermarket --> %d\n",id,activecustomers);
    fflush(stdout);
    //If number of customers is equal to C-E ==> wake up E customers and let them enter the supermarket
    if (activecustomers==(cf->C-cf->E)) for(int i=0;i<3;i++) pthread_cond_signal(&accond);
    pthread_mutex_unlock(&acmutex);
    
    return NULL; 
}

void * smcheckout(void *arg) {
    int id = ((supermarketcheckout*)arg)->id;
    unsigned int seed=((customer*)arg)->id; //Creating seed
    long randomtime;
    while(1){
    pthread_mutex_lock(&queuemutex[id-1]);
    while(queuelength(qs[id-1])==0) pthread_cond_wait(&smcond[id-1],&queuemutex[id-1]); //Wait until the queue is empty
    queue *qcs=removecustomer(&qs[id-1]); //Serve the customer that is the first in the queue
    pthread_mutex_unlock(&queuemutex[id-1]);

    //Number of time to scan the product and let the customer pay
    while ((randomtime = rand_r(&seed) % 80)<20); //Random number of time interval: 20-80
    randomtime=randomtime*cf->S;
    struct timespec t={(randomtime/1000),((randomtime%1000)*1000000)};
    nanosleep(&t,&t); //Sleeping randomtime mseconds

    //Signal to the customer that the cashier has done
    pthread_mutex_lock(&queuemutex[id-1]);
    (qcs->cs)->queuedone=1;
    pthread_cond_signal(&queuecond[id-1]);
    pthread_mutex_unlock(&queuemutex[id-1]);
    if(activecustomers==0) break;
    }

    return NULL;
}

void * directorT() {
    //printf("DIRECTOR\n");
}

int main(int argc, char const *argv[])
{
    supermarketcheckout * smdata;
    customer * csdata;

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

    //Creating the "container" for customer and supermarket checkouts
    csdata = malloc(cf->numcustomers*sizeof(customer));
    smdata = malloc(cf->K*sizeof(supermarketcheckout));

    if (!csdata || !smdata ){
        fprintf(stderr, "malloc fallita\n");
	    exit(EXIT_FAILURE);
    }

    //Spawing threads
    spawnthreads(csdata, smdata);



    // for (int i=0;i<cf->numcustomers;i++) printcs(csdata[i]);
    // for (int i=0;i<cf->K;i++) printsm(smdata[i]);


    free(cf); free(csdata); free(smdata);
    return 0;
}





void spawnthreads(customer * csdata, supermarketcheckout * smdata ){
    
    int NumSMcheckouts=cf->K;
    int numCustomers=cf->numcustomers;

    pthread_t * cs;
    pthread_t * smchecks;
    pthread_t director;

    cs = malloc(numCustomers*sizeof(pthread_t));
    smchecks = malloc(NumSMcheckouts*sizeof(pthread_t));

    if (!cs || !smchecks){
        fprintf(stderr, "malloc fallita\n");
	    exit(EXIT_FAILURE);
    }

    if (pthread_create(&director,NULL,directorT,NULL)!=0) {
        fprintf(stderr,"Director thread creation, failed!");
            exit(EXIT_FAILURE);
    }

    for (int i=0;i<numCustomers;i++) {
        setupcs(&csdata[i],i);
        if (pthread_create(&cs[i],NULL,customerT,&csdata[i])!=0) {
            fprintf(stderr,"customer %d: thread creation, failed!",i);
            exit(EXIT_FAILURE);
        }
    }

    for (int i=0;i<NumSMcheckouts;i++){
        setupsm(&smdata[i],i);
        if (pthread_create(&smchecks[i],NULL,smcheckout,&smdata[i])!=0) {
            fprintf(stderr,"supermarketcheckout %d: thread creation, failed!",i);
            exit(EXIT_FAILURE);
        }
    }

    if (pthread_join(director,NULL) == -1 ) {
            fprintf(stderr,"Director thread join, failed!");
        }

    for (int i=0;i<numCustomers;i++) {
        if (pthread_join(cs[i],NULL) == -1 ) {
            fprintf(stderr,"customer %d: thread join, failed!",i);
        }
    }

    for (int i=0;i<NumSMcheckouts;i++) {
        if (pthread_join(smchecks[i],NULL) == -1 ) {
            fprintf(stderr,"supermarketcheckout %d: thread join, failed!",i);
        }
    }

    free(cs); free(smchecks);

}

void CreateQueueManagement(){

    //Creating queues for the K supermarket checkouts
    if ((qs=(queue**) malloc((cf->K)*sizeof(queue*)))==NULL) {
        fprintf(stderr, "malloc failed\n");
        exit(EXIT_FAILURE);  
    }
    for (int i=0;i<cf->K; i++) qs[i]=createqueues(); 

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
}