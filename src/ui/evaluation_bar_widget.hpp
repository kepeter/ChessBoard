/*
 * Copyright (c) 2026 Kornfeld Eliyahu Peter
 * License: MIT
 */

#ifndef CHESSBOARD_UI_EVALUATION_BAR_WIDGET_HPP_
#define CHESSBOARD_UI_EVALUATION_BAR_WIDGET_HPP_

#include <optional>

#include <QWidget>

namespace chess {
// A vertical white/black eval bar. Unflipped, White fills from the bottom;
// SetFlipped(true) swaps the ends to match a flipped board, keeping the human's
// color at the bottom.
class EvaluationBarWidget : public QWidget {
  Q_OBJECT

 public:
  explicit EvaluationBarWidget(QWidget* parent = nullptr);

  // Score from White's perspective (positive favors White). nullopt shows a
  // neutral bar.
  void SetScoreCp(std::optional<int> score_cp);
  void SetScoreMate(std::optional<int> score_mate_white_pov);
  void Clear();

  void SetFlipped(bool flipped);

 protected:
  void paintEvent(QPaintEvent* event) override;

 private:
  std::optional<int> score_cp_;
  std::optional<int> score_mate_;
  bool flipped_ = false;

  double WhiteFraction() const;
};
}  // namespace chess

#endif  // CHESSBOARD_UI_EVALUATION_BAR_WIDGET_HPP_
