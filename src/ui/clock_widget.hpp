/*
 * Copyright (c) 2026 Kornfeld Eliyahu Peter
 * License: MIT
 */

#ifndef CHESSBOARD_UI_CLOCK_WIDGET_HPP_
#define CHESSBOARD_UI_CLOCK_WIDGET_HPP_

#include <chrono>

#include <QFont>
#include <QWidget>

namespace chess {
class ClockWidget : public QWidget {
  Q_OBJECT

 public:
  explicit ClockWidget(QWidget* parent = nullptr);

  void SetState(bool active, bool flagged, std::chrono::milliseconds remaining);

 protected:
  void paintEvent(QPaintEvent* event) override;
  void changeEvent(QEvent* event) override;

 private:
  bool active_ = false;
  bool flagged_ = false;
  std::chrono::milliseconds remaining_{0};
  // Bold, enlarged copy of the font, rebuilt only when the base font changes.
  QFont time_font_;

  void RebuildTimeFont();

  static QString FormatTime(std::chrono::milliseconds t);
};
}  // namespace chess

#endif  // CHESSBOARD_UI_CLOCK_WIDGET_HPP_
