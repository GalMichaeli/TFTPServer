#include "server.h"

int main(int argc, char *argv[])
{
	// Input error check
	int retval;
	input_t in_data = { 0 };
	if ((retval = check_args(argc, argv, &in_data)) < 0)
		exit_no_abandon();

	// Server setup
	struct sockaddr_in srv_addr = { 0 };
	int sfd;

	if ((sfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		exit_no_abandon();

	srv_addr.sin_family = AF_INET;
	srv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	srv_addr.sin_port = htons(in_data.port);

	if (bind(sfd, (struct sockaddr *) &srv_addr, sizeof(srv_addr)) < 0)
		exit_no_abandon();

	// Select setup
	fd_set read_fd_set;

	// Client details (I think it has to be in a different file)
	struct sockaddr_in cl_addr = { 0 };
	socklen_t cl_len = sizeof(cl_addr);
	
	char buf[MAX_PKT_SIZE] = { 0 };
	// Main loop
	for (;;) {
		int retval;
		FD_ZERO(&read_fd_set);
		FD_SET(sfd, &read_fd_set);
		// wait on select for WRQ
		retval = select(sfd + 1, &read_fd_set, NULL, NULL, NULL);
		// printf("returned from select\n");
		if (retval < 0)
			exit_no_abandon();


		// check if message is WRQ
		retval = recvfrom(sfd, (void *) buf, MAX_PKT_SIZE, 0,
				  (struct sockaddr *) &cl_addr, &cl_len);
		// printf("returned from recvfrom\n");
		if (retval < 0) {
			// printf("error: recvfrom failed\n");
			exit_no_abandon();
		} 
		else if (retval == 0) {
			// printf("Client has shut down\n");
			continue;
		}
		else if (buf_wrq(buf)) {
			// printf("calling service\n");
			if (service(sfd, &cl_addr, &in_data, (char *) &buf[2]) < 0) 
				continue;	
		}
		else {
			// printf("unknown user\n");
			if (send_error(EUU, sfd, &cl_addr) < 0) {
				exit_no_abandon();
			}
			continue;
		}
	}

	close(sfd);
	return 0;
}
