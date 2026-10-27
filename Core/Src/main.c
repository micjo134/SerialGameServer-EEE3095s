/**
 ******************************************************************************
 * @file    main.c
 * @brief   Tic-Tac-Toe Serial Game Server — STM32F051C8Tx
 *
 * Architecture
 * ------------
 *   Application layer  : main loop — LED feedback, button edge-detection
 *   Transport layer    : UART framing, TX ring-buffer, RX command queue
 *   Game engine        : pure FSM logic, no I/O dependencies
 *   HAL / BSP          : clock, GPIO, USART1 initialisation
 *
 * UART framing
 * ------------
 *   - All messages are ASCII, newline-terminated (\r or \n)
 *   - Maximum frame length : RX_BUFFER_SIZE (64) bytes
 *   - Oversized or timed-out frames return NACK:BADCMD
 *
 * Copyright (c) 2026 STMicroelectronics. All rights reserved.
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "game.h"
#include "stm32f0xx_hal.h"
#include <string.h>
#include <stdio.h>

/* ===========================================================================
 * Pin definitions
 * =========================================================================== */
#define LED_Pin           GPIO_PIN_5    /* Game LED  : X=ON | O=OFF | OVER=blink */
#define LED_GPIO_Port     GPIOB

#define LED_CONN_Pin      GPIO_PIN_4    /* Conn LED  : ON=connected               */
#define LED_CONN_Port     GPIOB

#define BUTTON_Pin        GPIO_PIN_0    /* User button : active-low, pull-up      */
#define BUTTON_GPIO_Port  GPIOA

#define UART_TX_Pin       GPIO_PIN_9    /* USART1 TX : AF1                        */
#define UART_RX_Pin       GPIO_PIN_10   /* USART1 RX : AF1                        */
#define UART_GPIO_Port    GPIOA

/* ===========================================================================
 * UART — RX framing
 * =========================================================================== */
UART_HandleTypeDef huart1;

#define RX_BUFFER_SIZE    64U           /* max frame length incl. terminator      */
#define FRAME_TIMEOUT_MS  1000U         /* ms before a partial frame is discarded */

static uint8_t           rx_byte;                      /* single-byte ISR staging */
static char              cmd_buf[RX_BUFFER_SIZE];      /* builds one frame        */
static volatile uint16_t cmd_idx     = 0U;
static volatile uint32_t t_last_byte = 0U;             /* tick of last RX byte    */
static volatile uint8_t  rx_discard  = 0U;             /* 1 = draining oversize frame */

/* RX command queue — ISR enqueues, main loop dequeues                        */
#define CMD_QUEUE_SIZE    4U
static volatile uint8_t  cmd_queue_head = 0U;
static volatile uint8_t  cmd_queue_tail = 0U;
static char              cmd_queue[CMD_QUEUE_SIZE][RX_BUFFER_SIZE];

/* ===========================================================================
 * UART — TX ring buffer
 *
 * UART_SendString() copies into this buffer and returns immediately.
 * HAL_UART_TxCpltCallback() drains it one byte at a time via Transmit_IT,
 * so the main loop and dispatch() are never blocked waiting for TX.
 * =========================================================================== */
#define TX_BUFFER_SIZE    256U          /* power of 2                             */
static uint8_t           tx_buf[TX_BUFFER_SIZE];
static volatile uint16_t tx_head = 0U; /* write index — main loop                */
static volatile uint16_t tx_tail = 0U; /* read  index — TX ISR                   */
static volatile uint8_t  tx_busy = 0U; /* 1 = Transmit_IT already in flight      */

/* ===========================================================================
 * Game engine — private state
 * =========================================================================== */
static GameState   s_state;
static GamePlayer  s_turn;
static GameOutcome s_outcome;
static uint8_t     s_board[GAME_BOARD_CELLS];
static uint8_t     s_connected;

/* ===========================================================================
 * Private function prototypes
 * =========================================================================== */
void        SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_GPIO_Init(void);
static void MX_USART1_UART_Init(void);

static void UART_SendString(const char *msg);
static void dispatch(const char *line);

static void       reset_board(void);
static GamePlayer opponent(GamePlayer p);
static GamePlayer find_winner(const uint8_t *board);
static uint8_t    board_full(const uint8_t *board);

/* ===========================================================================
 * main()
 * =========================================================================== */

/**
 * @brief  Application entry point.
 *
 * Initialises all peripherals and the game engine, then enters the
 * superloop which handles (in order each iteration):
 *   1. Deferred UART command dispatch
 *   2. Partial-frame timeout
 *   3. Game-state LED update
 *   4. Button edge detection
 *
 * @retval int  Never returns.
 */
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART1_GPIO_Init();
    MX_USART1_UART_Init();

    Game_Init();
    HAL_UART_Receive_IT(&huart1, &rx_byte, 1);

    uint32_t t_blink  = 0U;
    uint32_t t_btn    = 0U;
    uint8_t  btn_prev = 0U;

    while (1)
    {
        /* ------------------------------------------------------------------
         * 1. Deferred command dispatch
         *    The RX ISR enqueues complete lines; we drain one per iteration
         *    so that HAL_UART_Transmit_IT is never called from ISR context.
         * ------------------------------------------------------------------ */
        if (cmd_queue_tail != cmd_queue_head)
        {
            char line[RX_BUFFER_SIZE];
            memcpy(line, cmd_queue[cmd_queue_tail], RX_BUFFER_SIZE);
            cmd_queue_tail = (cmd_queue_tail + 1U) % CMD_QUEUE_SIZE;
            dispatch(line);
        }

        /* ------------------------------------------------------------------
         * 2. Partial-frame timeout
         *    If rx_discard is active, wait for the peer to go silent before
         *    clearing it — prevents the next frame being swallowed.
         *    Otherwise, expire a partial frame that never received a newline.
         * ------------------------------------------------------------------ */
        if (rx_discard)
        {
            if ((HAL_GetTick() - t_last_byte) >= FRAME_TIMEOUT_MS)
                rx_discard = 0U;
        }
        else if (cmd_idx > 0U)
        {
            if ((HAL_GetTick() - t_last_byte) >= FRAME_TIMEOUT_MS)
            {
                cmd_idx = 0U;
                uint8_t next = (cmd_queue_head + 1U) % CMD_QUEUE_SIZE;
                if (next != cmd_queue_tail)
                {
                    memcpy(cmd_queue[cmd_queue_head], "TIMEOUT", 8U);
                    cmd_queue_head = next;
                }
            }
        }

        /* ------------------------------------------------------------------
         * 3. Game-state LED
         *    IN_PROGRESS : solid ON  = X to move | solid OFF = O to move
         *    GAME_OVER   : 2 Hz blink
         *    IDLE        : off
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
            if ((now - t_blink) >= 250U)
            {
                t_blink = now;
                HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
            }
            break;
        }

        default:
            HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
            break;
        }

        /* ------------------------------------------------------------------
         * 4. Button — edge detection (no blocking wait)
         *    Detects the released→pressed transition so a held button fires
         *    only once.  200 ms debounce via timestamp comparison.
         *    Press during IN_PROGRESS : resign + new game
         *    Press during IDLE/OVER   : new game
         * ------------------------------------------------------------------ */
        {
            uint8_t btn_now = (HAL_GPIO_ReadPin(BUTTON_GPIO_Port, BUTTON_Pin)
                               == GPIO_PIN_RESET) ? 1U : 0U;

            if (btn_now && !btn_prev)
            {
                uint32_t now = HAL_GetTick();
                if ((now - t_btn) >= 200U)
                {
                    t_btn = now;
                    if (Game_IsConnected())
                    {
                        if (Game_GetState() == GAME_STATE_IN_PROGRESS)
                            (void)Game_Resign();

                        if (Game_New() == GAME_OK)
                        {
                            char board[GAME_BOARD_CELLS + 1U];
                            Game_EncodeBoard(board);
                            char response[GAME_BOARD_CELLS + 12U];
                            snprintf(response, sizeof(response),
                                     "OK:BOARD:%s\r\n", board);
                            UART_SendString(response);
                        }
                    }
                }
            }
            btn_prev = btn_now;
        }
    }
}

/* ===========================================================================
 * UART — transport layer
 * =========================================================================== */

/**
 * @brief  Queue a null-terminated string for non-blocking TX.
 *
 * Copies bytes into the TX ring buffer and kicks off HAL_UART_Transmit_IT
 * if the UART is idle.  Returns immediately — never blocks.
 * Bytes are silently dropped if the ring buffer is full.
 *
 * @param  msg  Null-terminated ASCII string to transmit.
 */
static void UART_SendString(const char *msg)
{
    while (*msg)
    {
        uint16_t next = (tx_head + 1U) % TX_BUFFER_SIZE;
        if (next == tx_tail) break;             /* buffer full — drop remainder */
        tx_buf[tx_head] = (uint8_t)*msg++;
        tx_head = next;
    }

    __disable_irq();
    if (!tx_busy && tx_tail != tx_head)
    {
        tx_busy = 1U;
        HAL_UART_Transmit_IT(&huart1, &tx_buf[tx_tail], 1U);
    }
    __enable_irq();
}

/**
 * @brief  HAL TX-complete callback.
 *
 * Advances the TX ring-buffer tail and fires the next Transmit_IT, or
 * clears tx_busy when the buffer is fully drained.
 * Runs entirely in interrupt context — no blocking calls.
 *
 * @param  huart  UART handle that completed transmission.
 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART1) return;

    tx_tail = (tx_tail + 1U) % TX_BUFFER_SIZE;

    if (tx_tail != tx_head)
        HAL_UART_Transmit_IT(&huart1, &tx_buf[tx_tail], 1U);
    else
        tx_busy = 0U;
}

/**
 * @brief  Parse one complete ASCII line and execute the game command.
 *
 * Called from the main loop (never from ISR context) after a complete
 * newline-terminated frame has been dequeued from the RX command queue.
 *
 * Wire protocol:
 *
 *   GUI → STM32          STM32 → GUI
 *   CONNECT            → OK
 *   DISCONNECT         → OK
 *   NEW                → OK:BOARD:......... (empty board)
 *   MOVE:<0-8>         → OK:BOARD:<9chars>  (legal move, game ongoing)
 *                      → WIN:X / WIN:O / DRAW  (terminal move)
 *                      → NACK:RANGE / NACK:OCCUPIED / NACK:NOTURN
 *   RESIGN             → OK:GAMEOVER
 *   STATE              → BOARD:<9chars>
 *   OVERFLOW / TIMEOUT → NACK:BADCMD  (internal sentinels)
 *   <unknown>          → NACK:BADCMD
 *   <wrong FSM state>  → NACK:NOGAME / NACK:BADCMD
 *
 * @param  line  Null-terminated command string (no trailing \r\n).
 */
static void dispatch(const char *line)
{
    GameStatus st = GAME_ERR_BADCMD;
    char board[GAME_BOARD_CELLS + 1U];

    /* Internal sentinels set by the framing layer */
    if (strcmp(line, "OVERFLOW") == 0 || strcmp(line, "TIMEOUT") == 0)
    {
        UART_SendString("NACK:BADCMD\r\n");
        return;
    }

    if (strcmp(line, "CONNECT") == 0)
    {
        st = Game_Connect();
        if (st == GAME_OK) { UART_SendString("OK\r\n"); return; }
    }
    else if (strcmp(line, "DISCONNECT") == 0)
    {
        st = Game_Disconnect();
        if (st == GAME_OK) { UART_SendString("OK\r\n"); return; }
    }
    else if (strcmp(line, "NEW") == 0)
    {
        st = Game_New();
        if (st == GAME_OK)
        {
            Game_EncodeBoard(board);
            char response[GAME_BOARD_CELLS + 12U];
            snprintf(response, sizeof(response), "OK:BOARD:%s\r\n", board);
            UART_SendString(response);
            return;
        }
    }
    else if (strncmp(line, "MOVE:", 5) == 0)
    {
        const char *arg = line + 5;
        if (!(arg[0] >= '0' && arg[0] <= '8') || arg[1] != '\0')
        {
            UART_SendString("NACK:RANGE\r\n");
            return;
        }
        st = Game_Move((uint8_t)(arg[0] - '0'));
        if (st == GAME_OK)
        {
            Game_EncodeBoard(board);
            char response[GAME_BOARD_CELLS + 20U];
            switch (Game_GetOutcome())
            {
            case GAME_OUTCOME_X:    snprintf(response, sizeof(response), "WIN:X\r\n");               break;
            case GAME_OUTCOME_O:    snprintf(response, sizeof(response), "WIN:O\r\n");               break;
            case GAME_OUTCOME_DRAW: snprintf(response, sizeof(response), "DRAW\r\n");                break;
            default:                snprintf(response, sizeof(response), "OK:BOARD:%s\r\n", board);  break;
            }
            UART_SendString(response);
            return;
        }
    }
    else if (strcmp(line, "RESIGN") == 0)
    {
        st = Game_Resign();
        if (st == GAME_OK) { UART_SendString("OK:GAMEOVER\r\n"); return; }
    }
    else if (strcmp(line, "STATE") == 0)
    {
        if (!Game_IsConnected() || Game_GetState() == GAME_STATE_IDLE)
        {
            UART_SendString("NACK:NOGAME\r\n");
            return;
        }
        Game_EncodeBoard(board);
        char response[GAME_BOARD_CELLS + 10U];
        snprintf(response, sizeof(response), "BOARD:%s\r\n", board);
        UART_SendString(response);
        return;
    }
    else
    {
        UART_SendString("NACK:BADCMD\r\n");
        return;
    }

    /* Map any unhandled GameStatus to a NACK */
    switch (st)
    {
    case GAME_OK:                                                            break;
    case GAME_ERR_RANGE:    UART_SendString("NACK:RANGE\r\n");              break;
    case GAME_ERR_OCCUPIED: UART_SendString("NACK:OCCUPIED\r\n");           break;
    case GAME_ERR_NOTURN:   UART_SendString("NACK:NOTURN\r\n");             break;
    case GAME_ERR_NOGAME:   UART_SendString("NACK:NOGAME\r\n");             break;
    case GAME_ERR_BADCMD:
    case GAME_ERR_STATE:    UART_SendString("NACK:BADCMD\r\n");             break;
    }
}

/**
 * @brief  HAL RX-complete callback — byte-by-byte frame assembler.
 *
 * Called from ISR context after every received byte on USART1.
 * Assembles bytes into cmd_buf until a newline arrives, then copies
 * the complete frame into the circular command queue for the main loop.
 *
 * Oversized frames (> RX_BUFFER_SIZE - 1): the OVERFLOW sentinel is
 * enqueued and rx_discard is set so remaining bytes are dropped until
 * the next newline clears the flag.
 *
 * @param  huart  UART handle that received a byte.
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART1) return;

    char c = (char)rx_byte;
    HAL_UART_Receive_IT(&huart1, &rx_byte, 1);  /* re-arm immediately */
    t_last_byte = HAL_GetTick();

    if (rx_discard)
    {
        /* Draining the tail of an oversized frame — wait for its newline */
        if (c == '\n' || c == '\r')
        {
            rx_discard = 0U;
            cmd_idx    = 0U;
        }
        return;
    }

    if (c == '\n' || c == '\r')
    {
        if (cmd_idx > 0U)
        {
            cmd_buf[cmd_idx] = '\0';
            uint8_t next = (cmd_queue_head + 1U) % CMD_QUEUE_SIZE;
            if (next != cmd_queue_tail)             /* enqueue if space available */
            {
                memcpy(cmd_queue[cmd_queue_head], cmd_buf, cmd_idx + 1U);
                cmd_queue_head = next;
            }
            cmd_idx = 0U;
        }
    }
    else if (cmd_idx < RX_BUFFER_SIZE - 1U)
    {
        cmd_buf[cmd_idx++] = c;
    }
    else
    {
        /* Frame exceeded RX_BUFFER_SIZE — discard and notify */
        cmd_idx    = 0U;
        rx_discard = 1U;
        uint8_t next = (cmd_queue_head + 1U) % CMD_QUEUE_SIZE;
        if (next != cmd_queue_tail)
        {
            memcpy(cmd_queue[cmd_queue_head], "OVERFLOW", 9U);
            cmd_queue_head = next;
        }
    }
}

/**
 * @brief  HAL UART error callback.
 *
 * Clears the overrun-error flag and re-arms RX so a hardware framing
 * glitch cannot permanently disable the receiver.
 *
 * @param  h  UART handle that raised the error.
 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *h)
{
    if (h->Instance == USART1)
    {
        __HAL_UART_CLEAR_OREFLAG(h);
        HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
    }
}

/* ===========================================================================
 * Game engine — lifecycle
 * =========================================================================== */

/**
 * @brief  Reset all game state to power-on defaults.
 *
 * Sets the engine to disconnected/IDLE with a clear board.
 * Must be called once at startup before any other Game_* function.
 */
void Game_Init(void)
{
    s_connected = 0U;
    s_state     = GAME_STATE_IDLE;
    s_turn      = GAME_PLAYER_X;
    s_outcome   = GAME_OUTCOME_NONE;
    reset_board();
}

/* ===========================================================================
 * Game engine — commands
 *
 * All state mutation happens AFTER full validation so that a rejected
 * request can never corrupt the authoritative game state.
 * =========================================================================== */

/**
 * @brief  Establish a client session.
 *
 * Transitions the engine from disconnected to IDLE and lights the
 * connection LED.  Rejected if a session is already active.
 *
 * @retval GAME_OK       Session established.
 * @retval GAME_ERR_BADCMD Already connected.
 */
GameStatus Game_Connect(void)
{
    if (s_connected) return GAME_ERR_BADCMD;

    s_connected = 1U;
    s_state     = GAME_STATE_IDLE;
    s_turn      = GAME_PLAYER_X;
    s_outcome   = GAME_OUTCOME_NONE;
    reset_board();
    HAL_GPIO_WritePin(LED_CONN_Port, LED_CONN_Pin, GPIO_PIN_SET);
    return GAME_OK;
}

/**
 * @brief  Terminate the client session.
 *
 * Resets all game state and extinguishes the connection LED.
 * Valid from any FSM state while connected.
 *
 * @retval GAME_OK       Session ended.
 * @retval GAME_ERR_BADCMD Not connected.
 */
GameStatus Game_Disconnect(void)
{
    if (!s_connected) return GAME_ERR_BADCMD;

    s_connected = 0U;
    s_state     = GAME_STATE_IDLE;
    s_turn      = GAME_PLAYER_X;
    s_outcome   = GAME_OUTCOME_NONE;
    reset_board();
    HAL_GPIO_WritePin(LED_CONN_Port, LED_CONN_Pin, GPIO_PIN_RESET);
    return GAME_OK;
}

/**
 * @brief  Start a new game.
 *
 * Clears the board, resets turn to X and enters IN_PROGRESS.
 * Rejected if a game is already in progress (client must RESIGN first).
 *
 * @retval GAME_OK       Game started.
 * @retval GAME_ERR_BADCMD Not connected, or game already in progress.
 */
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

/**
 * @brief  Place the current player's piece on cell @p idx.
 *
 * Full pre-validation: connection, FSM state, index range and occupancy
 * are all checked before any state is mutated.  After a valid move,
 * win/draw is detected and the FSM advances to GAME_OVER if terminal.
 *
 * @param  idx  Board cell index 0–8 (row-major, 0 = top-left).
 * @retval GAME_OK          Move accepted (check Game_GetOutcome() for result).
 * @retval GAME_ERR_BADCMD  Not connected.
 * @retval GAME_ERR_NOGAME  No game in progress.
 * @retval GAME_ERR_RANGE   idx >= GAME_BOARD_CELLS.
 * @retval GAME_ERR_OCCUPIED Cell already taken.
 */
GameStatus Game_Move(uint8_t idx)
{
    if (!s_connected)                      return GAME_ERR_BADCMD;
    if (s_state != GAME_STATE_IN_PROGRESS) return GAME_ERR_NOGAME;
    if (idx >= GAME_BOARD_CELLS)           return GAME_ERR_RANGE;
    if (s_board[idx] != GAME_PLAYER_NONE)  return GAME_ERR_OCCUPIED;

    s_board[idx] = (uint8_t)s_turn;

    GamePlayer w = find_winner(s_board);
    if (w != GAME_PLAYER_NONE)
    {
        s_outcome = (w == GAME_PLAYER_X) ? GAME_OUTCOME_X : GAME_OUTCOME_O;
        s_state   = GAME_STATE_GAME_OVER;
        s_turn    = GAME_PLAYER_NONE;
        return GAME_OK;
    }
    if (board_full(s_board))
    {
        s_outcome = GAME_OUTCOME_DRAW;
        s_state   = GAME_STATE_GAME_OVER;
        s_turn    = GAME_PLAYER_NONE;
        return GAME_OK;
    }

    s_turn = opponent(s_turn);
    return GAME_OK;
}

/**
 * @brief  Forfeit the current game.
 *
 * The resigning player loses; the opponent is recorded as the winner.
 * Only valid while a game is IN_PROGRESS.
 *
 * @retval GAME_OK         Resignation accepted; FSM → GAME_OVER.
 * @retval GAME_ERR_NOGAME No game in progress, or not connected.
 */
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

/* ===========================================================================
 * Game engine — queries
 * =========================================================================== */

/** @brief Return the current FSM state. */
GameState   Game_GetState(void)    { return s_state; }

/** @brief Return whose turn it is (GAME_PLAYER_NONE when game is over). */
GamePlayer  Game_GetTurn(void)     { return s_turn; }

/** @brief Return the game outcome (GAME_OUTCOME_NONE while in progress). */
GameOutcome Game_GetOutcome(void)  { return s_outcome; }

/** @brief Return 1 if a client session is active, 0 otherwise. */
uint8_t     Game_IsConnected(void) { return s_connected; }

/**
 * @brief  Encode the board as a 9-character null-terminated string.
 *
 * Cell encoding: 'X' = player X | 'O' = player O | '.' = empty.
 * Cells are ordered row-major: index 0 = top-left, 8 = bottom-right.
 *
 * @param  out  Caller-supplied buffer of at least GAME_BOARD_CELLS + 1 bytes.
 */
void Game_EncodeBoard(char out[GAME_BOARD_CELLS + 1U])
{
    for (uint8_t i = 0U; i < GAME_BOARD_CELLS; i++)
    {
        switch ((GamePlayer)s_board[i])
        {
        case GAME_PLAYER_X: out[i] = 'X'; break;
        case GAME_PLAYER_O: out[i] = 'O'; break;
        default:            out[i] = '.'; break;
        }
    }
    out[GAME_BOARD_CELLS] = '\0';
}

/* ===========================================================================
 * Game engine — internal helpers
 * =========================================================================== */

/**
 * @brief  Clear all cells to GAME_PLAYER_NONE.
 */
static void reset_board(void)
{
    for (uint8_t i = 0U; i < GAME_BOARD_CELLS; i++)
        s_board[i] = GAME_PLAYER_NONE;
}

/**
 * @brief  Return the opposing player.
 *
 * @param  p  GAME_PLAYER_X or GAME_PLAYER_O.
 * @retval    The other player.
 */
static GamePlayer opponent(GamePlayer p)
{
    return (p == GAME_PLAYER_X) ? GAME_PLAYER_O : GAME_PLAYER_X;
}

/**
 * @brief  Scan all 8 win lines and return the winning player, or NONE.
 *
 * Checks 3 rows, 3 columns and 2 diagonals against a static lookup table.
 *
 * @param  board  9-element board array (GAME_PLAYER_* values).
 * @retval        Winning player, or GAME_PLAYER_NONE if no winner yet.
 */
static GamePlayer find_winner(const uint8_t *board)
{
    static const uint8_t lines[8][3] = {
        {0,1,2}, {3,4,5}, {6,7,8},  /* rows      */
        {0,3,6}, {1,4,7}, {2,5,8},  /* columns   */
        {0,4,8}, {2,4,6}             /* diagonals */
    };

    for (uint8_t i = 0U; i < 8U; i++)
    {
        GamePlayer a = (GamePlayer)board[lines[i][0]];
        if (a != GAME_PLAYER_NONE &&
            a == (GamePlayer)board[lines[i][1]] &&
            a == (GamePlayer)board[lines[i][2]])
            return a;
    }
    return GAME_PLAYER_NONE;
}

/**
 * @brief  Return 1 if every cell is occupied, 0 otherwise.
 *
 * @param  board  9-element board array.
 * @retval 1  Board is full (draw if no winner).
 * @retval 0  At least one empty cell remains.
 */
static uint8_t board_full(const uint8_t *board)
{
    for (uint8_t i = 0U; i < GAME_BOARD_CELLS; i++)
        if (board[i] == GAME_PLAYER_NONE) return 0U;
    return 1U;
}

/* ===========================================================================
 * Hardware initialisation
 * =========================================================================== */

/**
 * @brief  Configure GPIO pins for LEDs and the user button.
 *
 * PB5 — Game LED      : push-pull output, starts OFF
 * PB4 — Connection LED: push-pull output, starts OFF
 * PA0 — User button   : input with internal pull-up (active-low)
 */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    HAL_GPIO_WritePin(LED_GPIO_Port,  LED_Pin,      GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_CONN_Port,  LED_CONN_Pin, GPIO_PIN_RESET);

    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;

    g.Pin = LED_Pin;      HAL_GPIO_Init(LED_GPIO_Port,  &g);
    g.Pin = LED_CONN_Pin; HAL_GPIO_Init(LED_CONN_Port,  &g);

    g.Pin  = BUTTON_Pin;
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(BUTTON_GPIO_Port, &g);
}

/**
 * @brief  Configure PA9 (TX) and PA10 (RX) as USART1 alternate-function pins.
 *
 * Both pins use AF1, push-pull, high-speed, no pull resistor.
 * GPIOA clock is already enabled by MX_GPIO_Init().
 */
static void MX_USART1_GPIO_Init(void)
{
    GPIO_InitTypeDef g = {0};

    g.Pin       = UART_TX_Pin | UART_RX_Pin;
    g.Mode      = GPIO_MODE_AF_PP;
    g.Pull      = GPIO_NOPULL;
    g.Speed     = GPIO_SPEED_FREQ_HIGH;
    g.Alternate = GPIO_AF1_USART1;
    HAL_GPIO_Init(UART_GPIO_Port, &g);
}

/**
 * @brief  Initialise USART1 at 115200 8N1 with RX interrupt enabled.
 *
 * USART1 is clocked from PCLK1 (48 MHz after PLL config), so the
 * BRR register is computed correctly by HAL_UART_Init().
 * NVIC priority is set to 1 (below SysTick at 0).
 */
static void MX_USART1_UART_Init(void)
{
    __HAL_RCC_USART1_CLK_ENABLE();

    huart1.Instance                    = USART1;
    huart1.Init.BaudRate               = 115200;
    huart1.Init.WordLength             = UART_WORDLENGTH_8B;
    huart1.Init.StopBits               = UART_STOPBITS_1;
    huart1.Init.Parity                 = UART_PARITY_NONE;
    huart1.Init.Mode                   = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl              = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling           = UART_OVERSAMPLING_16;
    huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

    if (HAL_UART_Init(&huart1) != HAL_OK) Error_Handler();

    HAL_NVIC_SetPriority(USART1_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
}

/**
 * @brief  Configure the system clock to 48 MHz using the internal HSI PLL.
 *
 * Clock tree:
 *   HSI (8 MHz) → PLL ×6 → SYSCLK 48 MHz
 *   AHB  divider = 1  →  HCLK  48 MHz
 *   APB1 divider = 1  →  PCLK1 48 MHz  (feeds USART1)
 *
 * Flash latency is set to 1 wait-state as required above 24 MHz.
 */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef       osc  = {0};
    RCC_ClkInitTypeDef       clk  = {0};
    RCC_PeriphCLKInitTypeDef pclk = {0};

    osc.OscillatorType      = RCC_OSCILLATORTYPE_HSI;
    osc.HSIState            = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.PLL.PLLState        = RCC_PLL_ON;
    osc.PLL.PLLSource       = RCC_PLLSOURCE_HSI;
    osc.PLL.PLLMUL          = RCC_PLL_MUL6;    /* 8 MHz × 6 = 48 MHz */
    osc.PLL.PREDIV          = RCC_PREDIV_DIV1;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) Error_Handler();

    clk.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                       | RCC_CLOCKTYPE_PCLK1;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_1) != HAL_OK) Error_Handler();

    pclk.PeriphClockSelection = RCC_PERIPHCLK_USART1;
    pclk.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK1;
    if (HAL_RCCEx_PeriphCLKConfig(&pclk) != HAL_OK) Error_Handler();
}

/* ===========================================================================
 * Error handling
 * =========================================================================== */

/**
 * @brief  Fatal error handler — disables interrupts and halts.
 *
 * Called by HAL when a peripheral initialisation fails.
 * Attach a debugger and inspect the call stack to identify the source.
 */
void Error_Handler(void)
{
    __disable_irq();
    while (1) { }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file;
    (void)line;
}
#endif
