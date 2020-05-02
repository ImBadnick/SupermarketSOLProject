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

static pthread_mutex_t acmutex= PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t * queuemutex;
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
static config * cf;
static int activecustomers=0;
static queue * qs;

void spawnthreads(customer * csdata, supermarketcheckout * smdata);

void * customerT (void *arg) {

    int id = ((customer *)arg)->id; //ID CUSTOMER
    unsigned int seed=((customer*)arg)->id; //Creating seed
    long randomtime;

    pthread_mutex_lock(&acmutex); 
    //Testing if the number of customers in the supermarket is == then C
    while (activecustomers==cf->C){
        printf("Customer %d: is waiting to enter the supermarket\n",id); //Wait if the condition is true
        fflush(stdout);
        pthread_cond_wait(&cond,&acmutex);
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
    //joinqueue(qs[nqueue],((customer *)arg)); //After sleep, the costumer joins a queue to pay
    //After have paid the costumer leaves the supermarket
    pthread_mutex_lock(&acmutex);
    activecustomers--; 
    printf("Customer %d: leaved the supermarket --> %d\n",id,activecustomers);
    fflush(stdout);
    //If number of customers is equal to C-E ==> wake up E customers and let them enter the supermarket
    if (activecustomers==(cf->C-cf->E)) for(int i=0;i<3;i++) pthread_cond_signal(&cond);
    pthread_mutex_unlock(&acmutex);
    
    return NULL; 
}

void * smcheckout(void *arg) {
    int id = ((supermarketcheckout*)arg)->id;
    //printf("sm id: %d \n",id);
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

    // qs=malloc(cf->K*sizeof(queue));
    // for (int i=0;i<cf->K; i++) qs[i]=createqueues();

    // queuemutex=malloc(cf->K*sizeof(pthread_mutex_t));
    // for (int i=0;i<cf->K;i++){
    //     if (pthread_mutex_init(&queuemutex[i], NULL) != 0) {
    //         fprintf(stderr, "pthread_mutex_init queue fallita\n");
    //         exit(EXIT_FAILURE);                   
    //     }
    // }

    csdata = malloc(cf->numcustomers*sizeof(customer));
    smdata = malloc(cf->K*sizeof(supermarketcheckout));

    if (!csdata || !smdata ){
        fprintf(stderr, "malloc fallita\n");
	    exit(EXIT_FAILURE);
    }

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