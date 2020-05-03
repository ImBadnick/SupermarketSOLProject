typedef struct queue {
    customer * cs;
    pthread_mutex_t list_mutex;
}queue;


queue * createqueues() {
       struct queue * q = NULL;
       return q;
 }

void joinqueue(queue ** qs,customer * cs) {
    queue * q = malloc(sizeof(queue));
    q->cs=cs;
    q->next=NULL;
    queue * curr = (*qs);
    if ((*qs)==NULL){
        (*qs)=q;
        return;
    }

    while(curr->next!=NULL) curr=curr->next;
    curr->next = q;
    q->next=NULL;
    
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