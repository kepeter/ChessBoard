/*
 * Copyright (c) 2026 Kornfeld Eliyahu Peter
 * License: MIT
 */

#ifndef CHESSBOARD_UI_CLICKABLE_ROW_HPP_
#define CHESSBOARD_UI_CLICKABLE_ROW_HPP_

#include <QFrame>
#include <QMouseEvent>

namespace chess {
// A QFrame with a click signal, keeping QFrame's styling and layout.
class ClickableRow : public QFrame {
  Q_OBJECT

 public:
  explicit ClickableRow(QWidget* parent = nullptr) : QFrame(parent) {
    setCursor(Qt::PointingHandCursor);
  }

 signals:
  void clicked();

 protected:
  void mouseReleaseEvent(QMouseEvent* event) override {
    // A click requires the release to be over the frame, as with normal
    // buttons.
    if (event->button() == Qt::LeftButton && rect().contains(event->pos()))
      emit clicked();
    QFrame::mouseReleaseEvent(event);
  }
};
}  // namespace chess

#endif  // CHESSBOARD_UI_CLICKABLE_ROW_HPP_
