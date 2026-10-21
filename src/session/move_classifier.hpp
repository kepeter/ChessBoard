/*
 * Copyright (c) 2026 Kornfeld Eliyahu Peter
 * License: MIT
 */

#ifndef CHESSBOARD_SESSION_MOVE_CLASSIFIER_HPP_
#define CHESSBOARD_SESSION_MOVE_CLASSIFIER_HPP_

#include <optional>

#include <QColor>
#include <QObject>
#include <QString>

#include "models/piece.hpp"
#include "session/game_session.hpp"

namespace chess {
enum class MoveQuality {
  kBest,
  kGood,
  kInaccuracy,
  kMistake,
  kBlunder,
};

// Classifies each human move by how much evaluation (mover's perspective) it
// gave up versus the best eval available before the move. Only human moves are
// classified. The pre-move eval is the latest top line from
// GameSession::analysisUpdated; the post-move eval comes from
// GameSession::postMoveEvalReady, taken early in the engine's reply search, so
// no extra engine time is spent.
class MoveClassifier : public QObject {
  Q_OBJECT

 public:
  explicit MoveClassifier(GameSession& session, QObject* parent = nullptr);

 signals:
  // accuracy_percent is the running average over this game's classified moves.
  void moveClassified(MoveQuality quality, double accuracy_percent);

  void reset();

 private slots:
  void OnAnalysisUpdated(AnalysisInfo info);
  void OnMoveMade(PieceColor by);
  void OnPostMoveEvalReady(PieceColor mover, std::optional<int> score_cp,
                           std::optional<int> score_mate);
  void OnGameStarted();

 private:
  GameSession& session_;

  // Latest top-line eval (mover's centipawns) for the position on the board;
  // nullopt for a forced mate.
  std::optional<int> pending_pre_move_cp_;
  std::optional<int> pending_pre_move_mate_;

  // Captured when the human moves, awaiting the matching postMoveEvalReady.
  std::optional<int> captured_pre_move_cp_;
  std::optional<int> captured_pre_move_mate_;

  double total_accuracy_percent_ = 0.0;
  int classified_move_count_ = 0;

  void ClassifyAndEmit(std::optional<int> post_move_cp,
                       std::optional<int> post_move_mate);
};

// Maps centipawns lost (mover's perspective) to a quality; losing a forced
// mate is always a blunder.
MoveQuality ClassifySwing(int centipawns_lost, bool lost_forced_mate);
QString MoveQualityLabel(MoveQuality quality);
QColor MoveQualityColor(MoveQuality quality);
QString MoveQualitySymbol(MoveQuality quality);
}  // namespace chess

#endif  // CHESSBOARD_SESSION_MOVE_CLASSIFIER_HPP_
