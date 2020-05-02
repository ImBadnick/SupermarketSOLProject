typedef struct queue {
    customer cs;
    struct queue *next;
}queue;


void joinqueue(queue qs,customer cs) {
    queue * q = malloc(sizeof(queue));
    queue * curr = qs;
    if (qs==NULL){
        qs->next=NULL
        qs->cs=cs;
    }
    else{
        while(curr->next!=NULL) curr=curr->next;
        q->cs=cs;
        q->next=NULL;
        curr->next=q;
    }
}

void createqueues() {
    struct queue q = NULL;
    return q;
}