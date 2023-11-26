#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "server.h"

int not_number(char *arg)
{
	size_t len = strlen(arg);
	for (size_t i = 0; i < len; ++i) {
		if (!isdigit(arg[i]))
			return -1;
	}
	return 0;
}

int check_args(int argc, char *argv[], input_t *in_data)
{	
	if (argc != EXP_ARG_NUM)
		return -EARGS;

	if (strlen(argv[1]) > 5 || strlen(argv[2]) > 5 || strlen(argv[3]) > 5)
		return -EARGS;

	for (int i = 1; i < 4; ++i) {
		if (not_number(argv[i]))
			return -EARGS;
	}

	unsigned long port = strtoul(argv[1], NULL, 0);
	unsigned long timeout = strtoul(argv[2], NULL, 0);
	unsigned long max_resends = strtoul(argv[3], NULL, 0);

	if (port > MAX_US || timeout > MAX_US || max_resends > MAX_US)
		return -EARGS;

	in_data->port = (unsigned short) port;
	in_data->timeout = (unsigned short) timeout;
	in_data->max_resends = (unsigned short) max_resends;
	
	return 0;
}

void print_error(int retval)
{
	if (retval == -EARGS)
		fprintf(stderr, "TTFTP_ERROR: illegal arguments\n");

	else
		perror("TTFTP_ERROR");
}

void set_error(int errconst, void *error)
{
	switch (errconst) {
	case EUU:
		{
			err_UU *e = (err_UU *) error;
			e->opcode = htons(5);
			e->errcode = htons(7);
			strncpy(e->msg, "Unknown user", 12);
			e->term = 0;
			break;
		}
	case EFAE:
		{
			err_FAE *e = (err_FAE *) error;
			e->opcode = htons(5);
			e->errcode = htons(6);
			strncpy(e->msg, "File already exists", 19);
			e->term = 0;
			break;
		}
	case EUP:
		{
			err_UP *e = (err_UP *) error;
			e->opcode = htons(5);
			e->errcode = htons(4);
			strncpy(e->msg, "Unexpected Packet", 17);
			e->term = 0;		
			break;
		}
	case EBBN:
		{
			err_BBN *e = (err_BBN *) error;
			e->opcode = htons(5);
			e->errcode = htons(0);
			strncpy(e->msg, "Bad block number", 16);
			e->term = 0;		
			break;
		}
	case EAFT:
		{
			err_AFT *e = (err_AFT *) error;
			e->opcode = htons(5);
			e->errcode = htons(0);
			strncpy(e->msg, "Abandoning file transmission", 28);
			e->term = 0;
			break;
		}
	default:
		;
	}
}

// Create and send an ERROR packet
// Failes if 'sendto()' fails or if bad 'errconst' was passed
int send_error(int errconst, int sfd, struct sockaddr_in *pcl_addr)
{
	int retval;

	switch(errconst) {
	case EUU:
		{
			err_UU e;
			set_error(EUU, &e);
			retval = sendto(sfd, (void *) &e, sizeof(e), 0,
							(struct sockaddr *) pcl_addr, sizeof(*pcl_addr));
			if (retval != sizeof(e))
				retval = -ESYS;
			break;
		}
	case EFAE:
		{
			err_FAE e;
			set_error(EFAE, &e);
			retval = sendto(sfd, (void *) &e, sizeof(e), 0,
							(struct sockaddr *) pcl_addr, sizeof(*pcl_addr));
			if (retval != sizeof(e))
				retval = -ESYS;
			break;
		}
	case EUP:
		{
			err_UP e;
			set_error(EUP, &e);
			retval = sendto(sfd, (void *) &e, sizeof(e), 0,
							(struct sockaddr *) pcl_addr, sizeof(*pcl_addr));
			if (retval != sizeof(e))
				retval = -ESYS;
			break;
		}
	case EBBN:
		{
			err_BBN e;
			set_error(EBBN, &e);
			retval = sendto(sfd, (void *) &e, sizeof(e), 0,
							(struct sockaddr *) pcl_addr, sizeof(*pcl_addr));
			if (retval != sizeof(e))
				retval = -ESYS;
			break;
		}
	case EAFT:
		{
			err_AFT e;
			set_error(EAFT, &e);
			retval = sendto(sfd, (void *) &e, sizeof(e), 0,
							(struct sockaddr *) pcl_addr, sizeof(*pcl_addr));
			if (retval != sizeof(e))
				retval = -ESYS;
			break;
		}
	default:
		return -1;
	}

	return retval;
}

// Compare client addresses
int checkClientAddress(struct sockaddr_in *receivedAddr, struct sockaddr_in *expectedAddr)
{
	// Compare IP addresses
	if (strcmp(inet_ntoa(receivedAddr->sin_addr), inet_ntoa(expectedAddr->sin_addr)) != 0) {
		return 0;
	}

	// Compare port numbers
	if (receivedAddr->sin_port != expectedAddr->sin_port) {
		return 0;
	}

	return 1;
}

void reset_timeout(struct timeval *pto, unsigned short timeout_val)
{
	pto->tv_sec = timeout_val;
	pto->tv_usec = 0;
}

// Send an ACK packet
// Fails if 'sendto()' fails
int send_ack(unsigned short blockno, int sfd, struct sockaddr_in *pcl_addr)
{
	int retval;
	ack_t ack = {htons(OP_ACK), htons(blockno)};

	retval = sendto(sfd, (void *) &ack, sizeof(ack_t), 0,
					(struct sockaddr *) pcl_addr, sizeof(*pcl_addr));
	
	if (retval != sizeof(ack_t))
		return -ESYS;

	return retval;
}

int data_packet(data_t *pkt)
{
	unsigned short opcode = ntohs(pkt->opcode);
	return opcode == OP_DATA;
}

int buf_wrq(char *buf)
{
	unsigned short opcode = buf[0] + (buf[1] << 8);

	opcode = ntohs(opcode);
	return (opcode == OP_WRQ);
}

int extract_blockno(data_t *data)
{
	return ntohs(data->blockno);
}


// Close file and delete it
void abandon_tx(int fd, char *file)
{
	if (close(fd) < 0) {
		print_error(ESYS);
		exit(EXIT_FAILURE);
	}
	if (unlink(file) < 0) {
		print_error(ESYS);
		exit(EXIT_FAILURE);
	}
}

// Close and delete file and also terminate the process
void exit_abandon_tx(int fd, char *file)
{
	print_error(ESYS);
	close(fd);
	unlink(file);
	exit(EXIT_FAILURE);
}

void exit_no_abandon(void)
{
	print_error(ESYS);
	exit(EXIT_FAILURE);
}

// Receive data packet from client and write it to pdata
// Return < 0 if an error eccured, otherwise return the number of bytes written to pdata
int receive_data(int sfd, struct sockaddr_in *pcl_addr, unsigned short last_recv_block, data_t *pdata)
{
	int retval;
	unsigned short new_blockno;
	struct sockaddr_in temp_addr = { 0 };
	socklen_t temp_len = sizeof(temp_addr);

	retval = recvfrom(sfd, (void *) pdata, sizeof(data_t), 0,
						(struct sockaddr *) &temp_addr, &temp_len);

	if (retval < 0)
		return -ESYS;

	else if (retval == 0)
		return 0;

	// Check if packet origin is currently serviced client
	// and if it is not a WRQ packet
	else if (!checkClientAddress(&temp_addr, pcl_addr)) {
		if (send_error(EUP, sfd, &temp_addr) < 0)
			return -ESYS;
		return CONTINUE;		
	}
	else if (!data_packet(pdata)) {
		if (send_error(EUP, sfd, &temp_addr) < 0)
			return -ESYS;
		return -EUP;		
	}

	// Here we know:
	// 1. packet was received from the correct client
	// 2. packet is a DATA packet
	// 3. retval = number of bytes copied to 'data'
	//     => INCLUDING header!!!

	// Check if the block number is correct
	new_blockno = extract_blockno(pdata);
	if (new_blockno != last_recv_block + 1) {
		if (send_error(EBBN, sfd, &temp_addr) < 0)
			return -ESYS;
		return -EBBN;
	}
	
	return retval;		
}

int service(int sfd, struct sockaddr_in *pcl_addr, input_t *in_data, char *file)
{
	int fd, retval = 0, resend_counter = 0;
	unsigned short last_recv_block = 0;
	data_t data = { 0 };
	fd_set read_fd_set;
	struct timeval timeout = {in_data->timeout, 0};

	// Open requested file
	fd = open(file, O_CREAT | O_EXCL | O_RDWR, 0777);

	// Check if file already exists
	if (fd < 0) {
		if (errno == EEXIST) {
			if (send_error(EFAE, sfd, pcl_addr) < 0) {
				exit_no_abandon();
			}
			return -EFAE;
		}
		exit_no_abandon();
	}

	// Send WRQ ack
	if (send_ack(last_recv_block, sfd, pcl_addr) < 0)
		exit_abandon_tx(fd, file);
	
	while (1) {
		FD_ZERO(&read_fd_set);
		FD_SET(sfd, &read_fd_set);

		retval = select(sfd + 1, &read_fd_set, NULL, NULL, &timeout);

		// Error in 'select()'
		if (retval < 0)
			exit_abandon_tx(fd, file);

		// Timeout expired - resend ACK
		if (retval == 0) {
			resend_counter++;
			if (resend_counter > in_data->max_resends) {
				retval = -EAFT;
				if (send_error(EAFT, sfd, pcl_addr) < 0)
					retval = -ESYS;				
				break;
			}
			if (send_ack(last_recv_block, sfd, pcl_addr) < 0) {
				retval = -ESYS;
				break;
			}			
		}
		else { // Received data before timeout : retval > 0
			resend_counter = 0;
			retval = receive_data(sfd, pcl_addr, last_recv_block, &data);
			
			// Data packet is invalid
			if (retval <= 0)
				break;
			
			// Continue if packet wasn't from currently serviced client
			if (retval == CONTINUE)
				continue;

			// Increment block number and send ACK
			if (send_ack(++last_recv_block, sfd, pcl_addr) < 0) {
				retval = -ESYS;
				break;
			}

			// Write to file
			if (write(fd, data.data, retval - 4) < 0) {
				retval = -ESYS;
				break;
			}
			
			// If this is the last packet, close the file
			if (retval < MAX_PKT_SIZE) {
				close(fd);
				break;
			}
		}
		// If DATA packet is valid or timeout has expired
		reset_timeout(&timeout, in_data->timeout);
	} // while (1)

	if (retval == -ESYS)
		exit_abandon_tx(fd, file);
	else if (retval < 0)
		abandon_tx(fd, file);

	return retval;
}
