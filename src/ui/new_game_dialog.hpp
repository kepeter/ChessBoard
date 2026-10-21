/*
 * Copyright (c) 2026 Kornfeld Eliyahu Peter
 * License: MIT
 */

#ifndef CHESSBOARD_UI_NEW_GAME_DIALOG_HPP_
#define CHESSBOARD_UI_NEW_GAME_DIALOG_HPP_

#include <optional>
#include <string>

#include <QDialog>

#include "models/piece.hpp"
#include "models/time_control.hpp"

class QAbstractButton;
class QButtonGroup;
class QCheckBox;
class QLabel;
class QLineEdit;
class QSpinBox;
class QStackedWidget;

namespace chess {
class UciEngine;

class NewGameDialog : public QDialog {
  Q_OBJECT

 public:
  // `uci_engine` is only read, to seed the Ponder/Chess960 checkboxes; applying
  // the one-game overrides to the engine is the caller's job.
  explicit NewGameDialog(UciEngine& uci_engine, QWidget* parent = nullptr);

  PieceColor SelectedPlayerColor() const;
  TimeControl SelectedTimeControl() const;
  // 0..kStrengthPresetCount-1 for a preset, or kFullStrengthButtonId
  // (ui/strength_selector.hpp).
  int SelectedStrengthId() const;

  // One-game overrides: never saved to AppConfig (unlike side, time control and
  // strength). The caller sends them to the engine before starting and doesn't
  // restore them.
  bool SelectedPonder() const;
  bool SelectedChess960() const;

  // Custom starting position, or nullopt for the normal/random-960 start. Not
  // persisted.
  std::optional<std::string> SelectedStartingFen() const;

 private:
  // Button index: 0 = Black, 1 = Random, 2 = White.
  QButtonGroup* side_group_;

  // The checked id selects the page of preset_pages_: a TimeControlCategory
  // value, or kCustomCategoryId for the custom spin boxes.
  QButtonGroup* category_group_;
  QStackedWidget* preset_pages_;
  QSpinBox* custom_minutes_;
  QSpinBox* custom_increment_;

  QButtonGroup* strength_group_;

  QCheckBox* ponder_check_;
  QCheckBox* chess960_check_;

  QLineEdit* starting_fen_edit_;
  QLabel* starting_fen_status_;
};
}  // namespace chess

#endif  // CHESSBOARD_UI_NEW_GAME_DIALOG_HPP_
