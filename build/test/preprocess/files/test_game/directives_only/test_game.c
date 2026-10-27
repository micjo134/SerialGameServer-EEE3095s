void setUp(void)
{
    Game_Init();
}

void tearDown(void)
{
}




void test_game_init_should_set_idle_state_and_not_connected(void)
{
    TEST_ASSERT_EQUAL_UINT8(0U, Game_IsConnected());
    TEST_ASSERT_EQUAL(GAME_STATE_IDLE, Game_GetState());
    TEST_ASSERT_EQUAL(GAME_PLAYER_X, Game_GetTurn());
    TEST_ASSERT_EQUAL(GAME_OUTCOME_NONE, Game_GetOutcome());
}




void test_game_connect_should_succeed_and_set_connection_led(void)
{
    led_conn_set_Expect(1U);

    GameStatus status = Game_Connect();

    TEST_ASSERT_EQUAL(GAME_OK, status);
    TEST_ASSERT_EQUAL_UINT8(1U, Game_IsConnected());
    TEST_ASSERT_EQUAL(GAME_STATE_IDLE, Game_GetState());
}




void test_game_connect_should_fail_if_already_connected(void)
{
    led_conn_set_Expect(1U);
    (void)Game_Connect();

    GameStatus status = Game_Connect();

    TEST_ASSERT_EQUAL(GAME_ERR_BADCMD, status);
}




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




void test_game_new_should_start_game_in_progress_with_turn_X(void)
{
    led_conn_set_Expect(1U);
    (void)Game_Connect();

    GameStatus status = Game_New();

    TEST_ASSERT_EQUAL(GAME_OK, status);
    TEST_ASSERT_EQUAL(GAME_STATE_IN_PROGRESS, Game_GetState());
    TEST_ASSERT_EQUAL(GAME_PLAYER_X, Game_GetTurn());
}




void test_game_new_should_fail_if_game_already_in_progress(void)
{
    led_conn_set_Expect(1U);
    (void)Game_Connect();
    (void)Game_New();

    GameStatus status = Game_New();

    TEST_ASSERT_EQUAL(GAME_ERR_BADCMD, status);
}




void test_game_move_should_alternate_turns(void)
{
    led_conn_set_Expect(1U);
    (void)Game_Connect();
    (void)Game_New();


    GameStatus s1 = Game_Move(0U);
    TEST_ASSERT_EQUAL(GAME_OK, s1);
    TEST_ASSERT_EQUAL(GAME_PLAYER_O, Game_GetTurn());


    GameStatus s2 = Game_Move(1U);
    TEST_ASSERT_EQUAL(GAME_OK, s2);
    TEST_ASSERT_EQUAL(GAME_PLAYER_X, Game_GetTurn());
}




void test_game_move_should_reject_out_of_range_index(void)
{
    led_conn_set_Expect(1U);
    (void)Game_Connect();
    (void)Game_New();

    GameStatus status = Game_Move(9U);

    TEST_ASSERT_EQUAL(GAME_ERR_RANGE, status);
}




void test_game_move_should_reject_occupied_cell(void)
{
    led_conn_set_Expect(1U);
    (void)Game_Connect();
    (void)Game_New();

    (void)Game_Move(4U);
    GameStatus status = Game_Move(4U);

    TEST_ASSERT_EQUAL(GAME_ERR_OCCUPIED, status);
}




void test_game_move_should_detect_row_win_for_player_x(void)
{
    led_conn_set_Expect(1U);
    (void)Game_Connect();
    (void)Game_New();


    (void)Game_Move(0U);
    (void)Game_Move(3U);
    (void)Game_Move(1U);
    (void)Game_Move(4U);
    GameStatus status = Game_Move(2U);

    TEST_ASSERT_EQUAL(GAME_OK, status);
    TEST_ASSERT_EQUAL(GAME_STATE_GAME_OVER, Game_GetState());
    TEST_ASSERT_EQUAL(GAME_OUTCOME_X, Game_GetOutcome());
    TEST_ASSERT_EQUAL(GAME_PLAYER_NONE, Game_GetTurn());
}




void test_game_move_should_detect_diagonal_win(void)
{
    led_conn_set_Expect(1U);
    (void)Game_Connect();
    (void)Game_New();


    (void)Game_Move(0U);
    (void)Game_Move(1U);
    (void)Game_Move(4U);
    (void)Game_Move(2U);
    GameStatus status = Game_Move(8U);

    TEST_ASSERT_EQUAL(GAME_OK, status);
    TEST_ASSERT_EQUAL(GAME_STATE_GAME_OVER, Game_GetState());
    TEST_ASSERT_EQUAL(GAME_OUTCOME_X, Game_GetOutcome());
}




void test_game_move_should_detect_draw(void)
{
    led_conn_set_Expect(1U);
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

    TEST_ASSERT_EQUAL(GAME_OK, status);
    TEST_ASSERT_EQUAL(GAME_STATE_GAME_OVER, Game_GetState());
    TEST_ASSERT_EQUAL(GAME_OUTCOME_DRAW, Game_GetOutcome());
}




void test_game_resign_should_make_opponent_winner(void)
{
    led_conn_set_Expect(1U);
    (void)Game_Connect();
    (void)Game_New();


    GameStatus status = Game_Resign();

    TEST_ASSERT_EQUAL(GAME_OK, status);
    TEST_ASSERT_EQUAL(GAME_STATE_GAME_OVER, Game_GetState());
    TEST_ASSERT_EQUAL(GAME_OUTCOME_O, Game_GetOutcome());
}




void test_game_encode_board_format(void)
{
    led_conn_set_Expect(1U);
    (void)Game_Connect();
    (void)Game_New();

    (void)Game_Move(0U);
    (void)Game_Move(4U);

    char board[GAME_BOARD_CELLS + 1U];
    Game_EncodeBoard(board);

    TEST_ASSERT_EQUAL_STRING("X...O....", board);
}
