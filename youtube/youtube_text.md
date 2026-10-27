# YouTube upload text for Teensy Chess

Source code: https://github.com/KrisKasprzak/TeensyChess

## Title

I Put STOCKFISH on a $24 Microcontroller… and It DESTROYED Me ♟️

Alternatives:
- Stockfish Chess Engine on a Teensy 4.0 – No PC, No Internet! ♟️
- Can a $24 Teensy Beat Me at Chess? Stockfish on a Microcontroller
- I Built a Pocket Chess Computer That Runs STOCKFISH (Teensy 4.0)

## Description

I ported Stockfish, the legendary open-source chess engine, to a $24 Teensy 4.0 microcontroller with a 2.8" touch screen, and now I can't beat my own project. ♟️🔥

Everything runs on the Teensy's 600 MHz ARM Cortex-M7: no PC, no phone, no internet. Just tap a piece, tap where it goes, and TeensyFish (my Stockfish port) answers within seconds.

🧠 HOW DOES STOCKFISH FIT ON A MICROCONTROLLER?
Modern Stockfish uses an NNUE neural network that is tens of megabytes, but the Teensy has only 2 MB of flash and 1 MB of RAM. So TeensyFish keeps Stockfish's modern search, move generation and transposition table, and swaps the neural network for the handcrafted evaluation from Stockfish 11. Huge lookup tables were replaced with compact bit tricks, and the memory was squeezed into both RAM banks. It's still strong enough to crush most club players at full strength.

✨ FEATURES
♟️ 21 difficulty levels, from beginner to full strength
♟️ Play as white or black (the board flips)
♟️ Touch to move, with legal-move dots and last-move highlights
♟️ HINT button: the engine suggests your best move
♟️ UNDO and NEW GAME buttons
♟️ Material score and live evaluation bar
♟️ Checkmate, stalemate and draw detection, plus pawn promotion
♟️ Buzzer clicks on every move, and a warning beep on illegal moves
♟️ Touch calibration saved in EEPROM

🛠️ HARDWARE
• Teensy 4.0 (600 MHz ARM Cortex-M7, 1 MB RAM, 2 MB flash)
• 2.8" ILI9341 320×240 SPI TFT with XPT2046 touch
• Passive buzzer
• A few jumper wires

💾 SOURCE CODE, WIRING AND BUILD INSTRUCTIONS
https://github.com/KrisKasprzak/TeensyChess
Free and open source under the GPLv3, like Stockfish itself.

⏱️ CHAPTERS
0:00 Can a $24 board play like Stockfish?
0:00 The hardware
0:00 Squeezing Stockfish into 1 MB of RAM
0:00 Playing a game
0:00 Hints, undo and difficulty levels
0:00 Getting checkmated…
0:00 How to build your own
(Update the timestamps after editing; the first chapter must start at 0:00.)

🙏 CREDITS
Stockfish by the Stockfish developers: https://stockfishchess.org
Teensy and the display/touch libraries by Paul Stoffregen / PJRC: https://www.pjrc.com
TeensyFish is an unofficial port and is not affiliated with the Stockfish project.

👍 If you liked this, subscribe for more microcontroller projects, and tell me in the comments: could YOU beat TeensyFish at level 20?

#Stockfish #Teensy #Chess #Arduino #Microcontroller #DIY #ChessEngine #Electronics

## Tags

Paste into YouTube Studio's Tags field (comma separated, 472 of the 500 allowed characters):

stockfish, teensy, teensy 4.0, chess, chess engine, stockfish chess, arduino chess, microcontroller chess, diy chess computer, chess computer, electronic chess board, touch screen chess, ili9341, tft display, xpt2046, arduino project, teensy project, embedded systems, arm cortex m7, open source chess, chess ai, play chess against computer, stockfish port, homemade chess computer, electronics project, maker project, pjrc, nnue, chess programming, can you beat stockfish
