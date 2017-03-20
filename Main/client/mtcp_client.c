#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include "mtcp_common.h"
#include "mtcp_client.h"
#include "circular_queue.h"

/* -------------------- Global Variables -------------------- */

/* ThreadID for send thread and receive thread */
static pthread_t send_thread_pid;
static pthread_t recv_thread_pid;

static pthread_cond_t app_thread_sig = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t app_thread_sig_mutex = PTHREAD_MUTEX_INITIALIZER;

static pthread_cond_t send_thread_sig = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t send_thread_sig_mutex = PTHREAD_MUTEX_INITIALIZER;

static pthread_mutex_t info_mutex = PTHREAD_MUTEX_INITIALIZER;

// state check for whole program
// -1 : default false value
// 1  : 3-way hand shake
// 2  : data transmission
// 3  : 4-way hand shake
int32_t state = -1;
int32_t SEQ = 0;
int32_t ACK = 0;
int32_t lastreceive = -1;
int32_t sfd;  // socket_fd
int32_t all_sent = 0;
struct sockaddr_in *dest_addr; // server addr

struct send_pack{
    unsigned char buf[MAX_BUF_SIZE]; // local buffer for send thread

    // a flag to check whether some block of data has been received
    int32_t received;
};
struct send_pack *pack;

/* The Sending Thread and Receive Thread Function */
static void *send_thread();
static void *receive_thread();

// sending buffer
QUEUE* mtcp_buffer = NULL;

/* Connect Function Call (mtcp Version) */
void mtcp_connect(int socket_fd, struct sockaddr_in *server_addr){
    // change state to 3-way handshake
    // store server_addr & socket_fd
    pthread_mutex_lock(&info_mutex);
    mtcp_buffer = (QUEUE*) createqueue();
    dest_addr = (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));
    pack = (struct send_pack*)malloc(sizeof(struct send_pack)); // to keep track of a datapack
    pack->received = 0; //the datapack is not received yet obv
    sfd = socket_fd;
    memcpy(dest_addr,server_addr,sizeof(struct sockaddr_in));
    state = 1;
    pthread_mutex_unlock(&info_mutex);

    // create Sending & receive thread
    pthread_create(&send_thread_pid, NULL,
            (void * (*)(void *))send_thread,NULL);
    pthread_create(&recv_thread_pid, NULL,
            (void * (*)(void *))receive_thread,NULL);
    // wake up send thread
    pthread_mutex_lock(&send_thread_sig_mutex);
    pthread_cond_signal(&send_thread_sig);
    pthread_mutex_unlock(&send_thread_sig_mutex);

    // wait until connect(3-way handshake) success
    pthread_mutex_lock(&app_thread_sig_mutex);
    pthread_cond_wait(&app_thread_sig,&app_thread_sig_mutex);
    pthread_mutex_unlock(&app_thread_sig_mutex);



    printf("[CLIENT] App Thread: 3-way ok\n");

    return;
}

/* Write Function Call (mtcp Version) */
int mtcp_write(int socket_fd, unsigned char *buf, int buf_len){
    // 1. increment buffer
    // 2. change state to data transmission
    pthread_mutex_lock(&info_mutex);
    writeSendBuff(mtcp_buffer,buf,buf_len);
    state = 2;
    pack->received = 0; //the datapack is not received yet obv
    pthread_mutex_unlock(&info_mutex);
    // wake up send thread
    pthread_mutex_lock(&send_thread_sig_mutex);
    pthread_cond_signal(&send_thread_sig);
    pthread_mutex_unlock(&send_thread_sig_mutex);
    // none-blocking write!
    // return immediately
    // how to return with error
    printf("[CLIENT] App Thread: buf_len = %d\n", buf_len);
    return buf_len;
}

/* Close Function Call (mtcp Version) */
void mtcp_close(int socket_fd){
    // block until buffer clears
    while(!all_sent);
    // change state to 4-way
    pthread_mutex_lock(&info_mutex);
    state = 3;
    pthread_mutex_unlock(&info_mutex);

    // wake up send thread
    pthread_mutex_lock(&send_thread_sig_mutex);
    pthread_cond_signal(&send_thread_sig);
    pthread_mutex_unlock(&send_thread_sig_mutex);

    // wait until close(4-way handshake) success
    pthread_mutex_lock(&app_thread_sig_mutex);
    pthread_cond_wait(&app_thread_sig,&app_thread_sig_mutex);
    pthread_mutex_unlock(&app_thread_sig_mutex);

    return;
}

static void *send_thread(){
    int32_t shutdown = 0;  // flag: shutdown? 1:0
    // struct for time control
    struct timespec timeToWait;
    struct timeval now;
    mTCPHeader header = 0;
    while(!shutdown){
        int32_t local_lastreceive = -1;
        int32_t local_ack = -1;
        // mTCP packet for send
        mTCPPacket *packet = (mTCPPacket*)malloc(sizeof(mTCPPacket));
        // stage one - sleep 1s
        gettimeofday(&now,NULL);
        timeToWait.tv_sec = now.tv_sec+ 1;
        pthread_mutex_lock(&send_thread_sig_mutex);
        pthread_cond_timedwait(&send_thread_sig, &send_thread_sig_mutex, &timeToWait);
        pthread_mutex_unlock(&send_thread_sig_mutex);

        // check data transmission state
        pthread_mutex_lock(&info_mutex);
        local_lastreceive = lastreceive;
        local_ack = ACK;
        pthread_mutex_unlock(&info_mutex);
        printf("\n------------------------------------------\n");
        printf("[CLIENT] Send Thread Loop Started\n");
        printf("[CLIENT] Send Thread: state = %d\n",state);
        printf("------------------------------------------\n");
        // 3-way
        if(state == 1){
            // if SYN_ACK not received
            // the SYN packet is unsend/lost, resend the packet
            if(local_lastreceive != mTCP_SYN_ACK){
                // pack and send
                header = pack_header(mTCP_SYN, 0);
                packet->header = header;
                memset(packet->buffer, 0,1000);
                sendto(sfd, (void*)packet, sizeof(packet), 0, (struct sockaddr*)dest_addr,
                        sizeof(*dest_addr));
                printf("[CLIENT] Send Thread: mTCP_SYN Sent\n");
            }
            else if(local_lastreceive == mTCP_SYN_ACK){
                // if SYN-ACK received, send ACK
                // and then wake up application thread
                header = pack_header(mTCP_ACK, 0);
                packet->header = header;
                memset(packet->buffer, 0,1000);
                sendto(sfd, (void*)packet, sizeof(packet), 0, (struct sockaddr*)dest_addr,
                        sizeof(*dest_addr));
                printf("[CLIENT] Send Thread: (3-way) ACK sent\n");
                printf("[CLIENT] Send Thread: 3-way ok\n");
                // wake up application thread
                pthread_mutex_lock(&app_thread_sig_mutex);
                pthread_cond_signal(&app_thread_sig);
                pthread_mutex_unlock(&app_thread_sig_mutex);
            }
        }
        // data transmission
        else if(state == 2){
            // if data was lost, resend
            // error-proned: is there a better condition?
            if(pack->received == 0 && local_lastreceive != mTCP_SYN_ACK){
                // resemble data packet
                header = pack_header(mTCP_DATA,local_ack);
                packet->header = header;
                memcpy(packet->buffer,pack->buf,1000);
                sendto(sfd, (void*)packet, sizeof(packet), 0, (struct sockaddr*)dest_addr,
                        sizeof(*dest_addr));
                printf("[CLIENT] Send Thread: data to sent: %s\n\n",packet->buffer);
                printf("[CLIENT] Send Thread: data (SEQ: %d) resent\n",local_ack);
            }
            // all data sent have received by server,
            // and 1. send new data
            //     2. if no data left, terminate data transmission
            else{
                if(isempty(mtcp_buffer)){
                    all_sent = 1;
                    continue;
                }
                header = pack_header(mTCP_DATA,local_ack);
                packet->header = header;
                memset(packet->buffer, 0,1000);
                memset(pack->buf, 0, 1000);
                // Manipulate buffer
                pthread_mutex_lock(&info_mutex);
                SEQ = ACK; // update SEQ to lastest
                int j = 0;
                for(j = 0; j< 1000; j++){
                    // read from mtcp_buffer
                    // until it exhausts
                    int tmp = dequeue(mtcp_buffer);
                    if(tmp == -1){
                        break;
                    }
                    packet->buffer[j]=tmp;
                }
                memcpy(pack->buf,packet->buffer,sizeof(packet->buffer));
                pack->received = 0; // datapack sent, but not received
                pthread_mutex_unlock(&info_mutex);
                sendto(sfd, (void*)packet, sizeof(packet), 0, (struct
                            sockaddr*)dest_addr,sizeof(*dest_addr));
                printf("[CLIENT] Send Thread: data to sent: %s\n\n",packet->buffer);
                printf("[CLIENT] Send Thread: New data (SEQ: %d) sent\n",local_ack);
            }
        }
        else if(state == 3){
            // if FIN_ACK not received
            // the SYN packet is unsend/lost, resend the packet
            if(local_lastreceive != mTCP_FIN_ACK){
                header = pack_header(mTCP_FIN,0);
                packet->header = header;
                memset(packet->buffer, 0,1000);
                sendto(sfd, (void*)packet, sizeof(packet), 0, (struct
                            sockaddr *)dest_addr, sizeof(*dest_addr));
                printf("[CLIENT] Send Thread: mTCP_FIN (re)sent\n");
            }
            else if(local_lastreceive == mTCP_FIN_ACK){
                // if FIN-ACK received, send ACK
                // and then 1. terminate itself
                //          2. wake up application thread
                header = pack_header(mTCP_ACK,0);
                packet->header = header;
                memset(packet->buffer, 0,1000);
                sendto(sfd, (void*)packet, sizeof(packet), 0, (struct sockaddr*)dest_addr,
                        sizeof(*dest_addr));
                shutdown = 1;
                // wake up application thread
                pthread_mutex_lock(&app_thread_sig_mutex);
                pthread_cond_signal(&app_thread_sig);
                pthread_mutex_unlock(&app_thread_sig_mutex);
                printf("[CLIENT] Send Thread: FIN_ACK received, ACK sent\n");
                printf("[CLIENT] Send Thread: shutdown done\n");

            }
        }
        free(packet);
    }
    pthread_exit(0);
}

static void *receive_thread(){
    int32_t shutdown = 0;
    int32_t local_seq = -1;
    // local buffer for receive thread
    // it should be the received mtcp packet
    unsigned char buf[MAX_BUF_SIZE];
    // local buffer for header, type and rest(ACK/SEQ)
    mTCPHeader header = 0;
    int32_t type = -1; int32_t rest = -1;
    while(!shutdown){
        mTCPPacket *received = (mTCPPacket*)malloc(sizeof(mTCPPacket));
        int32_t length;
        memset(buf, 0, MAX_BUF_SIZE);
        socklen_t fromlen = sizeof(*dest_addr);
        length = recvfrom(sfd,  buf, MAX_BUF_SIZE,0,
                (struct sockaddr *)dest_addr, &fromlen);
        if(length <= 0){
            continue;
            fprintf(stderr,"Error on receiving data\n");
        }
        // get the hearder and unpack
        memcpy(&header,buf,4);
        unpack_header(&header, &type, &rest);

        printf("\n------------------------------------------\n");
        printf("[CLIENT] Receive Thread Loop Started\n");
        printf("[CLIENT] Receive Thread Loop: state = %d\n",state);
        printf("------------------------------------------\n");

        printf("[CLIENT] Receive Thread: On revceiving buf:\n");
        printf("[CLIENT]           type: %d\n", type);
        printf("[CLIENT]        SEQ/ACK: %d\n", rest);
        printf("[CLIENT] Receive Thread: header analysis\n\n");


        pthread_mutex_lock(&info_mutex);
        lastreceive = type;
        ACK = rest;
        local_seq = SEQ;
        pthread_mutex_unlock(&info_mutex);

        if(state == -1){
            fprintf(stderr,"State not updated I bet");
        }
        printf("[CLIENT] Receive Thread: ACK = %d\n", ACK);
        printf("[CLIENT] Receive Thread: data received, start analyzing state\n");
        printf("[CLIENT] Receive Thread: state = %d\n", state);
        if(state == 1){ // 3-way handshake
            // unpack header
            if(type == mTCP_SYN_ACK){
                // if SYN_ACK received, wake up send thread
                pthread_mutex_lock(&send_thread_sig_mutex);
                pthread_cond_signal(&send_thread_sig);
                pthread_mutex_unlock(&send_thread_sig_mutex);
                printf("[CLIENT] Receive Thread: SYN_ACK Received, switch to send thread\n");
            }else{
                fprintf(stderr,"Error on 3-way handshake\n");
            }
        }
        else if(state == 2){ // data transmission
            if(type == mTCP_ACK && rest > local_seq){
                // if ACK received,
                // update the datapack to marked it received
                // and TODO: try to get new data
                pthread_mutex_lock(&info_mutex);
                pack->received = 1;
                pthread_mutex_unlock(&info_mutex);

                // wake up send thread and send new data
                // note that seq is changed in send thread to for maintanence
                pthread_mutex_lock(&send_thread_sig_mutex);
                pthread_cond_signal(&send_thread_sig);
                pthread_mutex_unlock(&send_thread_sig_mutex);
                printf("[CLIENT] Receive Thread: ACK Received, switch to send thread\n");
            }
        }
        else if(state == 3){// 4-way handshake
            if(type == mTCP_FIN_ACK){
                // if FIN_ACK received, wake up send thread for termination
                // terminate the thread
                pthread_mutex_lock(&send_thread_sig_mutex);
                pthread_cond_signal(&send_thread_sig);
                pthread_mutex_unlock(&send_thread_sig_mutex);
                shutdown = 1;
                printf("[CLIENT] Receive Thread: ACK Received, switch to send thread\n");
                printf("[CLIENT] Receive Thread: shutdown\n");
            }
        }
        free(received);
    }
    pthread_exit(0);
}
