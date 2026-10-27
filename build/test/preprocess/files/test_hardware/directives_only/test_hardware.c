#define TEST_SENSOR_PIN GPIO_PIN_0
#define TEST_SENSOR_PORT GPIOA
#define TEST_LED_CONN_PIN GPIO_PIN_4
#define TEST_LED_CONN_PORT GPIOB
#define TEST_LED_GAME_PIN GPIO_PIN_5
#define TEST_LED_GAME_PORT GPIOB

void setUp(void)
{
}

void tearDown(void)
{
}




void test_sensor_read_should_return_1_when_pin_is_high(void)
{
    HAL_GPIO_ReadPin_ExpectAndReturn(TEST_SENSOR_PORT, TEST_SENSOR_PIN, GPIO_PIN_SET);

    uint8_t result = sensor_read();

    TEST_ASSERT_EQUAL_UINT8(1U, result);
}




void test_sensor_read_should_return_0_when_pin_is_low(void)
{
    HAL_GPIO_ReadPin_ExpectAndReturn(TEST_SENSOR_PORT, TEST_SENSOR_PIN, GPIO_PIN_RESET);

    uint8_t result = sensor_read();

    TEST_ASSERT_EQUAL_UINT8(0U, result);
}




void test_led_conn_set_should_set_pin_when_state_is_1(void)
{
    HAL_GPIO_WritePin_Expect(TEST_LED_CONN_PORT, TEST_LED_CONN_PIN, GPIO_PIN_SET);

    led_conn_set(1U);
}




void test_led_conn_set_should_reset_pin_when_state_is_0(void)
{
    HAL_GPIO_WritePin_Expect(TEST_LED_CONN_PORT, TEST_LED_CONN_PIN, GPIO_PIN_RESET);

    led_conn_set(0U);
}




void test_led_game_set_should_set_pin_when_state_is_1(void)
{
    HAL_GPIO_WritePin_Expect(TEST_LED_GAME_PORT, TEST_LED_GAME_PIN, GPIO_PIN_SET);

    led_game_set(1U);
}




void test_led_game_set_should_reset_pin_when_state_is_0(void)
{
    HAL_GPIO_WritePin_Expect(TEST_LED_GAME_PORT, TEST_LED_GAME_PIN, GPIO_PIN_RESET);

    led_game_set(0U);
}




void test_led_game_toggle_should_toggle_pin(void)
{
    HAL_GPIO_TogglePin_Expect(TEST_LED_GAME_PORT, TEST_LED_GAME_PIN);

    led_game_toggle();
}
