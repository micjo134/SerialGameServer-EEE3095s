#define STM32F0XX_HAL_H






typedef struct GPIO_TypeDef GPIO_TypeDef;

typedef enum
{
  GPIO_PIN_RESET = 0U,
  GPIO_PIN_SET
} GPIO_PinState;

#define GPIO_PIN_0 ((uint16_t)0x0001U)
#define GPIO_PIN_1 ((uint16_t)0x0002U)
#define GPIO_PIN_2 ((uint16_t)0x0004U)
#define GPIO_PIN_3 ((uint16_t)0x0008U)
#define GPIO_PIN_4 ((uint16_t)0x0010U)
#define GPIO_PIN_5 ((uint16_t)0x0020U)
#define GPIO_PIN_6 ((uint16_t)0x0040U)
#define GPIO_PIN_7 ((uint16_t)0x0080U)
#define GPIO_PIN_8 ((uint16_t)0x0100U)
#define GPIO_PIN_9 ((uint16_t)0x0200U)
#define GPIO_PIN_10 ((uint16_t)0x0400U)
#define GPIO_PIN_11 ((uint16_t)0x0800U)
#define GPIO_PIN_12 ((uint16_t)0x1000U)
#define GPIO_PIN_13 ((uint16_t)0x2000U)
#define GPIO_PIN_14 ((uint16_t)0x4000U)
#define GPIO_PIN_15 ((uint16_t)0x8000U)

#define GPIOA ((GPIO_TypeDef *)0x48000000UL)
#define GPIOB ((GPIO_TypeDef *)0x48000400UL)
#define GPIOC ((GPIO_TypeDef *)0x48000800UL)


GPIO_PinState     HAL_GPIO_ReadPin(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin);
void              HAL_GPIO_WritePin(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin, GPIO_PinState PinState);
void              HAL_GPIO_TogglePin(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin);


uint32_t          HAL_GetTick(void);
void              HAL_Delay(uint32_t Delay);





