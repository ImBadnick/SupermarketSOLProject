typedef struct supermarketcheckout {
    int id;
    int nproducts;
    int ncustomers;
    int time;
    int servicetime;
    int nclosure;
}supermarketcheckout;


void setupsm(supermarketcheckout * sm, int i) {
    sm->id = (i+1);
    sm->nproducts=0;
    sm->ncustomers=0;
    sm->time=0;
    sm->servicetime=0;
    sm->nclosure=0;   
}


void printsm(supermarketcheckout sm) {
    printf("%d %d %d %d %d %d\n",sm.id,sm.nproducts,sm.ncustomers,sm.time,sm.servicetime,sm.nclosure);
}