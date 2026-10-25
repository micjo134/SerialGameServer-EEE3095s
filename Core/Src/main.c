/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "game.h"
#include <string.h>
#include "stm32f0xx_hal.h"


#define LED_Pin            GPIO_PIN_5
#define LED_GPIO_Port      GPIOB
#define BUTTON_Pin         GPIO_PIN_0
#define BUTTON_GPIO_Port   GPIOA

/* ---------------- Authoritative state ---------------- */
static GameState   s_state;
static GamePlayer  s_turn;
static GameOutcome s_outcome;
static uint8_t     s_board[GAME_BOARD_CELLS];
static uint8_t     s_connected;

/* ---------------- Internal helpers ---------------- */
static void       reset_board(void);
static GamePlayer opponent(GamePlayer p);
static GamePlayer find_winner(const uint8_t *board);
static uint8_t    board_full(const uint8_t *board);



/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  

  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/* ==========================================================================
 * Lifecycle
 * ========================================================================== */
void Game_Init(void)
{
    s_connected = 0U;
    s_state     = GAME_STATE_IDLE;
    s_turn      = GAME_PLAYER_X;
    s_outcome   = GAME_OUTCOME_NONE;
    reset_board();
}

/* ==========================================================================
 * Commands — each one enforces the FSM and the rules.
 * A move is validated in full BEFORE any state is mutated.
 * ========================================================================== */
GameStatus Game_Connect(void)
{
    if (s_connected) return GAME_ERR_BADCMD;

    s_connected = 1U;
    s_state     = GAME_STATE_IDLE;
    s_turn      = GAME_PLAYER_X;
    s_outcome   = GAME_OUTCOME_NONE;
    reset_board();
    return GAME_OK;
}

GameStatus Game_Disconnect(void)
{
    s_connected = 0U;
    s_state     = GAME_STATE_IDLE;
    s_turn      = GAME_PLAYER_X;
    s_outcome   = GAME_OUTCOME_NONE;
    reset_board();
    return GAME_OK;
}

GameStatus Game_New(void)
{
    if (!s_connected)                       return GAME_ERR_BADCMD;
    if (s_state == GAME_STATE_IN_PROGRESS)  return GAME_ERR_BADCMD;

    reset_board();
    s_turn    = GAME_PLAYER_X;      /* X always moves first */
    s_outcome = GAME_OUTCOME_NONE;
    s_state   = GAME_STATE_IN_PROGRESS;
    return GAME_OK;
}

GameStatus Game_Move(uint8_t idx)
{
    /* ---- Validate: no state may change until all checks pass ---- */
    if (!s_connected)                       return GAME_ERR_BADCMD;
    if (s_state != GAME_STATE_IN_PROGRESS)  return GAME_ERR_NOGAME;
    if (idx >= GAME_BOARD_CELLS)            return GAME_ERR_RANGE;
    if (s_board[idx] != GAME_PLAYER_NONE)   return GAME_ERR_OCCUPIED;

    /* ---- add to cell ---- */
    s_board[idx] = (uint8_t)s_turn;

    /* ---- checks ---- */
    GamePlayer w = find_winner(s_board);
    if (w != GAME_PLAYER_NONE) {
        s_outcome = (w == GAME_PLAYER_X) ? GAME_OUTCOME_X : GAME_OUTCOME_O;
        s_state   = GAME_STATE_GAME_OVER;
        s_turn    = GAME_PLAYER_NONE;
        return GAME_OK;
    }
    if (board_full(s_board)) {
        s_outcome = GAME_OUTCOME_DRAW;
        s_state   = GAME_STATE_GAME_OVER;
        s_turn    = GAME_PLAYER_NONE;
        return GAME_OK;
    }

    /* ---- Game continues: hand the turn over ---- */
    s_turn = opponent(s_turn);
    return GAME_OK;
}

GameStatus Game_Resign(void)
{
    if ((!s_connected) || (s_state != GAME_STATE_IN_PROGRESS))
        return GAME_ERR_NOGAME;

    GamePlayer w = opponent(s_turn);        /* the resigning player loses */
    s_outcome = (w == GAME_PLAYER_X) ? GAME_OUTCOME_X : GAME_OUTCOME_O;
    s_state   = GAME_STATE_GAME_OVER;
    s_turn    = GAME_PLAYER_NONE;
    return GAME_OK;
}

GameStatus Game_State(void)
{
    return s_connected ? GAME_OK : GAME_ERR_BADCMD;
}

/* ==========================================================================
 * Queries
 * ========================================================================== */
GameState   Game_GetState(void)   { return s_state; }
GamePlayer  Game_GetTurn(void)    { return s_turn; }
GameOutcome Game_GetOutcome(void) { return s_outcome; }
uint8_t     Game_IsConnected(void){ return s_connected; }

uint8_t Game_GetCell(uint8_t idx)
{
    return (idx < GAME_BOARD_CELLS) ? s_board[idx] : (uint8_t)GAME_PLAYER_NONE;
}

void Game_EncodeBoard(char out[GAME_BOARD_CELLS + 1U])
{
    for (uint8_t i = 0U; i < GAME_BOARD_CELLS; i++) {
        switch ((GamePlayer)s_board[i]) {
        case GAME_PLAYER_X: out[i] = 'X'; break;
        case GAME_PLAYER_O: out[i] = 'O'; break;
        default:            out[i] = '.'; break;
        }
    }
    out[GAME_BOARD_CELLS] = '\0';
}

/* ==========================================================================
 * Internal helpers
 * ========================================================================== */
static void reset_board(void)
{
    for (size_t i = 0; i < GAME_BOARD_CELLS; i++) {
        s_board[i] = GAME_PLAYER_NONE;
    }
}
static GamePlayer opponent(GamePlayer p)
{
    return (p == GAME_PLAYER_X) ? GAME_PLAYER_O : GAME_PLAYER_X;
}

static GamePlayer find_winner(const uint8_t *board)
{
    /* Rows, columns, diagonals */
    static const uint8_t win_conditions[8][3] = {
        {0,1,2}, {3,4,5}, {6,7,8},
        {0,3,6}, {1,4,7}, {2,5,8},
        {0,4,8}, {2,4,6}
    };

    for (uint8_t i = 0U; i < 8U; i++) {
        GamePlayer a = (GamePlayer)board[win_conditions[i][0]];
        if ((a != GAME_PLAYER_NONE) &&
            (a == (GamePlayer)board[win_conditions[i][1]]) &&
            (a == (GamePlayer)board[win_conditions[i][2]]))
            return a;
    }
    return GAME_PLAYER_NONE;
}

static uint8_t board_full(const uint8_t *board)
{
    for (uint8_t i = 0U; i < GAME_BOARD_CELLS; i++) {
        if (board[i] == GAME_PLAYER_NONE) return 0U;
    }
    return 1U;
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* LED */
    HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
    g.Pin = LED_Pin; g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Pull = GPIO_NOPULL; g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_GPIO_Port, &g);

    /* Button */
    g.Pin = BUTTON_Pin; g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_PULLUP; g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(BUTTON_GPIO_Port, &g);
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
