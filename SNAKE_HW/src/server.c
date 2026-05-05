#include "server.h"
#include "debug.h"
#include "global.h"

int server_init(server_t *server, int port, int board_size, int max_snakes, unsigned int seed) {
	// null pointer
	if(!server) {
		return -1;
	}

	// create listening socket + bind to specific port + call listen()
	// create listening socket
	server->listen_fd = socket(AF_INET, SOCKET_STREAM, 0);
	if(server->listen_fd != 0) {
		pthread_mutex_destroy(&server->board_mutex);
        board_free(&server->board);
        return -1;
	}

	// binds to specific port using SO_REUSEADDR
    int reuse = 1;
    if (setsockopt(server->listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        close(server->listen_fd);
        pthread_mutex_destroy(&server->board_mutex);
        board_free(&server->board);
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((uint16_t)port);

    if (bind(server->listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(server->listen_fd);
        pthread_mutex_destroy(&server->board_mutex);
        board_free(&server->board);
        return -1;
    }

    // call listen() on listen_fd
    if (listen(server->listen_fd, 8) < 0) {
        close(server->listen_fd);
        pthread_mutex_destroy(&server->board_mutex);
        board_free(&server->board);
        return -1;
    }

	// initialize board
	if(board_init(&server->board, board_size, max_players, seed) != 0) {
		return -1;
	}

	// initialize board mutex
	if(pthread_mutex_init(&server->board_mutex, NULL) != 0) {
		board_free(&server->board);
		return -1;
	}

	// initialize client_fds and client_snake_fds
	for(int i=0; i<MAX_PLAYERS; i++) {
		server->client_fds[i] = -1;
		server->client_snake_fds[i] = -1;
	}

	// initialize server state
	server->running = 1;

	return 0;
}

void *server_game_loop(void *arg) {
	(void)arg;
	return NULL;
}

void *server_client_handler(void *arg) {
	(void)arg;
	return NULL;
}

int server_start(server_t *server) {
	(void)server;
	return 0;
}

void server_cleanup(server_t *server) { 
    (void)server;
}

int recv_exact(int fd, uint8_t *buf, size_t len) {
	(void)fd;
	(void)buf;
	(void)len;
	return 0;
}

int send_all(int fd, const uint8_t *buf, size_t len) {
	(void)fd;
	(void)buf;
	(void)len;
	return 0;
}