// SPDX-License-Identifier: GPL-3.0-or-later
// Teensy Chess - drawing the board, the info panel and the dialogs

#include "ui.h"

#include <font_Arial.h>
#include <font_ArialBold.h>

#include "config.h"
#include "hardware.h"
#include "pieces.h"
#include "src/engine/evaluate.h"
#include "touch_input.h"

using namespace Stockfish;

namespace {

// ---------------------------------------------------------------------------
// Tiny 3x5 font for the board coordinates (a-h, 1-8). Each glyph is 5 rows of
// 3 bits, most significant bit on the left.
// ---------------------------------------------------------------------------
const uint8_t FILE_GLYPHS[8][5] = {
  {0b000, 0b011, 0b101, 0b101, 0b011},  // a
  {0b100, 0b110, 0b101, 0b101, 0b110},  // b
  {0b000, 0b011, 0b100, 0b100, 0b011},  // c
  {0b001, 0b011, 0b101, 0b101, 0b011},  // d
  {0b000, 0b010, 0b111, 0b100, 0b011},  // e
  {0b001, 0b010, 0b111, 0b010, 0b010},  // f
  {0b011, 0b101, 0b011, 0b001, 0b110},  // g
  {0b100, 0b110, 0b101, 0b101, 0b101},  // h
};
const uint8_t RANK_GLYPHS[8][5] = {
  {0b010, 0b110, 0b010, 0b010, 0b111},  // 1
  {0b110, 0b001, 0b010, 0b100, 0b111},  // 2
  {0b110, 0b001, 0b010, 0b001, 0b110},  // 3
  {0b101, 0b101, 0b111, 0b001, 0b001},  // 4
  {0b111, 0b100, 0b110, 0b001, 0b110},  // 5
  {0b011, 0b100, 0b110, 0b101, 0b010},  // 6
  {0b111, 0b001, 0b010, 0b010, 0b010},  // 7
  {0b010, 0b101, 0b010, 0b101, 0b010},  // 8
};

// One square is composed here and sent to the display in a single writeRect()
uint16_t squareBuf[SQUARE_SIZE * SQUARE_SIZE];

// What is currently drawn on each square, to only redraw squares that changed
uint32_t drawnKey[64];
bool boardValid = false;

enum Marker : uint8_t { MARK_NONE, MARK_DOT, MARK_CAPTURE };

inline void bufPixel(int x, int y, uint16_t c) {
  if (x >= 0 && y >= 0 && x < SQUARE_SIZE && y < SQUARE_SIZE) squareBuf[y * SQUARE_SIZE + x] = c;
}

void bufGlyph(const uint8_t glyph[5], int x0, int y0, uint16_t c) {
  for (int y = 0; y < 5; y++)
    for (int x = 0; x < 3; x++)
      if (glyph[y] & (0b100 >> x)) bufPixel(x0 + x, y0 + y, c);
}

// Draws a piece icon centered in squareBuf
void bufPiece(Piece pc) {
  const uint8_t *icon = PIECE_ICONS[type_of(pc) - 1];
  bool white = color_of(pc) == WHITE;
  uint16_t body = white ? COLOR_WHITE_PIECE : COLOR_BLACK_PIECE;
  uint16_t outline = white ? COLOR_WHITE_OUTLINE : COLOR_BLACK_OUTLINE;
  const int off = (SQUARE_SIZE - PIECE_ICON_SIZE) / 2;

  for (int y = 0; y < PIECE_ICON_SIZE; y++)
    for (int x = 0; x < PIECE_ICON_SIZE; x++) {
      int i = y * PIECE_ICON_SIZE + x;
      int v = (icon[i >> 2] >> (6 - 2 * (i & 3))) & 3;
      if (v == 1)
        squareBuf[(y + off) * SQUARE_SIZE + x + off] = body;
      else if (v == 2)
        squareBuf[(y + off) * SQUARE_SIZE + x + off] = outline;
    }
}

// Screen position of the top-left corner of a square
void squareOrigin(Square s, bool flipped, int &x, int &y) {
  int col = flipped ? 7 - file_of(s) : file_of(s);
  int row = flipped ? rank_of(s) : 7 - rank_of(s);
  x = col * SQUARE_SIZE;
  y = row * SQUARE_SIZE;
}

void drawSquare(Square s, Piece pc, uint16_t bg, Marker marker, bool flipped) {
  bool lightSquare = (file_of(s) + rank_of(s)) & 1;

  for (int i = 0; i < SQUARE_SIZE * SQUARE_SIZE; i++) squareBuf[i] = bg;

  // Coordinates like on a real board: files along the bottom edge, ranks along
  // the left edge, in the color of the other square type
  uint16_t labelColor = lightSquare ? COLOR_DARK_SQUARE : COLOR_LIGHT_SQUARE;
  int x0, y0;
  squareOrigin(s, flipped, x0, y0);
  if (y0 == 7 * SQUARE_SIZE) bufGlyph(FILE_GLYPHS[file_of(s)], SQUARE_SIZE - 4, SQUARE_SIZE - 6, labelColor);
  if (x0 == 0) bufGlyph(RANK_GLYPHS[rank_of(s)], 1, 1, labelColor);

  if (pc != NO_PIECE) bufPiece(pc);

  if (marker == MARK_DOT) {
    // Filled circle of radius 4 in the middle of the square
    const int c = SQUARE_SIZE / 2;
    for (int y = -4; y <= 4; y++)
      for (int x = -4; x <= 4; x++)
        if (x * x + y * y <= 17) bufPixel(c + x - 1, c + y - 1, COLOR_TARGET_DOT);
  } else if (marker == MARK_CAPTURE) {
    // Triangles in the four corners
    for (int y = 0; y < 7; y++)
      for (int x = 0; x < 7 - y; x++) {
        bufPixel(x, y, COLOR_TARGET_DOT);
        bufPixel(SQUARE_SIZE - 1 - x, y, COLOR_TARGET_DOT);
        bufPixel(x, SQUARE_SIZE - 1 - y, COLOR_TARGET_DOT);
        bufPixel(SQUARE_SIZE - 1 - x, SQUARE_SIZE - 1 - y, COLOR_TARGET_DOT);
      }
  }

  tft.writeRect(x0, y0, SQUARE_SIZE, SQUARE_SIZE, squareBuf);
}

// ---------------------------------------------------------------------------
// Panel helpers
// ---------------------------------------------------------------------------

void textAt(const char *text, int x, int y, const ILI9341_t3_font_t &font, uint16_t color) {
  tft.setFont(font);
  tft.setTextColor(color);
  tft.setCursor(x, y);
  tft.print(text);
}

void textRight(const char *text, int xRight, int y, const ILI9341_t3_font_t &font, uint16_t color) {
  tft.setFont(font);
  textAt(text, xRight - tft.measureTextWidth(text), y, font, color);
}

void textCentered(const char *text, int xCenter, int y, const ILI9341_t3_font_t &font, uint16_t color) {
  tft.setFont(font);
  textAt(text, xCenter - tft.measureTextWidth(text) / 2, y, font, color);
}

// Draws text in the largest of the given fonts (largest first) that fits maxWidth
void textFit(const char *text, int x, int y, int maxWidth, const ILI9341_t3_font_t *const *fonts, int count,
             uint16_t color) {
  int i = 0;
  for (; i < count - 1; i++) {
    tft.setFont(*fonts[i]);
    if (tft.measureTextWidth(text) <= maxWidth) break;
  }
  textAt(text, x, y, *fonts[i], color);
}

const ILI9341_t3_font_t *const BOLD_FONTS[] = {&Arial_10_Bold, &Arial_9_Bold, &Arial_8_Bold};
const ILI9341_t3_font_t *const PLAIN_FONTS[] = {&Arial_10, &Arial_9, &Arial_8};

const char *BUTTON_LABELS[BTN_COUNT] ={"HINT", "UNDO", "NEW"};
bool buttonEnabled[BTN_COUNT] = {true, true, true};

int buttonY(int b) { return PANEL_BUTTON_Y0 + b * (PANEL_BUTTON_H + PANEL_BUTTON_GAP); }

void drawButton(int b, uint16_t fill, uint16_t textColor) {
  int y = buttonY(b);
  tft.fillRoundRect(PANEL_BUTTON_X, y, PANEL_BUTTON_W, PANEL_BUTTON_H, 4, fill);
  textCentered(BUTTON_LABELS[b], PANEL_BUTTON_X + PANEL_BUTTON_W / 2, y + 6, Arial_10_Bold, textColor);
}

// Promotion picker layout: a 2 x 2 grid of 34 px squares in the panel
const PieceType PROMO_PIECES[4] = {QUEEN, ROOK, BISHOP, KNIGHT};
const int PROMO_X0 = PANEL_X + 5, PROMO_Y0 = PANEL_POINTS_Y + 22, PROMO_CELL = 35;

// Generic dialog button
struct Rect {
  int x, y, w, h;
  bool contains(int px, int py) const { return px >= x && px < x + w && py >= y && py < y + h; }
};

void dialogButton(const Rect &r, const char *label, bool selected) {
  tft.fillRoundRect(r.x, r.y, r.w, r.h, 5, selected ? COLOR_BUTTON_PRESSED : COLOR_BUTTON);
  textCentered(label, r.x + r.w / 2, r.y + r.h / 2 - 6, Arial_12_Bold,
               selected ? COLOR_PANEL_BG : COLOR_BUTTON_TEXT);
}

}  // namespace

// ---------------------------------------------------------------------------
// Board
// ---------------------------------------------------------------------------

void uiInvalidateBoard() { boardValid = false; }

void uiDrawBoard(const Position &pos, const BoardView &view) {
  Square checkedKing = pos.checkers() ? pos.square<KING>(pos.side_to_move()) : SQ_NONE;

  for (Square s = SQ_A1; s <= SQ_H8; ++s) {
    bool light = (file_of(s) + rank_of(s)) & 1;
    uint16_t bg = light ? COLOR_LIGHT_SQUARE : COLOR_DARK_SQUARE;

    if (s == view.selected)
      bg = COLOR_SELECTED;
    else if (s == checkedKing)
      bg = COLOR_CHECK;
    else if (s == view.hintFrom || s == view.hintTo)
      bg = light ? COLOR_HINT_LIGHT : COLOR_HINT_DARK;
    else if (s == view.lastFrom || s == view.lastTo)
      bg = light ? COLOR_LAST_LIGHT : COLOR_LAST_DARK;

    Piece pc = pos.piece_on(s);
    Marker marker = MARK_NONE;
#if SHOW_LEGAL_MOVES
    if (view.targets & square_bb(s)) marker = pc == NO_PIECE ? MARK_DOT : MARK_CAPTURE;
#endif

    uint32_t key = uint32_t(pc) | (uint32_t(marker) << 4) | (uint32_t(view.flipped) << 6) | (uint32_t(bg) << 16);
    if (boardValid && drawnKey[s] == key) continue;

    drawSquare(s, pc, bg, marker, view.flipped);
    drawnKey[s] = key;
  }
  boardValid = true;
}

Square uiSquareAt(int x, int y, bool flipped) {
  if (x < 0 || y < 0 || x >= BOARD_PX || y >= BOARD_PX) return SQ_NONE;
  int col = x / SQUARE_SIZE, row = y / SQUARE_SIZE;
  File f = File(flipped ? 7 - col : col);
  Rank r = Rank(flipped ? row : 7 - row);
  return make_square(f, r);
}

// ---------------------------------------------------------------------------
// Info panel
// ---------------------------------------------------------------------------

void uiDrawPanel(int level) {
  tft.fillRect(PANEL_X, 0, PANEL_W, SCREEN_H, COLOR_PANEL_BG);

  // Header: engine name and strength level
  tft.fillRect(PANEL_X, 0, PANEL_W, PANEL_HEADER_H, COLOR_PANEL_HEADER);
  textAt(ENGINE_NAME, PANEL_X + 3, 6, Arial_8, COLOR_PANEL_TEXT);
  char lv[8];
  snprintf(lv, sizeof(lv), "%d", level);
  textRight(lv, SCREEN_W - 3, 6, Arial_8_Bold, COLOR_PANEL_ACCENT);

  tft.drawFastHLine(PANEL_X + 4, PANEL_POINTS_Y - 4, PANEL_W - 8, COLOR_PANEL_LINE);
  tft.drawFastHLine(PANEL_X + 4, PANEL_HINT_Y - 4, PANEL_W - 8, COLOR_PANEL_LINE);

  for (int b = 0; b < BTN_COUNT; b++) drawButton(b, buttonEnabled[b] ? COLOR_BUTTON : COLOR_BUTTON_OFF,
                                                 buttonEnabled[b] ? COLOR_BUTTON_TEXT : COLOR_PANEL_MUTED);
}

void uiDrawStatus(const char *line1, uint16_t color1, const char *line2, uint16_t color2) {
  tft.fillRect(PANEL_X, PANEL_STATUS_Y, PANEL_W, PANEL_POINTS_Y - 5 - PANEL_STATUS_Y, COLOR_PANEL_BG);
  textFit(line1, PANEL_X + 4, PANEL_STATUS_Y + 3, PANEL_W - 6, BOLD_FONTS, 3, color1);
  if (line2 && line2[0]) textFit(line2, PANEL_X + 4, PANEL_STATUS_Y + 18, PANEL_W - 6, PLAIN_FONTS, 3, color2);
}

void uiDrawPoints(int whitePoints, int blackPoints, bool hasEval, int evalWhite) {
  const int x = PANEL_X + 4, w = PANEL_W - 8;
  int y = PANEL_POINTS_Y;
  tft.fillRect(PANEL_X, y, PANEL_W, PANEL_HINT_Y - 5 - y, COLOR_PANEL_BG);

  // Material count of each side, with a small swatch of the piece color
  textAt("Points", x, y, Arial_8, COLOR_PANEL_MUTED);
  char buf[24];
  y += 12;
  tft.fillRect(x, y + 1, 8, 8, COLOR_WHITE_PIECE);
  snprintf(buf, sizeof(buf), "%d", whitePoints);
  textAt(buf, x + 11, y, Arial_10, COLOR_PANEL_TEXT);
  tft.fillRect(x + 38, y + 1, 8, 8, COLOR_BLACK_PIECE);
  tft.drawRect(x + 38, y + 1, 8, 8, COLOR_PANEL_MUTED);
  snprintf(buf, sizeof(buf), "%d", blackPoints);
  textAt(buf, x + 49, y, Arial_10, COLOR_PANEL_TEXT);

  // Who is ahead in material
  y += 15;
  int diff = whitePoints - blackPoints;
  if (diff == 0)
    textAt("Even", x, y, Arial_10_Bold, COLOR_PANEL_TEXT);
  else {
    snprintf(buf, sizeof(buf), "%s +%d", diff > 0 ? "White" : "Black", abs(diff));
    textAt(buf, x, y, Arial_10_Bold, COLOR_PANEL_ACCENT);
  }

  // Stockfish's evaluation in pawns (positive: white is better) and a bar
  y += 17;
  textAt("Eval", x, y, Arial_8, COLOR_PANEL_MUTED);
  int share = 50;  // white's share of the bar, in percent
  if (hasEval) {
    if (is_mate_or_mated(evalWhite)) {
      int plies = VALUE_MATE - abs(evalWhite);
      if (plies == 0)
        snprintf(buf, sizeof(buf), "#");  // the game is over
      else
        snprintf(buf, sizeof(buf), "%sM%d", evalWhite > 0 ? "+" : "-", (plies + 1) / 2);
      share = evalWhite > 0 ? 100 : 0;
    } else {
      int cp = evalWhite * 100 / Eval::PawnValueEg;
      snprintf(buf, sizeof(buf), "%c%d.%02d", cp >= 0 ? '+' : '-', abs(cp) / 100, abs(cp) % 100);
      share = 50 + 50 * evalWhite / (abs(evalWhite) + 600);
    }
    textRight(buf, x + w, y - 1, Arial_9, COLOR_PANEL_TEXT);
  } else
    textRight("-", x + w, y - 1, Arial_9, COLOR_PANEL_MUTED);

  y += 13;
  int whiteW = (w - 2) * share / 100;
  tft.drawRect(x, y, w, 9, COLOR_PANEL_MUTED);
  tft.fillRect(x + 1, y + 1, whiteW, 7, COLOR_WHITE_PIECE);
  tft.fillRect(x + 1 + whiteW, y + 1, w - 2 - whiteW, 7, COLOR_BLACK_PIECE);
}

void uiDrawHint(const char *text) {
  int y = PANEL_HINT_Y;
  tft.fillRect(PANEL_X, y, PANEL_W, PANEL_BUTTON_Y0 - 2 - y, COLOR_PANEL_BG);
  textAt("Hint", PANEL_X + 4, y, Arial_8, COLOR_PANEL_MUTED);
  if (text && text[0]) textAt(text, PANEL_X + 4, y + 12, Arial_12_Bold, COLOR_GOOD);
}

void uiDrawButtons(bool hintEnabled, bool undoEnabled, bool newEnabled) {
  bool want[BTN_COUNT] = {hintEnabled, undoEnabled, newEnabled};
  for (int b = 0; b < BTN_COUNT; b++) {
    buttonEnabled[b] = want[b];
    drawButton(b, want[b] ? COLOR_BUTTON : COLOR_BUTTON_OFF, want[b] ? COLOR_BUTTON_TEXT : COLOR_PANEL_MUTED);
  }
}

void uiFlashButton(PanelButton b) {
  if (b == BTN_NONE) return;
  drawButton(b, COLOR_BUTTON_PRESSED, COLOR_PANEL_BG);
  delay(120);
  drawButton(b, buttonEnabled[b] ? COLOR_BUTTON : COLOR_BUTTON_OFF,
             buttonEnabled[b] ? COLOR_BUTTON_TEXT : COLOR_PANEL_MUTED);
}

PanelButton uiButtonAt(int x, int y) {
  if (x < PANEL_BUTTON_X || x >= PANEL_BUTTON_X + PANEL_BUTTON_W) return BTN_NONE;
  for (int b = 0; b < BTN_COUNT; b++)
    if (y >= buttonY(b) - 1 && y < buttonY(b) + PANEL_BUTTON_H + 1)
      return buttonEnabled[b] ? PanelButton(b) : BTN_NONE;
  return BTN_NONE;
}

// ---------------------------------------------------------------------------
// Promotion picker
// ---------------------------------------------------------------------------

void uiDrawPromotionPicker(Color c) {
  int y = PANEL_POINTS_Y;
  tft.fillRect(PANEL_X, y, PANEL_W, PANEL_BUTTON_Y0 - 2 - y, COLOR_PANEL_BG);
  textAt("Promote to:", PANEL_X + 4, y + 4, Arial_9_Bold, COLOR_PANEL_ACCENT);

  for (int i = 0; i < 4; i++) {
    int x0 = PROMO_X0 + (i % 2) * PROMO_CELL;
    int y0 = PROMO_Y0 + (i / 2) * PROMO_CELL;
    tft.fillRoundRect(x0, y0, PROMO_CELL - 2, PROMO_CELL - 2, 4, (i & 1) ? COLOR_DARK_SQUARE : COLOR_LIGHT_SQUARE);

    // Reuse the square compositor for the icon
    uint16_t bg = (i & 1) ? COLOR_DARK_SQUARE : COLOR_LIGHT_SQUARE;
    for (int k = 0; k < SQUARE_SIZE * SQUARE_SIZE; k++) squareBuf[k] = bg;
    bufPiece(make_piece(c, PROMO_PIECES[i]));
    int off = (PROMO_CELL - 2 - SQUARE_SIZE) / 2;
    tft.writeRect(x0 + off, y0 + off, SQUARE_SIZE, SQUARE_SIZE, squareBuf);
  }
}

PieceType uiPromotionAt(int x, int y) {
  for (int i = 0; i < 4; i++) {
    int x0 = PROMO_X0 + (i % 2) * PROMO_CELL;
    int y0 = PROMO_Y0 + (i / 2) * PROMO_CELL;
    if (x >= x0 && x < x0 + PROMO_CELL && y >= y0 && y < y0 + PROMO_CELL) return PROMO_PIECES[i];
  }
  return NO_PIECE_TYPE;
}

// ---------------------------------------------------------------------------
// Full screen
// ---------------------------------------------------------------------------

void uiSplash() {
  tft.fillScreen(COLOR_PANEL_BG);

  // A row of pieces as decoration
  const PieceType row[6] = {ROOK, KNIGHT, BISHOP, QUEEN, KING, PAWN};
  for (int i = 0; i < 6; i++) {
    uint16_t bg = (i & 1) ? COLOR_DARK_SQUARE : COLOR_LIGHT_SQUARE;
    for (int k = 0; k < SQUARE_SIZE * SQUARE_SIZE; k++) squareBuf[k] = bg;
    bufPiece(make_piece(i < 3 ? WHITE : BLACK, row[i]));
    tft.writeRect(70 + i * SQUARE_SIZE, 40, SQUARE_SIZE, SQUARE_SIZE, squareBuf);
  }

  textCentered("Teensy Chess", SCREEN_W / 2, 90, Arial_24_Bold, COLOR_PANEL_TEXT);
  textCentered(ENGINE_NAME ", based on Stockfish", SCREEN_W / 2, 125, Arial_10, COLOR_PANEL_MUTED);
  textCentered("Tap the screen to calibrate touch", SCREEN_W / 2, 175, Arial_10, COLOR_PANEL_ACCENT);
}

bool uiNewGameDialog(uint8_t &humanWhite, uint8_t &level) {
  const Rect btnWhite{50, 62, 105, 34}, btnBlack{165, 62, 105, 34};
  const Rect btnMinus{50, 136, 44, 34}, btnPlus{226, 136, 44, 34};
  const Rect btnStart{50, 196, 105, 34}, btnCancel{165, 196, 105, 34};

  uint8_t white = humanWhite, lvl = level;
  bool redrawAll = true;

  while (true) {
    if (redrawAll) {
      tft.fillScreen(COLOR_PANEL_BG);
      textCentered("New Game", SCREEN_W / 2, 10, Arial_16_Bold, COLOR_PANEL_ACCENT);
      textAt("Play as", 50, 46, Arial_10, COLOR_PANEL_MUTED);
      textAt(ENGINE_NAME " level", 50, 120, Arial_10, COLOR_PANEL_MUTED);
      dialogButton(btnMinus, "-", false);
      dialogButton(btnPlus, "+", false);
      dialogButton(btnStart, "Start", false);
      dialogButton(btnCancel, "Cancel", false);
      redrawAll = false;
    }

    dialogButton(btnWhite, "White", white);
    dialogButton(btnBlack, "Black", !white);

    // Level number and a short description
    tft.fillRect(btnMinus.x + btnMinus.w + 2, 132, btnPlus.x - btnMinus.x - btnMinus.w - 4, 58, COLOR_PANEL_BG);
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", lvl);
    textCentered(buf, SCREEN_W / 2, 138, Arial_20_Bold, COLOR_PANEL_TEXT);
    const char *desc = lvl < 5 ? "beginner" : lvl < 10 ? "casual" : lvl < 15 ? "club player" : lvl < 20 ? "strong" : "full strength";
    textCentered(desc, SCREEN_W / 2, 166, Arial_9, COLOR_PANEL_MUTED);

    // Wait for a tap on one of the buttons
    int x, y;
    while (!getTap(x, y)) delay(10);

    if (btnWhite.contains(x, y)) white = 1;
    else if (btnBlack.contains(x, y)) white = 0;
    else if (btnMinus.contains(x, y) && lvl > 0) lvl--;
    else if (btnPlus.contains(x, y) && lvl < MAX_LEVEL) lvl++;
    else if (btnStart.contains(x, y)) {
      dialogButton(btnStart, "Start", true);
      humanWhite = white;
      level = lvl;
      waitForRelease();
      return true;
    } else if (btnCancel.contains(x, y)) {
      dialogButton(btnCancel, "Cancel", true);
      waitForRelease();
      return false;
    }
  }
}
