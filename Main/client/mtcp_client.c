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

/* -------------------- Global Variables -------------------- */

/* ThreadID for Sending Thread and Receiving Thread */
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
int32_t& ACK = SEQ;
int32_t lastreceive = -1;

threadpack pack;


/* Connect Function Call (mtcp Version) */
void mtcp_connect(int socket_fd, struct sockaddr_in *server_addr){
  // fill info for pack
  pthread_mutex_lock(&info_mutex);
  pack.socket_fd = socket_fd;
  pack.sockaddr = (struct sockaddr*) server_addr;
  pthread_mutex_unlock(*info_mutex);

  // create Sending & Receiving thread
  pthread_create(&send_thread_pid, NULL,
    (void * (*)(void *))send_thread,&pack),;
    pthread_create(&recv_thread_pid, NULL,
      (void * (*)(void *))receive_thread,&pack);


      // wait until connect success
      pthread_mutex_lock(&app_thread_sig_mutex);
      state = 0;
      pthread_cond_wait(&app_thread_sig,&app_thread_sig_mutex);
      pthread_mutex_unlock(&app_thread_sig_mutex);

      return;
    }

    /* Write Function Call (mtcp Version) */
    int mtcp_write(int socket_fd, unsigned char *buf, int buf_len){
      // fill info for pack
      pthread_mutex_lock(&info_mutex);
      pack.socket_fd = socket_fd;
      pack.buf = buf;
      pack.buflen = buf_len;
      pthread_mutex_unlock(*info_mutex);

      pthread_mutex_lock(&app_thread_sig_mutex);
      state = 2;

      pthread_cond_wait(&app_thread_sig,&app_thread_sig_mutex);
      pthread_mutex_unlock(&app_thread_sig_mutex);

    }

    /* Close Function Call (mtcp Version) */
    void mtcp_close(int socket_fd){

    }

    static void *send_thread(threadpack* tosend){
      int32_t shutdown = 0;
      struct timespec timeToWait;
      struct timespec now;
      int32_t i = 0;
      mTCPPacket* packet = (mTCPPacket*) malloc(sizeof(mTCPPacket));
      mTCPHeader header;
      while(!shutdown){
        int32_t local_lastreceive = -1;
        // stage one - sleep 1s
        gettimeofday(&now,NULL);
        timeToWait.tv_sec = now.tv_sec+timeIns;
        pthread_mutex_lock(&send_thread_sig_mutex);
        pthread_cond_timedwait(&send_thread_sig, &send_thread_sig_mutex, &timeToWait);
        pthread_mutex_unlock(&send_thread_sig_mutex);

        // check state
        pthread_mutex_lock(&info_mutex);
        local_lastreceive = lastreceive;
        local_seq = SEQ;
        pthread_mutex_unlock(&info_mutex);

        // 3-way
        if(state == 1){
          if(local_lastreceive != mTCP_SYN_ACK){
            // pack and send
            header = pack_header(mTCP_SYN, 0);
            packet->header = header;
            memcpy(packet->buffer, tosend->buf,1000);
            sendto(tosend->socket_fd, (void*)packet, sizeof(packet), tosend->dest_addr,
            sizeof(tosend->dest_addr));
          }
          else if(local_lastreceive == mTCP_SYN_ACK){
            // after receiving SYN-ACK, send ACK
            header = pack_header(mTCP_ACK, 0);
            packet->header = header;
            memcpy(packet->buffer, tosend->buf,1000);
            sendto(tosend->socket_fd, (void*)packet, sizeof(packet), tosend->dest_addr,
            sizeof(tosend->dest_addr));
          }
        }
        // data transmission
        else if(state == 2){
          header = pack_header(mTCP_DATA,local_seq);
          packet->header = header;
          // remember to check if SEQ is updated with buf_len
          memcpy(packet->buffer,tosend->buf + ,1000);
          sendto(tosend->socket_fd, (void*)packet, sizeof(packet), tosend->dest_addr,
          sizeof(tosend->dest_addr));
        }
        else if(state == 3){
          if(local_lastreceive != mTCP_FIN_ACK){
            header = pack_header(mTCP_FIN,0);
            memcpy(packet->buffer,tosend->buf,1000);
            sendto(tosend->socket_fd, (void*)packet, sizeof(packet), tosend->dest_addr, sizeof(tosend->dest_addr));
          }
          else if(local_lastreceive == mTCP_FIN_ACK){
            header = pack_header(mTCP_ACK,0);
            memcpy(packet->buffer,tosend->buf,1000);
            sendto(tosend->socket_fd, (void*)packet, sizeof(packet), tosend->dest_addr, sizeof(tosend->dest_addr));
            shutdown = 1;
          }
        }
      }
      // wake Application layer function
      pthread_mutex_lock(&app_thread_sig_mutex);
      pthread_cond_signal(&app_thread_sig);
      pthread_mutex_unlock(&app_thread_sig_mutex);
    }

    static void *receive_thread(threadpack* toreceive){
      int32_t shutdown = 0;
      char buf[MAX_BUF_SIZE];
      mTCPHeader* header = (mTCPHeader*) malloc(sizeof(mTCPHeader));
      int32_t type = -1; int32_t rest = -1;
      while(!shutdown){
        mTCPPacket *received = (mTCPPacket*)malloc(sizeof(mTCPPacket));
        int32_t length;
        length = recvfrom(toreceive->socket_fd,  buf, MAX_BUF_SIZE,
          toreceive->dest_addr, sizeof(toreceive->dest_addr));
          if(length <= 0){
            fprintf(stderr,"Error on receiving data\n");
          }
          memcpy(header,buf,4);
          unpack_header(header, &type, &rest);
          pthread_mutex_lock(&info_mutex);
          lastreceive = type;
          pthread_mutex_unlock(&info_mutex);
          switch (state) {
            case -1:
            fprintf(stderr,"State not updated I bet");
            case 1: // 3-way handshake
            // unpack header
            if(type == mTCP_SYN_ACK){
              pthread_mutex_lock(&send_thread_sig_mutex);
              pthread_cond_signal(&send_thread_sig);
              pthread_mutex_unlock(&send_thread_sig_mutex);
            }else{
              fprintf(stderr,"Error on 3-way handshake\n");
            }
            case 2: // data transmission
            pthread_mutex_lock(&info_mutex);
            if(type == mTCP_ACK && rest == SEQ){
              pthread_mutex_lock(&send_thread_sig_mutex);
              pthread_cond_signal(&send_thread_sig);
              pthread_mutex_unlock(&send_thread_sig_mutex);
            }
            pthread_mutex_unlock(&info_mutex);

            case 3: // 4-way handshake
            if(type == mTCP_FIN_ACK){
              pthread_mutex_lock(&send_thread_sig_mutex);
              pthread_cond_signal(&send_thread_sig);
              pthread_mutex_unlock(&send_thread_sig_mutex);
            }
          }
        }

      }

      int main(){




      }
