#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<sys/socket.h>
#include<netdb.h>
#include<fcntl.h>
#include<sys/inotify.h>

#define BYTES 1024
#define MAXPENDING 5

char *root;
int listenfd;
int notifyfd;

void start(char *port);
void respond(int n);

int main(int argc, char* argv[])
{
	struct sockaddr_in clientaddr;
	socklen_t addrlen;
	int client;
	char *port;

	if (argc > 1) {
		port = argv[1];
	}
	else {
		port = "8000";
	}

	root = getenv("PWD");
	printf("Starting server on port %s in %s\n", port, root);
	start(port);

	// TODO: handling requests in parallel
	while (1) {
		addrlen = sizeof(clientaddr);
		client = accept(listenfd, (struct sockaddr *) &clientaddr, &addrlen);

		if (client < 0) perror("accept() error");
		else respond(client);
	}
}

void start(char *port)
{
	struct addrinfo hints, *res, *p;

	memset(&hints, 0, sizeof(hints));

	hints.ai_family   = AF_INET;     // ipv4 addresses only
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags    = AI_PASSIVE;  // wildcard ip address

	if (getaddrinfo(NULL, port, &hints, &res) != 0) {
		perror("getaddrinfo() error");
		exit(EXIT_FAILURE);
	}

	for (p = res; p != NULL; p = p->ai_next) {
		listenfd = socket(p->ai_family, p->ai_socktype, 0);
		if (listenfd == -1) continue;
		if (bind(listenfd, p->ai_addr, p->ai_addrlen) == 0) break;
		close(listenfd);
	}

	freeaddrinfo(res);

	if (p==NULL) {
		perror("socket() or bind() error");
		exit(EXIT_FAILURE);
	}

	if (listen(listenfd, MAXPENDING) != 0) {
		perror("listen() error");
		exit(1);
	}

	// if ((notifyfd = inotify_init()) != 0) {
	// 	perror("inotify() error");
	// 	exit(1);
	// }

	// if (inotify_add_watch(notifyfd, root, IN_MODIFY) != 0) {

	// }
}

void respond(int client)
{
	char msg[99999], *reqline[3], data_to_send[BYTES], path[99999];
	int rcvd, fd, bytes_read;

	memset((void*) msg, '\0', 99999);
	rcvd = recv(client, msg, 99999, 0);

	if      (rcvd  < 0) fprintf(stderr, "recv() error\n");
	else if (rcvd == 0) fprintf(stderr, "Client got disconnected\n");
	else {
		reqline[0] = strtok(msg, " \t\n");
		if (strncmp(reqline[0], "GET\0", 4) == 0) {
			reqline[1] = strtok(NULL, " \t");
			reqline[2] = strtok(NULL, " \t\n");

			if (strncmp(reqline[2], "HTTP/1.1", 8) != 0) {
				write(client, "HTTP/1.1 400 Bad Request\n\n", 26);
				goto stop;
			}

			strcpy(path, root);
			strcpy(path + strlen(root), reqline[1]);

			printf("GET: %s\n", path);

			if ((fd = open(path, O_RDONLY)) < 0) {
				write(client, "HTTP/1.1 404 Not Found\n\n", 24);
				goto stop;
			}

			write(client, "HTTP/1.1 202 OK\n\n", 17);
			while ((bytes_read = read(fd, data_to_send, BYTES)) > 0)
				write(client, data_to_send, bytes_read);
		}
		else {
			printf("Not a GET request\n");
		}
	}

stop:
	shutdown(client, SHUT_RDWR);
	close(client);
}
