#ifndef GEN0334C107_STM32F0XX_HAL_H_CEEDLING
#define GEN0334C107_STM32F0XX_HAL_H_CEEDLING

#include <stdint.h>
#include <stdint.h>

typedef struct GPIO_TypeDef GPIO_TypeDef;

typedef enum
{
  GPIO_PIN_RESET = 0U,
  GPIO_PIN_SET
} GPIO_PinState;
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin);
void HAL_GPIO_WritePin(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin, GPIO_PinState PinState);
void HAL_GPIO_TogglePin(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin);


uint32_t HAL_GetTick(void);
void HAL_Delay(uint32_t Delay);

#endif // GEN0334C107_STM32F0XX_HAL_H_CEEDLING
