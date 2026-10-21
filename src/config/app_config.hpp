/*
 * Copyright (c) 2026 Kornfeld Eliyahu Peter
 * License: MIT
 */

#ifndef CHESSBOARD_CONFIG_APP_CONFIG_HPP_
#define CHESSBOARD_CONFIG_APP_CONFIG_HPP_

#include <array>
#include <functional>

#include <QColor>
#include <QFont>
#include <QMap>
#include <QString>

namespace chess {
// Mirrors PieceColor plus "Random"; config sits below models, so it can't use
// that header.
enum class NewGameSide {
  kBlack,
  kRandom,
  kWhite,
};

enum class PieceSet {
  kStixTwoMath,
  kFreeSerif,
  kNishikiTeki,
  kNotoSansSymbols2,
  kDejaVuSans,
};

enum class BoardTheme {
  kWood,
  kMetal,
  kMarble,
  kGlass,
};

// Tint palette independent of the texture theme; the actual colors are tuned
// per theme (see ColorsForThemeAndScheme).
enum class BoardColorScheme {
  kGrey,
  kBrown,
  kGreen,
  kBlue,
  kRed,
};

// Tints blended into the theme's base textures at paint time, not flat fills.
struct BoardColors {
  QColor light_square;
  QColor dark_square;
};

struct BoardTextureFiles {
  QString light_square;
  QString dark_square;
};

// Process-wide settings singleton backed by QSettings (an INI file in the
// standard config directory).
class AppConfig {
 public:
  static AppConfig& Instance();

  AppConfig(const AppConfig&) = delete;
  AppConfig& operator=(const AppConfig&) = delete;

  QString EnginePath() const;
  void SetEnginePath(const QString& path);

  // Saved UCI option overrides (name -> value), applied on every engine
  // (re)start.
  QMap<QString, QString> UciOptionOverrides() const;
  void SetUciOptionOverride(const QString& name, const QString& value);
  void RemoveUciOptionOverride(const QString& name);
  // Drops saved overrides that `is_valid(name, value)` rejects so the engine
  // default applies. The caller supplies the check because option ranges are
  // only known once the engine has started.
  void RemoveInvalidUciOptionOverrides(
      const std::function<bool(const QString&, const QString&)>& is_valid);

  BoardTheme Theme() const;
  void SetTheme(BoardTheme theme);
  BoardColorScheme ColorScheme() const;
  void SetColorScheme(BoardColorScheme scheme);
  BoardColors Colors() const;

  PieceSet Set() const;
  void SetSet(PieceSet set);

  // Last New Game selections, restored as the dialog's defaults.
  NewGameSide LastNewGameSide() const;
  void SetLastNewGameSide(NewGameSide side);

  int LastTimeControlMinutes() const;
  int LastTimeControlIncrementSeconds() const;
  void SetLastTimeControl(int minutes, int increment_seconds);

  // Last strength choice: 0..kStrengthPresetCount-1 for a preset,
  // kStrengthPresetCount for "Full strength" (ui/strength_selector.hpp's
  // kFullStrengthButtonId).
  int LastNewGameStrengthId() const;
  void SetLastNewGameStrengthId(int id);

  // UCI_Elo for each named strength preset (0 = weakest), editable on the
  // Engine tab.
  static constexpr int kStrengthPresetCount = 9;
  std::array<int, kStrengthPresetCount> StrengthPresetElos() const;
  void SetStrengthPresetElo(int index, int elo);

  bool ShowEvaluationBar() const;
  void SetShowEvaluationBar(bool show);
  bool ShowBestLines() const;
  void SetShowBestLines(bool show);
  bool ShowAnalysisArrows() const;
  void SetShowAnalysisArrows(bool show);
  bool ShowArrowsDuringEngineMove() const;
  void SetShowArrowsDuringEngineMove(bool show);
  bool EngineMoveSound() const;
  void SetEngineMoveSound(bool enabled);
  bool ShowMoveClassification() const;
  void SetShowMoveClassification(bool show);
  bool ShowMoveClassificationOnBoard() const;
  void SetShowMoveClassificationOnBoard(bool show);

 private:
  AppConfig();

  QString engine_path_;
  QMap<QString, QString> uci_option_overrides_;
  BoardTheme theme_;
  BoardColorScheme color_scheme_;
  PieceSet piece_set_;
  NewGameSide last_new_game_side_;
  int last_time_control_minutes_;
  int last_time_control_increment_seconds_;
  std::array<int, kStrengthPresetCount> strength_preset_elos_;
  int last_new_game_strength_id_;
  bool show_evaluation_bar_;
  bool show_best_lines_;
  bool show_analysis_arrows_;
  bool show_arrows_during_engine_move_;
  bool engine_move_sound_;
  bool show_move_classification_;
  bool show_move_classification_on_board_;
};

BoardColors ColorsForThemeAndScheme(BoardTheme theme, BoardColorScheme scheme);
BoardTextureFiles TextureFilesForTheme(BoardTheme theme);

QString FontAssetFileNameForSet(PieceSet set);

// Loads the set's font file as an application font; falls back to the default
// QFont on failure.
QFont LoadPieceFont(PieceSet set);

QString AssetsDirPath();
}  // namespace chess

#endif  // CHESSBOARD_CONFIG_APP_CONFIG_HPP_
