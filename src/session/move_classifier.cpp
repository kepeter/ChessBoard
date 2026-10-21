/*
 * Copyright (c) 2026 Kornfeld Eliyahu Peter
 * License: MIT
 */

#include "session/move_classifier.hpp"

#include <algorithm>

namespace chess {
namespace {
// Centipawns-lost thresholds, roughly following chess.com/lichess
// classification.
constexpr int kInaccuracyThreshold = 20;
constexpr int kMistakeThreshold = 50;
constexpr int kBlunderThreshold = 150;

// Per-quality accuracy score averaged over the game; a monotonic scale, not any
// site's exact formula.
double AccuracyForQuality(MoveQuality quality) {
  switch (quality) {
    case MoveQuality::kBest:
      return 100.0;
    case MoveQuality::kGood:
      return 90.0;
    case MoveQuality::kInaccuracy:
      return 70.0;
    case MoveQuality::kMistake:
      return 45.0;
    case MoveQuality::kBlunder:
      return 15.0;
  }
  return 100.0;
}
}  // namespace

MoveQuality ClassifySwing(int centipawns_lost, bool lost_forced_mate) {
  if (lost_forced_mate) return MoveQuality::kBlunder;

  if (centipawns_lost <= 0) return MoveQuality::kBest;
  if (centipawns_lost <= kInaccuracyThreshold) return MoveQuality::kGood;
  if (centipawns_lost <= kMistakeThreshold) return MoveQuality::kInaccuracy;
  if (centipawns_lost <= kBlunderThreshold) return MoveQuality::kMistake;
  return MoveQuality::kBlunder;
}

QString MoveQualityLabel(MoveQuality quality) {
  switch (quality) {
    case MoveQuality::kBest:
      return "Best";
    case MoveQuality::kGood:
      return "Good";
    case MoveQuality::kInaccuracy:
      return "Inaccuracy";
    case MoveQuality::kMistake:
      return "Mistake";
    case MoveQuality::kBlunder:
      return "Blunder";
  }
  return QString();
}

QColor MoveQualityColor(MoveQuality quality) {
  switch (quality) {
    case MoveQuality::kBest:
      return QColor(30, 140, 60);
    case MoveQuality::kGood:
      return QColor(90, 160, 60);
    case MoveQuality::kInaccuracy:
      return QColor(200, 160, 30);
    case MoveQuality::kMistake:
      return QColor(220, 120, 30);
    case MoveQuality::kBlunder:
      return QColor(200, 40, 40);
  }
  return QColor(128, 128, 128);
}

QString MoveQualitySymbol(MoveQuality quality) {
  switch (quality) {
    case MoveQuality::kBest:
      return QStringLiteral("★");
    case MoveQuality::kGood:
      return QStringLiteral("✓");
    case MoveQuality::kInaccuracy:
      return QStringLiteral("!");
    case MoveQuality::kMistake:
      return QStringLiteral("?");
    case MoveQuality::kBlunder:
      return QStringLiteral("✖");
  }
  return QString();
}

MoveClassifier::MoveClassifier(GameSession& session, QObject* parent)
    : QObject(parent), session_(session) {
  connect(&session_, &GameSession::analysisUpdated, this,
          &MoveClassifier::OnAnalysisUpdated);
  connect(&session_, &GameSession::moveMade, this, &MoveClassifier::OnMoveMade);
  connect(&session_, &GameSession::postMoveEvalReady, this,
          &MoveClassifier::OnPostMoveEvalReady);
  connect(&session_, &GameSession::gameStarted, this,
          &MoveClassifier::OnGameStarted);
}

void MoveClassifier::OnGameStarted() {
  pending_pre_move_cp_.reset();
  pending_pre_move_mate_.reset();
  captured_pre_move_cp_.reset();
  captured_pre_move_mate_.reset();
  total_accuracy_percent_ = 0.0;
  classified_move_count_ = 0;
  emit reset();
}

void MoveClassifier::OnMoveMade(PieceColor by) {
  if (by != session_.PlayerColor()) {
    // The engine's own move isn't classified, and the pending pre-move eval is
    // for a position that is gone.
    pending_pre_move_cp_.reset();
    pending_pre_move_mate_.reset();
    return;
  }

  captured_pre_move_cp_ = pending_pre_move_cp_;
  captured_pre_move_mate_ = pending_pre_move_mate_;
  pending_pre_move_cp_.reset();
  pending_pre_move_mate_.reset();
}

// UCI scores are already from the side to move (the human here), so no flip is
// needed.
void MoveClassifier::OnAnalysisUpdated(AnalysisInfo info) {
  if (info.multipv != 1) return;

  pending_pre_move_cp_ = info.score_cp;
  pending_pre_move_mate_ = info.score_mate;
}

void MoveClassifier::OnPostMoveEvalReady(PieceColor mover,
                                         std::optional<int> score_cp,
                                         std::optional<int> score_mate) {
  if (mover != session_.PlayerColor()) return;

  ClassifyAndEmit(score_cp, score_mate);
}

void MoveClassifier::ClassifyAndEmit(std::optional<int> post_move_cp,
                                     std::optional<int> post_move_mate) {
  // No pre-move eval (analysis was off or just enabled): nothing to compare
  // against.
  if (!captured_pre_move_cp_ && !captured_pre_move_mate_) return;

  // No post-move eval (the engine's search failed): leave the move unclassified
  // rather than scoring it as 0.00.
  if (!post_move_cp && !post_move_mate) {
    captured_pre_move_cp_.reset();
    captured_pre_move_mate_.reset();
    return;
  }

  // A forced mate is kept if the post-move mate is no more than one move
  // longer.
  bool lost_forced_mate = captured_pre_move_mate_ &&
                          *captured_pre_move_mate_ > 0 &&
                          !(post_move_mate && *post_move_mate > 0 &&
                            *post_move_mate <= *captured_pre_move_mate_ + 1);

  int centipawns_lost = 0;
  if (!lost_forced_mate) {
    // Already being mated before the move: there was no better move to miss, so
    // it is never penalized.
    if (captured_pre_move_mate_ && *captured_pre_move_mate_ < 0) {
      centipawns_lost = 0;
    } else {
      int pre_cp = captured_pre_move_cp_.value_or(0);
      // A mate score is treated as a huge centipawn value.
      int post_cp = post_move_mate ? (*post_move_mate > 0 ? 100000 : -100000)
                                   : post_move_cp.value_or(0);
      centipawns_lost = std::max(0, pre_cp - post_cp);
    }
  }

  MoveQuality quality = ClassifySwing(centipawns_lost, lost_forced_mate);

  classified_move_count_++;
  total_accuracy_percent_ += AccuracyForQuality(quality);
  double running_accuracy = total_accuracy_percent_ / classified_move_count_;

  captured_pre_move_cp_.reset();
  captured_pre_move_mate_.reset();

  emit moveClassified(quality, running_accuracy);
}
}  // namespace chess
