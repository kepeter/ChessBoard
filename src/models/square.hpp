/*
 * Copyright (c) 2026 Kornfeld Eliyahu Peter
 * License: MIT
 */

#ifndef CHESSBOARD_MODELS_SQUARE_HPP_
#define CHESSBOARD_MODELS_SQUARE_HPP_

#include <string>

namespace chess {

struct Square {
  int file = 0;
  int rank = 0;

  Square() = default;
  Square(int file, int rank) : file(file), rank(rank) {}

  bool IsOnBoard() const {
    return file >= 0 && file < 8 && rank >= 0 && rank < 8;
  }

  std::string Algebraic() const {
    std::string s;
    s += static_cast<char>('a' + file);
    s += static_cast<char>('1' + rank);

    return s;
  }

  // Returns an off-board square (IsOnBoard() is false) for malformed input.
  static Square Parse(const std::string& algebraic) {
    if (algebraic.size() < 2) return Square(-1, -1);

    int file = algebraic[0] - 'a';
    int rank = algebraic[1] - '1';

    return Square(file, rank);
  }

  bool operator==(const Square& other) const {
    return file == other.file && rank == other.rank;
  }
  bool operator!=(const Square& other) const { return !(*this == other); }
};
}  // namespace chess

// Hash for unordered containers; file*8+rank is unique for on-board squares.
template <>
struct std::hash<chess::Square> {
  size_t operator()(const chess::Square& s) const noexcept {
    return static_cast<size_t>(s.file) * 8 + static_cast<size_t>(s.rank);
  }
};

#endif  // CHESSBOARD_MODELS_SQUARE_HPP_
