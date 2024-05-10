#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

int empty(struct queue_t * q) {
        if (q == NULL) return 1;
	return (q->size == 0);
}

void enqueue(struct queue_t * q, struct pcb_t * proc) {//what happen if the queue is full
        /* TODO: put a new process to queue [q] */
        if(q->size<MAX_QUEUE_SIZE){
                q->proc[q->size++] = proc;
        }//else what happend?
}

struct pcb_t * dequeue(struct queue_t * q) {
        /* TODO: return a pcb whose prioprity is the highest
         * in the queue [q] and remember to remove it from q
         * */
        struct pcb_t * d_proc = NULL;
        if(!empty(q)){
                int max_prio = 0;
                for(int i = 0; i < q->size; i++){
                        if(q->proc[i]->priority<q->proc[max_prio]->priority)
                                max_prio = i;       
                }
                d_proc = q->proc[max_prio];
                //shift and overide to remove
                for(int i = max_prio; i < q->size - 1; i++){
                        q->proc[i] = q->proc[i+1];
                }
                q->size--;
        }
	return d_proc;
}
