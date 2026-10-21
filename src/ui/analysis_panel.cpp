/*
 * Copyright (c) 2026 Kornfeld Eliyahu Peter
 * License: MIT
 */

#include "ui/analysis_panel.hpp"

#include <algorithm>

#include <QVBoxLayout>

#include "ui/best_lines_widget.hpp"
#include "ui/evaluation_bar_widget.hpp"
#include "ui/move_classification_widget.hpp"

namespace chess {
AnalysisPanel::AnalysisPanel(ChessEngine& engine, GameSession& session,
                             QWidget* parent)
    : QWidget(parent),
      engine_(engine),
      session_(session),
      move_classifier_(session) {
  // Not layout-managed: MainWindowCentral sizes it to the board's grid rect
  // (see EvalBar()).
  eval_bar_ = new EvaluationBarWidget(this);
  eval_bar_->setFixedWidth(kBarWidth);

  best_lines_ = new BestLinesWidget(this);
  best_lines_->setMinimumWidth(kLinesWidth);
  classification_ = new MoveClassificationWidget(this);
  classification_->setMinimumWidth(kClassificationWidth);

  // Classification goes below the best lines, on the human's side; it isn't
  // mirrored since only human moves are classified.
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, kBarWidth + 8, 0);
  layout->addWidget(best_lines_, 1);
  layout->addWidget(classification_);

  connect(&session_, &GameSession::analysisUpdated, this,
          &AnalysisPanel::OnAnalysisUpdated);
  connect(&move_classifier_, &MoveClassifier::moveClassified, this,
          &AnalysisPanel::OnMoveClassified);
  connect(&move_classifier_, &MoveClassifier::reset, this,
          &AnalysisPanel::OnClassificationReset);
  // The bar's orientation must match the board, which flips with the human's
  // color.
  connect(&session_, &GameSession::gameStarted, this, [this]() {
    eval_bar_->SetFlipped(session_.PlayerColor() == PieceColor::kBlack);
  });

  eval_bar_->SetFlipped(session_.PlayerColor() == PieceColor::kBlack);
  UpdateVisibility();
}

void AnalysisPanel::SetShowEvaluationBar(bool show) {
  show_eval_bar_ = show;
  UpdateVisibility();
  emit wantsAnalysisChanged();
}

void AnalysisPanel::SetShowBestLines(bool show) {
  show_best_lines_ = show;
  UpdateVisibility();
  emit wantsAnalysisChanged();
}

void AnalysisPanel::SetShowClassification(bool show) {
  show_classification_ = show;
  UpdateVisibility();
  emit wantsAnalysisChanged();
}

void AnalysisPanel::SetClassifyForBoardBadge(bool enabled) {
  classify_for_board_badge_ = enabled;
  emit wantsAnalysisChanged();
}

void AnalysisPanel::UpdateVisibility() {
  eval_bar_->setVisible(show_eval_bar_);
  best_lines_->setVisible(show_best_lines_);
  classification_->setVisible(show_classification_);
  setVisible(show_eval_bar_ || show_best_lines_ || show_classification_);

  // Clear immediately so stale values don't reappear when re-enabled.
  if (!show_eval_bar_) eval_bar_->Clear();
  if (!show_best_lines_) best_lines_->ClearLines();
  if (!show_classification_) classification_->Reset();
}

int AnalysisPanel::EffectiveMinimumWidth() const {
  if (!show_eval_bar_ && !show_best_lines_ && !show_classification_) return 0;

  // best_lines_ and classification share one column, as wide as the wider of
  // the two. The eval bar is positioned externally in a margin reserved by the
  // layout, so its width is added separately.
  int column_width = std::max(show_best_lines_ ? kLinesWidth : 0,
                              show_classification_ ? kClassificationWidth : 0);
  int bar_reserve = show_eval_bar_ ? kBarWidth + 8 : 0;
  return column_width + bar_reserve;
}

// Only the top line drives the bar.
void AnalysisPanel::OnAnalysisUpdated(AnalysisInfo info) {
  const PieceColor side_to_move = session_.AnalysisSideToMove();
  best_lines_->SetPosition(session_.AnalysisFen(), side_to_move);

  if (info.multipv == 1) {
    // UCI scores are from the side to move; the bar reads from White's side, so
    // Black-to-move scores are negated.
    int sign = side_to_move == PieceColor::kWhite ? 1 : -1;

    if (info.score_mate)
      eval_bar_->SetScoreMate(*info.score_mate * sign);
    else
      eval_bar_->SetScoreCp(info.score_cp
                                ? std::optional<int>(*info.score_cp * sign)
                                : std::nullopt);
  }

  best_lines_->SetLine(info);
}

void AnalysisPanel::OnMoveClassified(MoveQuality quality,
                                     double accuracy_percent) {
  classification_->AddClassification(quality, accuracy_percent);
  emit moveClassifiedForBoard(quality);
}

void AnalysisPanel::OnClassificationReset() { classification_->Reset(); }
}  // namespace chess
