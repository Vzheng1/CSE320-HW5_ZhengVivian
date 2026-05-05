#include <criterion/criterion.h>
#include <criterion/logging.h>
#include <stdlib.h>

#include "game_board.h"
#include "global.h"

/* ========================================================================= */
/*  board_init tests                                                         */
/* ========================================================================= */

Test(board_suite, init_basic) {
	game_board_t board;
	int ret = board_init(&board, 20, 4, 42);
	cr_assert_eq(ret, 0, "board_init should return 0 on success");
	cr_assert_eq(board.size, 20);
	cr_assert_eq(board.max_snakes, 4);
	cr_assert_eq(board.num_snakes, 0);
	cr_assert_not_null(board.cells);
	board_free(&board);
}

// initilize basic board
Test(board, init_basic_1) {
    game_board_t board;

    int ret = board_init(&board, 20, 4, 1234);

    cr_assert_eq(ret, 0);
    cr_assert_eq(board.size, 20);
    cr_assert_eq(board.num_snakes, 0);
    cr_assert_not_null(board.cells);

    // check the walls
    cr_assert_eq(board.cells[0], CELL_WALL);
    cr_assert_eq(board.cells[19], CELL_WALL);
    cr_assert_eq(board.cells[19 * 20], CELL_WALL);

    board_free(&board);
}

// invalid board size
Test(board, init_invalid_size) {
    game_board_t board;
    cr_assert_eq(board_init(&board, 5, 4, 1), -1);
}

// null pointer to board
Test(board, init_null) {
    cr_assert_eq(board_init(NULL, 20, 4, 1), -1);
}

// place apple on board
Test(board, place_apple) {
    game_board_t board;
    board_init(&board, 10, 4, 42);

    int ret = board_place_apple(&board);

    cr_assert_eq(ret, 0);

    position_t apple = board.apple;
    int idx = apple.y * board.size + apple.x;

    cr_assert_eq(board.cells[idx], CELL_APPLE);

    board_free(&board);
}

// place apple on full board
Test(board, place_apple_full) {
    game_board_t board;
    board_init(&board, 10, 4, 1);

    // fill everything except walls
    for (int y = 1; y < 9; y++) {
        for (int x = 1; x < 9; x++) {
            board.cells[y * 10 + x] = CELL_SNAKE_0;
        }
    }

    cr_assert_eq(board_place_apple(&board), -1);

    board_free(&board);
}

// determine apple placement 
Test(board, apple_deterministic) {
    game_board_t b1, b2;

    board_init(&b1, 10, 4, 42);
    board_init(&b2, 10, 4, 42);

    cr_assert_eq(b1.apple.x, b2.apple.x);
    cr_assert_eq(b1.apple.y, b2.apple.y);

    board_free(&b1);
    board_free(&b2);
}

// make sure apple is not on wall or snake
Test(board, apple_not_on_wall_or_snake) {
    game_board_t board;
    board_init(&board, 10, 4, 1);

    int id;
    board_add_snake(&board, &id);

    position_t apple = board.apple;
    cell_t c = board.cells[apple.y * 10 + apple.x];

    cr_assert_eq(c, CELL_APPLE);

    // Ensure not wall
    cr_assert_neq(apple.x, 0);
    cr_assert_neq(apple.y, 0);

    board_free(&board);
}

// add snake to board
Test(board, add_snake_basic) {
    game_board_t board;
    board_init(&board, 20, 4, 1);

    int id;
    int ret = board_add_snake(&board, &id);

    cr_assert_eq(ret, 0);
    cr_assert_eq(id, 0);
    cr_assert_eq(board.num_snakes, 1);

    snake_t *s = &board.snakes[id];
    cr_assert_eq(s->alive, 1);
    cr_assert_eq(s->length, 1);

    board_free(&board);
}

// add snake to full board
Test(board, add_snake_full) {
    game_board_t board;
    board_init(&board, 20, 1, 1);

    int id;
    board_add_snake(&board, &id);

    cr_assert_eq(board_add_snake(&board, &id), -1);

    board_free(&board);
}

// remove snake should clear all cells it was one
Test(board, remove_snake_clears_cells) {
    game_board_t board;
    board_init(&board, 10, 4, 1);

    int id;
    board_add_snake(&board, &id);

    snake_t *s = &board.snakes[id];
    position_t pos = s->body[0];

    board_remove_snake(&board, id);

    cr_assert_eq(board.cells[pos.y * 10 + pos.x], CELL_EMPTY);
    cr_assert_eq(s->alive, 0);
}

// tick should move all snakes on the board
Test(board, tick_moves_all) {
    game_board_t board;
    board_init(&board, 10, 4, 1);

    int id1, id2;
    board_add_snake(&board, &id1);
    board_add_snake(&board, &id2);

    position_t before1 = board.snakes[id1].body[0];

    board_tick(&board);

    position_t after1 = board.snakes[id1].body[0];

    cr_assert_neq(after1.x, before1.x);

    board_free(&board);
}

// order matters for tick
Test(board, tick_order_consistency) {
    game_board_t board;
    board_init(&board, 10, 4, 1);

    int id1, id2;
    board_add_snake(&board, &id1);
    board_add_snake(&board, &id2);

    // Force them into potential conflict
    board.snakes[id1].direction = DIR_RIGHT;
    board.snakes[id2].direction = DIR_LEFT;

    cr_assert_eq(board_tick(&board), 0);
}

