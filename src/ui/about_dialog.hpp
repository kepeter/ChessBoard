/*
 * Copyright (c) 2026 Kornfeld Eliyahu Peter
 * License: MIT
 */

#ifndef CHESSBOARD_UI_ABOUT_DIALOG_HPP_
#define CHESSBOARD_UI_ABOUT_DIALOG_HPP_

#include <QDialog>

namespace chess {
class AboutDialog : public QDialog {
  Q_OBJECT

 public:
  explicit AboutDialog(QWidget* parent = nullptr);
};
}  // namespace chess

#endif  // CHESSBOARD_UI_ABOUT_DIALOG_HPP_
