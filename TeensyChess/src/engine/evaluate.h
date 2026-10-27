/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// Teensy port: current Stockfish evaluates with an NNUE network whose weights
// are far larger than the Teensy 4.0's 2 MB of flash. This is a handcrafted
// evaluation in the style of Stockfish 11, the last release with a classical
// evaluation function.

#ifndef EVALUATE_H_INCLUDED
#define EVALUATE_H_INCLUDED

#include "types.h"

namespace Stockfish {

class Position;

namespace Eval {

// One pawn in the endgame, used to convert internal values to pawns for display
constexpr Value PawnValueEg = 213;

void init();

// Static evaluation from the point of view of the side to move
Value evaluate(const Position& pos);

}  // namespace Eval

}  // namespace Stockfish

#endif  // #ifndef EVALUATE_H_INCLUDED
