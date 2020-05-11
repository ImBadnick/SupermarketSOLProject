typedef struct queue {
    struct queuenode *head;
    int queueopen;
}queue;

typedef struct queuenode{
    customer *cs;
    struct queuenode *next;
}queuenode;

queue * createqueues(int id) {
       struct queue * q = malloc(sizeof(queue));
       q->head=NULL;
       q->queueopen=0;
       return q;
 }

int joinqueue(queue ** qs,customer * cs) {
    queuenode * q;
    if((q=malloc(sizeof(queuenode)))==NULL){
        return -1;
    }
    q->cs=cs;
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

queuenode * removecustomer(queue **qs) {
    queuenode * q = (*qs)->head;
    (*qs)->head=((*qs)->head)->next;
    return q;
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