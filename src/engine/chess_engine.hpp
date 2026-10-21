/*
 * Copyright (c) 2026 Kornfeld Eliyahu Peter
 * License: MIT
 */

#ifndef CHESSBOARD_ENGINE_CHESS_ENGINE_HPP_
#define CHESSBOARD_ENGINE_CHESS_ENGINE_HPP_

#include <array>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "models/move.hpp"
#include "models/piece.hpp"
#include "models/square.hpp"

namespace chess {
enum class DrawReason {
  kNone,
  kStalemate,
  kFiftyMoveRule,
  kThreefoldRepetition,
  kInsufficientMaterial
};

class ChessEngine {
 public:
  ChessEngine();

  // Standard starting position, or a random Chess960 back rank mirrored for
  // both sides.
  void Reset(bool chess960 = false);

  // Replaces the position from a FEN. Chess960 is inferred from Shredder-style
  // file letters in the castling field, not from king placement (a mid-game
  // king may have left its start file); with "-" it defaults to standard, which
  // is harmless since no castling remains. Returns false, leaving the position
  // unchanged, for malformed or impossible positions: not one king per side,
  // pawns on the back ranks, more than 8 pawns, the side not to move in check,
  // or an en passant square no double push could produce.
  bool LoadFen(const std::string& fen);

  PieceColor SideToMove() const { return side_to_move_; }
  const std::vector<Move>& History() const { return history_; }

  bool IsCheck() const { return is_check_; }
  bool IsCheckmate() const { return is_checkmate_; }
  bool IsStalemate() const { return is_stalemate_; }
  bool IsDraw() const { return draw_reason_ != DrawReason::kNone; }

  DrawReason GetDrawReason() const { return draw_reason_; }

  std::optional<Piece> PieceAt(Square sq) const;
  std::vector<Square> GetLegalMoves(Square from) const;
  // promotion_type applies only to pawn promotions (default Queen); King or
  // Pawn is rejected.
  std::optional<Move> TryMakeMove(
      Square from, Square to,
      std::optional<PieceType> promotion_type = std::nullopt);
  std::optional<Square> FindKing(PieceColor color) const;
  std::string ToFen() const;
  bool IsChess960() const { return chess960_; }

  // File the king-side or queen-side rook started on (0/7 in standard chess). A
  // castling Move::to targets this square, so callers translating other
  // castling notations must use it.
  int RookStartFile(bool king_side) const {
    return rook_start_file_[king_side ? 1 : 0];
  }

  // The square the moved piece ends on: Move::to except for castling, where it
  // is the king's real destination.
  static Square LandingSquare(const Move& move);

  // File the king lands on after a castle; Move::to holds the rook's square
  // instead. Returns nullopt for non-castling moves.
  static std::optional<int> CastleKingDestinationFile(MoveSpecial special);

 private:
  // Indexed [file][rank], both 0-7.
  using Board = std::array<std::array<std::optional<Piece>, 8>, 8>;

  Board board_{};
  PieceColor side_to_move_ = PieceColor::kWhite;
  std::vector<Move> history_;
  // Square capturable en passant; set only right after a double push.
  std::optional<Square> en_passant_target_;

  bool is_check_ = false;
  bool is_checkmate_ = false;
  bool is_stalemate_ = false;

  DrawReason draw_reason_ = DrawReason::kNone;

  // Rook files (index 0 = queen-side, 1 = king-side) and king file shared by
  // both colors, since both start from the same back rank. Set by
  // Reset()/LoadFen().
  bool chess960_ = false;
  int king_start_file_ = 4;
  int rook_start_file_[2] = {0, 7};

  // Half-moves since the last pawn move or capture; 100 triggers the fifty-move
  // draw.
  int halfmove_clock_ = 0;

  // Fullmove number of the starting position. ToFen() derives the current
  // number from this plus the plies played; if Black moved first, that first
  // ply already completes the starting fullmove.
  int starting_fullmove_number_ = 1;
  bool started_with_black_to_move_ = false;

  // Occurrences of each RepetitionKey(), for threefold repetition.
  std::unordered_map<std::string, int> position_counts_;

  void PlaceBackRank(int rank, PieceColor color, const PieceType order[8]);
  static void RandomChess960BackRank(PieceType order[8]);
  // Board, side to move, castling rights and en passant square, so identical
  // positions share a key.
  std::string RepetitionKey() const;
  std::string EnPassantKeyPart() const;
  bool IsInsufficientMaterial() const;

  bool DestinationHoldsOwnUnmovedRook(Square square, PieceColor color) const;
  Move BuildMove(Square from, Square to, const Piece& piece,
                 std::optional<PieceType> promotion_type) const;
  void ApplyMove(const Move& move);
  void ApplyCastle(int rank, int rook_from_file, int king_to_file,
                   int rook_to_file);
  void UpdateGameStatus();

  // Pseudo-legal: follows piece movement but may leave the mover's king in
  // check.
  std::vector<Square> GeneratePseudoLegalMoves(Square from,
                                               const Piece& piece) const;
  std::vector<Square> PawnMoves(Square from, PieceColor color) const;
  std::vector<Square> KnightMoves(Square from, PieceColor color) const;
  std::vector<Square> KingMoves(Square from, PieceColor color) const;
  std::vector<Square> StepMoves(
      Square from, PieceColor color,
      const std::vector<std::pair<int, int>>& offsets) const;
  bool HasPieceAtOffsets(Square square, PieceColor color, PieceType type,
                         const std::vector<std::pair<int, int>>& offsets) const;
  std::vector<Square> CastlingMoves(Square from, PieceColor color) const;
  bool CanCastleTo(int rank, PieceColor color, int rook_file, int king_to_file,
                   int rook_to_file) const;
  std::vector<Square> SlidingMoves(
      Square from, PieceColor color,
      const std::vector<std::pair<int, int>>& dirs) const;

  // Plays the move on a scratch copy to see whether it exposes the mover's
  // king.
  bool MovesIntoCheck(Square from, Square to, const Piece& piece) const;
  bool IsKingInCheck(PieceColor color) const;
  bool IsSquareAttacked(Square square, PieceColor by_color) const;
  // match_a/match_b are the piece types attacking along dirs (e.g. Bishop and
  // Queen on diagonals).
  bool IsAttackedAlongDirs(Square square, PieceColor by_color,
                           const std::vector<std::pair<int, int>>& dirs,
                           PieceType match_a, PieceType match_b) const;

  std::vector<Square> AllPiecesOfColor(PieceColor color) const;

  bool CanCastle(PieceColor color, bool king_side) const;

  // SAN disambiguation for `from` -> `to`: empty when no rival of the same type
  // can reach `to`; else the origin file, else the rank, else both (Nbd2, R1a3,
  // Qa1e5). Must be called before the move is applied.
  std::string Disambiguation(Square from, Square to, const Piece& piece) const;

  // `disambiguation` must come from Disambiguation() computed before the move
  // was applied.
  std::string BuildNotation(const Move& move,
                            const std::string& disambiguation) const;
  std::string AppendCheckSuffix(const std::string& san, const Move& move) const;
};
}  // namespace chess

#endif  // CHESSBOARD_ENGINE_CHESS_ENGINE_HPP_
