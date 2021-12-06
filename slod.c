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
#define DIGIT(x) '0' <= x && x <= '9'

char *port = "8000";
char *root = NULL;
bool watch = false;
bool hide  = false;

int listenfd;
int notifyfd;

void start(char *port);
void respond(int n);

void handler(int sig)
{
	printf("\b\bShutting down server\n");
	close(listenfd);
	exit(EXIT_SUCCESS);
}

static const char usage[] = "Usage: %s [options] [ROOT]\n"
"  -h, --help       Show this help message and quit\n"
"  -p, --port PORT  Specify port to listen on\n"
"      --no-hidden  Do not show hidden files on directory index\n"
"  -l, --live       Enable livereload\n";


static const struct option long_options[] = {
	{"help",      no_argument,       0, 'h'},
	{"port",      required_argument, 0, 'p'},
	{"live",      no_argument,       0, 'l'},
	{"no-hidden", no_argument,       0, 'n'},
	{0},
};

int main(int argc, char* argv[])
{
	struct sockaddr_in clientaddr;
	socklen_t addrlen;
	int client, option;

	while ((option = getopt_long(argc, argv, "hp:l", long_options, NULL)) != -1) {
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
			case 'n': // hide hidden files
				hide = true;
				break;
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
	    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
	    	perror("setsockopt(SO_REUSEADDR) error");
		if (bind(listenfd, p->ai_addr, p->ai_addrlen) == 0) break;
		close(listenfd);
	}

	freeaddrinfo(res);

	if (p == NULL) {
		perror("socket() or bind() error");
		exit(EXIT_FAILURE);
	}


	if (listen(listenfd, MAXPENDING) != 0) {
		perror("listen() error");
		exit(EXIT_FAILURE);
	}

	// if ((notifyfd = inotify_init()) != 0) {
	// 	perror("inotify() error");
	// 	exit(1);
	// }

	// if (inotify_add_watch(notifyfd, root, IN_MODIFY) != 0) {

	// }
}

int cmpfiles(const struct dirent **a, const struct dirent **b) {
	const char *x = (*a)->d_name, *y = (*b)->d_name;
	size_t k, kx, ky;

	for (k = 0; k < 256; k++) {
		if (x[k] == '\0') return 0;
		if (y[k] == '\0') return 1;

		if (DIGIT(x[k]) && DIGIT(y[k])) {
			kx = ky = k;
			while (DIGIT(x[kx+1])) kx++;
			while (DIGIT(y[ky+1])) ky++;

			if (kx < ky) return 0;
			if (ky < kx) return 1;

			while (k < kx) {
		      if (x[k] < y[k]) return 0;
		      if (x[k] > y[k]) return 1;
			  k++;
			}
		}

		if (x[k] < y[k]) return 0;
		if (x[k] > y[k]) return 1;
	}

	return 0;
};

void index_dir(int fd, const char *realpath, const char *path)
{
	struct dirent **namelist;
	char filename[9999], fullpath[9999];
	struct stat path_stat;
	int n, k;
	bool isDir;
	size_t len;

	if (path[0] == '/') path++;

	if ((n = scandir(realpath, &namelist, NULL, cmpfiles)) < 0) {
		fprintf(stderr, "Failed to scan directory %s\n", path);
		return;
	}

	dprintf(fd, "<!doctype html><html>"
			     "<head><meta charset=\"utf-8\">"
				 "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
				 "<title>%s</title>"
				 "<style>body{font:16px monospace;margin:2em;line-height:1.6}"
				 "ul{padding:0;list-style:none;margin:0}"
				 "li{overflow:hidden;white-space:nowrap;text-overflow:ellipsis}"
				 "a{text-decoration:none}a:hover{background:#eee}"
				 "@media screen and (max-width:500px){body{font-size:14px}}"
				 "@media(prefers-color-scheme:dark){"
				 "body{background:#222;color:#fff}"
				 "a{color:#fff}a:visited{color:#aaa}a:hover{background:#333}"
				 "}</style>"
				 "</head><body><ul>", path);

	for(k = 0; k < n; k++) {
		strcpy(filename, namelist[k]->d_name);

		// never display .
		if (strcmp(filename, ".") == 0) continue;
		// never display .. for root
		else if (strcmp(filename, "..") == 0) { if (path[0] == '\0') continue; }
		// possibly skip hidden file
		else if (hide && filename[0] == '.') continue;

		strcpy(fullpath, realpath);
		strcat(fullpath, filename);

		// if folder, append /
		stat(fullpath, &path_stat);
		if (S_ISDIR(path_stat.st_mode) != 0) strcat(filename, "/");

		dprintf(fd, "<li><a href=\"./%s\">%s</a></li>", filename, filename);
	}

	dprintf(fd, "</ul></body></html>");

	free(namelist);
}

void respond(int client)
{
	char msg[99999], data_to_send[BYTES], realpath[99999], indexpath[99999];
	char *method, *path, *protocol;
	int rcvd, fd, bytes_read;
	struct stat path_stat;
	size_t path_len;

	memset((void*) msg, '\0', 99999);
	rcvd = recv(client, msg, 99999, 0);

	if      (rcvd  < 0) fprintf(stderr, "recv() error\n");
	else if (rcvd == 0) fprintf(stderr, "Client got disconnected\n");
	else {
		method   = strtok(msg, " \t\n");
		path     = strtok(NULL, " \t");
		protocol = strtok(NULL, " \t\n");

		if (strncmp(method, "GET\0", 4) != 0 || strncmp(protocol, "HTTP/1.1", 8) != 0) {
			dprintf(client, "HTTP/1.1 400 Bad Request\n\n");
			goto stop;
		}

		strcpy(realpath, root);
		strcpy(realpath + strlen(root), path);

		path_len = strlen(realpath);

		// remove trailing slash
		// if (realpath[path_len - 1] == '/') {
		// 	realpath[path_len - 1] = '\0';
		// }

		printf("GET %s\n", realpath);

		if (access(realpath, F_OK) < 0) {
			dprintf(client, "HTTP/1.1 404 Not Found\n\n");
			goto stop;
		}

		stat(realpath, &path_stat);

		if (S_ISDIR(path_stat.st_mode) != 0) {
			// if the path does not end with a slash, redirect
			if (path[strlen(path) - 1] != '/') {
				dprintf(client, "HTTP/1.1 301 Moved Permanently\nLocation: %s/\n\n", path);
				goto stop;
			}

			strcpy(indexpath, realpath);
			strcpy(indexpath + strlen(realpath), "/index.html");

			if (access(indexpath, R_OK) < 0) {
				// problem: we write many times, if the client stops listening it's not good
				dprintf(client, "HTTP/1.1 202 OK\n\n");
				index_dir(client, realpath, path);
				goto stop;
			}

			strcpy(realpath, indexpath);
		}


		if ((fd = open(realpath, O_RDONLY)) < 0)
			goto stop;

		dprintf(client, "HTTP/1.1 202 OK\nCache-Control: no-store\n\n");

		// TODO: inject script in html file
		while ((bytes_read = read(fd, data_to_send, BYTES)) > 0)
			write(client, data_to_send, bytes_read);
	}

stop:
	shutdown(client, SHUT_RDWR);
	close(client);
}
