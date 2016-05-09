/* YOUR FILE-HEADER COMMENT HERE */

#define _POSIX_SOURCE

#include <errno.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#define BABBLENET_PORT 4210

static int babblenet_socket = -1;

/**
 * Initializes the network connection to the BabbleNet server.
 *
 * If unable to connect to the server, then display an error message
 * and abort the program.
 *
 * You do not need to modify this function.
 */
static void init_babblenet_connection(void)
{
	struct addrinfo hints, *result, *p;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	int ret = getaddrinfo("localhost", "4210", &hints, &result);
	if (ret) {
		fprintf(stderr, "Could not resolve localhost: %s\n", gai_strerror(ret));
		exit(EXIT_FAILURE);
	}

	for (p = result; p; p = p->ai_next) {
		babblenet_socket = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (babblenet_socket < 0) {
			continue;
		}
		if (connect(babblenet_socket, p->ai_addr, p->ai_addrlen) >= 0) {
			break;
		}
		close(babblenet_socket);
		babblenet_socket = -1;
	}

	freeaddrinfo(result);
	if (babblenet_socket < 0) {
		fprintf(stderr, "Could not connect to BabbleNet\n");
		exit(EXIT_FAILURE);
	}
}

/**
 * Send a message to the BabbleNet server.
 *
 * You do not need to modify this function.
 *
 * @param babble buffer containing babble to send
 * @param buffer_len number of bytes from @babble to send
 *
 * @return true if babble was successfully sent, false if not
 */
static bool babblenet_send(const void *babble, size_t buffer_len)
{
	if (babblenet_socket < 0) {
		fprintf(stderr, "Not connected to BabbleNet\n");
		return false;
	}

	ssize_t retval;
	do {
		retval = write(babblenet_socket, babble, buffer_len);
	} while (retval < 0 && errno == EINTR);

	if (retval < 0) {
		perror("failed to send babble");
		return false;
	}
	else if (retval != buffer_len) {
		fprintf(stderr, "unable to send %zu bytes (only sent %zu)\n", buffer_len, retval);
		return false;
	}
	return true;
}

int main(void)
{
	init_babblenet_connection();

	/* YOUR CODE HERE */

	if (babblenet_socket >= 0) {
		close(babblenet_socket);
	}
	return 0;
}
