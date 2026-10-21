/*
 * Copyright (c) 2026 Kornfeld Eliyahu Peter
 * License: MIT
 */

#ifndef CHESSBOARD_UI_PROMOTION_DIALOG_HPP_
#define CHESSBOARD_UI_PROMOTION_DIALOG_HPP_

#include <QDialog>
#include <QFont>
#include <QPoint>

#include "models/piece.hpp"

namespace chess {
class PromotionDialog : public QDialog {
  Q_OBJECT

 public:
  PromotionDialog(PieceColor color, QFont piece_font, QPoint global_anchor,
                  QWidget* parent = nullptr);

  PieceType SelectedType() const { return selected_type_; }

 private:
  PieceType selected_type_ = PieceType::kQueen;
};
}  // namespace chess

#endif  // CHESSBOARD_UI_PROMOTION_DIALOG_HPP_
