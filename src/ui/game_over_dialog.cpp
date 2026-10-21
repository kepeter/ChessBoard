/*
 * Copyright (c) 2026 Kornfeld Eliyahu Peter
 * License: MIT
 */

#include "ui/game_over_dialog.hpp"

#include <QDialogButtonBox>
#include <QLabel>
#include <QVBoxLayout>

#include "ui/dialog_helpers.hpp"

namespace chess {
GameOverDialog::GameOverDialog(const QString& message, QWidget* parent)
    : QDialog(parent) {
  setWindowTitle("Game Over");
  setModal(true);

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(24, 20, 24, 20);
  layout->setSpacing(16);

  auto* label = new QLabel(message, this);
  label->setAlignment(Qt::AlignHCenter);
  layout->addWidget(label);

  layout->addWidget(MakeCloseButtonBox(this));

  // Wider than the message alone so the dialog doesn't look cramped.
  adjustSize();
  setFixedWidth(geometry().size().width() + 200);
  setFixedHeight(geometry().size().height());
}
}  // namespace chess
