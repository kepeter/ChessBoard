/*
 * Copyright (c) 2026 Kornfeld Eliyahu Peter
 * License: MIT
 */

#include "config/app_config.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QFontDatabase>
#include <QSettings>
#include <QStandardPaths>
#include <QStringList>
#include <QtGlobal>

namespace chess {
namespace {
constexpr const char* kEnginePathKey = "engine/path";
constexpr const char* kUciOptionsGroup = "engine/uci_options";
constexpr const char* kThemeKey = "board/theme";
constexpr const char* kColorSchemeKey = "board/color_scheme";
constexpr const char* kPieceSetKey = "board/piece_set";
constexpr const char* kNewGameSideKey = "newgame/side";
constexpr const char* kNewGameMinutesKey = "newgame/minutes";
constexpr const char* kNewGameIncrementKey = "newgame/increment_seconds";
constexpr const char* kStrengthPresetKeyPrefix = "engine/strength_preset_";
constexpr const char* kNewGameStrengthIdKey = "newgame/strength_id";
constexpr const char* kShowEvaluationBarKey = "analysis/show_evaluation_bar";
constexpr const char* kShowBestLinesKey = "analysis/show_best_lines";
constexpr const char* kShowAnalysisArrowsKey = "analysis/show_arrows";
constexpr const char* kShowArrowsDuringEngineMoveKey =
    "analysis/show_arrows_during_engine_move";
constexpr const char* kEngineMoveSoundKey = "sound/engine_move";
constexpr const char* kShowMoveClassificationKey =
    "analysis/show_classification";
constexpr const char* kShowMoveClassificationOnBoardKey =
    "analysis/show_classification_on_board";

constexpr const char* kDefaultEnginePath = "stockfish";
constexpr BoardTheme kDefaultTheme = BoardTheme::kWood;
constexpr BoardColorScheme kDefaultColorScheme = BoardColorScheme::kBrown;
constexpr PieceSet kDefaultPieceSet = PieceSet::kStixTwoMath;
constexpr NewGameSide kDefaultNewGameSide = NewGameSide::kRandom;
constexpr int kDefaultTimeControlMinutes = 30;
constexpr int kDefaultTimeControlIncrementSeconds = 0;
constexpr int kDefaultNewGameStrengthId = AppConfig::kStrengthPresetCount;
constexpr bool kDefaultShowEvaluationBar = false;
constexpr bool kDefaultShowBestLines = false;
constexpr bool kDefaultShowAnalysisArrows = false;
constexpr bool kDefaultShowArrowsDuringEngineMove = false;
constexpr bool kDefaultEngineMoveSound = false;
constexpr bool kDefaultShowMoveClassification = false;
constexpr bool kDefaultShowMoveClassificationOnBoard = false;

// Default Elo per preset, within Stockfish's UCI_Elo bounds (1320-3190) so no
// two collide after clamping.
constexpr std::array<int, AppConfig::kStrengthPresetCount>
    kDefaultStrengthPresetElos = {
        1320, 1450, 1600, 1750, 1900, 2100, 2350, 2700, 3190,
};

// Valid ranges for values read from the file; anything outside falls back to
// the default.
constexpr int kMinStrengthPresetElo = 1320;
constexpr int kMaxStrengthPresetElo = 3190;
constexpr int kMinTimeControlMinutes = 1;
constexpr int kMaxTimeControlMinutes = 180;
constexpr int kMinTimeControlIncrementSeconds = 0;
constexpr int kMaxTimeControlIncrementSeconds = 60;

// Maps an enum to/from its settings string. entries[0] is the default for both
// directions (unlisted value, unrecognized string).
template <typename Enum>
struct EnumStringMap {
  struct Entry {
    Enum value;
    const char* name;
  };
  const Entry* entries;
  size_t count;

  QString NameOf(Enum value) const {
    for (size_t i = 0; i < count; ++i) {
      if (entries[i].value == value) return entries[i].name;
    }

    return entries[0].name;
  }

  Enum FromName(const QString& name) const {
    for (size_t i = 0; i < count; ++i) {
      if (name == entries[i].name) return entries[i].value;
    }

    return entries[0].value;
  }
};

template <typename Enum, size_t N>
constexpr EnumStringMap<Enum> MakeEnumStringMap(
    const typename EnumStringMap<Enum>::Entry (&entries)[N]) {
  return EnumStringMap<Enum>{entries, N};
}

constexpr EnumStringMap<NewGameSide>::Entry kNewGameSideEntries[] = {
    {NewGameSide::kRandom, "random"},
    {NewGameSide::kBlack, "black"},
    {NewGameSide::kWhite, "white"},
};
constexpr auto kNewGameSideMap =
    MakeEnumStringMap<NewGameSide>(kNewGameSideEntries);

constexpr EnumStringMap<BoardTheme>::Entry kThemeEntries[] = {
    {BoardTheme::kWood, "wood"},
    {BoardTheme::kMetal, "metal"},
    {BoardTheme::kMarble, "marble"},
    {BoardTheme::kGlass, "glass"},
};
constexpr auto kThemeMap = MakeEnumStringMap<BoardTheme>(kThemeEntries);

constexpr EnumStringMap<BoardColorScheme>::Entry kColorSchemeEntries[] = {
    {BoardColorScheme::kBrown, "brown"}, {BoardColorScheme::kGrey, "grey"},
    {BoardColorScheme::kGreen, "green"}, {BoardColorScheme::kBlue, "blue"},
    {BoardColorScheme::kRed, "red"},
};
constexpr auto kColorSchemeMap =
    MakeEnumStringMap<BoardColorScheme>(kColorSchemeEntries);

constexpr EnumStringMap<PieceSet>::Entry kPieceSetEntries[] = {
    {PieceSet::kStixTwoMath, "stix_two_math"},
    {PieceSet::kFreeSerif, "free_serif"},
    {PieceSet::kNishikiTeki, "nishiki_teki"},
    {PieceSet::kNotoSansSymbols2, "noto_sans_symbols2"},
    {PieceSet::kDejaVuSans, "dejavusans"},
};
constexpr auto kPieceSetMap = MakeEnumStringMap<PieceSet>(kPieceSetEntries);

QString NewGameSideName(NewGameSide side) {
  return kNewGameSideMap.NameOf(side);
}
NewGameSide NewGameSideFromName(const QString& name) {
  return kNewGameSideMap.FromName(name);
}

QString ThemeName(BoardTheme theme) { return kThemeMap.NameOf(theme); }
BoardTheme ThemeFromName(const QString& name) {
  return kThemeMap.FromName(name);
}

QString ColorSchemeName(BoardColorScheme scheme) {
  return kColorSchemeMap.NameOf(scheme);
}
BoardColorScheme ColorSchemeFromName(const QString& name) {
  return kColorSchemeMap.FromName(name);
}

QString PieceSetName(PieceSet set) { return kPieceSetMap.NameOf(set); }
PieceSet PieceSetFromName(const QString& name) {
  return kPieceSetMap.FromName(name);
}

// Re-opened on each access; QSettings is cheap and this keeps writes durable.
QSettings OpenSettings() {
  QString dir =
      QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);

  QDir().mkpath(dir);

  return QSettings(QDir(dir).filePath("config.ini"), QSettings::IniFormat);
}

}  // namespace

// Tints blended into each theme's base texture (see BoardWidget's TintTexture).
// Values are hand-picked per (theme, scheme) pair because the same nominal
// color needs different RGB against different textures.
BoardColors ColorsForThemeAndScheme(BoardTheme theme, BoardColorScheme scheme) {
  switch (theme) {
    case BoardTheme::kMetal: {
      switch (scheme) {
        case BoardColorScheme::kGrey:
          return BoardColors{.light_square = QColor(0xe6, 0xea, 0xee),
                             .dark_square = QColor(0x3a, 0x41, 0x49)};
        case BoardColorScheme::kGreen:
          return BoardColors{.light_square = QColor(0xc7, 0xea, 0xc0),
                             .dark_square = QColor(0x1a, 0x5c, 0x38)};
        case BoardColorScheme::kBlue:
          return BoardColors{.light_square = QColor(0xb9, 0xdc, 0xf2),
                             .dark_square = QColor(0x12, 0x4c, 0x78)};
        case BoardColorScheme::kRed:
          return BoardColors{.light_square = QColor(0xf0, 0xb8, 0xac),
                             .dark_square = QColor(0x8a, 0x1e, 0x18)};
        case BoardColorScheme::kBrown:
        default:
          return BoardColors{.light_square = QColor(0xe8, 0xc9, 0x9a),
                             .dark_square = QColor(0x6e, 0x44, 0x22)};
      }
    }

    case BoardTheme::kMarble: {
      switch (scheme) {
        case BoardColorScheme::kGrey:
          return BoardColors{.light_square = QColor(0xd6, 0xd6, 0xd2),
                             .dark_square = QColor(0x4a, 0x4c, 0x4e)};
        case BoardColorScheme::kGreen:
          return BoardColors{.light_square = QColor(0xdb, 0xe0, 0xcf),
                             .dark_square = QColor(0x35, 0x49, 0x3c)};
        case BoardColorScheme::kBlue:
          return BoardColors{.light_square = QColor(0xd2, 0xdd, 0xe4),
                             .dark_square = QColor(0x2c, 0x40, 0x54)};
        case BoardColorScheme::kRed:
          return BoardColors{.light_square = QColor(0xe3, 0xcf, 0xca),
                             .dark_square = QColor(0x64, 0x2a, 0x28)};
        case BoardColorScheme::kBrown:
        default:
          return BoardColors{.light_square = QColor(0xe6, 0xd8, 0xc0),
                             .dark_square = QColor(0x5a, 0x46, 0x33)};
      }
    }

    case BoardTheme::kGlass: {
      switch (scheme) {
        case BoardColorScheme::kGrey:
          return BoardColors{.light_square = QColor(0xd2, 0xd6, 0xd8),
                             .dark_square = QColor(0x40, 0x46, 0x49)};
        case BoardColorScheme::kGreen:
          return BoardColors{.light_square = QColor(0xcd, 0xe6, 0xda),
                             .dark_square = QColor(0x22, 0x4d, 0x3d)};
        case BoardColorScheme::kBlue:
          return BoardColors{.light_square = QColor(0xcf, 0xe4, 0xea),
                             .dark_square = QColor(0x2d, 0x50, 0x5c)};
        case BoardColorScheme::kRed:
          return BoardColors{.light_square = QColor(0xe8, 0xd4, 0xd2),
                             .dark_square = QColor(0x5e, 0x28, 0x2a)};
        case BoardColorScheme::kBrown:
        default:
          return BoardColors{.light_square = QColor(0xe0, 0xd6, 0xc4),
                             .dark_square = QColor(0x4d, 0x40, 0x30)};
      }
    }

    case BoardTheme::kWood:
    default: {
      switch (scheme) {
        case BoardColorScheme::kGrey:
          return BoardColors{.light_square = QColor(0xdd, 0xdb, 0xc0),
                             .dark_square = QColor(0x82, 0x80, 0x60)};
        case BoardColorScheme::kGreen:
          return BoardColors{.light_square = QColor(0xd9, 0xdc, 0xac),
                             .dark_square = QColor(0x4b, 0x5b, 0x33)};
        case BoardColorScheme::kBlue:
          return BoardColors{.light_square = QColor(0xc8, 0xd4, 0xdb),
                             .dark_square = QColor(0x30, 0x4b, 0x5e)};
        case BoardColorScheme::kRed:
          return BoardColors{.light_square = QColor(0xe8, 0xb3, 0x9a),
                             .dark_square = QColor(0x9c, 0x33, 0x2a)};
        case BoardColorScheme::kBrown:
        default:
          return BoardColors{.light_square = QColor(0xe4, 0xc4, 0x92),
                             .dark_square = QColor(0x8a, 0x5a, 0x33)};
      }
    }
  }
}

BoardTextureFiles TextureFilesForTheme(BoardTheme theme) {
  switch (theme) {
    case BoardTheme::kMetal:
      return BoardTextureFiles{"metal_light_base.png", "metal_dark_base.png"};
    case BoardTheme::kMarble:
      return BoardTextureFiles{"marble_light_base.png", "marble_dark_base.png"};
    case BoardTheme::kGlass:
      return BoardTextureFiles{"glass_light_base.png", "glass_dark_base.png"};

    case BoardTheme::kWood:
    default:
      return BoardTextureFiles{"wood_light_base.png", "wood_dark_base.png"};
  }
}

QString FontAssetFileNameForSet(PieceSet set) {
  switch (set) {
    case PieceSet::kFreeSerif:
      return "FreeSerif.otf";
    case PieceSet::kNishikiTeki:
      return "NishikiTeki.ttf";
    case PieceSet::kNotoSansSymbols2:
      return "NotoSansSymbols2.ttf";
    case PieceSet::kDejaVuSans:
      return "DejaVuSans.ttf";

    case PieceSet::kStixTwoMath:
    default: {
      return "STIXTwoMath.otf";
    }
  }
}

QFont LoadPieceFont(PieceSet set) {
  QString font_path = AssetsDirPath() + "/" + FontAssetFileNameForSet(set);
  int font_id = QFontDatabase::addApplicationFont(font_path);

  return font_id >= 0
             ? QFont(QFontDatabase::applicationFontFamilies(font_id).first())
             : QFont();
}

AppConfig& AppConfig::Instance() {
  static AppConfig instance;

  return instance;
}

AppConfig::AppConfig() {
  QSettings settings = OpenSettings();
  engine_path_ = settings.value(kEnginePathKey, kDefaultEnginePath).toString();

  settings.beginGroup(kUciOptionsGroup);
  for (const QString& key : settings.childKeys())
    uci_option_overrides_[key] = settings.value(key).toString();
  settings.endGroup();
  theme_ = ThemeFromName(
      settings.value(kThemeKey, ThemeName(kDefaultTheme)).toString());
  color_scheme_ = ColorSchemeFromName(
      settings.value(kColorSchemeKey, ColorSchemeName(kDefaultColorScheme))
          .toString());
  piece_set_ = PieceSetFromName(
      settings.value(kPieceSetKey, PieceSetName(kDefaultPieceSet)).toString());
  last_new_game_side_ = NewGameSideFromName(
      settings.value(kNewGameSideKey, NewGameSideName(kDefaultNewGameSide))
          .toString());
  last_time_control_minutes_ =
      settings.value(kNewGameMinutesKey, kDefaultTimeControlMinutes).toInt();
  last_time_control_increment_seconds_ =
      settings.value(kNewGameIncrementKey, kDefaultTimeControlIncrementSeconds)
          .toInt();
  if (last_time_control_minutes_ < kMinTimeControlMinutes ||
      last_time_control_minutes_ > kMaxTimeControlMinutes ||
      last_time_control_increment_seconds_ < kMinTimeControlIncrementSeconds ||
      last_time_control_increment_seconds_ > kMaxTimeControlIncrementSeconds) {
    last_time_control_minutes_ = kDefaultTimeControlMinutes;
    last_time_control_increment_seconds_ = kDefaultTimeControlIncrementSeconds;
  }

  for (int i = 0; i < kStrengthPresetCount; ++i) {
    strength_preset_elos_[i] =
        settings
            .value(QString(kStrengthPresetKeyPrefix) + QString::number(i),
                   kDefaultStrengthPresetElos[i])
            .toInt();
    if (strength_preset_elos_[i] < kMinStrengthPresetElo ||
        strength_preset_elos_[i] > kMaxStrengthPresetElo) {
      strength_preset_elos_[i] = kDefaultStrengthPresetElos[i];
    }
  }

  last_new_game_strength_id_ =
      settings.value(kNewGameStrengthIdKey, kDefaultNewGameStrengthId).toInt();
  if (last_new_game_strength_id_ < 0 ||
      last_new_game_strength_id_ > kStrengthPresetCount) {
    last_new_game_strength_id_ = kDefaultNewGameStrengthId;
  }

  show_evaluation_bar_ =
      settings.value(kShowEvaluationBarKey, kDefaultShowEvaluationBar).toBool();
  show_best_lines_ =
      settings.value(kShowBestLinesKey, kDefaultShowBestLines).toBool();
  show_analysis_arrows_ =
      settings.value(kShowAnalysisArrowsKey, kDefaultShowAnalysisArrows)
          .toBool();
  show_arrows_during_engine_move_ =
      settings
          .value(kShowArrowsDuringEngineMoveKey,
                 kDefaultShowArrowsDuringEngineMove)
          .toBool();
  engine_move_sound_ =
      settings.value(kEngineMoveSoundKey, kDefaultEngineMoveSound).toBool();
  show_move_classification_ =
      settings.value(kShowMoveClassificationKey, kDefaultShowMoveClassification)
          .toBool();
  show_move_classification_on_board_ =
      settings
          .value(kShowMoveClassificationOnBoardKey,
                 kDefaultShowMoveClassificationOnBoard)
          .toBool();
}

QString AppConfig::EnginePath() const { return engine_path_; }

void AppConfig::SetEnginePath(const QString& path) {
  engine_path_ = path;
  QSettings settings = OpenSettings();

  settings.setValue(kEnginePathKey, engine_path_);
}

QMap<QString, QString> AppConfig::UciOptionOverrides() const {
  return uci_option_overrides_;
}

void AppConfig::SetUciOptionOverride(const QString& name,
                                     const QString& value) {
  uci_option_overrides_[name] = value;
  QSettings settings = OpenSettings();

  settings.beginGroup(kUciOptionsGroup);
  settings.setValue(name, value);
  settings.endGroup();
}

void AppConfig::RemoveInvalidUciOptionOverrides(
    const std::function<bool(const QString&, const QString&)>& is_valid) {
  const QMap<QString, QString> overrides = uci_option_overrides_;
  for (auto it = overrides.cbegin(); it != overrides.cend(); ++it) {
    if (!is_valid(it.key(), it.value())) RemoveUciOptionOverride(it.key());
  }
}

void AppConfig::RemoveUciOptionOverride(const QString& name) {
  uci_option_overrides_.remove(name);
  QSettings settings = OpenSettings();

  settings.beginGroup(kUciOptionsGroup);
  settings.remove(name);
  settings.endGroup();
}

BoardTheme AppConfig::Theme() const { return theme_; }

void AppConfig::SetTheme(BoardTheme theme) {
  theme_ = theme;
  QSettings settings = OpenSettings();

  settings.setValue(kThemeKey, ThemeName(theme_));
}

BoardColorScheme AppConfig::ColorScheme() const { return color_scheme_; }

void AppConfig::SetColorScheme(BoardColorScheme scheme) {
  color_scheme_ = scheme;
  QSettings settings = OpenSettings();

  settings.setValue(kColorSchemeKey, ColorSchemeName(color_scheme_));
}

BoardColors AppConfig::Colors() const {
  return ColorsForThemeAndScheme(theme_, color_scheme_);
}

PieceSet AppConfig::Set() const { return piece_set_; }

void AppConfig::SetSet(PieceSet set) {
  piece_set_ = set;
  QSettings settings = OpenSettings();

  settings.setValue(kPieceSetKey, PieceSetName(piece_set_));
}

NewGameSide AppConfig::LastNewGameSide() const { return last_new_game_side_; }

void AppConfig::SetLastNewGameSide(NewGameSide side) {
  last_new_game_side_ = side;
  QSettings settings = OpenSettings();

  settings.setValue(kNewGameSideKey, NewGameSideName(last_new_game_side_));
}

int AppConfig::LastTimeControlMinutes() const {
  return last_time_control_minutes_;
}
int AppConfig::LastTimeControlIncrementSeconds() const {
  return last_time_control_increment_seconds_;
}

void AppConfig::SetLastTimeControl(int minutes, int increment_seconds) {
  last_time_control_minutes_ = minutes;
  last_time_control_increment_seconds_ = increment_seconds;
  QSettings settings = OpenSettings();

  settings.setValue(kNewGameMinutesKey, last_time_control_minutes_);
  settings.setValue(kNewGameIncrementKey, last_time_control_increment_seconds_);
}

std::array<int, AppConfig::kStrengthPresetCount> AppConfig::StrengthPresetElos()
    const {
  return strength_preset_elos_;
}

void AppConfig::SetStrengthPresetElo(int index, int elo) {
  if (index < 0 || index >= kStrengthPresetCount) return;
  strength_preset_elos_[index] = elo;
  QSettings settings = OpenSettings();

  settings.setValue(QString(kStrengthPresetKeyPrefix) + QString::number(index),
                    elo);
}

int AppConfig::LastNewGameStrengthId() const {
  return last_new_game_strength_id_;
}

void AppConfig::SetLastNewGameStrengthId(int id) {
  last_new_game_strength_id_ = id;
  QSettings settings = OpenSettings();

  settings.setValue(kNewGameStrengthIdKey, last_new_game_strength_id_);
}

bool AppConfig::ShowEvaluationBar() const { return show_evaluation_bar_; }

void AppConfig::SetShowEvaluationBar(bool show) {
  show_evaluation_bar_ = show;
  QSettings settings = OpenSettings();

  settings.setValue(kShowEvaluationBarKey, show_evaluation_bar_);
}

bool AppConfig::ShowBestLines() const { return show_best_lines_; }

void AppConfig::SetShowBestLines(bool show) {
  show_best_lines_ = show;
  QSettings settings = OpenSettings();

  settings.setValue(kShowBestLinesKey, show_best_lines_);
}

bool AppConfig::ShowAnalysisArrows() const { return show_analysis_arrows_; }

void AppConfig::SetShowAnalysisArrows(bool show) {
  show_analysis_arrows_ = show;
  QSettings settings = OpenSettings();

  settings.setValue(kShowAnalysisArrowsKey, show_analysis_arrows_);
}

bool AppConfig::ShowArrowsDuringEngineMove() const {
  return show_arrows_during_engine_move_;
}

void AppConfig::SetShowArrowsDuringEngineMove(bool show) {
  show_arrows_during_engine_move_ = show;
  QSettings settings = OpenSettings();

  settings.setValue(kShowArrowsDuringEngineMoveKey,
                    show_arrows_during_engine_move_);
}

bool AppConfig::EngineMoveSound() const { return engine_move_sound_; }

void AppConfig::SetEngineMoveSound(bool enabled) {
  engine_move_sound_ = enabled;
  QSettings settings = OpenSettings();

  settings.setValue(kEngineMoveSoundKey, engine_move_sound_);
}

bool AppConfig::ShowMoveClassification() const {
  return show_move_classification_;
}

void AppConfig::SetShowMoveClassification(bool show) {
  show_move_classification_ = show;
  QSettings settings = OpenSettings();

  settings.setValue(kShowMoveClassificationKey, show_move_classification_);
}

bool AppConfig::ShowMoveClassificationOnBoard() const {
  return show_move_classification_on_board_;
}

void AppConfig::SetShowMoveClassificationOnBoard(bool show) {
  show_move_classification_on_board_ = show;
  QSettings settings = OpenSettings();

  settings.setValue(kShowMoveClassificationOnBoardKey,
                    show_move_classification_on_board_);
}

// Assets are located relative to the executable, not the working directory:
// <exe dir>/assets (build tree) or <exe dir>/../share/chessboard/assets
// (installed). If neither exists a warning lists the paths tried and the first
// is returned anyway.
QString AssetsDirPath() {
  const QString exe_dir = QCoreApplication::applicationDirPath();
  const QStringList candidates = {
      QDir(exe_dir).filePath("assets"),
      QDir(exe_dir).filePath("../share/chessboard/assets"),
  };

  for (const QString& candidate : candidates) {
    if (QFileInfo(candidate).isDir()) return QDir(candidate).canonicalPath();
  }

  static bool warned = false;
  if (!warned) {
    warned = true;
    qWarning("ChessBoard: assets folder not found; looked in: %s",
             qPrintable(candidates.join(", ")));
  }

  return candidates.first();
}
}  // namespace chess
