/*
 * Copyright (c) 2026 Kornfeld Eliyahu Peter
 * License: MIT
 */

#include "engine/chess_engine.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace chess {
namespace {
const std::vector<std::pair<int, int>> kDiagonalDirs = {
    {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
const std::vector<std::pair<int, int>> kStraightDirs = {
    {1, 0}, {-1, 0}, {0, 1}, {0, -1}};
const std::vector<std::pair<int, int>> kAllDirs = {
    {1, 1}, {1, -1}, {-1, 1}, {-1, -1}, {1, 0}, {-1, 0}, {0, 1}, {0, -1}};
const std::vector<std::pair<int, int>> kKnightOffsets = {
    {1, 2}, {2, 1}, {2, -1}, {1, -2}, {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2}};

// King/rook destination files after castling. Chess960 uses the same final
// squares as standard chess wherever the pieces started.
constexpr int kFinalKingSideKingFile = 6;
constexpr int kFinalKingSideRookFile = 5;
constexpr int kFinalQueenSideKingFile = 2;
constexpr int kFinalQueenSideRookFile = 3;

}  // namespace

std::optional<int> ChessEngine::CastleKingDestinationFile(MoveSpecial special) {
  if (special == MoveSpecial::kCastleKingSide) return kFinalKingSideKingFile;
  if (special == MoveSpecial::kCastleQueenSide) return kFinalQueenSideKingFile;
  return std::nullopt;
}

Square ChessEngine::LandingSquare(const Move& move) {
  if (auto king_file = CastleKingDestinationFile(move.special))
    return Square(*king_file, move.from.rank);
  return move.to;
}

ChessEngine::ChessEngine() { Reset(); }

void ChessEngine::Reset(bool chess960) {
  board_ = Board{};
  history_.clear();
  side_to_move_ = PieceColor::kWhite;
  en_passant_target_.reset();
  is_check_ = false;
  is_checkmate_ = false;
  is_stalemate_ = false;
  draw_reason_ = DrawReason::kNone;
  halfmove_clock_ = 0;
  starting_fullmove_number_ = 1;
  started_with_black_to_move_ = false;
  position_counts_.clear();
  chess960_ = chess960;

  PieceType order[8] = {PieceType::kRook,   PieceType::kKnight,
                        PieceType::kBishop, PieceType::kQueen,
                        PieceType::kKing,   PieceType::kBishop,
                        PieceType::kKnight, PieceType::kRook};
  if (chess960) RandomChess960BackRank(order);

  king_start_file_ = 4;
  rook_start_file_[0] = 0;
  rook_start_file_[1] = 7;
  for (int f = 0; f < 8; f++) {
    if (order[f] == PieceType::kKing) king_start_file_ = f;
  }
  int rook_index = 0;
  for (int f = 0; f < 8; f++) {
    if (order[f] == PieceType::kRook) rook_start_file_[rook_index++] = f;
  }

  PlaceBackRank(0, PieceColor::kWhite, order);

  for (int f = 0; f < 8; f++)
    board_[f][1] = Piece(PieceType::kPawn, PieceColor::kWhite);
  for (int f = 0; f < 8; f++)
    board_[f][6] = Piece(PieceType::kPawn, PieceColor::kBlack);

  PlaceBackRank(7, PieceColor::kBlack, order);

  position_counts_[RepetitionKey()] = 1;
}

namespace {
std::optional<PieceType> ParseFenPieceLetter(char c) {
  switch (std::tolower(static_cast<unsigned char>(c))) {
    case 'p':
      return PieceType::kPawn;
    case 'n':
      return PieceType::kKnight;
    case 'b':
      return PieceType::kBishop;
    case 'r':
      return PieceType::kRook;
    case 'q':
      return PieceType::kQueen;
    case 'k':
      return PieceType::kKing;
    default:
      return std::nullopt;
  }
}
}  // namespace

bool ChessEngine::LoadFen(const std::string& fen) {
  std::istringstream iss(fen);
  std::string placement, active_color, castling, en_passant;
  int halfmove = 0, fullmove = 1;

  if (!(iss >> placement >> active_color >> castling >> en_passant))
    return false;
  iss >> halfmove >>
      fullmove;  // optional trailing fields; defaults stand if absent

  Board new_board{};
  int rank = 7, file = 0;
  for (char c : placement) {
    if (c == '/') {
      if (file != 8) return false;
      rank--;
      file = 0;
    } else if (std::isdigit(static_cast<unsigned char>(c))) {
      file += c - '0';
    } else if (auto type = ParseFenPieceLetter(c)) {
      if (rank < 0 || file >= 8) return false;
      PieceColor color = std::isupper(static_cast<unsigned char>(c))
                             ? PieceColor::kWhite
                             : PieceColor::kBlack;
      new_board[file][rank] = Piece(*type, color);
      file++;
    } else {
      return false;
    }
  }
  if (rank != 0 || file != 8) return false;

  PieceColor new_side_to_move;
  if (active_color == "w")
    new_side_to_move = PieceColor::kWhite;
  else if (active_color == "b")
    new_side_to_move = PieceColor::kBlack;
  else
    return false;

  // A playable position has one king per side and no pawns on the first or last
  // rank.
  int king_count[2] = {0, 0};
  int pawn_count[2] = {0, 0};
  for (int f = 0; f < 8; f++) {
    for (int r = 0; r < 8; r++) {
      const auto& p = new_board[f][r];
      if (!p) continue;
      const int side = p->color == PieceColor::kWhite ? 0 : 1;
      if (p->type == PieceType::kKing) king_count[side]++;
      if (p->type == PieceType::kPawn) {
        pawn_count[side]++;
        if (r == 0 || r == 7) return false;
      }
    }
  }
  if (king_count[0] != 1 || king_count[1] != 1) return false;
  if (pawn_count[0] > 8 || pawn_count[1] > 8) return false;

  // The side that just moved must not have left its own king attacked (this
  // also rules out adjacent kings). The attack test reads board_, so the new
  // board is swapped in temporarily.
  {
    Board previous = board_;
    board_ = new_board;
    bool opponent_in_check = IsKingInCheck(Opposite(new_side_to_move));
    board_ = previous;
    if (opponent_in_check) return false;
  }

  // Pieces default to has_moved=true; only the king/rook squares named by the
  // castling rights are reset, so a position with no rights disables castling
  // without a separate flag.
  for (int f = 0; f < 8; f++) {
    for (int r = 0; r < 8; r++) {
      if (new_board[f][r]) new_board[f][r]->has_moved = true;
    }
  }

  // King files are needed to classify Shredder-FEN rook letters as king-side
  // (right of the king) or queen-side, and to validate KQkq rights.
  auto find_king_file = [&](PieceColor color, int r) {
    for (int f = 0; f < 8; f++) {
      auto& p = new_board[f][r];
      if (p && p->type == PieceType::kKing && p->color == color) return f;
    }
    return -1;
  };
  int white_king_file = find_king_file(PieceColor::kWhite, 0);
  int black_king_file = find_king_file(PieceColor::kBlack, 7);

  bool new_chess960 = false;
  int new_king_start_file = 4;
  int new_rook_start_file[2] = {0, 7};

  auto mark_unmoved = [&](int f, int r) {
    if (new_board[f][r]) new_board[f][r]->has_moved = false;
  };

  if (castling != "-") {
    for (char c : castling) {
      bool is_white = std::isupper(static_cast<unsigned char>(c));
      PieceColor color = is_white ? PieceColor::kWhite : PieceColor::kBlack;
      int rank0 = is_white ? 0 : 7;
      int king_file = is_white ? white_king_file : black_king_file;
      if (king_file < 0) return false;

      char lower =
          static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
      int rook_file;
      if (lower == 'k') {
        // Standard notation: the king-side rook, wherever it is.
        rook_file = -1;
        for (int f = king_file + 1; f < 8; f++) {
          auto& p = new_board[f][rank0];
          if (p && p->type == PieceType::kRook && p->color == color)
            rook_file = f;
        }
      } else if (lower == 'q') {
        // Standard notation: the outermost queen-side rook.
        rook_file = -1;
        for (int f = king_file - 1; f >= 0; f--) {
          auto& p = new_board[f][rank0];
          if (p && p->type == PieceType::kRook && p->color == color)
            rook_file = f;
        }
      } else if (lower >= 'a' && lower <= 'h') {
        // Shredder-FEN: the letter is the rook's file.
        rook_file = lower - 'a';
        new_chess960 = true;
      } else {
        return false;
      }
      if (rook_file < 0) return false;

      new_king_start_file = king_file;
      if (rook_file > king_file)
        new_rook_start_file[1] = rook_file;
      else
        new_rook_start_file[0] = rook_file;

      mark_unmoved(king_file, rank0);
      mark_unmoved(rook_file, rank0);
    }
  }

  std::optional<Square> new_en_passant;
  if (en_passant != "-") {
    if (en_passant.size() != 2) return false;
    if (en_passant[0] < 'a' || en_passant[0] > 'h') return false;

    // The target is the square the pawn skipped: rank 6 after a black double
    // push, rank 3 after a white one. The pawn must stand in front of it, with
    // the target and the pawn's start square empty.
    const bool white_to_move = new_side_to_move == PieceColor::kWhite;
    if (en_passant[1] != (white_to_move ? '6' : '3')) return false;

    const int ep_file = en_passant[0] - 'a';
    const int target_rank = white_to_move ? 5 : 2;
    const int pawn_rank = white_to_move ? 4 : 3;
    const int start_rank = white_to_move ? 6 : 1;
    const PieceColor pusher =
        white_to_move ? PieceColor::kBlack : PieceColor::kWhite;

    const auto& pawn = new_board[ep_file][pawn_rank];
    if (!pawn || pawn->type != PieceType::kPawn || pawn->color != pusher)
      return false;
    if (new_board[ep_file][target_rank] || new_board[ep_file][start_rank])
      return false;

    new_en_passant = Square::Parse(en_passant);
  }

  board_ = new_board;
  side_to_move_ = new_side_to_move;
  en_passant_target_ = new_en_passant;
  chess960_ = new_chess960;
  king_start_file_ = new_king_start_file;
  rook_start_file_[0] = new_rook_start_file[0];
  rook_start_file_[1] = new_rook_start_file[1];
  halfmove_clock_ = std::max(0, halfmove);
  starting_fullmove_number_ = std::max(1, fullmove);
  started_with_black_to_move_ = new_side_to_move == PieceColor::kBlack;
  history_.clear();
  is_check_ = false;
  is_checkmate_ = false;
  is_stalemate_ = false;
  draw_reason_ = DrawReason::kNone;
  position_counts_.clear();
  position_counts_[RepetitionKey()] = 1;

  UpdateGameStatus();
  return true;
}

void ChessEngine::PlaceBackRank(int rank, PieceColor color,
                                const PieceType order[8]) {
  for (int f = 0; f < 8; f++) board_[f][rank] = Piece(order[f], color);
}

// Standard 960 generation: bishops on opposite colors, then queen, then two
// knights chosen among the remaining five files, then king between the two
// rooks on the last three. Every one of the 960 positions is equally likely.
void ChessEngine::RandomChess960BackRank(PieceType order[8]) {
  static std::mt19937 rng(std::random_device{}());

  for (int f = 0; f < 8; f++)
    order[f] = PieceType::kKing;  // placeholder for "empty"

  std::uniform_int_distribution<int> dark_dist(0, 3);
  int dark_square_file = dark_dist(rng) * 2;
  std::uniform_int_distribution<int> light_dist(0, 3);
  int light_square_file = light_dist(rng) * 2 + 1;
  order[dark_square_file] = PieceType::kBishop;
  order[light_square_file] = PieceType::kBishop;

  auto empty_files = [&]() {
    std::vector<int> files;
    for (int f = 0; f < 8; f++) {
      if (order[f] == PieceType::kKing) files.push_back(f);
    }
    return files;
  };

  std::vector<int> remaining = empty_files();
  std::uniform_int_distribution<int> queen_dist(
      0, static_cast<int>(remaining.size()) - 1);
  order[remaining[queen_dist(rng)]] = PieceType::kQueen;

  // Choose the knights' files among the 5 remaining; the other 3 are king and
  // rooks.
  remaining = empty_files();
  std::uniform_int_distribution<int> knight_a_dist(
      0, static_cast<int>(remaining.size()) - 1);
  int knight_a = knight_a_dist(rng);
  std::uniform_int_distribution<int> knight_b_dist(
      0, static_cast<int>(remaining.size()) - 2);
  int knight_b = knight_b_dist(rng);
  if (knight_b >= knight_a) knight_b++;
  order[remaining[knight_a]] = PieceType::kKnight;
  order[remaining[knight_b]] = PieceType::kKnight;

  // The king takes the middle of the last 3 files, so it sits between the
  // rooks.
  remaining = empty_files();
  std::sort(remaining.begin(), remaining.end());

  order[remaining[0]] = PieceType::kRook;
  order[remaining[1]] = PieceType::kKing;
  order[remaining[2]] = PieceType::kRook;
}

std::optional<Piece> ChessEngine::PieceAt(Square sq) const {
  if (!sq.IsOnBoard()) return std::nullopt;

  return board_[sq.file][sq.rank];
}

std::vector<Square> ChessEngine::GetLegalMoves(Square from) const {
  auto piece = PieceAt(from);

  if (!piece || piece->color != side_to_move_) return {};

  std::vector<Square> result;

  for (auto to : GeneratePseudoLegalMoves(from, *piece)) {
    if (!MovesIntoCheck(from, to, *piece)) result.push_back(to);
  }

  return result;
}

std::optional<Move> ChessEngine::TryMakeMove(
    Square from, Square to, std::optional<PieceType> promotion_type) {
  if (promotion_type && (*promotion_type == PieceType::kKing ||
                         *promotion_type == PieceType::kPawn))
    return std::nullopt;

  auto piece = PieceAt(from);

  if (!piece || piece->color != side_to_move_) return std::nullopt;

  auto legal = GetLegalMoves(from);

  if (std::find(legal.begin(), legal.end(), to) == legal.end())
    return std::nullopt;

  Move move = BuildMove(from, to, *piece, promotion_type);

  // Computed before the move, while the rivals are still in place.
  std::string disambiguation = Disambiguation(from, to, *piece);

  bool resets_halfmove_clock =
      piece->type == PieceType::kPawn || move.piece_captured.has_value();

  ApplyMove(move);

  side_to_move_ = Opposite(side_to_move_);

  halfmove_clock_ = resets_halfmove_clock ? 0 : halfmove_clock_ + 1;

  // Pawn moves and captures are irreversible, so earlier positions can't recur.
  if (resets_halfmove_clock) position_counts_.clear();

  position_counts_[RepetitionKey()]++;

  UpdateGameStatus();

  move.is_check = is_check_;
  move.is_checkmate = is_checkmate_;
  move.notation = BuildNotation(move, disambiguation);
  history_.push_back(move);

  return move;
}

// A king move onto a rook-start file is a castle only if that square holds the
// mover's own unmoved rook; otherwise it is an ordinary move or capture.
bool ChessEngine::DestinationHoldsOwnUnmovedRook(Square square,
                                                 PieceColor color) const {
  auto rook = PieceAt(square);
  return rook && rook->type == PieceType::kRook && rook->color == color &&
         !rook->has_moved;
}

Move ChessEngine::BuildMove(Square from, Square to, const Piece& piece,
                            std::optional<PieceType> promotion_type) const {
  Move move;
  move.from = from;
  move.to = to;
  move.piece_moved = piece;

  bool is_pawn = piece.type == PieceType::kPawn;
  int d_rank_abs = std::abs(to.rank - from.rank);

  if (is_pawn && d_rank_abs == 2) {
    move.special = MoveSpecial::kDoublePawnPush;
  } else if (is_pawn && en_passant_target_ && to == *en_passant_target_ &&
             from.file != to.file) {
    // The captured pawn is beside the destination, not on it.
    move.special = MoveSpecial::kEnPassant;
    move.piece_captured = PieceAt(Square(to.file, from.rank));
  } else if (piece.type == PieceType::kKing && !piece.has_moved &&
             from.file == king_start_file_ && to.rank == from.rank &&
             (to.file == rook_start_file_[0] ||
              to.file == rook_start_file_[1]) &&
             to.file != from.file &&
             DestinationHoldsOwnUnmovedRook(to, piece.color)) {
    // Castling is encoded as the king moving onto its own rook's square (the
    // UCI_Chess960 convention). The extra checks stop an ordinary king move
    // landing on a former rook square from being read as a castle.
    move.special = to.file == rook_start_file_[1]
                       ? MoveSpecial::kCastleKingSide
                       : MoveSpecial::kCastleQueenSide;
  } else if (is_pawn && (to.rank == 0 || to.rank == 7)) {
    move.special = MoveSpecial::kPromotion;
    move.promotion_type = promotion_type.value_or(PieceType::kQueen);
  }

  // A castle's `to` holds the rook, but it is not a capture.
  bool is_castle = move.special == MoveSpecial::kCastleKingSide ||
                   move.special == MoveSpecial::kCastleQueenSide;
  if (!move.piece_captured && !is_castle) move.piece_captured = PieceAt(to);

  return move;
}

void ChessEngine::ApplyMove(const Move& move) {
  if (move.special == MoveSpecial::kCastleKingSide) {
    ApplyCastle(move.from.rank, move.to.file, kFinalKingSideKingFile,
                kFinalKingSideRookFile);
    en_passant_target_.reset();
    return;
  }
  if (move.special == MoveSpecial::kCastleQueenSide) {
    ApplyCastle(move.from.rank, move.to.file, kFinalQueenSideKingFile,
                kFinalQueenSideRookFile);
    en_passant_target_.reset();
    return;
  }

  Piece piece = move.piece_moved;
  board_[move.from.file][move.from.rank] = std::nullopt;

  if (move.special == MoveSpecial::kEnPassant) {
    board_[move.to.file][move.from.rank] = std::nullopt;
  }

  Piece placed_piece = piece;

  if (move.special == MoveSpecial::kPromotion) {
    placed_piece = Piece(*move.promotion_type, piece.color);
  }

  placed_piece.has_moved = true;
  board_[move.to.file][move.to.rank] = placed_piece;

  // Set only by a double pawn push; any other move clears it.
  en_passant_target_ =
      move.special == MoveSpecial::kDoublePawnPush
          ? std::optional<Square>(
                Square(move.from.file, (move.from.rank + move.to.rank) / 2))
          : std::nullopt;
}

// Clears every square the king or rook may occupy before placing both, since in
// Chess960 they can already sit on each other's destination.
void ChessEngine::ApplyCastle(int rank, int rook_from_file, int king_to_file,
                              int rook_to_file) {
  Piece king = *board_[king_start_file_][rank];
  Piece rook = *board_[rook_from_file][rank];

  board_[king_start_file_][rank] = std::nullopt;
  board_[rook_from_file][rank] = std::nullopt;
  board_[king_to_file][rank] = std::nullopt;
  board_[rook_to_file][rank] = std::nullopt;

  king.has_moved = true;
  rook.has_moved = true;
  board_[king_to_file][rank] = king;
  board_[rook_to_file][rank] = rook;
}

void ChessEngine::UpdateGameStatus() {
  is_check_ = IsKingInCheck(side_to_move_);
  bool has_any_legal_move = false;

  for (auto sq : AllPiecesOfColor(side_to_move_)) {
    if (!GetLegalMoves(sq).empty()) {
      has_any_legal_move = true;
      break;
    }
  }

  is_checkmate_ = is_check_ && !has_any_legal_move;
  is_stalemate_ = !is_check_ && !has_any_legal_move;

  if (is_checkmate_) {
    // Checkmate outranks the fifty-move rule when both occur on the same move.
    draw_reason_ = DrawReason::kNone;
  } else if (is_stalemate_) {
    draw_reason_ = DrawReason::kStalemate;
  } else if (halfmove_clock_ >= 100) {
    draw_reason_ = DrawReason::kFiftyMoveRule;
  } else if (position_counts_[RepetitionKey()] >= 3) {
    draw_reason_ = DrawReason::kThreefoldRepetition;
  } else if (IsInsufficientMaterial()) {
    draw_reason_ = DrawReason::kInsufficientMaterial;
  } else {
    draw_reason_ = DrawReason::kNone;
  }
}

std::string ChessEngine::RepetitionKey() const {
  std::ostringstream key;

  for (int rank = 0; rank < 8; rank++) {
    for (int file = 0; file < 8; file++) {
      auto& p = board_[file][rank];
      key << (p ? p->Symbol() : '.');
    }
  }

  key << (side_to_move_ == PieceColor::kWhite ? 'w' : 'b');
  key << (CanCastle(PieceColor::kWhite, true) ? 'K' : '-');
  key << (CanCastle(PieceColor::kWhite, false) ? 'Q' : '-');
  key << (CanCastle(PieceColor::kBlack, true) ? 'k' : '-');
  key << (CanCastle(PieceColor::kBlack, false) ? 'q' : '-');
  key << EnPassantKeyPart();

  return key.str();
}

// FIDE counts the en passant square only if a pawn of the side to move can
// legally capture onto it.
std::string ChessEngine::EnPassantKeyPart() const {
  if (!en_passant_target_) return "-";

  const int pawn_rank = side_to_move_ == PieceColor::kWhite
                            ? en_passant_target_->rank - 1
                            : en_passant_target_->rank + 1;
  for (int df : {-1, 1}) {
    Square from(en_passant_target_->file + df, pawn_rank);
    if (!from.IsOnBoard()) continue;

    auto p = PieceAt(from);
    if (!p || p->type != PieceType::kPawn || p->color != side_to_move_)
      continue;

    auto moves = GetLegalMoves(from);
    if (std::find(moves.begin(), moves.end(), *en_passant_target_) !=
        moves.end())
      return en_passant_target_->Algebraic();
  }

  return "-";
}

// Insufficient material: bare kings, king + one minor piece, or kings with
// bishops on same-colored squares.
bool ChessEngine::IsInsufficientMaterial() const {
  std::vector<PieceType> minor_pieces;

  for (int f = 0; f < 8; f++) {
    for (int r = 0; r < 8; r++) {
      auto& p = board_[f][r];

      if (!p) continue;

      switch (p->type) {
        case PieceType::kPawn:
        case PieceType::kRook:
        case PieceType::kQueen:
          return false;

        case PieceType::kBishop:
        case PieceType::kKnight: {
          minor_pieces.push_back(p->type);

          break;
        }

        case PieceType::kKing:
          break;
      }
    }
  }

  if (minor_pieces.size() <= 1) return true;

  if (minor_pieces.size() == 2 && minor_pieces[0] == PieceType::kBishop &&
      minor_pieces[1] == PieceType::kBishop) {
    std::vector<Square> bishop_squares;

    for (int f = 0; f < 8; f++) {
      for (int r = 0; r < 8; r++) {
        auto& p = board_[f][r];

        if (p && p->type == PieceType::kBishop)
          bishop_squares.push_back(Square(f, r));
      }
    }

    if (bishop_squares.size() == 2) {
      // Same file+rank parity means same-colored squares.
      bool same_color = (bishop_squares[0].file + bishop_squares[0].rank) % 2 ==
                        (bishop_squares[1].file + bishop_squares[1].rank) % 2;

      return same_color;
    }
  }

  return false;
}

std::vector<Square> ChessEngine::GeneratePseudoLegalMoves(
    Square from, const Piece& piece) const {
  switch (piece.type) {
    case PieceType::kPawn:
      return PawnMoves(from, piece.color);
    case PieceType::kKnight:
      return KnightMoves(from, piece.color);
    case PieceType::kBishop:
      return SlidingMoves(from, piece.color, kDiagonalDirs);
    case PieceType::kRook:
      return SlidingMoves(from, piece.color, kStraightDirs);
    case PieceType::kQueen:
      return SlidingMoves(from, piece.color, kAllDirs);
    case PieceType::kKing:
      return KingMoves(from, piece.color);
    default:
      return {};
  }
}

std::vector<Square> ChessEngine::PawnMoves(Square from,
                                           PieceColor color) const {
  std::vector<Square> result;
  int dir = color == PieceColor::kWhite ? 1 : -1;
  int start_rank = color == PieceColor::kWhite ? 1 : 6;

  Square one_step(from.file, from.rank + dir);

  if (one_step.IsOnBoard() && !PieceAt(one_step)) {
    result.push_back(one_step);
    Square two_step(from.file, from.rank + 2 * dir);

    if (from.rank == start_rank && !PieceAt(two_step))
      result.push_back(two_step);
  }

  for (int df : {-1, 1}) {
    Square capture(from.file + df, from.rank + dir);

    if (!capture.IsOnBoard()) continue;

    auto target = PieceAt(capture);

    if (target && target->color != color) {
      result.push_back(capture);
    } else if (!target && en_passant_target_ &&
               capture == *en_passant_target_) {
      result.push_back(capture);
    }
  }

  return result;
}

std::vector<Square> ChessEngine::KnightMoves(Square from,
                                             PieceColor color) const {
  return StepMoves(from, color, kKnightOffsets);
}

std::vector<Square> ChessEngine::StepMoves(
    Square from, PieceColor color,
    const std::vector<std::pair<int, int>>& offsets) const {
  std::vector<Square> result;

  for (auto [df, dr] : offsets) {
    Square to(from.file + df, from.rank + dr);

    if (!to.IsOnBoard()) continue;

    auto target = PieceAt(to);

    if (!target || target->color != color) result.push_back(to);
  }

  return result;
}

std::vector<Square> ChessEngine::KingMoves(Square from,
                                           PieceColor color) const {
  std::vector<Square> result = StepMoves(from, color, kAllDirs);

  for (auto castle : CastlingMoves(from, color)) result.push_back(castle);

  return result;
}

namespace {
std::vector<int> FilesBetween(int a, int b) {
  std::vector<int> files;
  for (int f = std::min(a, b) + 1; f < std::max(a, b); f++) files.push_back(f);
  return files;
}
}  // namespace

// Checks one castling direction for any king/rook start files: the rook must be
// unmoved, every square either piece crosses or lands on (other than their own
// current squares) must be empty, and the king may not start on, cross or land
// on an attacked square.
bool ChessEngine::CanCastleTo(int rank, PieceColor color, int rook_file,
                              int king_to_file, int rook_to_file) const {
  auto king = PieceAt(Square(king_start_file_, rank));
  if (!king || king->type != PieceType::kKing || king->has_moved) return false;

  auto rook = PieceAt(Square(rook_file, rank));
  if (!rook || rook->type != PieceType::kRook || rook->has_moved) return false;

  auto is_king_or_rook_square = [&](int f) {
    return f == king_start_file_ || f == rook_file;
  };

  for (int f : FilesBetween(king_start_file_, king_to_file)) {
    if (!is_king_or_rook_square(f) && PieceAt(Square(f, rank))) return false;
  }
  if (!is_king_or_rook_square(king_to_file) &&
      king_to_file != king_start_file_ && PieceAt(Square(king_to_file, rank)))
    return false;

  for (int f : FilesBetween(rook_file, rook_to_file)) {
    if (!is_king_or_rook_square(f) && PieceAt(Square(f, rank))) return false;
  }
  if (!is_king_or_rook_square(rook_to_file) && rook_to_file != rook_file &&
      PieceAt(Square(rook_to_file, rank)))
    return false;

  int king_path_lo = std::min(king_start_file_, king_to_file);
  int king_path_hi = std::max(king_start_file_, king_to_file);
  for (int f = king_path_lo; f <= king_path_hi; f++) {
    if (IsSquareAttacked(Square(f, rank), Opposite(color))) return false;
  }

  return true;
}

std::vector<Square> ChessEngine::CastlingMoves(Square from,
                                               PieceColor color) const {
  std::vector<Square> result;

  int rank = color == PieceColor::kWhite ? 0 : 7;
  if (from != Square(king_start_file_, rank)) return result;

  if (CanCastleTo(rank, color, rook_start_file_[1], kFinalKingSideKingFile,
                  kFinalKingSideRookFile)) {
    result.push_back(Square(rook_start_file_[1], rank));
  }
  if (CanCastleTo(rank, color, rook_start_file_[0], kFinalQueenSideKingFile,
                  kFinalQueenSideRookFile)) {
    result.push_back(Square(rook_start_file_[0], rank));
  }

  return result;
}

std::vector<Square> ChessEngine::SlidingMoves(
    Square from, PieceColor color,
    const std::vector<std::pair<int, int>>& dirs) const {
  std::vector<Square> result;

  for (auto [df, dr] : dirs) {
    Square to(from.file + df, from.rank + dr);

    while (to.IsOnBoard()) {
      auto target = PieceAt(to);

      if (!target) {
        result.push_back(to);
      } else {
        if (target->color != color) result.push_back(to);

        break;
      }

      to = Square(to.file + df, to.rank + dr);
    }
  }

  return result;
}

bool ChessEngine::MovesIntoCheck(Square from, Square to,
                                 const Piece& piece) const {
  Board snapshot = board_;
  auto saved_en_passant = en_passant_target_;

  // Promotion type doesn't affect whether the king ends in check.
  Move move = BuildMove(from, to, piece, PieceType::kQueen);

  // Logically const: board_ and en_passant_target_ are restored below.
  ChessEngine* self = const_cast<ChessEngine*>(this);
  self->ApplyMove(move);

  bool in_check = IsKingInCheck(piece.color);

  self->board_ = snapshot;
  self->en_passant_target_ = saved_en_passant;

  return in_check;
}

bool ChessEngine::IsKingInCheck(PieceColor color) const {
  auto king_square = FindKing(color);

  return king_square && IsSquareAttacked(*king_square, Opposite(color));
}

std::optional<Square> ChessEngine::FindKing(PieceColor color) const {
  for (int f = 0; f < 8; f++) {
    for (int r = 0; r < 8; r++) {
      auto& p = board_[f][r];
      if (p && p->type == PieceType::kKing && p->color == color)
        return Square(f, r);
    }
  }

  return std::nullopt;
}

bool ChessEngine::IsSquareAttacked(Square square, PieceColor by_color) const {
  // An attacking pawn sits one rank behind the target, from its own side.
  int dir = by_color == PieceColor::kWhite ? 1 : -1;

  for (int df : {-1, 1}) {
    Square src(square.file + df, square.rank - dir);

    if (src.IsOnBoard()) {
      auto p = PieceAt(src);

      if (p && p->type == PieceType::kPawn && p->color == by_color) return true;
    }
  }

  if (HasPieceAtOffsets(square, by_color, PieceType::kKnight, kKnightOffsets))
    return true;
  if (HasPieceAtOffsets(square, by_color, PieceType::kKing, kAllDirs))
    return true;

  if (IsAttackedAlongDirs(square, by_color, kDiagonalDirs, PieceType::kBishop,
                          PieceType::kQueen))
    return true;
  if (IsAttackedAlongDirs(square, by_color, kStraightDirs, PieceType::kRook,
                          PieceType::kQueen))
    return true;

  return false;
}

bool ChessEngine::HasPieceAtOffsets(
    Square square, PieceColor color, PieceType type,
    const std::vector<std::pair<int, int>>& offsets) const {
  for (auto [df, dr] : offsets) {
    Square src(square.file + df, square.rank + dr);

    if (!src.IsOnBoard()) continue;

    auto p = PieceAt(src);

    if (p && p->type == type && p->color == color) return true;
  }

  return false;
}

bool ChessEngine::IsAttackedAlongDirs(
    Square square, PieceColor by_color,
    const std::vector<std::pair<int, int>>& dirs, PieceType match_a,
    PieceType match_b) const {
  for (auto [df, dr] : dirs) {
    Square cur(square.file + df, square.rank + dr);

    while (cur.IsOnBoard()) {
      auto target = PieceAt(cur);

      if (target) {
        if (target->color == by_color &&
            (target->type == match_a || target->type == match_b))
          return true;

        break;
      }

      cur = Square(cur.file + df, cur.rank + dr);
    }
  }

  return false;
}

std::vector<Square> ChessEngine::AllPiecesOfColor(PieceColor color) const {
  std::vector<Square> result;

  for (int f = 0; f < 8; f++) {
    for (int r = 0; r < 8; r++) {
      auto& p = board_[f][r];
      if (p && p->color == color) result.push_back(Square(f, r));
    }
  }

  return result;
}

std::string ChessEngine::ToFen() const {
  std::string rows[8];

  for (int rank = 7; rank >= 0; rank--) {
    std::ostringstream sb;

    int empty = 0;

    for (int file = 0; file < 8; file++) {
      auto& piece = board_[file][rank];

      if (!piece) {
        empty++;
        continue;
      }

      if (empty > 0) {
        sb << empty;
        empty = 0;
      }

      sb << piece->Symbol();
    }

    if (empty > 0) sb << empty;

    rows[7 - rank] = sb.str();
  }

  std::ostringstream placement;

  for (int i = 0; i < 8; i++) {
    if (i > 0) placement << '/';

    placement << rows[i];
  }

  std::string active_color = side_to_move_ == PieceColor::kWhite ? "w" : "b";
  std::string castling;

  if (chess960_) {
    // Shredder-FEN: with an arbitrary back rank, K/Q no longer identifies the
    // rook, and UCI_Chess960 engines expect file letters.
    char king_side_file = static_cast<char>('a' + rook_start_file_[1]);
    char queen_side_file = static_cast<char>('a' + rook_start_file_[0]);
    if (CanCastle(PieceColor::kWhite, true))
      castling += static_cast<char>(std::toupper(king_side_file));
    if (CanCastle(PieceColor::kWhite, false))
      castling += static_cast<char>(std::toupper(queen_side_file));
    if (CanCastle(PieceColor::kBlack, true)) castling += king_side_file;
    if (CanCastle(PieceColor::kBlack, false)) castling += queen_side_file;
  } else {
    if (CanCastle(PieceColor::kWhite, true)) castling += "K";
    if (CanCastle(PieceColor::kWhite, false)) castling += "Q";
    if (CanCastle(PieceColor::kBlack, true)) castling += "k";
    if (CanCastle(PieceColor::kBlack, false)) castling += "q";
  }
  if (castling.empty()) castling = "-";

  std::string en_passant =
      en_passant_target_ ? en_passant_target_->Algebraic() : "-";

  int plies_played = static_cast<int>(history_.size());
  int fullmove_number =
      starting_fullmove_number_ +
      (started_with_black_to_move_ ? plies_played + 1 : plies_played) / 2;

  std::ostringstream out;
  out << placement.str() << " " << active_color << " " << castling << " "
      << en_passant << " " << halfmove_clock_ << " " << fullmove_number;

  return out.str();
}

bool ChessEngine::CanCastle(PieceColor color, bool king_side) const {
  int rank = color == PieceColor::kWhite ? 0 : 7;
  auto& king = board_[king_start_file_][rank];
  if (!king || king->type != PieceType::kKing || king->has_moved) return false;

  auto& rook = board_[rook_start_file_[king_side ? 1 : 0]][rank];
  return rook && rook->type == PieceType::kRook && !rook->has_moved;
}

std::string ChessEngine::Disambiguation(Square from, Square to,
                                        const Piece& piece) const {
  if (piece.type == PieceType::kPawn || piece.type == PieceType::kKing)
    return "";

  // Rivals: same-type pieces that can legally reach `to` (a pinned piece
  // doesn't count).
  bool any_rival = false, rival_shares_file = false, rival_shares_rank = false;
  for (Square sq : AllPiecesOfColor(piece.color)) {
    if (sq == from) continue;
    auto other = PieceAt(sq);
    if (!other || other->type != piece.type) continue;

    std::vector<Square> targets = GetLegalMoves(sq);
    if (std::find(targets.begin(), targets.end(), to) == targets.end())
      continue;

    any_rival = true;
    if (sq.file == from.file) rival_shares_file = true;
    if (sq.rank == from.rank) rival_shares_rank = true;
  }

  if (!any_rival) return "";

  std::string file(1, static_cast<char>('a' + from.file));
  std::string rank(1, static_cast<char>('1' + from.rank));
  if (!rival_shares_file) return file;
  if (!rival_shares_rank) return rank;
  return file + rank;
}

std::string ChessEngine::BuildNotation(
    const Move& move, const std::string& disambiguation) const {
  if (move.special == MoveSpecial::kCastleKingSide)
    return AppendCheckSuffix("O-O", move);
  if (move.special == MoveSpecial::kCastleQueenSide)
    return AppendCheckSuffix("O-O-O", move);

  std::string piece_letter;
  switch (move.piece_moved.type) {
    case PieceType::kPawn:
      piece_letter = "";
      break;
    case PieceType::kKnight:
      piece_letter = "N";
      break;
    case PieceType::kBishop:
      piece_letter = "B";
      break;
    case PieceType::kRook:
      piece_letter = "R";
      break;
    case PieceType::kQueen:
      piece_letter = "Q";
      break;
    case PieceType::kKing:
      piece_letter = "K";
      break;
  }

  bool is_capture = move.piece_captured.has_value();
  std::string capture_prefix;
  if (move.piece_moved.type == PieceType::kPawn && is_capture) {
    capture_prefix = std::string(1, static_cast<char>('a' + move.from.file));
  }

  std::string san = piece_letter + disambiguation + capture_prefix +
                    (is_capture ? "x" : "") + move.to.Algebraic();

  if (move.special == MoveSpecial::kPromotion) {
    char promo_letter;
    switch (*move.promotion_type) {
      case PieceType::kQueen:
        promo_letter = 'Q';
        break;
      case PieceType::kRook:
        promo_letter = 'R';
        break;
      case PieceType::kBishop:
        promo_letter = 'B';
        break;
      case PieceType::kKnight:
        promo_letter = 'N';
        break;
      default:
        promo_letter = 'Q';
        break;
    }
    san += std::string("=") + promo_letter;
  }

  return AppendCheckSuffix(san, move);
}

std::string ChessEngine::AppendCheckSuffix(const std::string& san,
                                           const Move& move) const {
  if (move.is_checkmate) return san + "#";
  if (move.is_check) return san + "+";
  return san;
}
}  // namespace chess
