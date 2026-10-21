/*
 * Copyright (c) 2026 Kornfeld Eliyahu Peter
 * License: MIT
 */

#include "ui/promotion_dialog.hpp"

#include <utility>

#include <QHBoxLayout>
#include <QPushButton>

#include "models/piece.hpp"
#include "ui/dialog_helpers.hpp"
#include "ui/glyph_rendering.hpp"

namespace chess {
PromotionDialog::PromotionDialog(PieceColor color, QFont piece_font,
                                 QPoint global_anchor, QWidget* parent)
    : QDialog(parent) {
  setWindowTitle("Choose Promotion");
  setModal(true);

  constexpr int kButtonSize = 96;

  QFont glyph_font = GlyphFontForSquareSize(piece_font, kButtonSize);

  static const PieceType kChoices[4] = {PieceType::kQueen, PieceType::kRook,
                                        PieceType::kBishop, PieceType::kKnight};

  auto* layout = new QHBoxLayout(this);
  for (PieceType type : kChoices) {
    Piece promo_piece(type, color);
    auto* button = new QPushButton(promo_piece.Glyph(), this);
    button->setFixedSize(kButtonSize, kButtonSize);
    button->setFont(glyph_font);
    connect(button, &QPushButton::clicked, this, [this, type]() {
      selected_type_ = type;
      accept();
    });
    layout->addWidget(button);
  }

  // adjustSize() first so the real size is known, then center on global_anchor.
  adjustSize();
  EnsureTitleFits(this);

  move(global_anchor - QPoint(width() / 2, height() / 2));
}
}  // namespace chess
