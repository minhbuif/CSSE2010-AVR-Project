/*
 * game.c
 *
 * Authors: Jarrod Bennett, Cody Burnett, Bradley Stone, Yufeng Gao
 * Modified by: Vu Hai Minh Bui
 *
 * Game logic and state handler.
 */ 

#include "game.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include "ledmatrix.h"
#include "terminalio.h"


// ========================== NOTE ABOUT MODULARITY ==========================

// The functions and global variables defined with the static keyword can
// only be accessed by this source file. If you wish to access them in
// another C file, you can remove the static keyword, and define them with
// the extern keyword in the other C file (or a header file included by the
// other C file). While not assessed, it is suggested that you develop the
// project with modularity in mind. Exposing internal variables and functions
// to other .C files reduces modularity.


// ============================ GLOBAL VARIABLES =============================

// The game board, which is dynamically constructed by initialise_game() and
// updated throughout the game. The 0th element of this array represents the
// bottom row, and the 7th element of this array represents the top row.
static uint8_t board[MATRIX_NUM_ROWS][MATRIX_NUM_COLUMNS];

// The location of the player.
static uint8_t player_row;
static uint8_t player_col;

// A flag for keeping track of whether the player is currently visible.
static bool player_visible;

#define PLAYER_SYMBOL '@'
#define WALL_SYMBOL '#'
#define BOX_SYMBOL 'O'
#define TARGET_SYMBOL 'X'
#define EMPTY_SYMBOL ' '


// ========================== GAME LOGIC FUNCTIONS ===========================

// This function paints a square based on the object(s) currently on it.
// Update the terminal display to match the LED matrix

const char* wall_messages[] = {
	"I've hit the wall.",
	"I've bumped into the wall",
	"Ouch! A wall"
};

static void paint_square(uint8_t row, uint8_t col)
{
	switch (board[row][col] & OBJECT_MASK)
	{
		case ROOM:
			ledmatrix_update_pixel(row, col, COLOUR_BLACK);
			move_terminal_cursor(MATRIX_NUM_ROWS - 1 - row + 12, col + 32);
			
			set_display_attribute(BG_BLACK);
			printf(" ");
			break;
		case WALL:
			ledmatrix_update_pixel(row, col, COLOUR_WALL);
			move_terminal_cursor(MATRIX_NUM_ROWS - 1 - row + 12, col + 32);
			
			set_display_attribute(BG_YELLOW);
			printf(" ");
			break;
		case BOX:
			ledmatrix_update_pixel(row, col, COLOUR_BOX);
			move_terminal_cursor(MATRIX_NUM_ROWS - 1 - row + 12, col + 32);
			
			set_display_attribute(BG_MAGENTA);
			printf(" ");
			break;
		case TARGET:
			ledmatrix_update_pixel(row, col, COLOUR_TARGET);
			move_terminal_cursor(MATRIX_NUM_ROWS - 1 - row + 12, col + 32);
			
			set_display_attribute(BG_RED);
			printf(" ");
			break;
		case BOX | TARGET:
			ledmatrix_update_pixel(row, col, COLOUR_DONE);
			move_terminal_cursor(MATRIX_NUM_ROWS - 1 - row + 12, col + 32);
			
			set_display_attribute(BG_GREEN);
			printf(" ");
			break;
		default:
			break;
	}
}

// This function initialises the global variables used to store the game
// state, and renders the initial game display.
void initialise_game(void)
{
	// Short definitions of game objects used temporarily for constructing
	// an easier-to-visualise game layout.
	#define _	(ROOM)
	#define W	(WALL)
	#define T	(TARGET)
	#define B	(BOX)

	// The starting layout of level 1. In this array, the top row is the
	// 0th row, and the bottom row is the 7th row. This makes it visually
	// identical to how the pixels are oriented on the LED matrix, however
	// the LED matrix treats row 0 as the bottom row and row 7 as the top
	// row.
	static const uint8_t lv1_layout[MATRIX_NUM_ROWS][MATRIX_NUM_COLUMNS] =
	{
		{ _, W, _, W, W, W, _, W, W, W, _, _, W, W, W, W },
		{ _, W, T, W, _, _, W, T, _, B, _, _, _, _, T, W },
		{ _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _ },
		{ W, _, B, _, _, _, _, W, _, _, B, _, _, B, _, W },
		{ W, _, _, _, W, _, B, _, _, _, _, _, _, _, _, _ },
		{ _, _, _, _, _, _, T, _, _, _, _, _, _, _, _, _ },
		{ _, _, _, W, W, W, W, W, W, T, _, _, _, _, _, W },
		{ W, W, _, _, _, _, _, _, W, W, _, _, W, W, W, W }
	};

	// Undefine the short game object names defined above, so that you
	// cannot use use them in your own code. Use of single-letter names/
	// constants is never a good idea.
	#undef _
	#undef W
	#undef T
	#undef B

	// Set the initial player location (for level 1).
	player_row = 5;
	player_col = 2;

	// Make the player icon initially invisible.
	player_visible = false;

	// Copy the starting layout (level 1 map) to the board array, and flip
	// all the rows.
	for (uint8_t row = 0; row < MATRIX_NUM_ROWS; row++)
	{
		for (uint8_t col = 0; col < MATRIX_NUM_COLUMNS; col++)
		{
			board[MATRIX_NUM_ROWS - 1 - row][col] =
				lv1_layout[row][col];
		}
	}

	// Draw the game board (map).
	for (uint8_t row = 0; row < MATRIX_NUM_ROWS; row++)
	{
		for (uint8_t col = 0; col < MATRIX_NUM_COLUMNS; col++)
		{
			paint_square(row, col);
		}
	}
}

// This function flashes the player icon. If the icon is currently visible, it
// is set to not visible and removed from the display. If the player icon is
// currently not visible, it is set to visible and rendered on the display.
// The static global variable "player_visible" indicates whether the player
// icon is currently visible.
void flash_player(void)
{
	player_visible = !player_visible;
	if (player_visible)
	{
		// The player is visible, paint it with COLOUR_PLAYER.
		ledmatrix_update_pixel(player_row, player_col, COLOUR_PLAYER);
		move_terminal_cursor(MATRIX_NUM_ROWS - 1 - player_row + 12, player_col + 32);
		
		set_display_attribute(BG_CYAN);
		printf(" ");
	}
	else
	{
		// The player is not visible, paint the underlying square.
		paint_square(player_row, player_col);
		
		
	}
}

// This function handles player movements.
bool move_player(int8_t delta_row, int8_t delta_col)
{
	//                    Implementation Suggestions
	//                    ==========================
	//
	//    Below are some suggestions for how to implement the first few
	//    features. These are only suggestions, you are absolutely not
	//   required to follow them if you know what you're doing, they're
	//     just here to help you get started. The suggestions for the
	//       earlier features are more detailed than the later ones.
	//
	// +-----------------------------------------------------------------+
	// |            Move Player with Push Buttons/Terminal               |
	// +-----------------------------------------------------------------+
	// | 1. Remove the display of the player icon from the current       |
	// |    location.                                                    |
	// |      - You may find the function flash_player() useful.         |
	// | 2. Calculate the new location of the player.                    |
	// |      - You may find creating a function for this useful.        |
	// | 3. Update the player location (player_row and player_col).      |
	// | 4. Draw the player icon at the new player location.             |
	// |      - Once again, you may find the function flash_player()     |
	// |        useful.                                                  |
	// | 5. Reset the icon flash cycle in the caller function (i.e.,     |
	// |    play_game()).                                                |
	// +-----------------------------------------------------------------+
	//
	// +-----------------------------------------------------------------+
	// |                      Game Logic - Walls                         |
	// +-----------------------------------------------------------------+
	// | 1. Modify this function to return a flag/boolean for indicating |
	// |    move validity - you do not want to reset icon flash cycle on |
	// |    invalid moves.                                               |
	// | 2. Modify this function to check if there is a wall at the      |
	// |    target location.                                             |
	// | 3. If the target location contains a wall, print one of your 3  |
	// |    'hit wall' messages and return a value indicating an invalid |
	// |    move.                                                        |
	// | 4. Otherwise make the move, clear the message area of the       |
	// |    terminal and return a value indicating a valid move.         |
	// +-----------------------------------------------------------------+
	//
	// +-----------------------------------------------------------------+
	// |                      Game Logic - Boxes                         |
	// +-----------------------------------------------------------------+
	// | 1. Modify this function to check if there is a box at the       |
	// |    target location.                                             |
	// | 2. If the target location contains a box, see if it can be      |
	// |    pushed. If not, print a message and return a value           |
	// |    indicating an invalid move.                                  |
	// | 3. Otherwise push the box and move the player, then clear the   |
	// |    message area of the terminal and return a valid indicating a |
	// |    valid move.                                                  |
	// +-----------------------------------------------------------------+

	// <YOUR CODE HERE>
	int8_t new_player_row = player_row + delta_row;
	int8_t new_player_col = player_col + delta_col;

	// Handle wrapping around the edges (both positive and negative deltas)
	if (new_player_row >= MATRIX_NUM_ROWS) {
		new_player_row = 0;
		} else if (new_player_row < 0) {
		new_player_row = MATRIX_NUM_ROWS - 1;
	}

	if (new_player_col >= MATRIX_NUM_COLUMNS) {
		new_player_col = 0;
		} else if (new_player_col < 0) {
		new_player_col = MATRIX_NUM_COLUMNS - 1;
	}

	uint8_t target_square = board[new_player_row][new_player_col];
	int8_t next_square_row = new_player_row + delta_row;
	int8_t next_square_col = new_player_col + delta_col;

	// Handle wrapping around for the next square behind the box (for negative deltas as well)
	if (next_square_row >= MATRIX_NUM_ROWS) {
		next_square_row = 0;
		} else if (next_square_row < 0) {
		next_square_row = MATRIX_NUM_ROWS - 1;
	}

	if (next_square_col >= MATRIX_NUM_COLUMNS) {
		next_square_col = 0;
		} else if (next_square_col < 0) {
		next_square_col = MATRIX_NUM_COLUMNS - 1;
	}

	uint8_t next_square = board[next_square_row][next_square_col];

	// Check if the player is moving into a wall
	if (target_square == WALL) {
		move_terminal_cursor(5, 32);
		clear_to_end_of_line();
		int randomIndex = rand() % 3;
		printf("%s\n", wall_messages[randomIndex]);
		
		return false;
	}

	// Check if the player is moving into a box
	if (target_square == BOX || target_square == (BOX | TARGET)) {
		// Check if the next square behind the box is a wall or another box
		if (next_square == WALL) {
			return false;
			} else if (next_square == BOX || next_square == (BOX | TARGET)) {
			return false;
			} else if (next_square == TARGET) {
			// Valid push: move the box and the player
			board[next_square_row][next_square_col] = BOX | TARGET;
			} else {
				board[next_square_row][next_square_col] = BOX;
			}

			// Update the old box position to ROOM or TARGET
			if (target_square == (BOX | TARGET)) {
				board[new_player_row][new_player_col] = TARGET;
			} else {
				board[new_player_row][new_player_col] = ROOM;
			



			// Handle target square color change
			if (next_square == TARGET) {
			}

			// Before moving the player, clear the old player position
			paint_square(player_row, player_col);

			// Move the player to the original position of the box
			player_visible = true;
			flash_player();
			player_row = new_player_row;
			player_col = new_player_col;

			// Update display
			paint_square(player_row, player_col);  // Paint player's new position
			paint_square(next_square_row, next_square_col);  // Paint the new box position
			flash_player();  // Update player visibility
			return true;
		}
	}

	// Handle normal movement (no boxes involved)
	// Before moving the player, clear the old player position
	player_visible = true;
	flash_player();

	player_row = new_player_row;
	player_col = new_player_col;
	flash_player();  // Update player visibility
	return true;
}

// This function checks if the game is over (i.e., the level is solved), and
// returns true iff (if and only if) the game is over.
bool is_game_over(void) {
	// <YOUR CODE HERE>.
	for (int i = 0; i < 8; i++) {
		for (int j = 0; j < 16; j++) {
			if (board[i][j] == TARGET) {
				return false;
			}
		}
	}
	return true;
}
void hides_player_when_game_over(void) {
	board[player_row][player_col] = ROOM;
	paint_square(player_row, player_col);
}

	
	



