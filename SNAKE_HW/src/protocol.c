#include "protocol.h"
#include "debug.h"

int protocol_serialize_welcome(uint8_t *buf, size_t buf_len, int player_id, int board_size, int max_players) {
	// invalid pointer, buffer size too small, 
	if(!buf || buf_len < 4 || max_players < MAX_PLAYERS_MIN || max_players > MAX_PLAYERS_MAX || 
		board_size < 10 || board_size > 50) {
		return -1;
	}

	// 0 = Type: 0x10
	// 1 = assigned player id
	// 2 = board size (N)
	// 3 = max players
	buf[0] = MSG_WELCOME;
	buf[1] = (uint8_t)player_id;
	buf[2] = (uint8_t)board_size;
	buf[3] = (uint8_t)max_players;

	return 4;
}

int protocol_serialize_game_state(uint8_t *buf, size_t buf_len, const game_board_t *board) {
	// null pointers, buffer too small
	if(!buf || !board || buf_len < 6) {
		return -1;
	}
	return 0;
}

int protocol_serialize_dead(uint8_t *buf, size_t buf_len, int player_id) {
	// null pointers, buffer too small
	if(!buf || buf_len < 2) {
		return -1;
	}

	// 0 = type 0x30
	// 1 = dead player id
	buf[0] = MSG_PLAYER_DEAD;
	buf[1] = (uint8_t)player_id;

	return 2;
}

int protocol_serialize_game_over(uint8_t *buf, size_t buf_len, int winner_id) {
	// null pointers, buffer too small
	if(!buf || buf_len < 2) {
		return -1;
	}

	// 0 = type 0x40
	// 1 = winner player id OR 0xFF is no winner
	buf[0] = MSG_GAME_OVER;
	buf[1] = (uint8_t)winner_id;

	return 2;
}

int protocol_serialize_error(uint8_t *buf, size_t buf_len, uint8_t error_code) {
	// null pointers, buffer too small
	if(!buf || buf_len < 2) {
		return -1;
	}

	// 0 = type 0xF0
	// 1 = error code -> 0x01 game full, 0x02 already joined, 0x03 invalid message
	buf[0] = MSG_ERROR;
	buf[1] = error_code;

	return 2;
}

int protocol_deserialize_client_msg(const uint8_t *buf, size_t buf_len, uint8_t *out_type, uint8_t *out_payload) {
	(void)buf;
	(void)buf_len;
	(void)out_type;
	(void)out_payload;
	return 0;
}
