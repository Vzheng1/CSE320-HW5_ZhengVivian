#include "game_board.h"
#include "server.h"
#include "snake.h"
#include "protocol.h"
#include "global.h"
#include "debug.h"

#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>

int main(int argc, char *argv[]) {
	// intialize necessary input arguments
	int port = -1;
	int board_size = BOARD_SIZE_DEFAULT;
	int max_players = MAX_PLAYERS_DEFAULT;
	unsigned int seed = (unsigned int)time(NULL);

	// process arguments using getopt
	int opt;
	while((opt = getopt(argc, argv, "p:b:s:m:h")) != -1) {
		switch(opt) {
			// get port number
			case 'p':
				port = atoi(optarg);

				// check that port number is valid
				if (port <= 0 || port > 65535) {
					ERR_INVALID_PORT(optarg);
					PRINT_USAGE();
					return EXIT_FAILURE;
				}
				break;

			// get board size
			case 'b':
				board_size = atoi(optarg);
				break;

			// get seed
			case 's':
				seed = atoi(optarg);
				break;

			// get max players
			case 'm':
				max_players = atoi(optarg);
				break;

			// print help message
			case 'h':
				PRINT_USAGE();
				return EXIT_SUCCESS;

			// invalid input
			default:
				PRINT_USAGE();
				return EXIT_FAILURE;
		}
	}

	// check that port number is given/saved since it is required
	if(port < 0) {
		ERR_PORT_REQUIRED();
		PRINT_USAGE();
		return EXIT_FAILURE;
	}

	// validate board size is correct: min=10, max=50
	if(board_size < BOARD_SIZE_MIN || board_size > BOARD_SIZE_MAX) {
		ERR_INVALID_BOARD_SIZE(board_size);
		PRINT_USAGE();
		return EXIT_FAILURE;
	}

	// validate max_player is correct: min=1, max=8
	if(max_players < MAX_PLAYERS_MIN || max_players > MAX_PLAYERS_MAX) {
		ERR_INVALID_MAX_PLAYERS(max_players);
		PRINT_USAGE();
		return EXIT_FAILURE;
	}

	// start server with given input -> check for bind error
	server_t server;
	memset(&server, 0, sizeof(server_t));
    if (server_init(&server, port, board_size, max_players, seed) < 0) {
        ERR_BIND_FAILED(port);
        return EXIT_FAILURE;
    }

	// if server bind successful, print success message
	PRINT_SERVER_STARTED(port, board_size, max_players);
	int result = server_start(&server);

	server_cleanup(&server);
	return (result == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}