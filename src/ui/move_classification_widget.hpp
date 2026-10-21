/*
 * Copyright (c) 2026 Kornfeld Eliyahu Peter
 * License: MIT
 */

#ifndef CHESSBOARD_UI_MOVE_CLASSIFICATION_WIDGET_HPP_
#define CHESSBOARD_UI_MOVE_CLASSIFICATION_WIDGET_HPP_

#include <array>
#include <optional>

#include <QWidget>

#include "session/move_classifier.hpp"

namespace chess {
// A row of colored badges counting the human's moves per MoveQuality, plus a
// running accuracy percentage.
class MoveClassificationWidget : public QWidget {
  Q_OBJECT

 public:
  explicit MoveClassificationWidget(QWidget* parent = nullptr);

  void AddClassification(MoveQuality quality, double accuracy_percent);
  void Reset();

 protected:
  void paintEvent(QPaintEvent* event) override;
  bool event(QEvent* event) override;

 private:
  static constexpr int kQualityCount = 5;
  std::array<int, kQualityCount> counts_{};
  // nullopt: no moves classified yet (accuracy hidden).
  std::optional<double> accuracy_percent_;

  // Width of the badge area, shared by paintEvent and the tooltip hit-test.
  double BadgesAreaWidth() const;
};
}  // namespace chess

#endif  // CHESSBOARD_UI_MOVE_CLASSIFICATION_WIDGET_HPP_
