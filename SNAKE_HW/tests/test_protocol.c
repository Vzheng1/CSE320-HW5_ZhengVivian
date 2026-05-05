#include <arpa/inet.h>
#include <criterion/criterion.h>
#include <criterion/logging.h>
#include <string.h>

#include "game_board.h"
#include "global.h"
#include "protocol.h"

/* ========================================================================= */
/*  protocol_serialize_welcome tests                                         */
/* ========================================================================= */

Test(protocol_suite, welcome_basic) {
	uint8_t buf[16];
	int ret = protocol_serialize_welcome(buf, sizeof(buf), 2, 20, 4);
	cr_assert_eq(ret, 4, "Welcome message should be 4 bytes");
	cr_assert_eq(buf[0], MSG_WELCOME, "Byte 0 should be MSG_WELCOME (0x10)");
	cr_assert_eq(buf[1], 2, "Byte 1 should be player_id (2)");
	cr_assert_eq(buf[2], 20, "Byte 2 should be board_size (20)");
	cr_assert_eq(buf[3], 4, "Byte 3 should be max_players (4)");
}

// buffer too small
Test(protocol, welcome_buffer_small) {
    uint8_t buf[2];

    cr_assert_eq(protocol_serialize_welcome(buf, 2, 1, 10, 4), -1);
}

// test welcome 
Test(protocol, welcome) {
    uint8_t buf[4];

    int len = protocol_serialize_welcome(buf, 4, 2, 20, 4);

    cr_assert_eq(len, 4);
    cr_assert_eq(buf[0], 0x10);
    cr_assert_eq(buf[1], 2);
    cr_assert_eq(buf[2], 20);
    cr_assert_eq(buf[3], 4);
}

// test player dead 
Test(protocol, dead) {
    uint8_t buf[2];

    int len = protocol_serialize_dead(buf, 2, 3);

    cr_assert_eq(len, 2);
    cr_assert_eq(buf[0], 0x30);
    cr_assert_eq(buf[1], 3);
}

// test game state
Test(protocol, game_state_small_buffer) {
    game_board_t board;
    board_init(&board, 10, 4, 1);

    uint8_t buf[5]; // too small

    cr_assert_eq(protocol_serialize_game_state(buf, 5, &board), -1);

    board_free(&board);
}

// game state, game over, error

// test deserialize client message
Test(protocol, deserialize_valid) {
    uint8_t buf[2] = {0x02, 0x03};

    uint8_t type, payload;

    int ret = protocol_deserialize_client_msg(buf, 2, &type, &payload);

    cr_assert_eq(ret, 0);
    cr_assert_eq(type, 0x02);
    cr_assert_eq(payload, 0x03);
}

// test deserialize client message
Test(protocol, deserialize_invalid) {
    uint8_t buf[1] = {0x01};

    uint8_t t, p;

    cr_assert_eq(protocol_deserialize_client_msg(buf, 1, &t, &p), -1);
}