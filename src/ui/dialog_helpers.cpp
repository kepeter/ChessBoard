/*
 * Copyright (c) 2026 Kornfeld Eliyahu Peter
 * License: MIT
 */

#include "ui/dialog_helpers.hpp"

#include <QDialog>
#include <QDialogButtonBox>
#include <QFontMetrics>

namespace chess {
QDialogButtonBox* MakeCloseButtonBox(QDialog* owner) {
  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, owner);
  // Close has the reject role; both signals accept so exec() reports Accepted.
  QObject::connect(buttons, &QDialogButtonBox::rejected, owner,
                   &QDialog::accept);
  QObject::connect(buttons, &QDialogButtonBox::accepted, owner,
                   &QDialog::accept);

  return buttons;
}

void EnsureTitleFits(QDialog* dialog) {
  // Titlebar fonts are bold and larger than the content font, so measure with
  // an enlarged bold copy.
  QFont title_font = dialog->font();
  title_font.setBold(true);
  title_font.setPointSize(title_font.pointSize() + 2);
  QFontMetrics title_metrics(title_font);
  // The extra 120 px leaves room for the window-control buttons.
  int title_width =
      title_metrics.horizontalAdvance(dialog->windowTitle()) + 120;

  if (title_width > dialog->width()) dialog->setFixedWidth(title_width);
}
}  // namespace chess
