/*
 * Copyright (c) 2026 Kornfeld Eliyahu Peter
 * License: MIT
 */

#ifndef CHESSBOARD_UI_GAME_OVER_DIALOG_HPP_
#define CHESSBOARD_UI_GAME_OVER_DIALOG_HPP_

#include <QDialog>
#include <QString>

namespace chess {
class GameOverDialog : public QDialog {
  Q_OBJECT

 public:
  explicit GameOverDialog(const QString& message, QWidget* parent = nullptr);
};
}  // namespace chess

#endif  // CHESSBOARD_UI_GAME_OVER_DIALOG_HPP_
