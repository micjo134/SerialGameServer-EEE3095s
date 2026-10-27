/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body — Tic-Tac-Toe Serial Game Server
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
#include "stm32f0xx_hal.h"
#include <string.h>
#include <stdio.h>

/* --------------------------------------------------------------------------
 * Pin definitions
 * -------------------------------------------------------------------------- */
#define LED_Pin          GPIO_PIN_5       /* Game LED: X=ON, O=OFF, OVER=blink */
#define LED_GPIO_Port    GPIOB

#define LED_CONN_Pin     GPIO_PIN_4       /* Connection LED: ON=connected       */
#define LED_CONN_Port    GPIOB

#define UART_TX_Pin      GPIO_PIN_9       /* USART1 TX — AF1                    */
#define UART_RX_Pin      GPIO_PIN_10      /* USART1 RX — AF1                    */
#define UART_GPIO_Port   GPIOA

/* --------------------------------------------------------------------------
 * UART / framing
 * -------------------------------------------------------------------------- */
UART_HandleTypeDef huart1;

#define RX_BUFFER_SIZE   64U             /* max frame length incl. terminator  */
#define FRAME_TIMEOUT_MS 1000U           /* abandon a partial frame after 1 s  */

static uint8_t  rx_byte;                 /* single-byte ISR staging area       */
static char     cmd_buf[RX_BUFFER_SIZE]; /* accumulates chars until '\n'       */
static volatile uint16_t cmd_idx      = 0U;
static volatile uint32_t t_last_byte  = 0U;       /* tick of most-recent received byte  */

/* Deferred dispatch — ISR writes here, main loop reads and clears           */
#define CMD_QUEUE_SIZE   4U
static volatile uint8_t  cmd_queue_head = 0U;
static volatile uint8_t  cmd_queue_tail = 0U;
static volatile uint8_t rx_discard = 0U;   /* 1 = flushing an oversized frame */
static char cmd_queue[CMD_QUEUE_SIZE][RX_BUFFER_SIZE];

/* --------------------------------------------------------------------------
 * TX ring buffer — feeds HAL_UART_Transmit_IT one byte at a time so
 * UART_SendString never blocks the main loop or dispatch().
 * -------------------------------------------------------------------------- */
#define TX_BUFFER_SIZE  256U             /* must be a power of 2               */
static uint8_t  tx_buf[TX_BUFFER_SIZE];
static volatile uint16_t tx_head = 0U;  /* write index (main loop)            */
static volatile uint16_t tx_tail = 0U;  /* read  index (TX ISR)               */
static volatile uint8_t  tx_busy = 0U;  /* 1 = Transmit_IT already in flight  */

/* --------------------------------------------------------------------------
 * Game engine — private state
 * -------------------------------------------------------------------------- */
static GameState   s_state;
static GamePlayer  s_turn;
static GameOutcome s_outcome;
static uint8_t     s_board[GAME_BOARD_CELLS];
static uint8_t     s_connected;

/* --------------------------------------------------------------------------
 * Private function prototypes
 * -------------------------------------------------------------------------- */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_GPIO_Init(void);
static void MX_USART1_UART_Init(void);

/* transport layer */
static void UART_SendString(const char *msg);
static void dispatch(const char *line);

/* game engine helpers */
static void       reset_board(void);
static GamePlayer opponent(GamePlayer p);
static GamePlayer find_winner(const uint8_t *board);
static uint8_t    board_full(const uint8_t *board);

/* ==========================================================================
 * main()
 * ========================================================================== */
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART1_GPIO_Init();
    MX_USART1_UART_Init();

    /* Initialise game engine — waits for CONNECT command over UART */
    Game_Init();

    /* Arm the UART receive interrupt for the first byte */
    HAL_UART_Receive_IT(&huart1, &rx_byte, 1);

    uint32_t t_blink = 0U;

    while (1)
    {
        /* ---- Deferred command dispatch ------------------------------------
         * ISR enqueues complete lines; we drain one per loop iteration.
         * Keeps HAL_UART_Transmit out of interrupt context entirely.
         * ------------------------------------------------------------------ */
        if (cmd_queue_tail != cmd_queue_head) {
            char line[RX_BUFFER_SIZE];
            memcpy(line, cmd_queue[cmd_queue_tail], RX_BUFFER_SIZE);
            cmd_queue_tail = (cmd_queue_tail + 1U) % CMD_QUEUE_SIZE;
            dispatch(line);
        }

        /* ---- Frame timeout ------------------------------------------------
         * If a partial command sits in the buffer for more than 1 s without
         * a newline, discard it so the next good command is not corrupted.
         * ------------------------------------------------------------------ */

        if (rx_discard) {
        /*Without this, a peer that sends an oversized frame and then goes silent leaves rx_discard = 1 forever 
          the next legitimate command gets its body swallowed until its terminator clears the flag*/

            if ((HAL_GetTick() - t_last_byte) >= FRAME_TIMEOUT_MS) {
                rx_discard = 0U;         /* no extra NACK — OVERFLOW already sent */
            }
        }
        else if (cmd_idx > 0U) {
            uint32_t now = HAL_GetTick();
            uint32_t last = t_last_byte; 
            if ((now - last) >= FRAME_TIMEOUT_MS) {
                cmd_idx = 0U;
                rx_discard = 1U;
                uint8_t next = (cmd_queue_head + 1U) % CMD_QUEUE_SIZE;
                if (next != cmd_queue_tail) {
                    memcpy(cmd_queue[cmd_queue_head], "TIMEOUT", 8U);
                    cmd_queue_head = next;
//                dispatch("TIMEOUT");

                }
            }
        }

        /* ---- LED feedback --------------------------------------------------
         *   solid ON   = IN_PROGRESS, X to move
         *   solid OFF  = IN_PROGRESS, O to move
         *   2 Hz blink = GAME_OVER
         *   off        = IDLE
         * ------------------------------------------------------------------ */
        switch (Game_GetState())
        {
        case GAME_STATE_IN_PROGRESS:
            HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin,
                (Game_GetTurn() == GAME_PLAYER_X) ? GPIO_PIN_SET : GPIO_PIN_RESET);
            break;

        case GAME_STATE_GAME_OVER:
        {
            uint32_t now = HAL_GetTick();
            if ((now - t_blink) >= 250U) {
                t_blink = now;
                HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
            }
            break;
        }

        default: /* IDLE */
            HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
            break;
        }

    }
}

/* ==========================================================================
 * UART — transport layer
 * ========================================================================== */


void HAL_UART_ErrorCallback(UART_HandleTypeDef *h) {
    if (h->Instance == USART1) {
        __HAL_UART_CLEAR_OREFLAG(h);
        HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
    }
}
/**
 * @brief  Queue a null-terminated string for non-blocking TX.
 *         Copies bytes into the ring buffer and kicks off Transmit_IT
 *         if the UART is not already sending.
 */
static void UART_SendString(const char *msg)
{
    /* Copy into ring buffer — drop bytes silently if it fills up */
    while (*msg) {
        uint16_t next = (tx_head + 1U) % TX_BUFFER_SIZE;
        if (next == tx_tail) break;          /* buffer full — drop remainder   */
        tx_buf[tx_head] = (uint8_t)*msg++;
        tx_head = next;
    }

    /* Kick off transmission if the UART is idle */
    __disable_irq();
    if (!tx_busy && tx_tail != tx_head) {
        tx_busy = 1U;
        HAL_UART_Transmit_IT(&huart1, &tx_buf[tx_tail], 1U);
    }
    __enable_irq();
}

/**
 * @brief  HAL TX-complete callback — advance the tail and send the next byte,
 *         or mark the UART idle when the buffer is drained.
 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART1) return;

    tx_tail = (tx_tail + 1U) % TX_BUFFER_SIZE;

    if (tx_tail != tx_head) {
        /* More bytes waiting — send the next one */
        HAL_UART_Transmit_IT(&huart1, &tx_buf[tx_tail], 1U);
    } else {
        tx_busy = 0U;   /* buffer drained — UART now idle */
    }
}

/**
 * @brief  Parse one complete line and execute the command.
 *
 * Wire protocol (brief §Reference message protocol):
 *
 *   GUI → STM32        STM32 → GUI
 *   CONNECT          → OK
 *   DISCONNECT       → OK
 *   NEW              → OK:BOARD:......... (empty board)
 *   MOVE:<0-8>       → OK:BOARD:<9chars>   (legal, game ongoing)
 *                    → WIN:X / WIN:O / DRAW (terminal move)
 *                    → NACK:OCCUPIED / NACK:RANGE / NACK:NOTURN
 *   RESIGN           → OK:GAMEOVER
 *   STATE            → BOARD:<9chars>
 *   <unknown>        → NACK:BADCMD
 *   <bad state>      → NACK:NOGAME / NACK:BADCMD
 */
static void dispatch(const char *line)
{
    // GameStatus st;
    GameStatus st = GAME_ERR_BADCMD;
    char board[GAME_BOARD_CELLS + 1U];

    /* ---- Internal sentinels (not valid wire commands) -------------------- */
    if (strcmp(line, "OVERFLOW") == 0 || strcmp(line, "TIMEOUT") == 0) {
        UART_SendString("NACK:BADCMD\r\n");
        return;
    }

    /* ---- CONNECT --------------------------------------------------------- */
    if (strcmp(line, "CONNECT") == 0)
    {
        st = Game_Connect();
        if (st == GAME_OK) { UART_SendString("OK\r\n"); return; }
    }

    /* ---- DISCONNECT ------------------------------------------------------ */
    else if (strcmp(line, "DISCONNECT") == 0)
    {
        st = Game_Disconnect();
        if (st == GAME_OK)
        {
            UART_SendString("OK\r\n");
            return; 
        }
    }

    /* ---- NEW ------------------------------------------------------------- */
    else if (strcmp(line, "NEW") == 0)
    {
        st = Game_New();
        if (st == GAME_OK) {
            Game_EncodeBoard(board);
            char response[GAME_BOARD_CELLS + 12U];
            snprintf(response, sizeof(response), "OK:BOARD:%s\r\n", board);
            UART_SendString(response);
            return;
        }
    }

    /* ---- MOVE:<index> ---------------------------------------------------- */
    else if (strncmp(line, "MOVE:", 5) == 0)
    {

        const char *arg = line + 5;
        if (!(arg[0] >= '0' && arg[0] <= '8') || arg[1] != '\0') {
        UART_SendString("NACK:RANGE\r\n");   /* or NACK:BADCMD */
            return;
        }
        st = Game_Move((uint8_t)(arg[0] - '0'));

        if (st == GAME_OK) {
            Game_EncodeBoard(board);
            char response[GAME_BOARD_CELLS + 20U];
            switch (Game_GetOutcome()) {
            case GAME_OUTCOME_X:
                snprintf(response, sizeof(response), "WIN:X\r\n");
                break;
            case GAME_OUTCOME_O:
                snprintf(response, sizeof(response), "WIN:O\r\n");
                break;
            case GAME_OUTCOME_DRAW:
                snprintf(response, sizeof(response), "DRAW\r\n");
                break;
            default: /* game still in progress */
                snprintf(response, sizeof(response), "OK:BOARD:%s\r\n", board);
                break;
            }
            UART_SendString(response);
            return;
        }
    }

    /* ---- RESIGN ---------------------------------------------------------- */
    else if (strcmp(line, "RESIGN") == 0)
    {
        st = Game_Resign();
        if (st == GAME_OK) { UART_SendString("OK:GAMEOVER\r\n"); return; }
    }

    /* ---- STATE ----------------------------------------------------------- */
    else if (strcmp(line, "STATE") == 0)
    {
        /* Allowed in IN_PROGRESS and GAME_OVER; not in IDLE */
        GameState gs = Game_GetState();
        if (!Game_IsConnected() || gs == GAME_STATE_IDLE) {
            UART_SendString("NACK:NOGAME\r\n");
            return;
        }
        Game_EncodeBoard(board);
        char response[GAME_BOARD_CELLS + 10U];
        snprintf(response, sizeof(response), "BOARD:%s\r\n", board);
        UART_SendString(response);
        return;
    }

    /* ---- Unknown command ------------------------------------------------- */
    else
    {
        UART_SendString("NACK:BADCMD\r\n");
        return;
    }

    /* ---- Map GameStatus → NACK ------------------------------------------ */
    switch (st) {
    case GAME_OK:           /* already handled above with early return */  break;
    case GAME_ERR_RANGE:    UART_SendString("NACK:RANGE\r\n");             break;
    case GAME_ERR_OCCUPIED: UART_SendString("NACK:OCCUPIED\r\n");          break;
    case GAME_ERR_NOTURN:   UART_SendString("NACK:NOTURN\r\n");            break;
    case GAME_ERR_NOGAME:   UART_SendString("NACK:NOGAME\r\n");            break;
    case GAME_ERR_BADCMD:
    case GAME_ERR_STATE:    UART_SendString("NACK:BADCMD\r\n");            break;
    }
}

/**
 * @brief  HAL callback — fires after each received byte on USART1.
 *         Assembles bytes into cmd_buf and calls dispatch() on newline.
 *         Oversized frames are silently dropped; the buffer resets.
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART1) return;

    char c = (char)rx_byte;
    /* Re-arm for the next byte */
    HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
    t_last_byte = HAL_GetTick();   /* update timeout stamp on every byte */


    
    /* ---- 1. Flushing the tail of an oversized frame -------------------- */
    if (rx_discard) {
        if (c == '\n'|| c == '\r') {
            rx_discard = 0U;       /* terminator reached — ready for next frame */
            cmd_idx    = 0U;       /* already 0, but explicit for clarity       */
        }
        /* every other byte is silently dropped */
    }

    else if (c == '\n' || c == '\r') {
        if (cmd_idx > 0U) {
            cmd_buf[cmd_idx] = '\0';
            /* Enqueue into circular buffer — main loop drains it */
            uint8_t next = (cmd_queue_head + 1U) % CMD_QUEUE_SIZE;
            if (next != cmd_queue_tail) {  /* drop if queue full */
                memcpy(cmd_queue[cmd_queue_head], cmd_buf, cmd_idx + 1U);
                cmd_queue_head = next;
            }
            cmd_idx = 0U;
        }
    } else if (cmd_idx < RX_BUFFER_SIZE - 1U) {
        cmd_buf[cmd_idx++] = c;
    } else {
        /* Buffer full — oversized frame: enqueue sentinel */
        cmd_idx = 0U;
        rx_discard = 1U;  
        uint8_t next = (cmd_queue_head + 1U) % CMD_QUEUE_SIZE;
        if (next != cmd_queue_tail) {
            memcpy(cmd_queue[cmd_queue_head], "OVERFLOW", 9U);
            cmd_queue_head = next;
        }
    }

}

/* ==========================================================================
 * Game engine — lifecycle
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
 * Game engine — commands
 * All state mutation happens AFTER full validation.
 * ========================================================================== */
GameStatus Game_Connect(void)
{
    /* If already connected, do a clean reset before reconnecting */
    if (s_connected)  return GAME_ERR_BADCMD;
    s_connected = 1U;
    s_state     = GAME_STATE_IDLE;
    HAL_GPIO_WritePin(LED_CONN_Port, LED_CONN_Pin, GPIO_PIN_SET);
    return GAME_OK;
}

GameStatus Game_Disconnect(void)
{
    if (!s_connected) return GAME_ERR_BADCMD;   /* not connected */

    s_connected = 0U;
    s_state     = GAME_STATE_IDLE;
    s_turn      = GAME_PLAYER_X;
    s_outcome   = GAME_OUTCOME_NONE;
    reset_board();
    HAL_GPIO_WritePin(LED_CONN_Port, LED_CONN_Pin, GPIO_PIN_RESET); /* LED OFF */
    return GAME_OK;
}

GameStatus Game_New(void)
{
    if (!s_connected)                      return GAME_ERR_BADCMD;
    if (s_state == GAME_STATE_IN_PROGRESS) return GAME_ERR_BADCMD;
    reset_board();
    s_turn    = GAME_PLAYER_X;
    s_outcome = GAME_OUTCOME_NONE;
    s_state   = GAME_STATE_IN_PROGRESS;
    return GAME_OK;
}

GameStatus Game_Move(uint8_t idx)
{
    if (!s_connected)                      return GAME_ERR_BADCMD;
    if (s_state != GAME_STATE_IN_PROGRESS) return GAME_ERR_NOGAME;
    if (idx >= GAME_BOARD_CELLS)           return GAME_ERR_RANGE;
    if (s_board[idx] != GAME_PLAYER_NONE)  return GAME_ERR_OCCUPIED;

    s_board[idx] = (uint8_t)s_turn;

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

    s_turn = opponent(s_turn);
    return GAME_OK;
}

GameStatus Game_Resign(void)
{
    if (!s_connected || s_state != GAME_STATE_IN_PROGRESS)
        return GAME_ERR_NOGAME;
    GamePlayer w = opponent(s_turn);
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
 * Game engine — queries
 * ========================================================================== */
GameState   Game_GetState(void)    { return s_state; }
GamePlayer  Game_GetTurn(void)     { return s_turn; }
GameOutcome Game_GetOutcome(void)  { return s_outcome; }
uint8_t     Game_IsConnected(void) { return s_connected; }

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
 * Game engine — internal helpers
 * ========================================================================== */
static void reset_board(void)
{
    for (uint8_t i = 0U; i < GAME_BOARD_CELLS; i++) {
        s_board[i] = GAME_PLAYER_NONE;
    }
}

static GamePlayer opponent(GamePlayer p)
{
    return (p == GAME_PLAYER_X) ? GAME_PLAYER_O : GAME_PLAYER_X;
}

static GamePlayer find_winner(const uint8_t *board)
{
    static const uint8_t lines[8][3] = {
        {0,1,2}, {3,4,5}, {6,7,8},   /* rows      */
        {0,3,6}, {1,4,7}, {2,5,8},   /* columns   */
        {0,4,8}, {2,4,6}              /* diagonals */
    };
    for (uint8_t i = 0U; i < 8U; i++) {
        GamePlayer a = (GamePlayer)board[lines[i][0]];
        if (a != GAME_PLAYER_NONE &&
            a == (GamePlayer)board[lines[i][1]] &&
            a == (GamePlayer)board[lines[i][2]])
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

/* ==========================================================================
 * Hardware initialisation
 * ========================================================================== */

/**
 * @brief  Configure LEDs (PB4, PB5).
 */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* Game LED — PB5, push-pull output, starts OFF */
    HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
    g.Pin   = LED_Pin;
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_GPIO_Port, &g);

    /* Connection LED — PB4, push-pull output, starts OFF */
    HAL_GPIO_WritePin(LED_CONN_Port, LED_CONN_Pin, GPIO_PIN_RESET);
    g.Pin   = LED_CONN_Pin;
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_CONN_Port, &g);
}

/**
 * @brief  Configure PA9 (TX) and PA10 (RX) as USART1 alternate-function pins.
 */
static void MX_USART1_GPIO_Init(void)
{
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();

    g.Pin       = UART_TX_Pin | UART_RX_Pin;
    g.Mode      = GPIO_MODE_AF_PP;
    g.Pull      = GPIO_NOPULL;
    g.Speed     = GPIO_SPEED_FREQ_HIGH;
    g.Alternate = GPIO_AF1_USART1;
    HAL_GPIO_Init(UART_GPIO_Port, &g);
}

/**
 * @brief  Initialise USART1: 115200 8N1, RX interrupt enabled.
 */
static void MX_USART1_UART_Init(void)
{
    __HAL_RCC_USART1_CLK_ENABLE();

    huart1.Instance          = USART1;
    huart1.Init.BaudRate     = 115200;
    huart1.Init.WordLength   = UART_WORDLENGTH_8B;
    huart1.Init.StopBits     = UART_STOPBITS_1;
    huart1.Init.Parity       = UART_PARITY_NONE;
    huart1.Init.Mode         = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

    if (HAL_UART_Init(&huart1) != HAL_OK) {
        Error_Handler();
    }

    HAL_NVIC_SetPriority(USART1_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
}

/**
 * @brief  System clock: HSI → PLL ×6 = 48 MHz SYSCLK, USART1 clocked from PCLK1.
 */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef       RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef       RCC_ClkInitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInit     = {0};

    RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState            = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource       = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLMUL          = RCC_PLL_MUL6;   /* 8 MHz × 6 = 48 MHz */
    RCC_OscInitStruct.PLL.PREDIV          = RCC_PREDIV_DIV1;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                     | RCC_CLOCKTYPE_PCLK1;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;  /* use PLL */
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;          /* HCLK  = 48 MHz */
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;            /* PCLK1 = 48 MHz */
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK) {
        Error_Handler();
    }

    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART1;
    PeriphClkInit.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK1;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) {
        Error_Handler();
    }
}

/* ==========================================================================
 * Error / assert
 * ========================================================================== */
void Error_Handler(void)
{
    __disable_irq();
    while (1) { }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file; (void)line;
}
#endif /* USE_FULL_ASSERT */
