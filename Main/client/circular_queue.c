#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "circular_queue.h"

QUEUE* createqueue(){
  struct queue* q=(QUEUE*)malloc(sizeof(QUEUE));
  q->front=q->rear=-1;
  return q;
}

int isempty(struct queue*q){
  return(q->front==-1);
}

int isfull(QUEUE*q){
  return((q->rear+1)%BUFFSIZE==q->front);
}

int enqueue(QUEUE *q, char data){
  if(isfull(q))
  {
    fprintf(stderr,"\nQUEUE is FULL\n");
    return -1;
  }
  if(q->front==-1)
  {
    q->front=q->rear=q->rear+1%BUFFSIZE;
  }
  else
  {
    q->rear=q->rear+1%BUFFSIZE;
  }
  q->buffer[q->rear]=data;
}

int dequeue(QUEUE *q){
  int data;
  if(isempty(q))
  {
    fprintf(stderr, "\nQUEUE is Empty\n");
    return -1;
  }

  data=q->buffer[q->front];
  if(q->front==q->rear)
    q->front=q->rear=-1;
  else
    q->front=q->front+1%BUFFSIZE;

  return data;
}

//  Manipulate send buffer using enenqueue
int writeSendBuff(QUEUE *q, const char* data, int length){
  int i = 0;
  for(i = 0; i < length; i++){
    if(enqueue(q, data[i])){
      return -1;
    };
  }
  return 0;
}


// Wipe out data from the beginning
int wipeSendBuff(QUEUE *q, int length){
  int i = 0;
  for(i = 0; i < length; i++){
    if(dequeue(q)==-1){
      return -1;
    }
  }
  return 0;
}
