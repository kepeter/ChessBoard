/*
 * Copyright (c) 2026 Kornfeld Eliyahu Peter
 * License: MIT
 */

#ifndef CHESSBOARD_UI_SETTINGS_DIALOG_HPP_
#define CHESSBOARD_UI_SETTINGS_DIALOG_HPP_

#include <functional>
#include <string>
#include <vector>

#include <QDialog>
#include <QString>

class QComboBox;
class QFormLayout;
class QLabel;
class QLineEdit;
class QVBoxLayout;

namespace chess {
class UciEngine;
struct UciOption;

// Tabbed settings: Board (theme, color, piece set) and Engine (path plus the
// UCI options the engine advertises). Board changes are written to AppConfig
// immediately, which is what gives the live preview.
class SettingsDialog : public QDialog {
  Q_OBJECT

 public:
  explicit SettingsDialog(UciEngine& uci_engine, QWidget* parent = nullptr);

 signals:
  void boardAppearanceChanged();
  // Also needed by widgets holding their own QFont copy (e.g. PromotionDialog).
  void pieceSetChanged();

 private:
  UciEngine& uci_engine_;

  QComboBox* theme_combo_;
  QComboBox* color_scheme_combo_;
  QComboBox* piece_set_combo_;
  QLineEdit* engine_path_edit_;
  QLabel* engine_status_label_;
  QVBoxLayout* engine_tab_layout_;
  // Everything below the path field; replaced wholesale by
  // RebuildEngineOptions.
  QWidget* engine_options_container_;
  // Guards against editingFinished re-applying an unchanged path.
  std::string last_applied_engine_path_;
  int engine_options_generation_ = 0;

  // One per option row, so "Reset all" can run each row's reset.
  std::vector<std::function<void()>> engine_option_resetters_;

  QWidget* MakeBoardTab();
  QWidget* MakeEngineTab();
  // Tears down the options area and requests options from the (possibly
  // restarting) engine. engine_options_generation_ makes replies to older
  // requests be ignored.
  void RebuildEngineOptions();
  void ClearEngineOptions();
  void PopulateEngineOptions(const std::vector<UciOption>& available);
  void AddOptionRow(QFormLayout* form, QWidget* parent, const UciOption& option,
                    const QString& current);
  QWidget* MakeStrengthGroupBox(QWidget* parent,
                                const std::vector<UciOption>& available);

  void OnThemeChanged(int index);
  void OnColorSchemeChanged(int index);
  void OnPieceSetChanged(int index);
  void OnEnginePathEdited();
  void RefreshEngineStatus();
};
}  // namespace chess

#endif  // CHESSBOARD_UI_SETTINGS_DIALOG_HPP_
