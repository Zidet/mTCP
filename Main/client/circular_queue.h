#ifndef __CIRCULAR_QUEUE__

#define __CIRCULAR_QUEUE__


#define BUFFSIZE 1024*8

typedef struct myqueue{
  int front,rear;
  char buffer[BUFFSIZE];
}QUEUE;

QUEUE* createqueue();

int isempty(struct queue*q);

int isfull(QUEUE*q);

int enqueue(QUEUE *q, char data);

int dequeue(QUEUE *q);

//  Manipulate send buffer using enenqueue
int writeSendBuff(QUEUE *q, const char* data, int length);


// Wipe out data from the beginning
int wipeSendBuff(QUEUE *q, int length);

#endif
