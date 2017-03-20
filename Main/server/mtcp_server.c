#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "mtcp_server.h"
#include "mtcp_common.h"

/* ThreadID for Sending Thread and Receiving Thread */
static pthread_t send_thread_pid;
static pthread_t recv_thread_pid;

static pthread_cond_t app_thread_sig = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t app_thread_sig_mutex = PTHREAD_MUTEX_INITIALIZER;

static pthread_cond_t send_thread_sig = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t send_thread_sig_mutex = PTHREAD_MUTEX_INITIALIZER;

static pthread_mutex_t info_mutex = PTHREAD_MUTEX_INITIALIZER;
//global Variables
int32_t state = -1;
int32_t SEQ = 0;
int32_t ACK = 0;
int32_t lastreceive = -1;
int32_t sfd; //socket_fd
char buff[MAX_BUF_SIZE];
struct sockaddr_in *dest_addr;
/* The Sending Thread and Receive Thread Function */
static void *send_thread();
static void *receive_thread();

void mtcp_accept(int socket_fd, struct sockaddr_in *server_addr){
    //3-way-handshake
    pthread_mutex_lock(&info_mutex);
    dest_addr = (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));
    sfd = socket_fd;
    memcpy(dest_addr,server_addr,sizeof(struct sockaddr_in));
    state=1;
    pthread_mutex_unlock(&info_mutex);
    //create thread
    pthread_create(&send_thread_pid, NULL, (void * (*)(void *))receive_thread, NULL);
    pthread_create(&recv_thread_pid, NULL, (void * (*)(void *))send_thread, NULL);


    // wake up send thread
    //pthread_mutex_lock(&send_thread_sig_mutex);
    //pthread_cond_signal(&send_thread_sig);
    //pthread_mutex_unlock(&send_thread_sig_mutex);

    //wait until accept success
    pthread_mutex_lock(&app_thread_sig_mutex);
    pthread_cond_wait(&app_thread_sig, &app_thread_sig_mutex);
    pthread_mutex_unlock(&app_thread_sig_mutex);

    printf("[SERVER] App Thread: 3-way ok\n");
    return;
}

int mtcp_read(int socket_fd, unsigned char *buf, int buf_len){
    // check whether buf is empty
    /*
    if (strlen(buf)==0){
      pthread_cond_wait(&app_thread_sig, &app_thread_sig_mutex);
    }
    else{
      fprintf(stderr, "Buffer is not empty\n");
    }
    */
    //change state to data transmission
    pthread_mutex_lock(&info_mutex);
    state=2;
    pthread_mutex_unlock(&info_mutex);
    //wait until data transmission success
    pthread_mutex_unlock(&app_thread_sig_mutex);
    pthread_cond_wait(&app_thread_sig,&app_thread_sig_mutex);
    pthread_mutex_unlock(&app_thread_sig_mutex);
    //wake up
    //read data from the receive buffer
    memcpy(&buf, buff, strlen(buff));

    return strlen(buff);
}

void mtcp_close(int socket_fd){
    //change state to 4-way
    pthread_mutex_lock(&info_mutex);
    state=3;
    pthread_mutex_unlock(&info_mutex);

    //wait until close success
    pthread_mutex_lock(&app_thread_sig_mutex);
    pthread_cond_wait(&app_thread_sig, &app_thread_sig_mutex);
    pthread_mutex_unlock(&app_thread_sig_mutex);

    return;
}

static void *send_thread(){
    int32_t local_ack=0;
    int32_t shutdown = 0;
    //int32_t local_seq;
    while(!shutdown){
      mTCPPacket* packet = (mTCPPacket*) malloc(sizeof(mTCPPacket));
      mTCPHeader header;
      //sleep
      pthread_cond_wait(&send_thread_sig, &send_thread_sig_mutex);
      //check state
      pthread_mutex_lock(&info_mutex);
      //local_seq = SEQ;
      pthread_mutex_unlock(&info_mutex);
      if (state==1){
        header=pack_header(mTCP_SYN_ACK,0);
        packet->header=header;
        memset(packet->buffer, 0, 1000);
        sendto(sfd, (void*)packet, sizeof(packet), 0, (struct sockaddr*)dest_addr,
                sizeof(*dest_addr));
      }
      else if(state==2){
        // send ACK packet out
        local_ack=SEQ+200;
        header=pack_header(mTCP_ACK, local_ack);
        packet->header=header;
        pthread_mutex_lock(&info_mutex);
        ACK=SEQ+200;
        memset(packet->buffer, 0, 1000);
        pthread_mutex_unlock(&info_mutex);
        sendto(sfd, (void*)packet, sizeof(packet), 0, (struct sockaddr*)dest_addr,
                sizeof(*dest_addr));
        //wake up application thread
        pthread_mutex_lock(&app_thread_sig_mutex);
        pthread_cond_signal(&app_thread_sig);
        pthread_mutex_unlock(&app_thread_sig_mutex);
      }
      else if(state==3){
        header=pack_header(mTCP_FIN_ACK, 0);
        packet->header=header;
        memset(packet->buffer, 0, 1000);
        sendto(sfd, (void*)packet, sizeof(packet), 0, (struct sockaddr*)dest_addr,
                sizeof(*dest_addr));
        shutdown=1;
      }
      free(packet);
    }
    pthread_exit(0);
}

static void *receive_thread(){
    int32_t shutdown = 0;
    char buf[MAX_BUF_SIZE];
    //int32_t local_seq=-1;
    mTCPHeader header = 0;
    int32_t type = -1; int32_t rest = -1;
    while(!shutdown){
      //mTCPPacket *received = (mTCPPacket*)malloc(sizeof(mTCPPacket));
      int32_t length;
      socklen_t fromlen=sizeof(*dest_addr);
      length = recvfrom(sfd,  buf, MAX_BUF_SIZE,0,
              (struct sockaddr *)dest_addr, &fromlen);
      if(length <= 0){
          continue;
          fprintf(stderr,"Error on receiving data\n");
        }
        memcpy(&header,buf,4);
        memcpy(&buff,buf, strlen(buf));
        unpack_header(&header, &type, &rest);

        pthread_mutex_lock(&info_mutex);
        lastreceive = type;
        ACK=rest;
        //local_seq=SEQ;
        pthread_mutex_unlock(&info_mutex);
            if(state == -1){
                fprintf(stderr,"State not updated I bet");
            }
            if(state == 1){ // 3-way handshake
                if(type == mTCP_SYN){
                //wake up send thread
                    pthread_mutex_lock(&send_thread_sig_mutex);
                    pthread_cond_signal(&send_thread_sig);
                    pthread_mutex_unlock(&send_thread_sig_mutex);
                }
                else if(type==mTCP_ACK){
                //wake up app thread to return
                    pthread_mutex_lock(&send_thread_sig_mutex);
                    pthread_cond_signal(&app_thread_sig);
                    pthread_mutex_unlock(&send_thread_sig_mutex);
                    printf("[SERVER] Receive Thread: 3-way ok\n");
                }
                else{
                    fprintf(stderr,"Error on 3-way handshake at server\n");
                }
            }
            if(state == 2){ // data transmission
                printf("state = %d\n",state);
                if(type == mTCP_DATA){
                    pthread_mutex_lock(&info_mutex);
                    SEQ=ACK;
                    pthread_mutex_unlock(&info_mutex);
                    //wake up sending thread
                    pthread_mutex_lock(&send_thread_sig_mutex);
                    pthread_cond_signal(&send_thread_sig);
                    pthread_mutex_unlock(&send_thread_sig_mutex);
                }
                else{
                    fprintf(stderr,"Error on data transmission at server\n");
                }
            }
            if(state == 3){ // 4-way handshake
                if(type == mTCP_FIN){
                    //wake up send thread
                    pthread_mutex_lock(&send_thread_sig_mutex);
                    pthread_cond_signal(&send_thread_sig);
                    pthread_mutex_unlock(&send_thread_sig_mutex);
                }
                if(type == mTCP_ACK){
                //wake up application thread
                    pthread_mutex_lock(&app_thread_sig_mutex);
                    pthread_cond_signal(&app_thread_sig);
                    pthread_mutex_unlock(&app_thread_sig_mutex);
                    //terminate itself
                    shutdown=1;
                }
            }
            //free(received);
        }
        pthread_exit(0);
}
