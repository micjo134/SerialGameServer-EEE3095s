#include "unity.h"
#include "mock_stm32f0xx_hal.h"
#include "hardware.h"

void setUp(void)
{
}

void tearDown(void)
{
}




void test_sensor_read_should_return_1_when_pin_is_high(void)
{
    HAL_GPIO_ReadPin_CMockExpectAndReturn(33, ((GPIO_TypeDef *)0x48000000UL), ((uint16_t)0x0001U), GPIO_PIN_SET);

    uint8_t result = sensor_read();

    UnityAssertEqualUintNumber((UNITY_UINT)(UNITY_UINT8)((1U)), (UNITY_UINT)(UNITY_UINT8)((result)), ( ((void *)0) ), (UNITY_UINT)(37), UNITY_DISPLAY_STYLE_UINT8);
}




void test_sensor_read_should_return_0_when_pin_is_low(void)
{
    HAL_GPIO_ReadPin_CMockExpectAndReturn(45, ((GPIO_TypeDef *)0x48000000UL), ((uint16_t)0x0001U), GPIO_PIN_RESET);

    uint8_t result = sensor_read();

    UnityAssertEqualUintNumber((UNITY_UINT)(UNITY_UINT8)((0U)), (UNITY_UINT)(UNITY_UINT8)((result)), ( ((void *)0) ), (UNITY_UINT)(49), UNITY_DISPLAY_STYLE_UINT8);
}




void test_led_conn_set_should_set_pin_when_state_is_1(void)
{
    HAL_GPIO_WritePin_CMockExpect(57, ((GPIO_TypeDef *)0x48000400UL), ((uint16_t)0x0010U), GPIO_PIN_SET);

    led_conn_set(1U);
}




void test_led_conn_set_should_reset_pin_when_state_is_0(void)
{
    HAL_GPIO_WritePin_CMockExpect(67, ((GPIO_TypeDef *)0x48000400UL), ((uint16_t)0x0010U), GPIO_PIN_RESET);

    led_conn_set(0U);
}




void test_led_game_set_should_set_pin_when_state_is_1(void)
{
    HAL_GPIO_WritePin_CMockExpect(77, ((GPIO_TypeDef *)0x48000400UL), ((uint16_t)0x0020U), GPIO_PIN_SET);

    led_game_set(1U);
}




void test_led_game_set_should_reset_pin_when_state_is_0(void)
{
    HAL_GPIO_WritePin_CMockExpect(87, ((GPIO_TypeDef *)0x48000400UL), ((uint16_t)0x0020U), GPIO_PIN_RESET);

    led_game_set(0U);
}




void test_led_game_toggle_should_toggle_pin(void)
{
    HAL_GPIO_TogglePin_CMockExpect(97, ((GPIO_TypeDef *)0x48000400UL), ((uint16_t)0x0020U));

    led_game_toggle();
}

