/*
 * Copyright (c) 2026 Kornfeld Eliyahu Peter
 * License: MIT
 */

#include "ui/license_dialog.hpp"

#include <QDialogButtonBox>
#include <QTextEdit>
#include <QVBoxLayout>

#include "ui/dialog_helpers.hpp"
#include "ui/font_metadata.hpp"

namespace chess {
namespace {
QString OrPlaceholder(const QString& value) {
  return value.isEmpty() ? QString("(not present in font)") : value;
}
}  // namespace

LicenseDialog::LicenseDialog(const QString& title,
                             const QString& font_file_path, QWidget* parent)
    : QDialog(parent) {
  setWindowTitle(title);
  setModal(true);
  resize(720, 360);

  FontMetadata metadata = ReadFontMetadata(font_file_path);

  QString text = "Copyright\n" + OrPlaceholder(metadata.copyright) +
                 "\n\nLicense\n" + OrPlaceholder(metadata.license_description) +
                 "\n\nLicense URL\n" + OrPlaceholder(metadata.license_url);

  auto* text_edit = new QTextEdit(this);
  text_edit->setReadOnly(true);
  text_edit->setPlainText(text);

  auto* buttons = MakeCloseButtonBox(this);

  auto* layout = new QVBoxLayout(this);
  layout->addWidget(text_edit);
  layout->addWidget(buttons);
}
}  // namespace chess
