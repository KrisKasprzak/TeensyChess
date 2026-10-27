// SPDX-License-Identifier: GPL-3.0-or-later
// Teensy Chess - drawing the board, the info panel and the dialogs

#pragma once

#include <Arduino.h>

#include "game.h"

// What to highlight on the board
struct BoardView {
  bool flipped = false;               // true when the player has black (black at the bottom)
  Square selected = Stockfish::SQ_NONE;  // piece picked up by the player (orange)
  Bitboard targets = 0;               // where the selected piece can go
  Square lastFrom = Stockfish::SQ_NONE, lastTo = Stockfish::SQ_NONE;  // engine's last move
  Square hintFrom = Stockfish::SQ_NONE, hintTo = Stockfish::SQ_NONE;  // hint move
};

enum PanelButton : int8_t { BTN_NONE = -1, BTN_HINT, BTN_UNDO, BTN_NEW, BTN_COUNT };

// ---- Board ----

// Forget what is on screen so the next uiDrawBoard() redraws every square
void uiInvalidateBoard();

// Draws the squares whose content changed since the last call
void uiDrawBoard(const Position &pos, const BoardView &view);

// The square under screen point (x, y), or SQ_NONE if outside the board
Square uiSquareAt(int x, int y, bool flipped);

// ---- Info panel ----

void uiDrawPanel(int level);
void uiDrawStatus(const char *line1, uint16_t color1, const char *line2, uint16_t color2);
// evalWhite: engine score from white's point of view (internal units), only shown if hasEval
void uiDrawPoints(int whitePoints, int blackPoints, bool hasEval, int evalWhite);
void uiDrawHint(const char *text);
void uiDrawButtons(bool hintEnabled, bool undoEnabled, bool newEnabled);
void uiFlashButton(PanelButton b);
PanelButton uiButtonAt(int x, int y);

// ---- Promotion picker (replaces the points and hint sections of the panel) ----

void uiDrawPromotionPicker(Color c);
// Returns the chosen piece type, or NO_PIECE_TYPE if (x, y) is not on a choice
PieceType uiPromotionAt(int x, int y);

// ---- Full screen ----

void uiSplash();
// Lets the user choose a color and a level. Returns false if cancelled.
bool uiNewGameDialog(uint8_t &humanWhite, uint8_t &level);
