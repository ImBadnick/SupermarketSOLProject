typedef struct queue {
    customer * cs;
    struct queue *next;
}queue;

queue * createqueues() {
       struct queue * q = NULL;
       return q;
 }

int joinqueue(queue ** qs,customer * cs) {
    queue * q;
    if((q=malloc(sizeof(queue)))==NULL){
        return -1;
    }
    q->cs=cs;
    q->next=NULL;
    queue * curr = (*qs);
    if ((*qs)==NULL){
        (*qs)=q;
        return 1;
    }

    while(curr->next!=NULL) curr=curr->next;
    curr->next = q;
    q->next=NULL;
    return 1;
}

queue * removecustomer(queue **qs) {
    queue * q = (*qs);
    (*qs)=(*qs)->next;
    return q;
}

int queuelength(queue *qs) {
    queue *curr=qs;
    int counter=0;
    while(curr!=NULL){
        counter++;
        curr=curr->next;
    }
    return counter;
}

int queuesearch(queue *qs) {
    queue *curr=qs;
    int counter=0;
    while(curr!=NULL){
        counter++;
        curr=curr->next;
    }
    return counter;
}