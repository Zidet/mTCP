
#ifndef __CIRCULAR_QUEUE__

#define __CIRCULAR_QUEUE__


#define BUFFSIZE 268435456

typedef struct queue{
  int front;
  int rear;
  char buffer[BUFFSIZE];
}QUEUE;

QUEUE* createqueue();

int isempty(QUEUE*q);

int isfull(QUEUE*q);

int queuesize(QUEUE *q);

int enqueue(QUEUE *q, char data);

int dequeue(QUEUE *q);

//  Manipulate send buffer using enenqueue
int writeSendBuff(QUEUE *q, unsigned char* data, int length);


// Wipe out data from the beginning
int wipeSendBuff(QUEUE *q, int length);

#endif
