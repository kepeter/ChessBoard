/*
 * Copyright (c) 2026 Kornfeld Eliyahu Peter
 * License: MIT
 */

#ifndef CHESSBOARD_UI_ANALYSIS_PANEL_HPP_
#define CHESSBOARD_UI_ANALYSIS_PANEL_HPP_

#include <QWidget>

#include "engine/chess_engine.hpp"
#include "session/game_session.hpp"
#include "session/move_classifier.hpp"

namespace chess {
class EvaluationBarWidget;
class BestLinesWidget;
class MoveClassificationWidget;

// Left-of-board panel with the evaluation bar and best lines. Each is shown
// independently; the panel collapses to zero width when all are hidden.
class AnalysisPanel : public QWidget {
  Q_OBJECT

 public:
  static constexpr int kBarWidth = 28;
  static constexpr int kLinesWidth = 260;
  static constexpr int kClassificationWidth = 160;

  AnalysisPanel(ChessEngine& engine, GameSession& session,
                QWidget* parent = nullptr);

  void SetShowEvaluationBar(bool show);
  void SetShowBestLines(bool show);
  void SetShowClassification(bool show);
  // Shows nothing here: enables classification for MainWindow's board badge and
  // keeps WantsAnalysis() true even if all sub-widgets are hidden.
  void SetClassifyForBoardBadge(bool enabled);

  int EffectiveMinimumWidth() const;

  // Positioned by MainWindowCentral, not this panel's layout, so it can match
  // the board's grid rect. This panel still owns it and feeds it.
  EvaluationBarWidget* EvalBar() const { return eval_bar_; }

  // Whether any sub-widget or the board badge needs analysis. The panel only
  // reports this; GameSession's analysis is shared with other consumers (e.g.
  // arrows).
  bool WantsAnalysis() const {
    return show_eval_bar_ || show_best_lines_ || show_classification_ ||
           classify_for_board_badge_;
  }

 signals:
  // Fired when WantsAnalysis() may have changed, so MainWindow can recompute
  // the shared analysis flag.
  void wantsAnalysisChanged();

  // Forwards moveClassified for the board badge, independent of this panel's
  // own counter row.
  void moveClassifiedForBoard(MoveQuality quality);

 private slots:
  void OnAnalysisUpdated(AnalysisInfo info);
  void OnMoveClassified(MoveQuality quality, double accuracy_percent);
  void OnClassificationReset();

 private:
  ChessEngine& engine_;
  GameSession& session_;
  MoveClassifier move_classifier_;

  EvaluationBarWidget* eval_bar_;
  BestLinesWidget* best_lines_;
  MoveClassificationWidget* classification_;

  bool show_eval_bar_ = false;
  bool show_best_lines_ = false;
  bool show_classification_ = false;
  bool classify_for_board_badge_ = false;

  void UpdateVisibility();
};
}  // namespace chess

#endif  // CHESSBOARD_UI_ANALYSIS_PANEL_HPP_
