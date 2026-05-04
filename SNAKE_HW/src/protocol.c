#include "protocol.h"
#include "debug.h"

#include <string.h>
#include <arpa/inet.h>

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

	// 0 = type 0x30
	// 1 = number snakes alive
	// 2 = apple X position
	// 4 = apple Y position
	// 6 = snake data
	int offset = 0;
	buf[offset++] = MSG_GAME_STATE;
	
	// get the number of snakes alive
	int snakes_alive = 0;
	for(int i=0; i<MAX_PLAYERS; i++) {
		if(board->snakes[i].alive) {
			snakes_alive++;
		}
	}
	buf[offset++] = (uint8_t)snakes_alive;

	// get the x and y position [big endian uint16] of the apple
    uint16_t apple_x = htons((uint16_t)board->apple.x);
    uint16_t apple_y = htons((uint16_t)board->apple.y);

	// copy into buffer
    memcpy(&buf[offset], &apple_x, 2);
    offset += 2;
    memcpy(&buf[offset], &apple_y, 2);
    offset += 2;

    // for each snake alive, save the snake data -> each 4+(length*4) bytes
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (board->snakes[i].alive) {
            const snake_t *snake = &board->snakes[i];

            // make sure there is enough space in the buffer -> error if too big
            if ((size_t)offset + 4 + (snake->length * 4) > buf_len) {
                return -1;
            }

			// 0 = snake id (1 byte)
            buf[offset++] = (uint8_t)snake->id;

			// 1 = snake length (2 bytes)
            uint16_t length = htons((uint16_t)snake->length);
            memcpy(&buf[offset], &length, 2);
            offset += 2;

			// 3 = direction (1 byte)
            buf[offset++] = (uint8_t)snake->direction;

            // 4 = body segments, length pairs of (x,y), head first
            for (int j = 0; j < snake->length; j++) {
				// get the x and y position [big endian uint16] of body
                uint16_t x = htons((uint16_t)snake->body[j].x);
                uint16_t y = htons((uint16_t)snake->body[j].y);
                
				// copy to buffer
				memcpy(&buf[offset], &x, 2);
                offset += 2;
                memcpy(&buf[offset], &y, 2);
                offset += 2;
            }
        }
    }

	// return the total number of bytes in the buffer
	return offset + 1;
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
	// null pointers, buffer too small
	if(!buf || !out_type || !out_payload || buf_len < 2) {
		return -1;
	}

	// get type and payload from the buffer
	uint8_t type = buf[0];
    uint8_t payload = buf[1];

    // validate the message type -> type should be JOIN, DIRECTION, or LEAVE -> error if not any of these
    if (type != MSG_JOIN && type != MSG_DIRECTION && type != MSG_LEAVE) {
        return -1;
    }

	// save type and payload to pointers
    *out_type = type;
    *out_payload = payload;

	// return 0 on success
    return 0;
}
