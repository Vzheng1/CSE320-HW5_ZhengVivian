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
	if(board_place_apple() < 0) {
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
	(void)board;
	return 0;
}

int board_place_apple(game_board_t *board) {
	(void)board;
	return 0;
}

int board_add_snake(game_board_t *board, int *out_id) {
	(void)board;
	(void)out_id;
	return 0;
}

int board_remove_snake(game_board_t *board, int snake_id) {
	(void)board;
	(void)snake_id;
	return 0;
}

int board_tick(game_board_t *board) {
	(void)board;
	return 0;
}
