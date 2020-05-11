typedef struct config {
    int K;
    int C;
    int E;
    int T;
    int P;
    int S;
    int S1;
    int S2;
    int numcustomers;
    int smopen;
    int directornews;
}config;


int confcheck(config * cf) {
    if (!(cf->P>=0 && cf->T>10 && cf->K>0 && (cf->E>0 && cf->E<cf->C) && cf->C>1 && cf->S1>0 && cf->numcustomers>0 && cf->S1<cf->S2 && cf->smopen>0 && cf->smopen<=cf->K && cf->directornews>0)){
        fprintf(stderr,"conf not valid, constraints: P>=0, T>10, K>1, 0<E<C, C>1, min<=max\n");
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
    cf->S1=2; //if there are S1 supermarket checkouts with at most 1 customer --> close one supermarket checkouts
    cf->S2=10; //if there is a supermarket checkouts with S2 customers --> open one supermarket checkouts
    cf->numcustomers=100;
    cf->smopen=3;
    cf->directornews=3000;

    if ((control=confcheck(cf))==-1) return NULL;
    return cf;
}

