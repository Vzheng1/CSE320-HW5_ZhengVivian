#include <criterion/criterion.h>
#include <criterion/logging.h>

#include "game_board.h"
#include "global.h"
#include "snake.h"

/* ---------------------------------------------------------------------------
 * Helper: set up a board with one snake at a known position
 * ---------------------------------------------------------------------------
 */
static game_board_t setup_board_with_snake(int size, unsigned int seed,
                                           int *out_id) {
	game_board_t board;
	board_init(&board, size, 4, seed);
	board_add_snake(&board, out_id);
	return board;
}

/* ========================================================================= */
/*  snake_set_direction tests                                                */
/* ========================================================================= */

Test(snake_suite, set_direction_valid) {
	snake_t snake = {0};
	setup_board_with_snake(20, 42,  NULL); //Not used, just here to avoid compiler error
	snake.direction = DIR_RIGHT;
	snake.next_direction = DIR_RIGHT;
	snake.alive = 1;
	int ret = snake_set_direction(&snake, DIR_UP);
	cr_assert_eq(ret, 0, "Setting non-opposite direction should succeed");
	cr_assert_eq(snake.next_direction, DIR_UP,
	             "next_direction should be updated to DIR_UP");
}

// test opposite direction movement
Test(snake, set_direction_opposite) {
    snake_t s = {0};
    s.direction = DIR_UP;

    int ret = snake_set_direction(&s, DIR_DOWN);

	// should be ignored
    cr_assert_eq(ret, 0); 
	// next direction remains unchanged
    cr_assert_eq(s.next_direction, 0); 
}

// test change to same direction -> no problem
Test(snake, set_direction_same) {
    snake_t s = {0};
    s.direction = DIR_RIGHT;

    snake_set_direction(&s, DIR_RIGHT);

    cr_assert_eq(s.next_direction, DIR_RIGHT);
}

// set to invalid direction
Test(snake, set_direction_invalid_enum) {
    snake_t s = {0};
    s.direction = DIR_RIGHT;

    cr_assert_eq(snake_set_direction(&s, 99), -1);
}

// null direction
Test(snake, set_direction_null) {
    int ret = snake_set_direction(NULL, DIR_UP);
    cr_assert_eq(ret, -1);
}

// snake longer than max
Test(snake, max_length_cap) {
    game_board_t board;
    board_init(&board, 10, 4, 1);

    int id;
    board_add_snake(&board, &id);

    snake_t *s = &board.snakes[id];
    s->length = MAX_SNAKE_LENGTH;

    // Put apple in front
    position_t h = s->body[0];
    board.cells[h.y * 10 + (h.x + 1)] = CELL_APPLE;

    snake_advance(&board, id);

    cr_assert_eq(s->length, MAX_SNAKE_LENGTH); // no overflow
}

// test snake advance - normal movement
Test(snake, advance_normal) {
    game_board_t board;
    board_init(&board, 10, 4, 1);

    int id;
    board_add_snake(&board, &id);

    snake_t *s = &board.snakes[id];
    position_t old = s->body[0];

    int ret = snake_advance(&board, id);

    cr_assert_eq(ret, 0);

    position_t new = s->body[0];
    cr_assert_eq(new.x, old.x + 1);

    board_free(&board);
}

// test snake advance - eat apple
Test(snake, advance_eat_apple) {
    game_board_t board;
    cr_assert_eq(board_init(&board, 10, 4, 1), 0);

    int id;
    cr_assert_eq(board_add_snake(&board, &id), 0);

    snake_t *s = &board.snakes[id];

    // Clear existing apple
    position_t old_apple = board.apple;
    board.cells[old_apple.y * board.size + old_apple.x] = CELL_EMPTY;

    // Place apple directly in front of snake
    position_t head = s->body[0];
    position_t apple_pos = {head.x + 1, head.y};

    board.cells[apple_pos.y * board.size + apple_pos.x] = CELL_APPLE;
    board.apple = apple_pos;

    int old_len = s->length;

    int ret = snake_advance(&board, id);

    // Assertions
    cr_assert_eq(ret, 2);
    cr_assert_eq(s->length, old_len + 1);

    // New head is at apple
    cr_assert_eq(s->body[0].x, apple_pos.x);
    cr_assert_eq(s->body[0].y, apple_pos.y);

    // Board updated
    cr_assert_eq(board.cells[apple_pos.y * board.size + apple_pos.x],
                 CELL_SNAKE_0 + id);

    board_free(&board);
}

// test snake advance - collision
Test(snake, advance_wall_collision) {
    game_board_t board;
    board_init(&board, 10, 4, 1);

    int id;
    board_add_snake(&board, &id);

    snake_t *s = &board.snakes[id];

    // move toward wall
    s->body[0].x = 8;
    s->direction = DIR_RIGHT;

    int ret = snake_advance(&board, id);

    cr_assert_eq(ret, 1);
    cr_assert_eq(s->alive, 0);

    board_free(&board);
}

// snake advance - snake hits itself
Test(snake, self_collision) {
    game_board_t board;
    cr_assert_eq(board_init(&board, 10, 4, 1), 0);

    int id;
    cr_assert_eq(board_add_snake(&board, &id), 0);

    snake_t *s = &board.snakes[id];

    // Clear original position
    position_t orig = s->body[0];
    board.cells[orig.y * board.size + orig.x] = CELL_EMPTY;

    // Build a U-shape that will collide
    s->length = 4;
    s->body[0] = (position_t){5,5};
    s->body[1] = (position_t){4,5};
    s->body[2] = (position_t){4,6};
    s->body[3] = (position_t){5,6};

    // Sync board with snake body
    for (int i = 0; i < s->length; i++) {
        position_t p = s->body[i];
        board.cells[p.y * board.size + p.x] = CELL_SNAKE_0 + id;
    }

    // Move DOWN into its own body (5,6)
    s->direction = DIR_DOWN;
    s->next_direction = DIR_DOWN;

    int ret = snake_advance(&board, id);

    cr_assert_eq(ret, 1);
    cr_assert_eq(s->alive, 0);

    board_free(&board);
}

// snake advance - hit other snake
Test(snake, collide_other_snake) {
    game_board_t board;
    board_init(&board, 10, 4, 1);

    int id1, id2;
    board_add_snake(&board, &id1);
    board_add_snake(&board, &id2);

    snake_t *s1 = &board.snakes[id1];
    snake_t *s2 = &board.snakes[id2];

    // Force collision
    s2->body[0] = (position_t){s1->body[0].x + 1, s1->body[0].y};
    board.cells[s2->body[0].y * 10 + s2->body[0].x] = CELL_SNAKE_1;

    int ret = snake_advance(&board, id1);

    cr_assert_eq(ret, 1);
}

// make sure board and state match
Test(snake, board_consistency_after_move) {
    game_board_t board;
    board_init(&board, 10, 4, 1);

    int id;
    board_add_snake(&board, &id);

    snake_advance(&board, id);

    snake_t *s = &board.snakes[id];

    for (int i = 0; i < s->length; i++) {
        position_t p = s->body[i];
        cr_assert_eq(board.cells[p.y * 10 + p.x], CELL_SNAKE_0 + id);
    }

    board_free(&board);
}