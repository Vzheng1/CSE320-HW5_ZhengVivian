#include "server.h"
#include "debug.h"
#include "global.h"
#include "protocol.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <time.h>

int server_init(server_t *server, int port, int board_size, int max_snakes, unsigned int seed) {
	// null pointer
	if(!server) {
		return -1;
	}

	// create listening socket + bind to specific port + call listen()
	// create listening socket
	server->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
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
	if(board_init(&server->board, board_size, max_snakes, seed) != 0) {
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
		server->client_snake_ids[i] = -1;
	}

	// initialize server state
	server->running = 1;

	return 0;
}

void *server_game_loop(void *arg) {
	// game loop thread function -> runs its own thread + responsible for advancing game state + broadcast updates
	// arg = pointer to server_t
	server_t *server = (server_t *)arg;
	if(!server) {
		return NULL;
	}

	// loop -> function exit when server->running is set to 0 :
	while(server->running) {
		// (1) sleep for TICK_INTERVAL_MS ms (default 200ms) -> use usleep(TICK_INTERVAL_MS * 1000)
		usleep(TICK_INTERVAL_MS);
		// lock mutex since on its own thread
		pthread_mutex_lock(&server->board_mutex);

		// (2a) track snakes alive before tick
		int alive_before_tick[MAX_PLAYERS];
		for(int i = 0; i< MAX_PLAYERS; i++) {
			alive_before_tick[i] = server->board.snakes[i].alive;
		}
		
		// (2b) call board_tick() to advance ALL snakes
		board_tick(&server->board);

		// (3) check for dead snakes (just died) after tick -> send PLAYER_DEAD message to snakes client
		for(int i=0; i<MAX_PLAYERS; i++) {
			// if snack has just died, send PLAYER_DEAD message
			if(alive_before_tick[i] && !server->board.snakes[i].alive) {
				// iterate through snake ids and client fds to send message
				for(int j=0; j<MAX_PLAYERS; j++) {
					//
					if(server->client_snake_ids[j] == i) {
						uint8_t dead_msg[2];
                        protocol_serialize_dead(dead_msg, sizeof(dead_msg), i);
                        send_all(server->client_fds[j], dead_msg, 2);
                        break;
					}
				}
			}
		}

		// (4) serialize game state with protocol_serialize_game_state()
		uint8_t game_state_buf[GAME_STATE_BUF_SIZE];
		int game_state_len = protocol_serialize_game_state(game_state_buf, sizeof(game_state_buf), &server->board);

		// (5) broadcast serialzied game state to ALL connected clients -> iterate through client_fds + call send() for each valid fd
		if(game_state_len > 0) {
			for(int i=0; i<MAX_PLAYERS; i++) {
				if(server->client_fds[i] >= 0) {
					send_all(server->client_fds[i], game_state_buf, (size_t)game_state_len);
				}
			}
		}

		// unlock mutex once loop is done
        pthread_mutex_unlock(&server->board_mutex);
	}

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
    // null pointer
	if(!server) {
		return;
	}

	// shut down server -> set to 0
	server->running = 0;

	// close client sockets
	for(int i=0; i<MAX_PLAYERS; i++) {
		if(server->client_fds[i] >= 0) {
			close(server->client_fds[i]);
			server->client_fds[i] = -1;
		}
	}

	// close listening socket
	if(server->listen_fd >= 0) {
		close(server->listen_fd);
		server->listen_fd = -1;
	}

	// free board
	board_free(&server->board);

	// destory the mutex
	pthread_mutex_destroy(&server->board_mutex);
}

int recv_exact(int fd, uint8_t *buf, size_t len) {
	// invalid input, null pointers
	if(fd < 0 || !buf) {
		return -1;
	}
	// read exactly len bytes from fd into buffer
	size_t received = 0;
    while (received < len) {
        ssize_t n = recv(fd, &buf[received], len - received, 0);
        
		// n = -1 -> error -> return -1
		if (n < 0) {
            return -1;  // Error
        }
		// n = 0 -> disconnect -> return -1
        if (n == 0) {
            return -1;  
        }

		// increment by number of bytes read
        received += (size_t)n;
    }
	
    return 0;
}

int send_all(int fd, const uint8_t *buf, size_t len) {
	// invalid input, null pointer 
	if(fd < 0 || !buf) {
		return -1;
	}

	// send exactly len bytes from buffer to fd
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(fd, &buf[sent], len - sent, 0);
        
		// n = -1 -> error -> return -1
		if (n < 0) {
            return -1;  
        }

		// increment by number of bytes sent
        sent += (size_t)n;
    }
	return 0;
}