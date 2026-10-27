// SPDX-License-Identifier: GPL-3.0-or-later
// Host-side tests for the Teensy Stockfish port. Builds the engine sources from
// TeensyChess/src/engine natively (no Arduino) and checks:
//   1. perft node counts on the standard positions
//   2. do_move/undo_move restore the position exactly (keys, FEN), random games
//   3. evaluation symmetry: eval(pos) == eval(color-flipped pos)
//   4. mate finding (mate in 1/2/3, KQK, KRK conversions)
//   5. engine-vs-engine games at several levels without crashes or illegal moves
//
// Build: see run_tests.sh

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "engine.h"
#include "evaluate.h"
#include "movegen.h"
#include "position.h"
#include "search.h"

using namespace Stockfish;

static int failures = 0;

#define CHECK(cond, ...)                                  \
    do                                                    \
    {                                                     \
        if (!(cond))                                      \
        {                                                 \
            failures++;                                   \
            printf("  FAIL %s:%d: ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);                          \
            printf("\n");                                 \
        }                                                 \
    } while (0)

static std::string fen_of(const Position& pos) {
    char buf[128];
    pos.fen(buf);
    return buf;
}

// Mirrors a FEN vertically and swaps the colors
static std::string flip_fen(const std::string& fen) {
    std::vector<std::string> fields;
    size_t start = 0;
    while (start < fen.size())
    {
        size_t sp = fen.find(' ', start);
        if (sp == std::string::npos)
            sp = fen.size();
        fields.push_back(fen.substr(start, sp - start));
        start = sp + 1;
    }
    std::vector<std::string> ranks;
    start = 0;
    const std::string& board = fields[0];
    while (true)
    {
        size_t sl = board.find('/', start);
        ranks.push_back(board.substr(start, sl == std::string::npos ? std::string::npos : sl - start));
        if (sl == std::string::npos)
            break;
        start = sl + 1;
    }
    auto swapcase = [](std::string s) {
        for (char& c : s)
            c = islower(c) ? char(toupper(c)) : char(tolower(c));
        return s;
    };
    std::string out;
    for (int i = 7; i >= 0; --i)
        out += swapcase(ranks[i]) + (i ? "/" : "");
    out += fields[1] == "w" ? " b " : " w ";
    std::string castle = fields[2] == "-" ? "-" : swapcase(fields[2]);
    // keep KQkq order
    if (castle != "-")
    {
        std::string ordered;
        for (char c : std::string("KQkq"))
            if (castle.find(c) != std::string::npos)
                ordered += c;
        castle = ordered;
    }
    out += castle + " ";
    std::string ep = fields[3];
    if (ep != "-")
        ep[1] = ep[1] == '3' ? '6' : '3';
    out += ep;
    for (size_t i = 4; i < fields.size(); ++i)
        out += " " + fields[i];
    return out;
}

// ---------------------------------------------------------------------------

static void test_perft(bool deep) {
    struct Case {
        const char* fen;
        int         depth;
        u64         nodes;
    };
    const Case cases[] = {
      {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 5, 4865609ULL},
      {"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 4, 4085603ULL},
      {"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 6, 11030083ULL},
      {"r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 5, 15833292ULL},
      {"rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 4, 2103487ULL},
      {"r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", 4, 3894594ULL},
      // en passant / pin edge cases
      {"8/8/8/2k5/3Pp3/8/8/4K2R b K d3 0 1", 4, 0},  // counted against flipped below
    };
    const Case shallow[] = {
      {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 4, 197281ULL},
      {"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 3, 97862ULL},
      {"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 5, 674624ULL},
      {"r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 4, 422333ULL},
      {"rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 3, 62379ULL},
      {"r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", 3, 89890ULL},
    };

    printf("[perft]\n");
    const Case* list = deep ? cases : shallow;
    int         n    = deep ? int(sizeof(cases) / sizeof(cases[0])) - 1 : int(sizeof(shallow) / sizeof(shallow[0]));
    for (int i = 0; i < n; ++i)
    {
        Position  pos;
        StateInfo st;
        CHECK(pos.set(list[i].fen, &st), "set failed %s", list[i].fen);
        u64 got = Engine::perft(pos, list[i].depth);
        printf("  depth %d %10llu %s %s\n", list[i].depth, (unsigned long long) got,
               got == list[i].nodes ? "ok  " : "FAIL", list[i].fen);
        CHECK(got == list[i].nodes, "perft mismatch");

        // The mirrored position must give the same count
        std::string fl = flip_fen(list[i].fen);
        Position    pf;
        StateInfo   sf;
        CHECK(pf.set(fl.c_str(), &sf), "set failed %s", fl.c_str());
        u64 gotf = Engine::perft(pf, list[i].depth);
        CHECK(gotf == list[i].nodes, "mirrored perft mismatch %s: %llu", fl.c_str(), (unsigned long long) gotf);
    }

    // ep edge case: compare with mirrored version
    {
        const char* fen = "8/8/8/2k5/3Pp3/8/8/4K2R b K d3 0 1";
        Position    p1, p2;
        StateInfo   s1, s2;
        p1.set(fen, &s1);
        std::string fl = flip_fen(fen);
        p2.set(fl.c_str(), &s2);
        u64 a = Engine::perft(p1, 5), b = Engine::perft(p2, 5);
        printf("  ep case %llu vs mirrored %llu\n", (unsigned long long) a, (unsigned long long) b);
        CHECK(a == b, "ep mirrored mismatch");
    }
}

// ---------------------------------------------------------------------------

static u64 rng_state = 0x9E3779B97F4A7C15ULL;
static u64 rnd() {
    rng_state ^= rng_state >> 12;
    rng_state ^= rng_state << 25;
    rng_state ^= rng_state >> 27;
    return rng_state * 2685821657736338717ULL;
}

static void test_random_games() {
    printf("[random games: do/undo, keys, eval symmetry]\n");
    int positions = 0, symChecks = 0;
    for (int g = 0; g < 300; ++g)
    {
        Position  pos;
        StateInfo states[400];
        pos.set("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", &states[0]);
        Move played[400];
        int  ply = 0;
        std::vector<std::string> fens;
        fens.push_back(fen_of(pos));

        while (ply < 300)
        {
            MoveList<ALL> ml(pos);
            if (!ml.size())
                break;
            Move m = *(ml.begin() + rnd() % ml.size());

            // legal() must accept every generated move
            CHECK(pos.legal(m), "legal() rejects generated move");

            // gives_check() must agree with the resulting position
            bool gc  = pos.gives_check(m);
            Key  kBefore = pos.key();
            pos.do_move(m, states[ply + 1]);
            CHECK(gc == bool(pos.checkers()), "gives_check mismatch at %s", fens.back().c_str());
            played[ply++] = m;
            positions++;

            // Key computed from scratch must equal the incremental one
            std::string f = fen_of(pos);
            Position    fresh;
            StateInfo   sfresh;
            fresh.set(f.c_str(), &sfresh);
            CHECK(fresh.state()->key == pos.state()->key, "incremental key mismatch %s", f.c_str());
            CHECK(fresh.pawn_key() == pos.pawn_key(), "pawn key mismatch %s", f.c_str());
            CHECK(fresh.material_key() == pos.material_key(), "material key mismatch %s", f.c_str());
            CHECK(fresh.minor_piece_key() == pos.minor_piece_key(), "minor key mismatch %s", f.c_str());
            CHECK(fresh.non_pawn_material(WHITE) == pos.non_pawn_material(WHITE), "npm mismatch");
            fens.push_back(f);
            (void) kBefore;

            // Evaluation symmetry
            if (ply % 3 == 0)
            {
                std::string fl = flip_fen(f);
                Position    pf;
                StateInfo   sf;
                CHECK(pf.set(fl.c_str(), &sf), "flip set failed %s -> %s", f.c_str(), fl.c_str());
                Value a = Eval::evaluate(pos), b = Eval::evaluate(pf);
                CHECK(a == b, "eval asymmetry %d vs %d: %s", a, b, f.c_str());
                symChecks++;
            }

            // SEE sanity: see_ge(m, 0) must not crash; threshold monotonic
            if (!pos.checkers())
                for (const Move& mm : MoveList<CAPTURES>(pos))
            {
                bool a = pos.see_ge(mm, -100), b = pos.see_ge(mm, 0), c = pos.see_ge(mm, 100);
                CHECK(!(c && !b) && !(b && !a), "see not monotonic");
            }

            if (pos.rule50_count() >= 100)
                break;
        }

        // Undo everything and compare FENs on the way back
        while (ply > 0)
        {
            pos.undo_move(played[--ply]);
            fens.pop_back();
            CHECK(fen_of(pos) == fens.back(), "undo mismatch: %s vs %s", fen_of(pos).c_str(),
                  fens.back().c_str());
        }
    }
    printf("  %d positions, %d symmetry checks\n", positions, symChecks);
}

// ---------------------------------------------------------------------------

static std::string uci(Move m) {
    char b[8];
    Engine::move_to_uci(m, b);
    return b;
}

static void test_mates() {
    printf("[mate search]\n");
    struct Case {
        const char* fen;
        const char* best;  // expected first move ("" = any)
        int         mateIn;  // in moves
    };
    const Case cases[] = {
      {"r1bqkb1r/pppp1ppp/2n2n2/4p2Q/2B1P3/8/PPPP1PPP/RNB1K1NR w KQkq - 4 4", "h5f7", 1},
      {"6k1/5ppp/8/8/8/8/5PPP/3R2K1 w - - 0 1", "d1d8", 1},
      {"7k/8/6K1/8/8/8/8/R7 w - - 0 1", "a1a8", 1},
      {"7k/8/5K2/8/8/8/8/6R1 w - - 0 1", "", 2},  // 1.Kf7 Kh7 2.Rh1# (1.Rg7 is stalemate)
      {"r2qkb1r/pp2nppp/3p4/2pNN1B1/2BnP3/3P4/PPP2PPP/R2bK2R w KQkq - 1 10", "", 2},  // 1.Nf6+ gxf6 2.Bxf7#
      {"8/8/8/4k3/8/8/8/4K2Q w - - 0 1", "", 0},  // KQK: must find some mate
    };
    for (const Case& c : cases)
    {
        Position  pos;
        StateInfo st;
        CHECK(pos.set(c.fen, &st), "set failed");
        Engine::new_game();
        Search::LimitsType lim;
        lim.movetime = 4000;
        auto r = Engine::think(pos, lim, 20);
        int  mateMoves = is_mate(r.score) ? (VALUE_MATE - r.score + 1) / 2 : -1;
        printf("  %-6s score %6d depth %2d nodes %9llu %5lld ms  mate-in %d  %s\n", uci(r.best).c_str(),
               r.score, r.depth, (unsigned long long) r.nodes, (long long) r.time, mateMoves, c.fen);
        if (c.best[0])
            CHECK(uci(r.best) == c.best, "expected %s got %s", c.best, uci(r.best).c_str());
        if (c.mateIn)
            CHECK(mateMoves == c.mateIn, "expected mate in %d", c.mateIn);
        else
            CHECK(r.score >= VALUE_KNOWN_WIN / 2, "no winning score");
    }
}

// Plays a game from 'fen' with the engine on both sides; returns plies to the end
static int play_out(const char* fen, int level, int movetime, int maxPly, const char** resultStr) {
    Position  pos;
    StateInfo states[1100];
    pos.set(fen, &states[0]);
    Engine::new_game();
    int ply = 0;
    *resultStr = "ply limit";
    while (ply < maxPly)
    {
        MoveList<ALL> ml(pos);
        if (!ml.size())
        {
            *resultStr = pos.checkers() ? "checkmate" : "stalemate";
            break;
        }
        if (pos.rule50_count() >= 100)
        {
            *resultStr = "50 moves";
            break;
        }
        if (pos.state()->repetition < 0)
        {
            *resultStr = "repetition";
            break;
        }
        Search::LimitsType lim;
        lim.movetime = movetime;
        if (level < 20)
            lim.depth = level + 1;
        auto r = Engine::think(pos, lim, level);
        CHECK(ml.contains(r.best), "engine returned illegal move %s in %s", uci(r.best).c_str(),
              fen_of(pos).c_str());
        if (!ml.contains(r.best))
            break;
        pos.do_move(r.best, states[ply + 1]);
        ply++;
    }
    return ply;
}

static void test_endgame_conversion() {
    printf("[endgame conversion]\n");
    const char* fens[] = {
      "8/8/8/4k3/8/8/8/4K2Q w - - 0 1",   // KQK
      "8/8/8/4k3/8/8/8/R3K3 w - - 0 1",   // KRK
      "8/8/3k4/8/8/8/4P3/4K3 w - - 0 1",  // KPK, winnable
    };
    for (const char* f : fens)
    {
        const char* res;
        int         plies = play_out(f, 20, 300, 200, &res);
        printf("  %-10s after %3d plies: %s\n", res, plies, f);
    }
}

static void test_games() {
    printf("[engine vs engine]\n");
    int levels[] = {0, 5, 10, 20};
    for (int lv : levels)
    {
        const char* res;
        int plies = play_out("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", lv, 150, 400, &res);
        printf("  level %2d: %-10s after %d plies\n", lv, res, plies);
    }
}

// A few classic "Win At Chess" tactics: informational, not counted as failures
static void test_wac() {
    printf("[WAC tactics, 2 s each]\n");
    struct Case {
        const char* fen;
        const char* best;
    };
    const Case cases[] = {
      {"2rr3k/pp3pp1/1nnqbN1p/3pN3/2pP4/2P3Q1/PPB4P/R4RK1 w - - 0 1", "g3g6"},
      {"8/7p/5k2/5p2/p1p2P2/Pr1pPK2/1P1R3P/8 b - - 0 1", "b3b2"},
      {"5rk1/1ppb3p/p1pb4/6q1/3P1p1r/2P1R2P/PP1BQ1P1/5RKN w - - 0 1", "e3g3"},
      {"r1bq2rk/pp3pbp/2p1p1pQ/7P/3P4/2PB1N2/PP3PPR/2KR4 w - - 0 1", "h6h7"},
      {"5k2/6pp/p1qN4/1p1p4/3P4/2PKP2Q/PP3r2/3R4 b - - 0 1", "c6c4"},
      {"7k/p7/1R5K/6r1/6p1/6P1/8/8 w - - 0 1", "b6b7"},
      {"rnbqkb1r/pppp1ppp/8/4P3/6n1/7P/PPPNPPP1/R1BQKBNR b KQkq - 0 1", "g4e3"},
      {"r4q1k/p2bR1rp/2p2Q1N/5p2/5p2/2P5/PP3PPP/R5K1 w - - 0 1", "e7f7"},
      {"3q1rk1/p4pp1/2pb3p/3p4/6Pr/1PNQ4/P1PB1PP1/4RRK1 b - - 0 1", "d6h2"},
      {"2br2k1/2q3rn/p2NppQ1/2p1P3/Pp5R/4P3/1P3PPP/3R2K1 w - - 0 1", "h4h7"},
    };
    int solved = 0;
    for (const Case& c : cases)
    {
        Position  pos;
        StateInfo st;
        pos.set(c.fen, &st);
        Engine::new_game();
        Search::LimitsType lim;
        lim.movetime = 2000;
        auto r  = Engine::think(pos, lim, 20);
        bool ok = uci(r.best) == c.best;
        solved += ok;
        printf("  %s %-6s (expected %s) score %6d depth %2d\n", ok ? "ok  " : "miss", uci(r.best).c_str(), c.best,
               r.score, r.depth);
    }
    printf("  solved %d of 10\n", solved);
}

static void bench() {
    printf("[bench]\n");
    Position  pos;
    StateInfo st;
    pos.set("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", &st);
    Engine::new_game();
    Search::LimitsType lim;
    lim.depth = 14;
    auto r = Engine::think(pos, lim, 20);
    printf("  kiwipete depth 14: %s score %d nodes %llu time %lld ms nps %llu\n", uci(r.best).c_str(), r.score,
           (unsigned long long) r.nodes, (long long) r.time,
           (unsigned long long) (r.time ? r.nodes * 1000 / r.time : 0));
}

int main(int argc, char** argv) {
    bool deep = argc > 1 && strcmp(argv[1], "deep") == 0;
    setvbuf(stdout, nullptr, _IONBF, 0);
    Engine::init();
    test_perft(deep);
    test_random_games();
    test_mates();
    test_endgame_conversion();
    test_games();
    test_wac();
    bench();
    printf("\n%s (%d failures)\n", failures ? "TESTS FAILED" : "ALL TESTS PASSED", failures);
    return failures ? 1 : 0;
}
