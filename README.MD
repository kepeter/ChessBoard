[!WARNING]
**Disclaimer:** This project was a test to learn how to work with AI. About 80% of the code was created using Copilot. Beware...

# ChessBoard

A desktop chess app for playing against a UCI engine (such as Stockfish) with a coach-style helper alongside: live analysis, move feedback and optional on-board hints. Written in C++20 with Qt 6.

## Features

**Playing**
- Play against any UCI engine, as White, Black or random. Chess960 is supported.
- Selectable playing strength, from beginner presets up to full strength.
- Time controls with increment, with a per-move time column in the move list.
- Start from a custom FEN position.
- Full rules: castling, en passant, promotion, check, checkmate, stalemate, and draws by repetition, the fifty-move rule and insufficient material.
- Optional pondering: the engine thinks on your time.

**Help for the human player** (each is a toggle in the View menu)
- Evaluation bar.
- Best lines, with the number of lines set by the engine's MultiPV option.
- Analysis arrows on the board for the top line, three move pairs deep, with different colours for you and the engine.
- Move classification (for example a blunder or a good move), also shown as a badge on the board.

## Requirements

- A C++20 compiler and CMake 3.20 or newer.
- Qt 6.2 or newer with the **Widgets** and **Multimedia** modules.
- A UCI chess engine, for example [Stockfish](https://stockfishchess.org/). The default engine command is `stockfish`, so it should be on your `PATH`, or you can set the path in Settings.

## Project layout

| Folder | Contents |
|---|---|
| `src/engine` | Chess rules, move generation, FEN and SAN |
| `src/uci` | UCI engine process handling (a worker thread per engine process) |
| `src/session` | Game flow, clock, engine requests, analysis and move classification |
| `src/ui` | Qt widgets: board, sidebar, analysis panel, dialogs |
| `src/config` | Saved settings |
| `src/models` | Small shared types (moves, pieces, squares, time control) |
| `assets/` | Fonts, board textures and the move sound |

Code follows [Google's C++ Style Guide](https://google.github.io/styleguide/cppguide.html); formatting is defined by [.clang-format](.clang-format) (run `clang-format -i` on the sources).

## Status

More to add. Planned ideas are listed in [NEW_FEATURES.MD](NEW_FEATURES.MD). 

## License

The app's code is released under the [MIT License](LICENSE). The bundled fonts keep their own licences (see Credits).

## Credits

Piece fonts (DejaVu Sans, FreeSerif, Nishiki-teki, Noto Sans Symbols 2, STIX Two Math) come with their own licences. The About dialog shows each one.
