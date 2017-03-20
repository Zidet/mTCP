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
int32_t read_length;
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
    read_length=0;
    state=2;
    pthread_mutex_unlock(&info_mutex);
    //wait until data transmission success
    pthread_mutex_unlock(&app_thread_sig_mutex);
    pthread_cond_wait(&app_thread_sig,&app_thread_sig_mutex);
    pthread_mutex_unlock(&app_thread_sig_mutex);
    //wake up
    //read data from the receive buffer
    //memcpy(buf, buff, strlen(buff));

    return read_length;
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
        local_ack = ACK;
        pthread_mutex_unlock(&info_mutex);

        printf("\n------------------------------------------\n");
        printf("[SERVER] Send Thread Loop Started\n");
        printf("[SERVER] Send Thread: state = %d\n",state);
        printf("------------------------------------------\n");

        printf("[SERVER] Send Thread: state = %d\n", state);
        if (state==1){
            header=pack_header(mTCP_SYN_ACK,0);
            packet->header=header;
            memset(packet->buffer, 0, 1000);
            sendto(sfd, (void*)packet, sizeof(*packet), 0, (struct sockaddr*)dest_addr,
                    sizeof(*dest_addr));
            printf("[SERVER] Send Thread: SYN_ACK sent\n");
        }
        else if(state==2){
            // send ACK packet out
            header=pack_header(mTCP_ACK, local_ack);
            packet->header=header;
            pthread_mutex_lock(&info_mutex);
            pthread_mutex_unlock(&info_mutex);
            memset(packet->buffer, 0, 1000);

            sendto(sfd, (void*)packet, sizeof(*packet), 0, (struct sockaddr*)dest_addr,
                    sizeof(*dest_addr));
            printf("[SERVER] Send Thread: ACK (#%d) sent\n",local_ack);
            //wake up application thread
            pthread_mutex_lock(&app_thread_sig_mutex);
            pthread_cond_signal(&app_thread_sig);
            pthread_mutex_unlock(&app_thread_sig_mutex);
        }
        else if(state==3){
            header=pack_header(mTCP_FIN_ACK, 0);
            packet->header=header;
            memset(packet->buffer, 0, 1000);
            sendto(sfd, (void*)packet, sizeof(*packet), 0, (struct sockaddr*)dest_addr,
                    sizeof(*dest_addr));
            shutdown=1;
            printf("[SERVER] Send Thread: FIN-ACK sent\n");
        }
        free(packet);
    }
    pthread_exit(0);
}

static void *receive_thread(){
    int32_t shutdown = 0;
    char buf[MAX_BUF_SIZE];
    char buff[MAX_BUF_SIZE];
    memset(buf,0,MAX_BUF_SIZE);
    memset(buff,0,MAX_BUF_SIZE);
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
        printf("[SERVER] Receive Thread: Buffer Length = %d\n",length);
        memcpy(&header,buf,4);
        memcpy(buff,buf + 4, length-4);
        unpack_header(&header, &type, &rest);
        printf("\n------------------------------------------\n");
        printf("[SERVER] Receive Thread Loop Started\n");
        printf("[SERVER] Receive Thread Loop: state = %d\n",state);
        printf("------------------------------------------\n");


        printf("[SERVER] Receive Thread: On revceiving buf:\n");
        printf("[SERVER]           type: %d\n", type);
        printf("[SERVER]        SEQ/ACK: %d\n", rest);
        printf("[SERVER] Receive Thread: header analysis\n\n");

        pthread_mutex_lock(&info_mutex);
        lastreceive = type;
        ACK=rest;
        //local_seq=SEQ;
        pthread_mutex_unlock(&info_mutex);

        printf("[SERVER] Receive Thread: state = %d\n", state);
        if(state == -1){
            fprintf(stderr,"State not updated I bet");
            continue;
        }
        else if(state == 1){ // 3-way handshake
            if(type == mTCP_SYN){
                //wake up send thread
                printf("[SERVER] Receive Thread: SYN received\n");
                pthread_mutex_lock(&send_thread_sig_mutex);
                pthread_cond_signal(&send_thread_sig);
                pthread_mutex_unlock(&send_thread_sig_mutex);
            }
            else if(type==mTCP_ACK){
                //wake up app thread to return
                printf("[SERVER] Receive Thread: ACK received\n");
                pthread_mutex_lock(&send_thread_sig_mutex);
                pthread_cond_signal(&app_thread_sig);
                pthread_mutex_unlock(&send_thread_sig_mutex);
                printf("[SERVER] Receive Thread: 3-way ok\n");
            }
            else{
                fprintf(stderr,"Error on 3-way handshake at server\n");
            }
            continue;
        }
        else if(state == 2){ // data transmission
            //printf("state = %d\n",state);
            if(type == mTCP_DATA){
                printf("[SERVER] Receive Thread: data received\n");
                printf("[SERVER] Receive Thread: data: \n%s\n",buff);
                printf("[SERVER] Receive Thread: buff length = %ld\n",strlen(buff));
                pthread_mutex_lock(&info_mutex);
                ACK=ACK+strlen(buff);
                pthread_mutex_unlock(&info_mutex);
                //wake up sending thread
                pthread_mutex_lock(&send_thread_sig_mutex);
                pthread_cond_signal(&send_thread_sig);
                pthread_mutex_unlock(&send_thread_sig_mutex);
            }
            else{
                printf("why error on data transmission? type = %d\n",type);
                printf("\n");
                fprintf(stderr,"Error on data transmission at server\n");
            }
            continue;
        }
        else if(state == 3){ // 4-way handshake
            if(type == mTCP_FIN){
                //wake up send thread
                printf("[SERVER] Receive Thread: fin received\n");
                pthread_mutex_lock(&send_thread_sig_mutex);
                pthread_cond_signal(&send_thread_sig);
                pthread_mutex_unlock(&send_thread_sig_mutex);
            }
            else if(type == mTCP_ACK){
                //wake up application thread
                printf("[SERVER] Receive Thread: ACK received\n");
                pthread_mutex_lock(&app_thread_sig_mutex);
                pthread_cond_signal(&app_thread_sig);
                pthread_mutex_unlock(&app_thread_sig_mutex);
                //terminate itself
                shutdown=1;
            }
            continue;
        }
        //free(received);
    }
    pthread_exit(0);
}
