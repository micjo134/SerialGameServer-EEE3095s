/**
 ******************************************************************************
 * @file           : test_game.c
 * @brief          : Unit tests for Tic-Tac-Toe Game Engine (game.c)
 ******************************************************************************
 */

#include "unity.h"
#include "mock_hardware.h"
#include "game.h"

void setUp(void)
{
    Game_Init();
}

void tearDown(void)
{
}

/**
 * @brief Test initial state after Game_Init().
 */
void test_game_init_should_set_idle_state_and_not_connected(void)
{
    TEST_ASSERT_EQUAL_UINT8(0U, Game_IsConnected());
    TEST_ASSERT_EQUAL(GAME_STATE_IDLE, Game_GetState());
    TEST_ASSERT_EQUAL(GAME_PLAYER_X, Game_GetTurn());
    TEST_ASSERT_EQUAL(GAME_OUTCOME_NONE, Game_GetOutcome());
}

/**
 * @brief Test Game_Connect() transitions to connected and updates LED.
 */
void test_game_connect_should_succeed_and_set_connection_led(void)
{
    led_conn_set_Expect(1U);

    GameStatus status = Game_Connect();

    TEST_ASSERT_EQUAL(GAME_OK, status);
    TEST_ASSERT_EQUAL_UINT8(1U, Game_IsConnected());
    TEST_ASSERT_EQUAL(GAME_STATE_IDLE, Game_GetState());
}

/**
 * @brief Test Game_Connect() returns error when already connected.
 */
void test_game_connect_should_fail_if_already_connected(void)
{
    led_conn_set_Expect(1U);
    (void)Game_Connect();

    GameStatus status = Game_Connect();

    TEST_ASSERT_EQUAL(GAME_ERR_BADCMD, status);
}

/**
 * @brief Test Game_Disconnect() resets connection and turns off LED.
 */
void test_game_disconnect_should_reset_state_and_clear_led(void)
{
    led_conn_set_Expect(1U);
    (void)Game_Connect();

    led_conn_set_Expect(0U);
    GameStatus status = Game_Disconnect();

    TEST_ASSERT_EQUAL(GAME_OK, status);
    TEST_ASSERT_EQUAL_UINT8(0U, Game_IsConnected());
    TEST_ASSERT_EQUAL(GAME_STATE_IDLE, Game_GetState());
}

/**
 * @brief Test Game_New() starts a new game when connected.
 */
void test_game_new_should_start_game_in_progress_with_turn_X(void)
{
    led_conn_set_Expect(1U);
    (void)Game_Connect();

    GameStatus status = Game_New();

    TEST_ASSERT_EQUAL(GAME_OK, status);
    TEST_ASSERT_EQUAL(GAME_STATE_IN_PROGRESS, Game_GetState());
    TEST_ASSERT_EQUAL(GAME_PLAYER_X, Game_GetTurn());
}

/**
 * @brief Test Game_New() fails if already in progress.
 */
void test_game_new_should_fail_if_game_already_in_progress(void)
{
    led_conn_set_Expect(1U);
    (void)Game_Connect();
    (void)Game_New();

    GameStatus status = Game_New();

    TEST_ASSERT_EQUAL(GAME_ERR_BADCMD, status);
}

/**
 * @brief Test legal moves alternate turns between Player X and Player O.
 */
void test_game_move_should_alternate_turns(void)
{
    led_conn_set_Expect(1U);
    (void)Game_Connect();
    (void)Game_New();

    /* X moves at 0 */
    GameStatus s1 = Game_Move(0U);
    TEST_ASSERT_EQUAL(GAME_OK, s1);
    TEST_ASSERT_EQUAL(GAME_PLAYER_O, Game_GetTurn());

    /* O moves at 1 */
    GameStatus s2 = Game_Move(1U);
    TEST_ASSERT_EQUAL(GAME_OK, s2);
    TEST_ASSERT_EQUAL(GAME_PLAYER_X, Game_GetTurn());
}

/**
 * @brief Test out-of-range move is rejected.
 */
void test_game_move_should_reject_out_of_range_index(void)
{
    led_conn_set_Expect(1U);
    (void)Game_Connect();
    (void)Game_New();

    GameStatus status = Game_Move(9U);

    TEST_ASSERT_EQUAL(GAME_ERR_RANGE, status);
}

/**
 * @brief Test occupied cell move is rejected.
 */
void test_game_move_should_reject_occupied_cell(void)
{
    led_conn_set_Expect(1U);
    (void)Game_Connect();
    (void)Game_New();

    (void)Game_Move(4U); /* X moves center */
    GameStatus status = Game_Move(4U); /* O tries center */

    TEST_ASSERT_EQUAL(GAME_ERR_OCCUPIED, status);
}

/**
 * @brief Test winning condition detection (row 0, 1, 2 for X).
 */
void test_game_move_should_detect_row_win_for_player_x(void)
{
    led_conn_set_Expect(1U);
    (void)Game_Connect();
    (void)Game_New();

    /* X:0, O:3, X:1, O:4, X:2 (top row win) */
    (void)Game_Move(0U); /* X */
    (void)Game_Move(3U); /* O */
    (void)Game_Move(1U); /* X */
    (void)Game_Move(4U); /* O */
    GameStatus status = Game_Move(2U); /* X wins */

    TEST_ASSERT_EQUAL(GAME_OK, status);
    TEST_ASSERT_EQUAL(GAME_STATE_GAME_OVER, Game_GetState());
    TEST_ASSERT_EQUAL(GAME_OUTCOME_X, Game_GetOutcome());
    TEST_ASSERT_EQUAL(GAME_PLAYER_NONE, Game_GetTurn());
}

/**
 * @brief Test winning condition detection (diagonal 0, 4, 8).
 */
void test_game_move_should_detect_diagonal_win(void)
{
    led_conn_set_Expect(1U);
    (void)Game_Connect();
    (void)Game_New();

    /* X:0, O:1, X:4, O:2, X:8 (diagonal win) */
    (void)Game_Move(0U); /* X */
    (void)Game_Move(1U); /* O */
    (void)Game_Move(4U); /* X */
    (void)Game_Move(2U); /* O */
    GameStatus status = Game_Move(8U); /* X wins */

    TEST_ASSERT_EQUAL(GAME_OK, status);
    TEST_ASSERT_EQUAL(GAME_STATE_GAME_OVER, Game_GetState());
    TEST_ASSERT_EQUAL(GAME_OUTCOME_X, Game_GetOutcome());
}

/**
 * @brief Test draw detection when all 9 cells are full without a 3-in-a-row.
 */
void test_game_move_should_detect_draw(void)
{
    led_conn_set_Expect(1U);
    (void)Game_Connect();
    (void)Game_New();

    /*
     * Board:
     * X O X
     * X X O
     * O X O
     * Sequence: X:0, O:1, X:2, O:5, X:3, O:6, X:4, O:8, X:7
     */
    (void)Game_Move(0U); /* X */
    (void)Game_Move(1U); /* O */
    (void)Game_Move(2U); /* X */
    (void)Game_Move(5U); /* O */
    (void)Game_Move(3U); /* X */
    (void)Game_Move(6U); /* O */
    (void)Game_Move(4U); /* X */
    (void)Game_Move(8U); /* O */
    GameStatus status = Game_Move(7U); /* X (last cell) */

    TEST_ASSERT_EQUAL(GAME_OK, status);
    TEST_ASSERT_EQUAL(GAME_STATE_GAME_OVER, Game_GetState());
    TEST_ASSERT_EQUAL(GAME_OUTCOME_DRAW, Game_GetOutcome());
}

/**
 * @brief Test resignation awards victory to the opposing player.
 */
void test_game_resign_should_make_opponent_winner(void)
{
    led_conn_set_Expect(1U);
    (void)Game_Connect();
    (void)Game_New();

    /* X's turn to move, but X resigns -> O wins */
    GameStatus status = Game_Resign();

    TEST_ASSERT_EQUAL(GAME_OK, status);
    TEST_ASSERT_EQUAL(GAME_STATE_GAME_OVER, Game_GetState());
    TEST_ASSERT_EQUAL(GAME_OUTCOME_O, Game_GetOutcome());
}

/**
 * @brief Test board string encoding.
 */
void test_game_encode_board_format(void)
{
    led_conn_set_Expect(1U);
    (void)Game_Connect();
    (void)Game_New();

    (void)Game_Move(0U); /* X at 0 */
    (void)Game_Move(4U); /* O at 4 */

    char board[GAME_BOARD_CELLS + 1U];
    Game_EncodeBoard(board);

    TEST_ASSERT_EQUAL_STRING("X...O....", board);
}
