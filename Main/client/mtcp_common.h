#ifndef MTCP_COMMON_INCLUDE
#define MTCP_COMMON_INCLUDE

///////////////////////////////////////////////////////////////////////////////
//
//   File          : mtcp_common.h
//   Description   : This is the interface of the common funcions for the
//                   CSCI 4430 project
//   Author        : LIU Yide, Zhang Sibin, LIU Cheng-Tsung
//   Last Modified : 25/02/2017
//



// Include
#include <stdint.h>


// Useful constant
#define MAX_BUF_SIZE	1024
#define SERVER_PORT	12345

// Type definition
typedef int32_t mTCPHeader;  // Header for the mTCP protocol


// struct for mTCP packet

typedef struct mPacket{
    mTCPHeader header;
    char buffer[1000];
}mTCPPacket;


/*
   mTCP header type specification

   Bits  Segments
   ----- --------------
   0-4   Type variables
   5-32  SEQ/ACK

*/

// Type for the mTCP header
typedef enum{
    mTCP_SYN     = 0,
    mTCP_SYN_ACK = 1,
    mTCP_FIN     = 2,
    mTCP_FIN_ACK = 3,
    mTCP_ACK     = 4,
    mTCP_DATA    = 5,
}mTCPType;

// Functions

// Rack the mTCP header for the packet
mTCPHeader pack_header(int32_t type, int32_t seq);

// Unpack the mTCP header and retrieve information
int32_t unpack_header(mTCPHeader *head, int32_t *type, int32_t *seq);


#endif
