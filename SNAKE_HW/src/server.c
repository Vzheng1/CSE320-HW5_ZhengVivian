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

// client handler thread function -> on instance runs per connected client
// need to define a struct to pass server pointer + client fd to this function since pthread_create only takes one void* 
void *server_client_handler(void *arg) {
	// get client handler from arg + check if valid
	client_handler_arg_t *client_handler = (client_handler_arg_t *)arg;
	if(!client_handler) {
		return NULL;
	}

	// get server and client_fd from client handler 
	server_t *server = client_handler->server;
	int client_fd = client_handler->client_fd;
	int snake_id = -1;
	int client_slot = -1;

	// (1) wait for JOIN message from client
	// create buffer to receive message
	uint8_t msg_buf[CLIENT_MSG_SIZE];
    if (recv_exact(client_fd, msg_buf, CLIENT_MSG_SIZE) < 0) {
        goto cleanup;
    }

	// deserialize the client message to get the type and payload
    uint8_t msg_type, msg_payload;
    if (protocol_deserialize_client_msg(msg_buf, CLIENT_MSG_SIZE, &msg_type, &msg_payload) < 0) {
        uint8_t err_msg[2];
        protocol_serialize_error(err_msg, sizeof(err_msg), ERR_INVALID_MSG);
        send_all(client_fd, err_msg, 2);
        goto cleanup;
    }

	// (1a) if first message is NOT JOIN, send ERR_INVALID_MSG 0x03 + close
    if (msg_type != MSG_JOIN) {
        uint8_t err_msg[2];
        protocol_serialize_error(err_msg, sizeof(err_msg), ERR_INVALID_MSG);
        send_all(client_fd, err_msg, 2);
        goto cleanup;
    }

	// (2) add player's snake to game board BUT if board is full, send ERR_GAME_FULL 0x01 + close
	pthread_mutex_lock(&server->board_mutex);
    if (board_add_snake(&server->board, &snake_id) < 0) {
        pthread_mutex_unlock(&server->board_mutex);
        uint8_t err_msg[2];
        protocol_serialize_error(err_msg, sizeof(err_msg), ERR_GAME_FULL);
        send_all(client_fd, err_msg, 2);
        goto cleanup;
    }
    pthread_mutex_unlock(&server->board_mutex);

	// (3) send WELCOME message with assigned player id, board size, max players
	uint8_t welcome_msg[4];
    protocol_serialize_welcome(welcome_msg, sizeof(welcome_msg), snake_id, server->board.size, server->board.max_snakes);
    if (send_all(client_fd, welcome_msg, 4) < 0) {
        goto cleanup;
    }

	// (4) register client fd + snake id in client_fds and client_snake_ids arrays
	for (int i = 0; i < MAX_PLAYERS; i++) {
        if (server->client_fds[i] < 0) {
            client_slot = i;
            server->client_fds[i] = client_fd;
            server->client_snake_ids[i] = snake_id;
            break;
        }
    }

	// (5) enter loop reading 2 byte messages from client socket:
	while(server->running) {
		// 5d. if recv() returns 0 or error -> client disconnect, break out of loop
		if (recv_exact(client_fd, msg_buf, CLIENT_MSG_SIZE) < 0) {
            break;
        }

		// 5c. invalid messages -> send ERR_INVALID_MSG 0x03
		if (protocol_deserialize_client_msg(msg_buf, CLIENT_MSG_SIZE, &msg_type, &msg_payload) < 0) {
            uint8_t err_msg[2];
            protocol_serialize_error(err_msg, sizeof(err_msg), ERR_INVALID_MSG);
            send_all(client_fd, err_msg, 2);
            continue;
        }

		// 5a. DIRECTION -> update players direction
		if (msg_type == MSG_DIRECTION) {
            pthread_mutex_lock(&server->board_mutex);
            snake_set_direction(&server->board.snakes[snake_id], (direction_t)msg_payload);
            pthread_mutex_unlock(&server->board_mutex);

		// 5b. LEAVE -> break loop
        } else if (msg_type == MSG_LEAVE) {
            break;
        }
	}
	
	cleanup:
		// (6) on exit, leave/disconnext -> remove player's snake from game board
		pthread_mutex_lock(&server->board_mutex);
		board_remove_snake(&server->board, snake_id);
		pthread_mutex_unlock(&server->board_mutex);

		// (7) cleanup: clear client_fds and client_snake_ids entries to -1
		if (client_slot >= 0) {
			server->client_fds[client_slot] = -1;
			server->client_snake_ids[client_slot] = -1;
		}

		// (8) cleanup: close client socket + free argument struct
		close(client_fd);
		free(client_handler);

		return NULL;
}

// main server loop
int server_start(server_t *server) {
	// null pointer -> error
	if(!server) {
		return -1;
	}

	// (1) creates game loop thread -> calls pthread_create with server_game_loop + check for error
	pthread_t game_loop_thread;
    if (pthread_create(&game_loop_thread, NULL, server_game_loop, server) < 0) {
        return -1;
    }

	// (2) enters accept() loop -> break loop when server->running becomes 0:
	while(server->running) {
		//	2a. accepts new client connection
		//	2b. allocates new argument struct with server pointer + new client fd
		struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);

		int client_fd = accept(server->listen_fd, (struct sockaddr *)&client_addr,
                               &addr_len);
        if (client_fd < 0) {
            break; 
        }

		//	2c. creates new detatched thread -> call pthread_create with server_client_handler
		client_handler_arg_t *arg = (client_handler_arg_t *)malloc(sizeof(client_handler_arg_t));
        if (!arg) {
            close(client_fd);
            continue;
        }

        arg->server = server;
        arg->client_fd = client_fd;

        pthread_t client_thread;
        if (pthread_create(&client_thread, NULL, server_client_handler, arg) < 0) {
            free(arg);
            close(client_fd);
            continue;
        }

		// 	2d. thread should be created detached (pthread_detach) so resources auto freeed when it exits
		pthread_detach(client_thread);
	}
	
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