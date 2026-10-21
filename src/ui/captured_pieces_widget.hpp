/*
 * Copyright (c) 2026 Kornfeld Eliyahu Peter
 * License: MIT
 */

#ifndef CHESSBOARD_UI_CAPTURED_PIECES_WIDGET_HPP_
#define CHESSBOARD_UI_CAPTURED_PIECES_WIDGET_HPP_

#include <vector>

#include <QFont>
#include <QWidget>

#include "models/piece.hpp"

namespace chess {
// Icons of the pieces one player has captured, grouped by type, plus a "+N"
// material lead. The captured color is passed with every update, so the widget
// stays correct when the human changes sides.
class CapturedPiecesWidget : public QWidget {
  Q_OBJECT

 public:
  explicit CapturedPiecesWidget(QFont piece_font, QWidget* parent = nullptr);

  void SetPieceFont(QFont piece_font);

  // `captured` are pieces of `captured_color`; `material_lead` is the
  // capturer's point advantage (0 or less hides the badge).
  void SetCaptured(std::vector<PieceType> captured, PieceColor captured_color,
                   int material_lead);

 protected:
  void paintEvent(QPaintEvent* event) override;

 private:
  QFont piece_font_;
  // piece_font_ sized for icons, cached across paints.
  QFont icon_font_;
  std::vector<PieceType> captured_;
  PieceColor captured_color_ = PieceColor::kBlack;
  int material_lead_ = 0;
};
}  // namespace chess

#endif  // CHESSBOARD_UI_CAPTURED_PIECES_WIDGET_HPP_
