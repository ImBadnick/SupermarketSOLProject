
typedef struct queue {
    struct queuenode *head;
    int queueopen;
}queue;

typedef struct queuenode{
    customer *cs;
    struct queuenode *next;
}queuenode;

void printQueue(queue *qs, int id   );

queue * createqueues(int id) {
       struct queue * q = malloc(sizeof(queue));
       q->head=NULL;
       q->queueopen=0;
       return q;
 }

int joinqueue(queue ** qs,customer ** cs,int nqueue) {
    queuenode * q;
    if((q=malloc(sizeof(queuenode)))==NULL){
        return -1;
    }
    q->cs=(*cs);
    q->next=NULL;
    queuenode * curr = (*qs)->head;
    if ((*qs)->head==NULL){
        (*qs)->head=q;
        return 1;
    }
    while(curr->next!=NULL) curr=curr->next;
    curr->next = q;
    q->next=NULL;
    return 1;
}

customer * removecustomer(queue **qs) {
    queuenode * q = (*qs)->head;
    (*qs)->head=((*qs)->head)->next;    
    customer * tmp = q->cs;
    free(q);
    return tmp;
}

int queuelength(queue *qs) {
    queuenode *curr=qs->head;
    int counter=0;
    while(curr!=NULL){
        counter++;
        curr=curr->next;
    }
    return counter;
}

void printQueue(queue *qs, int id) {
    queuenode *curr=qs->head;
    printf("QUEUE %d: ",id);
    while(curr!=NULL){
        printf("%d -> ",(*(curr->cs)).id);
        curr=curr->next;
    }
    printf("\n");
    fflush(stdout);
}