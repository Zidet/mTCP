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
    seq = htonl(seq);

    // Pack
    mt = (type & 0xF) << 28;
    mr = (seq & 0x0FFFFFFF);
    head = mt | mr;

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
    *type = (*head >> 28) & 0xF;
    *seq = *head & 0x0FFFFFFF;
    *seq = ntohl(*seq);

    if(*type < 0 || *type > 5){
        fprintf(stderr, "Unpack_header fail: Invalid type retrieved\n");
        return -1;
    }

    return 0;
}

void create_packet(unsigned char *packet, unsigned char type, unsigned int seq, unsigned char *data, size_t data_len)
{
	memset(packet,0,MAX_BUF_SIZE+4);
	unsigned int header = htonl(((type & 0xf)<<28) | (seq & 0x0fffffff));
	*((unsigned int *)packet) = header;
	if(data)
		memcpy(packet+4,data,data_len);
}

unsigned char get_packet_type(unsigned char *packet)
{
	unsigned int header = ntohl(*((unsigned int *)packet));
	return (header>>28)&0xf;
}

unsigned int get_packet_seq(unsigned char *packet)
{
	unsigned int header = ntohl(*((unsigned int *)packet));
	return header & 0x0fffffff;
}
