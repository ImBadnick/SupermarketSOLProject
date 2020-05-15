#define MAX_LEN 256

typedef struct config {
    int K;
    int C;
    int E;
    int T;
    int P;
    int S;
    int S1;
    int S2;
    int smopen; //How much sm are opened at the start of the program
    int directornews; //Update time used from sm to send queue length to director
}config;

void printconf(config configvalues);

int confcheck(config * cf) {
    if (!(cf->P>=0 && cf->T>10 && cf->K>0 && (cf->E>0 && cf->E<cf->C) && cf->C>1 && cf->S1>0  && cf->S1<cf->S2 && cf->smopen>0 && cf->smopen<=cf->K && cf->directornews>0)){
        fprintf(stderr,"conf not valid, constraints: P>=0, T>10, K>1, 0<E<C, C>1, min<=max\n");
        return -1;
    }
    else return 1;
}

config * test(const char* configfile) {
    int control,i=0,j;
    config * cf;
    FILE *fd=NULL;
    char *buffer;
    char *cpy;

    if ((fd=fopen(configfile, "r")) == NULL) {
        fclose(fd);
	    return NULL;
    }

    if ((cf=malloc(sizeof(config))) == NULL) {
        fclose(fd); free(cf);
	    return NULL;    
    }

    if ((buffer=malloc(MAX_LEN*sizeof(char))) == NULL) {
         fclose(fd); free(cf); free(buffer);
         return NULL;
    }

    while(fgets(buffer, MAX_LEN, fd) != NULL) {
        j=0;
        cpy=buffer;
        while(*buffer!='=') { buffer++; j++;}
        buffer++;
        switch (i)
        {
            case 0: cf->K=atoi(buffer);
                break;
            case 1: cf->C=atoi(buffer);
                break;
            case 2: cf->E=atoi(buffer);
                break;
            case 3: cf->T=atoi(buffer);
                break;
            case 4: cf->P=atoi(buffer);
                break;
            case 5: cf->S=atoi(buffer);
                break;
            case 6: cf->S1=atoi(buffer);
                break;
            case 7: cf->S2=atoi(buffer);
                break;
            case 8: cf->smopen=atoi(buffer);
                break;
            case 9: cf->directornews=atoi(buffer);
                break;

            default:
                break;
        }
        i++;
        buffer=cpy;
    }

    if ((control=confcheck(cf))==-1) {
        fclose(fd); free(cf); free(buffer);
        return NULL;
    }
    free(buffer);
    fclose(fd); 
    
    return cf;
}


void printconf(config configvalues) {
    printf("%d %d %d %d %d %d %d %d %d %d\n", configvalues.K, configvalues.C, configvalues.E, configvalues.T, configvalues.P, configvalues.S, configvalues.S1, configvalues.S2, configvalues.smopen, configvalues.directornews);
}