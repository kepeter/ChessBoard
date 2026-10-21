/*
 * Copyright (c) 2026 Kornfeld Eliyahu Peter
 * License: MIT
 */

#ifndef CHESSBOARD_MODELS_PIECE_HPP_
#define CHESSBOARD_MODELS_PIECE_HPP_

#include <cctype>

#include <QString>

namespace chess {
enum class PieceType { kPawn, kKnight, kBishop, kRook, kQueen, kKing };

enum class PieceColor { kWhite, kBlack };

inline PieceColor Opposite(PieceColor color) {
  return color == PieceColor::kWhite ? PieceColor::kBlack : PieceColor::kWhite;
}

// Standard point values, used only for the material badge; King has none.
inline int PieceValue(PieceType type) {
  switch (type) {
    case PieceType::kPawn:
      return 1;
    case PieceType::kKnight:
      return 3;
    case PieceType::kBishop:
      return 3;
    case PieceType::kRook:
      return 5;
    case PieceType::kQueen:
      return 9;
    default:
      return 0;
  }
}

struct Piece {
  PieceType type;
  PieceColor color;
  // Tracks castling rights: a king or rook that has moved can not castle.
  bool has_moved = false;

  Piece(PieceType type, PieceColor color) : type(type), color(color) {}

  char Symbol() const {
    char c;
    switch (type) {
      case PieceType::kPawn: {
        c = 'P';
        break;
      }

      case PieceType::kKnight: {
        c = 'N';
        break;
      }

      case PieceType::kBishop: {
        c = 'B';
        break;
      }

      case PieceType::kRook: {
        c = 'R';
        break;
      }

      case PieceType::kQueen: {
        c = 'Q';
        break;
      }

      case PieceType::kKing: {
        c = 'K';
        break;
      }

      default: {
        c = '?';
        break;
      }
    }

    if (color == PieceColor::kBlack) c = static_cast<char>(std::tolower(c));

    return c;
  }

  // Unicode chess glyph; both colors use the "black" range and are colored by
  // the board widget.
  QString Glyph() const {
    switch (type) {
      case PieceType::kKing:
        return QString(QChar(0x265A));
      case PieceType::kQueen:
        return QString(QChar(0x265B));
      case PieceType::kRook:
        return QString(QChar(0x265C));
      case PieceType::kBishop:
        return QString(QChar(0x265D));
      case PieceType::kKnight:
        return QString(QChar(0x265E));
      case PieceType::kPawn:
        return QString(QChar(0x265F));
      default:
        return QString();
    }
  }
};
}  // namespace chess

#endif  // CHESSBOARD_MODELS_PIECE_HPP_
