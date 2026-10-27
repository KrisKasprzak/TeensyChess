/*
  Teensy Chess - play chess against TeensyFish, a port of the Stockfish chess engine,
  on a Teensy 4.0 with a 2.8" ILI9341 touch display

  The engine is a port of Stockfish (src/engine): its board representation, legal move
  generator, static exchange evaluation, transposition table, move ordering and search,
  with a handcrafted evaluation in place of the NNUE network (the network does not fit
  in the Teensy's 2 MB of flash).

  Screen (landscape, 320 x 240):
    - left:  the board, 8 x 30 px squares
    - right: the info panel with the game status, material points, the engine's
             evaluation, the hint text and the HINT / UNDO / NEW buttons

  Playing: tap one of your pieces (its square turns orange, dots show where it can go),
  then tap the destination square. Castle by moving the king two squares. A buzzer
  clicks on every accepted tap and on the engine's moves, and beeps on illegal taps.

  Touch calibration: runs automatically the first time, or tap the splash screen during
  the first 3 seconds after power-up. The calibration and the game settings are stored
  in EEPROM.

  Serial monitor (115200 baud) commands:
    p  run the perft move generator self-test
    b  run a short search benchmark
    f  print the current position (FEN)
    c  recalibrate the touch screen

  Wiring: see config.h
  Libraries: ILI9341_t3 and XPT2046_Touchscreen (both by Paul Stoffregen)
  Board: Teensy 4.0, CPU speed 600 MHz, Optimize "Faster", USB type "Serial"

  ---------------------------------------------------------------------------------------
  LICENSE

  Copyright (C) 2026 the Teensy Chess authors
  The chess engine in src/engine is derived from Stockfish,
  Copyright (C) 2004-2026 The Stockfish developers (see src/engine/AUTHORS).

  This program is free software: you can redistribute it and/or modify it under the
  terms of the GNU General Public License as published by the Free Software Foundation,
  either version 3 of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT ANY
  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
  PARTICULAR PURPOSE. See the GNU General Public License for more details. You should
  have received a copy of the GNU General Public License along with this program (the
  LICENSE file). If not, see <https://www.gnu.org/licenses/>.

  Because the engine is derived from Stockfish, the GPLv3 conditions apply to the whole
  program. In short, if you distribute this program or a modified version of it,
  including as a compiled .hex file or on a finished device, you must:
    - license it under the GPLv3 as well,
    - provide the complete corresponding source code (or a written offer to provide it),
    - keep the copyright and license notices, including those in src/engine, and
    - mark your changes as yours.
  The full license text is authoritative; this summary is not legal advice.

  The ILI9341_t3 and XPT2046_Touchscreen libraries are not part of this program; they
  are separate MIT-licensed libraries, which may be combined with GPLv3 code.
  ---------------------------------------------------------------------------------------
*/

#include <SPI.h>
#include <ILI9341_t3.h>
#include <XPT2046_Touchscreen.h>

#include "config.h"
#include "game.h"
#include "hardware.h"
#include "storage.h"
#include "touch_input.h"
#include "ui.h"
#include "src/engine/engine.h"
#include "src/engine/movegen.h"

using namespace Stockfish;

// ===========================
// Hardware
// ===========================
ILI9341_t3 tft = ILI9341_t3(TFT_CS, TFT_DC, TFT_RST);
XPT2046_Touchscreen touch(0, 1);  // no T_IRQ pin: the library polls touch pressure over SPI

// ===========================
// Game state
// ===========================
enum class State { PlayerTurn,
                   EngineTurn,
                   GameOver };

ChessGame game;
BoardView view;
State state = State::PlayerTurn;
Color humanColor = WHITE;

bool promotionPending = false;  // the promotion picker is shown
Square promoFrom = SQ_NONE, promoTo = SQ_NONE;

bool hasEval = false;  // evalWhite holds a Stockfish score
int evalWhite = 0;     // from white's point of view

char hintText[16] = "";        // SAN of the last hint
char lastEngineMove[16] = "";  // SAN of Stockfish's last move

// ===========================
// Screen updates
// ===========================

// Short click on the buzzer; tone() runs in the background, so this does not block
void click() {
  analogWriteFrequency(BUZZER_PIN, CLICK_FREQ_HZ);
  analogWrite(BUZZER_PIN, CLICK_VOLUME);
  delay(CLICK_MS);
  analogWrite(BUZZER_PIN, 0);
}

// Low beep for an illegal square or move, same PWM approach as click()
void warning() {
  analogWriteFrequency(BUZZER_PIN, WARN_FREQ_HZ);
  analogWrite(BUZZER_PIN, WARN_VOLUME);
  delay(WARN_MS);
  analogWrite(BUZZER_PIN, 0);
}

void refreshBoard() {
  uiDrawBoard(game.pos(), view);
}

void refreshPoints() {
  uiDrawPoints(game.materialPoints(WHITE), game.materialPoints(BLACK), hasEval, evalWhite);
}

void refreshStatus() {
  char line2[24];

  if (state == State::GameOver) {
    switch (game.result()) {
      case GameResult::WhiteMates:
      case GameResult::BlackMates:
        {
          bool whiteWon = game.result() == GameResult::WhiteMates;
          bool humanWon = whiteWon == (humanColor == WHITE);
          uiDrawStatus("Checkmate!", humanWon ? COLOR_GOOD : COLOR_BAD, humanWon ? "You win!" : "You lost",
                       COLOR_PANEL_TEXT);
          break;
        }
      case GameResult::Stalemate: uiDrawStatus("Draw", COLOR_PANEL_ACCENT, "Stalemate", COLOR_PANEL_TEXT); break;
      case GameResult::DrawFiftyMoves: uiDrawStatus("Draw", COLOR_PANEL_ACCENT, "50-move rule", COLOR_PANEL_TEXT); break;
      case GameResult::DrawRepetition: uiDrawStatus("Draw", COLOR_PANEL_ACCENT, "Repetition", COLOR_PANEL_TEXT); break;
      case GameResult::DrawMaterial: uiDrawStatus("Draw", COLOR_PANEL_ACCENT, "Material", COLOR_PANEL_TEXT); break;
      default: break;
    }
    return;
  }

  if (state == State::EngineTurn) {
    snprintf(line2, sizeof(line2), "Level %d", settings.level);
    uiDrawStatus("Thinking...", COLOR_PANEL_ACCENT, line2, COLOR_PANEL_MUTED);
    return;
  }

  if (lastEngineMove[0])
    snprintf(line2, sizeof(line2), ENGINE_SHORT ": %s", lastEngineMove);
  else
    snprintf(line2, sizeof(line2), "You are %s", humanColor == WHITE ? "white" : "black");

  if (game.pos().checkers())
    uiDrawStatus("Check!", COLOR_BAD, line2, COLOR_PANEL_TEXT);
  else
    uiDrawStatus("Your move", COLOR_GOOD, line2, COLOR_PANEL_TEXT);
}

// The player can take back a move when one of their moves is in the history
bool canUndo() {
  if (state == State::EngineTurn) return false;
  return game.plyCount() >= (humanColor == WHITE ? 1 : 2);
}

void refreshButtons() {
  uiDrawButtons(state == State::PlayerTurn, canUndo(), state != State::EngineTurn);
}

void refreshAll() {
  refreshBoard();
  refreshStatus();
  if (promotionPending)
    uiDrawPromotionPicker(humanColor);
  else {
    refreshPoints();
    uiDrawHint(hintText);
  }
  refreshButtons();
}

// Redraw everything, e.g. after a dialog covered the screen
void redrawScreen() {
  uiInvalidateBoard();
  uiDrawPanel(settings.level);
  refreshAll();
}

// ===========================
// Game flow
// ===========================

void startGame() {
  game.reset();
  Engine::new_game();

  humanColor = settings.humanWhite ? WHITE : BLACK;
  view = BoardView();
  view.flipped = humanColor == BLACK;
  promotionPending = false;
  hasEval = false;
  hintText[0] = lastEngineMove[0] = '\0';
  state = game.sideToMove() == humanColor ? State::PlayerTurn : State::EngineTurn;

  Serial.printf("New game: player has %s, level %d\n", humanColor == WHITE ? "white" : "black", settings.level);
  redrawScreen();
}

// Called after every move: decides whose turn it is and updates the screen
void afterMove() {
  view.selected = SQ_NONE;
  view.targets = 0;

  if (game.result() != GameResult::Ongoing)
    state = State::GameOver;
  else
    state = game.sideToMove() == humanColor ? State::PlayerTurn : State::EngineTurn;

  refreshAll();
}

void playerMove(Move m) {
  char san[16];
  game.san(m, san);
  Serial.printf("You: %s\n", san);

  game.makeMove(m);
  view.lastFrom = m.from_sq();
  view.lastTo = displayTo(m);
  view.hintFrom = view.hintTo = SQ_NONE;
  hintText[0] = '\0';
  afterMove();
}

void logSearch(const char *who, Move m, const Search::Result &r) {
  char uci[8];
  Engine::move_to_uci(m, uci);
  unsigned long nps = r.time > 0 ? (unsigned long)(r.nodes * 1000 / r.time) : 0;
  Serial.printf("%s: %s  score %d  depth %d  nodes %lu  time %lu ms  (%lu nodes/s)\n", who, uci, r.score, r.depth,
                (unsigned long)r.nodes, (unsigned long)r.time, nps);
}

void engineMove() {
  refreshStatus();  // "Thinking..."
  uiDrawButtons(false, false, false);

  Search::LimitsType limits;
  limits.movetime = ENGINE_TIME_MS(settings.level);
  if (settings.level < MAX_LEVEL) limits.depth = settings.level + 1;  // weaker levels search shallower

  Search::Result r = Engine::think(game.pos(), limits, settings.level);
  if (r.best == Move::none()) {  // no legal move (the game should already be over)
    afterMove();
    return;
  }
  logSearch("Stockfish", r.best, r);

  // The score is from the engine's point of view, before its move
  hasEval = true;
  evalWhite = game.sideToMove() == WHITE ? r.score : -r.score;

  game.san(r.best, lastEngineMove);
  game.makeMove(r.best);
  view.lastFrom = r.best.from_sq();
  view.lastTo = displayTo(r.best);
  click();
  afterMove();
}

void showHint() {
  uiDrawStatus("Thinking...", COLOR_PANEL_ACCENT, "Hint", COLOR_PANEL_MUTED);
  uiDrawButtons(false, false, false);

  Search::LimitsType limits;
  limits.movetime = HINT_TIME_MS;
  Search::Result r = Engine::think(game.pos(), limits, MAX_LEVEL);

  if (r.best != Move::none()) {
    logSearch("Hint", r.best, r);
    game.san(r.best, hintText);
    view.hintFrom = r.best.from_sq();
    view.hintTo = displayTo(r.best);
    hasEval = true;
    evalWhite = game.sideToMove() == WHITE ? r.score : -r.score;
  }
  refreshAll();
}

void undoMove() {
  // Take back moves until one of the player's moves is undone (normally the
  // engine's reply and the player's move before it)
  while (game.plyCount() > 0) {
    Color mover = ~game.sideToMove();
    game.undoMove();
    if (mover == humanColor) break;
  }

  Move last = game.lastMove();
  view = BoardView();
  view.flipped = humanColor == BLACK;
  if (last != Move::none()) {
    view.lastFrom = last.from_sq();
    view.lastTo = displayTo(last);
  }
  promotionPending = false;
  hasEval = false;
  hintText[0] = lastEngineMove[0] = '\0';
  Serial.println("Move taken back");
  afterMove();
}

void newGameDialog() {
  uint8_t white = settings.humanWhite, level = settings.level;
  if (uiNewGameDialog(white, level)) {
    settings.humanWhite = white;
    settings.level = level;
    saveSettings();
    startGame();
  } else
    redrawScreen();
}

// ===========================
// Touch handling
// ===========================


void selectSquare(Square s) {
  view.selected = s;
  view.targets = s == SQ_NONE ? 0 : game.targets(s);
  refreshBoard();
}

void cancelPromotion() {
  promotionPending = false;
  refreshPoints();
  uiDrawHint(hintText);
}

void handleBoardTap(Square s) {
  if (state != State::PlayerTurn) return;

  if (promotionPending) {  // tapping the board cancels the promotion
    cancelPromotion();
    selectSquare(SQ_NONE);
    return;
  }

  Piece pc = game.pos().piece_on(s);
  bool ownPiece = pc != NO_PIECE && color_of(pc) == humanColor;
  bool canMove = ownPiece && game.targets(s);  // one of our pieces with a legal move

  if (view.selected == SQ_NONE) {
    if (canMove) {
      click();
      selectSquare(s);
    } else
      warning();  // empty square, opponent's piece or a piece that cannot move
    return;
  }

  if (s == view.selected) {  // tap again to put the piece back
    selectSquare(SQ_NONE);
    return;
  }

  if (view.targets & square_bb(s)) {
    click();
    bool isPromotion;
    Move m = game.findMove(view.selected, s, QUEEN, isPromotion);
    if (isPromotion) {
      promotionPending = true;
      promoFrom = view.selected;
      promoTo = s;
      uiDrawPromotionPicker(humanColor);
      return;
    }
    if (m != Move::none()) playerMove(m);
    return;
  }

  // Another of our pieces that can move: pick that one up instead.
  // Anything else is an illegal move: warn and put the piece back.
  if (canMove) {
    click();
    selectSquare(s);
  } else {
    warning();
    selectSquare(SQ_NONE);
  }
}

void handlePanelTap(int x, int y) {
  if (promotionPending) {
    PieceType pt = uiPromotionAt(x, y);
    if (pt != NO_PIECE_TYPE) {
      bool isPromotion;
      Move m = game.findMove(promoFrom, promoTo, pt, isPromotion);
      promotionPending = false;
      if (m != Move::none()) playerMove(m);
      return;
    }
  }

  PanelButton b = uiButtonAt(x, y);
  if (b == BTN_NONE) return;
  uiFlashButton(b);
  waitForRelease();

  if (promotionPending) cancelPromotion();

  switch (b) {
    case BTN_HINT:
      if (state == State::PlayerTurn) showHint();
      break;
    case BTN_UNDO:
      if (canUndo()) undoMove();
      break;
    case BTN_NEW: newGameDialog(); break;
    default: break;
  }
}

// ===========================
// Serial console: self-tests
// ===========================

struct PerftCase {
  const char *fen;
  int depth;
  uint64_t nodes;
};

// Standard perft positions (chessprogramming.org/Perft_Results)
const PerftCase PERFT_CASES[] = {
  { "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 5, 4865609ULL },
  { "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 4, 4085603ULL },
  { "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 5, 674624ULL },
  { "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 4, 422333ULL },
  { "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 4, 2103487ULL },
  { "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", 4, 3894594ULL },
};

void runPerft() {
  Serial.println("Perft self-test");
  int failures = 0;
  for (const PerftCase &c : PERFT_CASES) {
    Position pos;
    StateInfo st;
    pos.set(c.fen, &st);
    uint32_t t0 = millis();
    uint64_t n = Engine::perft(pos, c.depth);
    uint32_t ms = millis() - t0;
    bool ok = n == c.nodes;
    failures += !ok;
    Serial.printf("  depth %d  %10lu nodes  %6lu ms  %s  %s\n", c.depth, (unsigned long)n, (unsigned long)ms,
                  ok ? "OK  " : "FAIL", c.fen);
  }
  Serial.printf("Perft: %s\n", failures ? "FAILED" : "all passed");
}

void runBench() {
  struct BenchCase {
    const char *fen;
    const char *note;
  };
  const BenchCase cases[] = {
    { "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", "start position" },
    { "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", "middlegame" },
    { "r1bqkb1r/pppp1ppp/2n2n2/4p2Q/2B1P3/8/PPPP1PPP/RNB1K1NR w KQkq - 4 4", "mate in 1: h5f7" },
    { "6k1/5ppp/8/8/8/8/5PPP/3R2K1 w - - 0 1", "back rank mate in 1: d1d8" },
    { "8/8/8/4k3/8/8/8/4K2Q w - - 0 1", "KQ vs K: should see a mate" },
  };

  Serial.println("Search benchmark (3 s per position)");
  uint64_t totalNodes = 0;
  uint32_t totalMs = 0;
  for (const BenchCase &c : cases) {
    Position pos;
    StateInfo st;
    pos.set(c.fen, &st);
    Engine::new_game();
    Search::LimitsType limits;
    limits.movetime = 3000;
    Search::Result r = Engine::think(pos, limits, MAX_LEVEL);
    char uci[8];
    Engine::move_to_uci(r.best, uci);
    Serial.printf("  %-6s score %6d  depth %2d  nodes %8lu  %5lu ms   %s\n", uci, r.score, r.depth,
                  (unsigned long)r.nodes, (unsigned long)r.time, c.note);
    totalNodes += r.nodes;
    totalMs += r.time;
  }
  Serial.printf("Total %lu nodes in %lu ms: %lu nodes/s\n", (unsigned long)totalNodes, (unsigned long)totalMs,
                (unsigned long)(totalMs ? totalNodes * 1000 / totalMs : 0));
  Engine::new_game();
}

void serialCommands() {
  if (!Serial.available()) return;
  char c = Serial.read();
  switch (c) {
    case 'p': runPerft(); break;
    case 'b':
      runBench();
      break;
    case 'f':
      {
        char fen[100];
        game.pos().fen(fen);
        Serial.println(fen);
        break;
      }
    case 'c':
      runTouchCalibration();
      redrawScreen();
      break;
    default: break;
  }
}

// ===========================
// Arduino entry points
// ===========================

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 1500) {}
  Serial.println("Teensy Chess - Stockfish port");

  // Keep the touch controller deselected while the display starts
  pinMode(TOUCH_CS, OUTPUT);
  digitalWrite(TOUCH_CS, HIGH);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  tft.begin();
  tft.setRotation(3);  // landscape, 320 x 240
  touch.begin();
  touch.setRotation(3);  // match the display orientation

  uiSplash();

  uint32_t t0 = millis();
  Engine::init();
  Serial.printf("Engine initialized in %lu ms\n", (unsigned long)(millis() - t0));

  bool haveSettings = loadSettings();
  if (!haveSettings || calibrationRequested(200)) runTouchCalibration();

  startGame();
}

void loop() {
  serialCommands();

  if (state == State::EngineTurn) {
    engineMove();
    return;
  }

  int x, y;
  if (!getTap(x, y)) return;

  if (x < BOARD_PX)
    handleBoardTap(uiSquareAt(x, y, view.flipped));
  else
    handlePanelTap(x, y);
}
