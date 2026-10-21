/*
 * Copyright (c) 2026 Kornfeld Eliyahu Peter
 * License: MIT
 */

#ifndef CHESSBOARD_UI_LICENSE_DIALOG_HPP_
#define CHESSBOARD_UI_LICENSE_DIALOG_HPP_

#include <QDialog>
#include <QString>

namespace chess {
class LicenseDialog : public QDialog {
  Q_OBJECT

 public:
  LicenseDialog(const QString& title, const QString& font_file_path,
                QWidget* parent = nullptr);
};
}  // namespace chess

#endif  // CHESSBOARD_UI_LICENSE_DIALOG_HPP_
