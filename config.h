typedef struct config {
    int K;
    int C;
    int E;
    int T;
    int P;
    int S;
    int max;
    int numcustomers;
}config;


int confcheck(config * cf) {
    if (!(cf->P>=0 && cf->T>10 && cf->K>1 && (cf->E>0 && cf->E<cf->C) && cf->C>1 && cf->max>0 && cf->numcustomers>0)){
        fprintf(stderr,"conf not valid, constraints: P>=0, T>10, K>1, 0<E<C, C>1\n");
        return -1;
    }
    else return 1;
}

config * test() {
    int control;
    config * cf;
    cf=malloc(sizeof(config));
    cf->K=6;
    cf->C=50;
    cf->E=3;
    cf->T=200;
    cf->P=100;
    cf->S=20;
    cf->max=4; //Number of max element to dont open another queue
    cf->numcustomers=100;

    if ((control=confcheck(cf))==-1) return NULL;
    return cf;
}

