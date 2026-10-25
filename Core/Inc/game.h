/*
 * game.h
 *
 *  Created on: Oct 25, 2026
 *      Author: mickey
 * ============================================================================
 * game.h — Server-authoritative tic-tac-toe engine (pure logic, no I/O)
 *
 * Responsible for:
 *   - Board state (9 cells, X / O / empty)
 *   - Turn ownership
 *   - FSM  : IDLE -> IN_PROGRESS -> GAME_OVER  (and back to IN_PROGRESS via NEW)
 *   - Move legality: range, occupancy, turn, FSM state
 *   - Win / draw detection
 *   - Resignation
 *
 * NOT responsible for:
 *   - UART / framing / NACK strings  (that belongs to the transport layer)
 *   - LED / button hardware          (that belongs to the application layer)
 * ============================================================================ */
#ifndef GAME_H
#define GAME_H

#include <stdint.h>

#define GAME_BOARD_CELLS   9U

typedef enum {
    GAME_STATE_IDLE = 0,
    GAME_STATE_IN_PROGRESS,
    GAME_STATE_GAME_OVER
} GameState;

typedef enum {
    GAME_PLAYER_NONE = 0,      /* also the "empty cell" value */
    GAME_PLAYER_X,
    GAME_PLAYER_O
} GamePlayer;

/* Status codes returned by the command functions.
 * The transport layer maps these to wire-level NACK reasons:
 *   GAME_ERR_RANGE     -> "NACK: RANGE"
 *   GAME_ERR_OCCUPIED  -> "NACK: OCCUPIED"
 *   GAME_ERR_NOTURN    -> "NACK: NOTURN"
 *   GAME_ERR_NOGAME    -> "NACK: NOGAME"
 *   GAME_ERR_BADCMD    -> "NACK: BADCMD"
 *   GAME_ERR_STATE     -> "NACK: BADCMD"  (or a dedicated reason if desired)
 */
typedef enum {
    GAME_OK = 0,
    GAME_ERR_RANGE,
    GAME_ERR_OCCUPIED,
    GAME_ERR_NOTURN,           /* reserved for multi-client scenarios */
    GAME_ERR_NOGAME,
    GAME_ERR_BADCMD,
    GAME_ERR_STATE
} GameStatus;

typedef enum {
    GAME_OUTCOME_NONE = 0,
    GAME_OUTCOME_X,
    GAME_OUTCOME_O,
    GAME_OUTCOME_DRAW
} GameOutcome;

/* -------- Lifecycle -------------------------------------------------------- */
void       Game_Init(void);

/* -------- Commands (the FSM + rules are enforced here) --------------------- */
GameStatus Game_Connect(void);       /* IDLE                    -> OK
                                        already connected       -> BADCMD  */
GameStatus Game_Disconnect(void);    /* any state               -> OK, resets */
GameStatus Game_New(void);           /* IDLE / GAME_OVER        -> OK
                                        IN_PROGRESS             -> BADCMD  */
GameStatus Game_Move(uint8_t idx);   /* IN_PROGRESS + legal     -> OK
                                        IN_PROGRESS + illegal   -> RANGE / OCCUPIED / NOTURN
                                        not IN_PROGRESS         -> NOGAME  */
GameStatus Game_Resign(void);        /* IN_PROGRESS             -> OK
                                        else                    -> NOGAME  */
GameStatus Game_State(void);         /* connected               -> OK
                                        not connected           -> BADCMD  */

/* -------- Queries ---------------------------------------------------------- */
GameState   Game_GetState(void);
GamePlayer  Game_GetTurn(void);
GameOutcome Game_GetOutcome(void);
uint8_t     Game_GetCell(uint8_t idx);           /* GAME_PLAYER_* per cell */
uint8_t     Game_IsConnected(void);
void        Game_EncodeBoard(char out[GAME_BOARD_CELLS + 1U]);  /* "X.O.X...." */

#endif /* GAME_H */
