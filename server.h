#ifndef SERVER_H
#define SERVER_H

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// General constants
#define EXP_ARG_NUM 4
#define MAX_US 65535
#define MAX_PKT_SIZE 516

// Error codes
#define EARGS 1
#define ESYS 2
#define EUU  11
#define EFAE 12
#define EUP  13
#define EBBN 14
#define EAFT 15

// Packet opcodes
#define OP_WRQ 2
#define OP_DATA 3
#define OP_ACK 4

// Sleep time for debugging
#define SLEEP_TIME 1

// Flag for when we received a packet from wrong user
// and we should continue waiting for packets from the current user
#define CONTINUE 1

// Struct to aggregate program input parameters
typedef struct {
	unsigned short port;
	unsigned short timeout;
	unsigned short max_resends;
} input_t;

// Struct representing a DATA packet
struct data {
	unsigned short opcode;
	unsigned short blockno;
	char data[512];
} __attribute__((packed));
typedef struct data data_t;

// Struct representing an ACK packet
struct ack {
	unsigned short opcode;
	unsigned short blockno;
} __attribute__((packed));
typedef struct ack ack_t;

// Errors
struct err_UU {
	unsigned short opcode;
	unsigned short errcode;
	char msg[12];
	char term;	
} __attribute__((packed));
typedef struct err_UU err_UU;

struct err_FAE {
	unsigned short opcode;
	unsigned short errcode;
	char msg[19];
	char term;	
} __attribute__((packed));
typedef struct err_FAE err_FAE;

struct err_UP {
	unsigned short opcode;
	unsigned short errcode;
	char msg[17];
	char term;	
} __attribute__((packed));
typedef struct err_UP err_UP;

struct err_BBN {
	unsigned short opcode;
	unsigned short errcode;
	char msg[16];
	char term;	
} __attribute__((packed));
typedef struct err_BBN err_BBN;

struct err_AFT {
	unsigned short opcode;
	unsigned short errcode;
	char msg[28];
	char term;	
} __attribute__((packed));
typedef struct err_AFT err_AFT;

// Function signatures
int check_args(int, char **, input_t *);
int send_error(int, int, struct sockaddr_in *);
int buf_wrq(char *);
void exit_no_abandon(void);
int service(int, struct sockaddr_in *, input_t *, char *);

#endif // SERVER_H