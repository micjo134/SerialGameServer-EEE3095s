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




void test_game_init_should_set_idle_state_and_not_connected(void)
{
    UnityAssertEqualUintNumber((UNITY_UINT)(UNITY_UINT8)((0U)), (UNITY_UINT)(UNITY_UINT8)((Game_IsConnected())), ( ((void *)0) ), (UNITY_UINT)(26), UNITY_DISPLAY_STYLE_UINT8);
    UnityAssertEqualIntNumber((UNITY_INT)((GAME_STATE_IDLE)), (UNITY_INT)((Game_GetState())), ( ((void *)0) ), (UNITY_UINT)(27), UNITY_DISPLAY_STYLE_INT);
    UnityAssertEqualIntNumber((UNITY_INT)((GAME_PLAYER_X)), (UNITY_INT)((Game_GetTurn())), ( ((void *)0) ), (UNITY_UINT)(28), UNITY_DISPLAY_STYLE_INT);
    UnityAssertEqualIntNumber((UNITY_INT)((GAME_OUTCOME_NONE)), (UNITY_INT)((Game_GetOutcome())), ( ((void *)0) ), (UNITY_UINT)(29), UNITY_DISPLAY_STYLE_INT);
}




void test_game_connect_should_succeed_and_set_connection_led(void)
{
    led_conn_set_CMockExpect(37, 1U);

    GameStatus status = Game_Connect();

    UnityAssertEqualIntNumber((UNITY_INT)((GAME_OK)), (UNITY_INT)((status)), ( ((void *)0) ), (UNITY_UINT)(41), UNITY_DISPLAY_STYLE_INT);
    UnityAssertEqualUintNumber((UNITY_UINT)(UNITY_UINT8)((1U)), (UNITY_UINT)(UNITY_UINT8)((Game_IsConnected())), ( ((void *)0) ), (UNITY_UINT)(42), UNITY_DISPLAY_STYLE_UINT8);
    UnityAssertEqualIntNumber((UNITY_INT)((GAME_STATE_IDLE)), (UNITY_INT)((Game_GetState())), ( ((void *)0) ), (UNITY_UINT)(43), UNITY_DISPLAY_STYLE_INT);
}




void test_game_connect_should_fail_if_already_connected(void)
{
    led_conn_set_CMockExpect(51, 1U);
    (void)Game_Connect();

    GameStatus status = Game_Connect();

    UnityAssertEqualIntNumber((UNITY_INT)((GAME_ERR_BADCMD)), (UNITY_INT)((status)), ( ((void *)0) ), (UNITY_UINT)(56), UNITY_DISPLAY_STYLE_INT);
}




void test_game_disconnect_should_reset_state_and_clear_led(void)
{
    led_conn_set_CMockExpect(64, 1U);
    (void)Game_Connect();

    led_conn_set_CMockExpect(67, 0U);
    GameStatus status = Game_Disconnect();

    UnityAssertEqualIntNumber((UNITY_INT)((GAME_OK)), (UNITY_INT)((status)), ( ((void *)0) ), (UNITY_UINT)(70), UNITY_DISPLAY_STYLE_INT);
    UnityAssertEqualUintNumber((UNITY_UINT)(UNITY_UINT8)((0U)), (UNITY_UINT)(UNITY_UINT8)((Game_IsConnected())), ( ((void *)0) ), (UNITY_UINT)(71), UNITY_DISPLAY_STYLE_UINT8);
    UnityAssertEqualIntNumber((UNITY_INT)((GAME_STATE_IDLE)), (UNITY_INT)((Game_GetState())), ( ((void *)0) ), (UNITY_UINT)(72), UNITY_DISPLAY_STYLE_INT);
}




void test_game_new_should_start_game_in_progress_with_turn_X(void)
{
    led_conn_set_CMockExpect(80, 1U);
    (void)Game_Connect();

    GameStatus status = Game_New();

    UnityAssertEqualIntNumber((UNITY_INT)((GAME_OK)), (UNITY_INT)((status)), ( ((void *)0) ), (UNITY_UINT)(85), UNITY_DISPLAY_STYLE_INT);
    UnityAssertEqualIntNumber((UNITY_INT)((GAME_STATE_IN_PROGRESS)), (UNITY_INT)((Game_GetState())), ( ((void *)0) ), (UNITY_UINT)(86), UNITY_DISPLAY_STYLE_INT);
    UnityAssertEqualIntNumber((UNITY_INT)((GAME_PLAYER_X)), (UNITY_INT)((Game_GetTurn())), ( ((void *)0) ), (UNITY_UINT)(87), UNITY_DISPLAY_STYLE_INT);
}




void test_game_new_should_fail_if_game_already_in_progress(void)
{
    led_conn_set_CMockExpect(95, 1U);
    (void)Game_Connect();
    (void)Game_New();

    GameStatus status = Game_New();

    UnityAssertEqualIntNumber((UNITY_INT)((GAME_ERR_BADCMD)), (UNITY_INT)((status)), ( ((void *)0) ), (UNITY_UINT)(101), UNITY_DISPLAY_STYLE_INT);
}




void test_game_move_should_alternate_turns(void)
{
    led_conn_set_CMockExpect(109, 1U);
    (void)Game_Connect();
    (void)Game_New();


    GameStatus s1 = Game_Move(0U);
    UnityAssertEqualIntNumber((UNITY_INT)((GAME_OK)), (UNITY_INT)((s1)), ( ((void *)0) ), (UNITY_UINT)(115), UNITY_DISPLAY_STYLE_INT);
    UnityAssertEqualIntNumber((UNITY_INT)((GAME_PLAYER_O)), (UNITY_INT)((Game_GetTurn())), ( ((void *)0) ), (UNITY_UINT)(116), UNITY_DISPLAY_STYLE_INT);


    GameStatus s2 = Game_Move(1U);
    UnityAssertEqualIntNumber((UNITY_INT)((GAME_OK)), (UNITY_INT)((s2)), ( ((void *)0) ), (UNITY_UINT)(120), UNITY_DISPLAY_STYLE_INT);
    UnityAssertEqualIntNumber((UNITY_INT)((GAME_PLAYER_X)), (UNITY_INT)((Game_GetTurn())), ( ((void *)0) ), (UNITY_UINT)(121), UNITY_DISPLAY_STYLE_INT);
}




void test_game_move_should_reject_out_of_range_index(void)
{
    led_conn_set_CMockExpect(129, 1U);
    (void)Game_Connect();
    (void)Game_New();

    GameStatus status = Game_Move(9U);

    UnityAssertEqualIntNumber((UNITY_INT)((GAME_ERR_RANGE)), (UNITY_INT)((status)), ( ((void *)0) ), (UNITY_UINT)(135), UNITY_DISPLAY_STYLE_INT);
}




void test_game_move_should_reject_occupied_cell(void)
{
    led_conn_set_CMockExpect(143, 1U);
    (void)Game_Connect();
    (void)Game_New();

    (void)Game_Move(4U);
    GameStatus status = Game_Move(4U);

    UnityAssertEqualIntNumber((UNITY_INT)((GAME_ERR_OCCUPIED)), (UNITY_INT)((status)), ( ((void *)0) ), (UNITY_UINT)(150), UNITY_DISPLAY_STYLE_INT);
}




void test_game_move_should_detect_row_win_for_player_x(void)
{
    led_conn_set_CMockExpect(158, 1U);
    (void)Game_Connect();
    (void)Game_New();


    (void)Game_Move(0U);
    (void)Game_Move(3U);
    (void)Game_Move(1U);
    (void)Game_Move(4U);
    GameStatus status = Game_Move(2U);

    UnityAssertEqualIntNumber((UNITY_INT)((GAME_OK)), (UNITY_INT)((status)), ( ((void *)0) ), (UNITY_UINT)(169), UNITY_DISPLAY_STYLE_INT);
    UnityAssertEqualIntNumber((UNITY_INT)((GAME_STATE_GAME_OVER)), (UNITY_INT)((Game_GetState())), ( ((void *)0) ), (UNITY_UINT)(170), UNITY_DISPLAY_STYLE_INT);
    UnityAssertEqualIntNumber((UNITY_INT)((GAME_OUTCOME_X)), (UNITY_INT)((Game_GetOutcome())), ( ((void *)0) ), (UNITY_UINT)(171), UNITY_DISPLAY_STYLE_INT);
    UnityAssertEqualIntNumber((UNITY_INT)((GAME_PLAYER_NONE)), (UNITY_INT)((Game_GetTurn())), ( ((void *)0) ), (UNITY_UINT)(172), UNITY_DISPLAY_STYLE_INT);
}




void test_game_move_should_detect_diagonal_win(void)
{
    led_conn_set_CMockExpect(180, 1U);
    (void)Game_Connect();
    (void)Game_New();


    (void)Game_Move(0U);
    (void)Game_Move(1U);
    (void)Game_Move(4U);
    (void)Game_Move(2U);
    GameStatus status = Game_Move(8U);

    UnityAssertEqualIntNumber((UNITY_INT)((GAME_OK)), (UNITY_INT)((status)), ( ((void *)0) ), (UNITY_UINT)(191), UNITY_DISPLAY_STYLE_INT);
    UnityAssertEqualIntNumber((UNITY_INT)((GAME_STATE_GAME_OVER)), (UNITY_INT)((Game_GetState())), ( ((void *)0) ), (UNITY_UINT)(192), UNITY_DISPLAY_STYLE_INT);
    UnityAssertEqualIntNumber((UNITY_INT)((GAME_OUTCOME_X)), (UNITY_INT)((Game_GetOutcome())), ( ((void *)0) ), (UNITY_UINT)(193), UNITY_DISPLAY_STYLE_INT);
}




void test_game_move_should_detect_draw(void)
{
    led_conn_set_CMockExpect(201, 1U);
    (void)Game_Connect();
    (void)Game_New();
    (void)Game_Move(0U);
    (void)Game_Move(1U);
    (void)Game_Move(2U);
    (void)Game_Move(5U);
    (void)Game_Move(3U);
    (void)Game_Move(6U);
    (void)Game_Move(4U);
    (void)Game_Move(8U);
    GameStatus status = Game_Move(7U);

    UnityAssertEqualIntNumber((UNITY_INT)((GAME_OK)), (UNITY_INT)((status)), ( ((void *)0) ), (UNITY_UINT)(222), UNITY_DISPLAY_STYLE_INT);
    UnityAssertEqualIntNumber((UNITY_INT)((GAME_STATE_GAME_OVER)), (UNITY_INT)((Game_GetState())), ( ((void *)0) ), (UNITY_UINT)(223), UNITY_DISPLAY_STYLE_INT);
    UnityAssertEqualIntNumber((UNITY_INT)((GAME_OUTCOME_DRAW)), (UNITY_INT)((Game_GetOutcome())), ( ((void *)0) ), (UNITY_UINT)(224), UNITY_DISPLAY_STYLE_INT);
}




void test_game_resign_should_make_opponent_winner(void)
{
    led_conn_set_CMockExpect(232, 1U);
    (void)Game_Connect();
    (void)Game_New();


    GameStatus status = Game_Resign();

    UnityAssertEqualIntNumber((UNITY_INT)((GAME_OK)), (UNITY_INT)((status)), ( ((void *)0) ), (UNITY_UINT)(239), UNITY_DISPLAY_STYLE_INT);
    UnityAssertEqualIntNumber((UNITY_INT)((GAME_STATE_GAME_OVER)), (UNITY_INT)((Game_GetState())), ( ((void *)0) ), (UNITY_UINT)(240), UNITY_DISPLAY_STYLE_INT);
    UnityAssertEqualIntNumber((UNITY_INT)((GAME_OUTCOME_O)), (UNITY_INT)((Game_GetOutcome())), ( ((void *)0) ), (UNITY_UINT)(241), UNITY_DISPLAY_STYLE_INT);
}




void test_game_encode_board_format(void)
{
    led_conn_set_CMockExpect(249, 1U);
    (void)Game_Connect();
    (void)Game_New();

    (void)Game_Move(0U);
    (void)Game_Move(4U);

    char board[9U + 1U];
    Game_EncodeBoard(board);

    UnityAssertEqualString((const char*)(("X...O....")), (const char*)((board)), ( ((void *)0) ), (UNITY_UINT)(259));
}

