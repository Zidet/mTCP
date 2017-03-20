///////////////////////////////////////////////////////////////////////////////
//
//   File          : mtcp_common.c
//   Description   : This is the implementation of the common funcions for the
//                   CSCI 4430 project
//   Author        : LIU Yide, Zhang Sibin, LIU Cheng-Tsung
//   Last Modified : 25/02/2017
//


// Project Include
#include <mtcp_common.h>

// Include
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <arpa/inet.h>

///////////////////////////////////////////////////////////////////////////////
//
//   Function      : pack_header
//   Description   : Pack the mTCP header for the packet
//
//   Inputs        : Type value, SEQ/ACK info
//   Outputs       : mTCP Header
mTCPHeader pack_header(int32_t type, int32_t seq){
    // Temp var
    mTCPHeader head;
    int32_t mt,mr;

    // convert seq to B-Endian
    //seq = htonl(seq);

    // Pack
    mt = (type & 0xF) << 28;
    mr = (seq & 0xFFFFFFF);
    head = mt | mr;
    head = htonl(head);

    if(type < 0 || type > 5){
        fprintf(stderr, "Pack_header fail: Invalid type range\n");
        return -1;
    }

    return head;
}

///////////////////////////////////////////////////////////////////////////////
//
//   Function      : unpack_header
//   Description   : Unpack the mTCP header and retrieve information
//
//   Inputs        : mTCPheader
//   Outputs       : 0 success, -1 fail
int32_t unpack_header(mTCPHeader *head, int32_t *type, int32_t *seq){
    *head = ntohl(*head);
    *type = (*head >> 28) & 0xF;
    *seq = *head & 0xFFFFFFF;

    if(*type < 0 || *type > 5){
        fprintf(stderr, "Unpack_header fail: Invalid type retrieved\n");
        return -1;
    }

    return 0;
}
