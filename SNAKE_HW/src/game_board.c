#include "game_board.h"
#include "debug.h"
#include "global.h"

int board_init(game_board_t *board, int size, int max_snakes, unsigned int seed) {
	// check for invalid inputs -> return -1 if invalid
	if(!board || size < 10 || size > 50 || max_snakes < 1 || max_snakes > 8) {
		return -1;
	}

	// initalize with inputs and set num_snakes to 0 since game did not start
	board->size = size;
	board->num_snakes = 0;
	board->max_snakes = max_snakes;
	board->rng_state = seed;

	// allocate cells array + check for error then set all cells to CELL_EMPTY
	board->cells = (cell_t *)malloc(size*size*sizeof(cell_t));
	if(!board->cells) {
		return -1;
	}

	for(int i=0; i<size*size; i++){
		board->cells[i] = CELL_EMPTY;
	}

	// initialize all snakes to deafult state -> set their id, alive, length status
	for (int i = 0; i < MAX_PLAYERS; i++) {
        board->snakes[i].id = i;
        board->snakes[i].alive = 0;
        board->snakes[i].length = 0;
    }

	// place walls around boarder of the board -> cells where x=0, x=size-1, y=0, y=size-1
	// position at (x,y) is board->cells[y * board->size + x]
	for (int x = 0; x < size; x++) {
        for (int y = 0; y < size; y++) {
            if (x == 0 || x == size - 1 || y == 0 || y == size - 1) {
                board->cells[y * size + x] = CELL_WALL;
            }
        }
    }

	// place first apple using board_place_apple() -> check for error
	if(board_place_apple(board) < 0) {
		board_free(board);
		return -1;
	}
	
	return 0;
}

void board_free(game_board_t *board) { 
	// free cells array and sets pointer to NULL
	if(board && board->cells) {
		free(board->cells);
		board->cells = NULL;
	}
}

unsigned int board_random(game_board_t *board) {
	board->rng_state = board->rng_state * 1103515245 + 12345;
    return (board->rng_state / 65536) % 32768;
}

int board_place_apple(game_board_t *board) {
	if(!board || !board->cells) {
		return -1;
	}
	// count total number of CELL_EMPTY cells on board -> return -1 if no empty cells
	int empty_count = 0;
	for(int i = 0; i < board->size * board->size; i++) {
		if(board->cells[i] == CELL_EMPTY) {
			empty_count++;
		}
	}
	if(empty_count == 0) {
		return -1;
	}

	// get random number from board_random(board) + compute target index = random_value % empty_count
	unsigned int random_number = board_random(board);
	int target_index = random_number % empty_count;

	// iterate through cells array in order (row by row, left to right) -> count until you reach target index
	// place CELL_APPLE at that cell + store position in board->apple (for both x,y)
	int count = 0;
	for(int y=0; y<board->size; y++) {
		for(int x=0; x<board->size; x++) {
			if(board->cells[y*board->size+x] == CELL_EMPTY) {
				if(count == target_index) {
					board->cells[y*board->size+x] = CELL_APPLE;
					board->apple.x = x;
					board->apple.y = y;
					return 0;
				}
				count++;
			}
		}
	}

	return -1;
}

int board_add_snake(game_board_t *board, int *out_id) {
	// null pointers, return -1
	if(!board || !out_id) {
		return -1;
	}

	// if no empty cells, return -1
	int empty_count = 0;
	for(int i = 0; i < board->size * board->size; i++) {
		if(board->cells[i] == CELL_EMPTY) {
			empty_count++;
		}
	}
	if(empty_count == 0) {
		return -1;
	}

	// (1) find first snake slot where alive==0 -> if none available, return -1
	int snake_id = -1;
	for(int i = 0; i< board->max_snakes; i++) {
		if(board->snakes[i].alive == 0) {
			snake_id = i;
			break;
		}
	}
	if(snake_id == -1) {
		return -1;
	}

	// (2) compute starting position based on snake id -> starting positive cycle through 4 quadarants of board interior
	int x, y = 0;
	int quad = snake_id % 4;

	// (2a) ID % 4 == 0: top-left quadrant at (size/4, size/4)
	// (2b) ID % 4 == 1: top-right quadrant at (3*size/4, size/4)
	// (2c) ID % 4 == 2: bottom-left quadrant at (size/4, 3*size/4)
	// (2d) ID % 4 == 3: bottom-right quadrant at (3*size/4, 3*size/4)
	if(quad == 0) {
		x = board->size/4;
		y = board->size/4;
	} else if (quad == 1) {
		x = 3*board->size/4;
		y = board->size/4;
	} else if (quad == 2) {
		x = board->size/4;
		y = 3*board->size/4;
	} else if (quad == 3) {
		x = 3*board->size/4;
		y = 3*board->size/4;
	}

	// (3) if computed starting cell is NOT CELL_EMPTY, scan rightward then downward (within interior) until empty cell is found
	// if no empty cell is found, return -1
	// Scan for empty cell in interior (skip walls)
    int start_x = x, start_y = y;
    int found = 0;
    while (1) {
		// if starting cell is CELL_EMPTY, end the loop -> is found
        if (x > 0 && x < board->size - 1 && y > 0 && y < board->size - 1) {
            if (board->cells[y*board->size+x] == CELL_EMPTY) {
                found = 1;
                break;
            }
        }
        // if not CELL_EMPTY, scan rightward then downward
        x++;
        if (x >= board->size-1) {
            x = 1;
            y++;
            if (y >= board->size-1) {
                y = 1;
            }
        }

        // check if we have looped back around -> if yes, stop loop
        if (x == start_x && y == start_y) {
            break;
        }
    }
	// if no empty cell is found, return -1
    if (!found) {
        return -1;
    }

	// (4) initialize snake -> set od, body[0] to start position, length = 1, direction = DIR_RIGHT, next_direction = DIR_RIGHT, alive = 1
	board->snakes[snake_id].id = snake_id;
    board->snakes[snake_id].body[0].x = x;
    board->snakes[snake_id].body[0].y = y;
    board->snakes[snake_id].length = 1;
    board->snakes[snake_id].direction = DIR_RIGHT;
    board->snakes[snake_id].next_direction = DIR_RIGHT;
    board->snakes[snake_id].alive = 1;

	// (5) set cell at starting position to CELL_SNAKE_O + id
    board->cells[y * board->size + x] = CELL_SNAKE_0 + snake_id;

	// (6) store asisgned ID in *out_id + increment board->num_snakes
	*out_id = snake_id;
    board->num_snakes++;

	return 0;
}

int board_remove_snake(game_board_t *board, int snake_id) {
	// invalid inputs -> return -1
	if(!board || snake_id < 0 || snake_id > MAX_PLAYERS) {
		return -1;
	}

	// get snake and check if the snake is alive -> if not, return -1
	snake_t *snake = &board->snakes[snake_id];
	if(!snake || snake->alive != 1) {
		return -1;
	}

    // set all cells occupied by snake to CELL_EMPTY
    for (int i = 0; i < snake->length; i++) {
        int x = snake->body[i].x;
        int y = snake->body[i].y;
        board->cells[y * board->size + x] = CELL_EMPTY;
    }

    // sets alive = 0, length = 0, and decrements board->num_snakes
    snake->alive = 0;
    snake->length = 0;
    board->num_snakes--;

	return 0;
}

int board_tick(game_board_t *board) {
	// null pointer
	if(!board) {
		return -1;
	}

	// iterates through all snakes in order of ID (0 through MAX_PLAYERS - 1) + for each alive snake, calls snake_advance()
	for(int i=0; i<MAX_PLAYERS; i++) {
		if(board->snakes[i].alive == 1) {
			snake_advance(board, i);
		}
	}
	
	return 0;
}
