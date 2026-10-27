// SPDX-License-Identifier: GPL-3.0-or-later
// Renders the Teensy Chess screens on a PC using the sketch's own drawing code and
// a framebuffer stand-in for the display, and saves them as PNG files.
// Build and run: tools/ui_preview/run_preview.sh

#include <cstdio>
#include <string>
#include <vector>

#include "config.h"
#include "game.h"
#include "hardware.h"
#include "storage.h"
#include "touch_input.h"
#include "ui.h"
#include "src/engine/engine.h"
#include "src/engine/movegen.h"

using namespace Stockfish;

MockSerial Serial;
ILI9341_t3 tft(TFT_CS, TFT_DC);
XPT2046_Touchscreen touch(TOUCH_CS);

static std::string outDir = ".";

// ---------------------------------------------------------------------------
// Minimal PNG writer (stored deflate blocks, no compression)
// ---------------------------------------------------------------------------
static uint32_t crcTable[256];
static void initCrc() {
  for (uint32_t n = 0; n < 256; n++) {
    uint32_t c = n;
    for (int k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
    crcTable[n] = c;
  }
}
static uint32_t crc(const std::vector<uint8_t> &d, size_t start) {
  uint32_t c = 0xFFFFFFFFu;
  for (size_t i = start; i < d.size(); i++) c = crcTable[(c ^ d[i]) & 0xFF] ^ (c >> 8);
  return c ^ 0xFFFFFFFFu;
}
static void put32(std::vector<uint8_t> &v, uint32_t x) {
  for (int i = 3; i >= 0; i--) v.push_back(uint8_t(x >> (8 * i)));
}
static void chunk(std::vector<uint8_t> &png, const char *type, const std::vector<uint8_t> &data) {
  put32(png, uint32_t(data.size()));
  size_t start = png.size();
  png.insert(png.end(), type, type + 4);
  png.insert(png.end(), data.begin(), data.end());
  put32(png, crc(png, start));
}

static void savePng(const char *name, int scale = 2) {
  const int W = ILI9341_t3::W * scale, H = ILI9341_t3::H * scale;
  std::vector<uint8_t> raw;
  for (int y = 0; y < H; y++) {
    raw.push_back(0);
    for (int x = 0; x < W; x++) {
      uint16_t c = tft.fb[(y / scale) * ILI9341_t3::W + x / scale];
      raw.push_back(uint8_t(((c >> 11) & 31) * 255 / 31));
      raw.push_back(uint8_t(((c >> 5) & 63) * 255 / 63));
      raw.push_back(uint8_t((c & 31) * 255 / 31));
    }
  }
  std::vector<uint8_t> z = {0x78, 0x01};
  uint32_t a = 1, b = 0;
  for (uint8_t v : raw) {
    a = (a + v) % 65521;
    b = (b + a) % 65521;
  }
  for (size_t i = 0; i < raw.size(); i += 65535) {
    size_t n = std::min<size_t>(65535, raw.size() - i);
    z.push_back(i + n >= raw.size() ? 1 : 0);
    z.push_back(uint8_t(n));
    z.push_back(uint8_t(n >> 8));
    z.push_back(uint8_t(~n));
    z.push_back(uint8_t(~n >> 8));
    z.insert(z.end(), raw.begin() + i, raw.begin() + i + n);
  }
  put32(z, (b << 16) | a);

  std::vector<uint8_t> png = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
  std::vector<uint8_t> ihdr;
  put32(ihdr, W);
  put32(ihdr, H);
  ihdr.insert(ihdr.end(), {8, 2, 0, 0, 0});
  chunk(png, "IHDR", ihdr);
  chunk(png, "IDAT", z);
  chunk(png, "IEND", {});
  std::string path = outDir + "/" + name;
  FILE *f = fopen(path.c_str(), "wb");
  fwrite(png.data(), 1, png.size(), f);
  fclose(f);
  printf("saved %s\n", path.c_str());
}

// ---------------------------------------------------------------------------

static ChessGame game;

static Move uciMove(const char *s) {
  for (const Move &m : MoveList<ALL>(game.pos())) {
    char b[8];
    Engine::move_to_uci(m, b);
    if (!strcmp(b, s)) return m;
  }
  printf("bad move %s\n", s);
  return Move::none();
}

static void play(const char *moves) {
  char buf[8];
  while (*moves) {
    int n = 0;
    while (*moves && *moves != ' ') buf[n++] = *moves++;
    buf[n] = 0;
    while (*moves == ' ') moves++;
    game.makeMove(uciMove(buf));
  }
}

static void drawScreen(const BoardView &view, const char *s1, uint16_t c1, const char *s2, const char *hint,
                       bool hasEval, int evalWhite, bool hintOn, bool undoOn) {
  uiInvalidateBoard();
  uiDrawPanel(settings.level);
  uiDrawBoard(game.pos(), view);
  uiDrawStatus(s1, c1, s2, COLOR_PANEL_TEXT);
  uiDrawPoints(game.materialPoints(WHITE), game.materialPoints(BLACK), hasEval, evalWhite);
  uiDrawHint(hint);
  uiDrawButtons(hintOn, undoOn, true);
}

static const char *pendingShot = nullptr;
static void shotBeforeTap() {
  if (pendingShot) {
    savePng(pendingShot);
    pendingShot = nullptr;
  }
}

int main(int argc, char **argv) {
  if (argc > 1) outDir = argv[1];
  initCrc();
  Engine::init();

  // Identity touch calibration: raw touch values are screen coordinates
  settings.calLeft = 0;
  settings.calRight = 319;
  settings.calTop = 0;
  settings.calBottom = 239;
  settings.level = 10;
  touch.beforeTap = shotBeforeTap;

  // 1. Splash
  uiSplash();
  savePng("1_splash.png");

  // 2. Start of a game, player has white
  game.reset();
  BoardView view;
  drawScreen(view, "Your move", COLOR_GOOD, "You are white", "", false, 0, true, false);
  savePng("2_start.png");

  // 3. Middlegame: a piece selected, Stockfish's last move, a hint and an evaluation
  play("e2e4 e7e5 g1f3 b8c6 f1c4 g8f6 f3g5 d7d5 e4d5 c6a5");
  view.lastFrom = SQ_C6;
  view.lastTo = SQ_A5;
  Search::LimitsType lim;
  lim.movetime = 300;
  auto r = Engine::think(game.pos(), lim, 20);
  char hint[16];
  game.san(r.best, hint);
  view.hintFrom = r.best.from_sq();
  view.hintTo = displayTo(r.best);
  view.selected = SQ_C4;
  view.targets = game.targets(SQ_C4);
  drawScreen(view, "Your move", COLOR_GOOD, "TF: Na5", hint, true, r.score, true, true);
  savePng("3_selected_hint.png");

  // 4. Checkmate against the player (playing black, so the board is flipped)
  game.reset();
  play("e2e4 e7e5 d1h5 b8c6 f1c4 g8f6 h5f7");
  view = BoardView();
  view.flipped = true;
  view.lastFrom = SQ_H5;
  view.lastTo = SQ_F7;
  drawScreen(view, "Checkmate!", COLOR_BAD, "You lost", "", true, VALUE_MATE, false, true);
  savePng("4_mate_flipped.png");

  // 5. Promotion picker
  game.reset();
  Position &p = game.pos();
  (void)p;
  game.reset();
  play("a2a4 b7b5 a4b5 a7a6 b5a6 c8b7 a6b7 h7h6");
  view = BoardView();
  view.selected = SQ_B7;
  view.targets = game.targets(SQ_B7);
  drawScreen(view, "Your move", COLOR_GOOD, "TF: h6", "", true, 400, true, true);
  uiDrawPromotionPicker(WHITE);
  savePng("5_promotion.png");

  // 6. New game dialog: tap Black, level +, then Start
  touch.tapCount = 0;
  touch.tapIndex = 0;
  auto queueTap = [](int x, int y) {
    touch.tapX[touch.tapCount] = x;
    touch.tapY[touch.tapCount] = y;
    touch.tapCount++;
  };
  queueTap(200, 80);   // Black
  queueTap(248, 150);  // +
  queueTap(100, 210);  // Start
  uint8_t white = 1, level = 10;
  pendingShot = "6_new_game_dialog.png";
  bool started = uiNewGameDialog(white, level);
  savePng("6b_new_game_after_taps.png");
  printf("dialog returned %d, white=%d level=%d\n", started, white, level);

  // 7. Touch calibration screens
  touch.tapCount = 0;
  touch.tapIndex = 0;
  queueTap(CAL_MARGIN, CAL_MARGIN);
  queueTap(SCREEN_W - CAL_MARGIN, SCREEN_H - CAL_MARGIN);
  pendingShot = "7_calibration.png";
  runTouchCalibration();
  savePng("7b_calibration_done.png");
  printf("calibration result: L %d R %d T %d B %d\n", settings.calLeft, settings.calRight, settings.calTop,
         settings.calBottom);
  return 0;
}
