#include "snake.h"
#include "game_board.h"
#include "server.h"

int snake_set_direction(snake_t *snake, direction_t dir) {
	// NULL pointer + invalid direction value
	if(!snake || dir < 0 || dir > 3) {
		return -1;
	}

	// check that new direction is not directly opposite to the current direction -> ignore + return 0
    // UP - DOWN and LEFT - RIGHT
	if ((snake->direction == DIR_UP && dir == DIR_DOWN) ||
        (snake->direction == DIR_DOWN && dir == DIR_UP) ||
        (snake->direction == DIR_LEFT && dir == DIR_RIGHT) ||
        (snake->direction == DIR_RIGHT && dir == DIR_LEFT)) {
        return 0; 
    }

	// if no issues, set the next direction to dir
    snake->next_direction = dir;

	return 0;
}

int snake_advance(struct game_board *board, int snake_id) {
	// null pointers, invalid inputs
	if(!board || snake_id < 0 || snake_id > MAX_PLAYERS) {
		return -1;
	}

	// get snake and check it is alive
	snake_t *snake = &board->snakes[snake_id];
	if(!snake || snake->alive != 1) {
		return -1;
	}
	
	// (1) copy next_direction into direction + apply the buffered input
	snake->direction = snake->next_direction;

	// (2) compute new head position by moving one cell in the current direction
	position_t head = snake->body[0];
	position_t new_head = head;

	switch(snake->direction) {
		//	DIR_UP: new_y = head_y - 1
		case DIR_UP:
			new_head.y--;
			break;

		// 	DIR_DOWN: new_y = head_y + 1
		case DIR_DOWN:
			new_head.y++;
			break;

		// 	DIR_LEFT: new_x = head_x - 1
		case DIR_LEFT:
			new_head.x--;
			break;

		// 	DIR_RIGHT: new_x = head_x + 1
		case DIR_RIGHT:
			new_head.x++;
			break;
		
		default:
			break;
	}
	
	// (3) check what is at the new head positio
	cell_t new_cell = board->cells[new_head.y * board->size + new_head.x];

	//	a. CELL_WALL or CELL_SNAKE_* (any snake, including itself): 
	//		- snake dies -> board_remove_snake() -> return 1 [death]
	if(new_cell == CELL_WALL || (new_cell >= CELL_SNAKE_0 && new_cell <= CELL_SNAKE_7)) {
		board_remove_snake(board, snake_id);
		return 1;
	}

	//	b. CELL_APPLE: 
	//		- snake grows -> do NOT remove tail -> move body forward (shift ALL segments down by one in body array) + place new head at body[0]
	//		- set cell to snake's cell value, increment length (up to MAX_SNAKE_LENGTH) -> board_place_apple()
	//		- return 2 [apple eaten]
	if(new_cell == CELL_APPLE) {
		// move body forward -> shift all segments down by one in body array forward then place new head at body[0]
        for (int i = snake->length - 1; i > 0; i--) {
            snake->body[i] = snake->body[i - 1];
        }
        snake->body[0] = new_head;

        // set cell to snake's cell value
        board->cells[new_head.y * board->size + new_head.x] = CELL_SNAKE_0 + snake_id;
        
		// increment length of snake
        if (snake->length < MAX_SNAKE_LENGTH) {
            snake->length++;
        }

		// place new apple and return
		board_place_apple(board);
		return 2;
	}

	// 	c. CELL_EMPTY: 
	// 		- normal movement -> clear tail cell to CELL_EMPTY, shift all body segments down by one, place the new head at body[0]
	// 		- set the new head cell to the snake's cell value -> return 0 [normal movement]
	// set tail cell to CELL_EMPTY
	int tail_index = snake->length-1;
	board->cells[snake->body[tail_index].y * board->size + snake->body[tail_index].x] = CELL_EMPTY;

	// shift all body segments down by one
	for(int i = snake->length -1; i > 0; i--) {
		snake->body[i] = snake->body[i - 1];
	}
	snake->body[0] = new_head;

	// set new head cell to snake's cell value
	board->cells[new_head.y * board->size + new_head.x] = CELL_SNAKE_0 + snake_id;

	return 0;
}