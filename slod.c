#include<stdio.h>
#include<stdbool.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<netdb.h>
#include<fcntl.h>
#include<getopt.h>
#include<signal.h>
#include<sys/inotify.h>
#include<sys/stat.h>
#include<dirent.h>

#define BYTES 1024
#define MAXPENDING 5

char *port = "8000";
char *root = NULL;
bool watch = false;

int listenfd;
int notifyfd;

void start(char *port);
void respond(int n);

void handler(int sig)
{
	// \b\b for removing ^C, maybe this is caused to my terminal
	printf("\b\bShutting down server\n");
	shutdown(listenfd, SHUT_RDWR);
	close(listenfd);
	exit(EXIT_SUCCESS);
}

static const char usage[] = "Usage: %s [options] [ROOT]\n"
"  -h, --help       Show this help message and quit.\n"
"  -p, --port PORT  Specify port to listen on.\n"
"  -l, --live       Enable livereload.\n";


static const struct option long_options[] = {
	{"help", no_argument,       0, 'h'},
	{"port", required_argument, 0, 'p'},
	{"live", no_argument,       0, 'l'},
	{0},
};

int main(int argc, char* argv[])
{
	struct sockaddr_in clientaddr;
	socklen_t addrlen;
	int client, option;

	while ((option = getopt_long(argc, argv, "hp:", long_options, NULL)) != -1) {
		switch (option) {
			case 'p': // custom port
				port = optarg;
				break;
			case 'l': // enable livereload
				watch = true;
				break;
			case 'h': // display help
				fprintf(stderr, usage, argv[0]);
				return EXIT_SUCCESS;
			default:
				fprintf(stderr, usage, argv[0]);
				return EXIT_FAILURE;
		}
	}

	if (optind == argc) {
		root = getenv("PWD");
	}
	else {
		root = argv[optind];
	}

	printf("Starting server on port %s in %s\n", port, root);
	start(port);

	signal(SIGINT, handler);

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

	if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
		perror("setsockopt(SO_REUSEADDR) error");

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


void writen(int fd, const char *text)
{
	write(fd, text, strlen(text));
}

void index_dir(int fd, const char *realpath, const char *path)
{
	struct dirent **namelist;
	int n;

	if (path[0] == '/') path++;

	if ((n = scandir(realpath, &namelist, NULL, alphasort)) < 0) {
		fprintf(stderr, "Failed to scan directory %s\n", path);
		return;
	}

	writen(fd, "<!doctype html><html><body><ul>");

	while(n--) {
		writen(fd, "<li><a href=\"");
		writen(fd, path);
		writen(fd, "/");
		writen(fd, namelist[n]->d_name);
		writen(fd, "\">");
		writen(fd, namelist[n]->d_name);
		writen(fd, "</a></li>");
	}

	writen(fd, "</ul></body></html>");

	free(namelist);
}

void respond(int client)
{
	char msg[99999], data_to_send[BYTES], realpath[99999];
	char *method, *path, *protocol;
	int rcvd, fd, bytes_read;
	struct stat path_stat;
	size_t path_len;

	memset((void*) msg, '\0', 99999);
	rcvd = recv(client, msg, 99999, 0);

	if      (rcvd  < 0) fprintf(stderr, "recv() error\n");
	else if (rcvd == 0) fprintf(stderr, "Client got disconnected\n");
	else {
		method = strtok(msg, " \t\n");
		if (strncmp(method, "GET\0", 4) == 0) {
			path     = strtok(NULL, " \t");
			protocol = strtok(NULL, " \t\n");

			if (strncmp(protocol, "HTTP/1.1", 8) != 0) {
				write(client, "HTTP/1.1 400 Bad Request\n\n", 26);
				goto stop;
			}

			strcpy(realpath, root);
			strcpy(realpath + strlen(root), path);

			path_len = strlen(realpath);

			// remove trailing slash
			if (realpath[path_len - 1] == '/') {
				realpath[path_len - 1] = '\0';
			}

			printf("GET %s\n", realpath);

			stat(realpath, &path_stat);

			if (S_ISDIR(path_stat.st_mode) != 0) {
				write(client, "HTTP/1.1 202 OK\n\n", 17);
				index_dir(client, realpath, path);
				goto stop;
			}

			if ((fd = open(realpath, O_RDONLY)) < 0) {
				write(client, "HTTP/1.1 404 Not Found\n\n", 24);
				goto stop;
			}

			write(client, "HTTP/1.1 202 OK\n\n", 17);

			// TODO: inject script in html file
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
