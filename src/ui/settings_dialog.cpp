/*
 * Copyright (c) 2026 Kornfeld Eliyahu Peter
 * License: MIT
 */

#include "ui/settings_dialog.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <limits>
#include <map>
#include <string>
#include <vector>

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMetaObject>
#include <QPointer>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStyle>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include "config/app_config.hpp"
#include "uci/uci_engine.hpp"
#include "ui/strength_selector.hpp"

namespace chess {
namespace {
struct ThemeOption {
  const char* label;
  BoardTheme theme;
};

constexpr ThemeOption kThemeOptions[] = {
    {"Wood", BoardTheme::kWood},
    {"Metal", BoardTheme::kMetal},
    {"Marble", BoardTheme::kMarble},
    {"Glass", BoardTheme::kGlass},
};

struct ColorSchemeOption {
  const char* label;
  BoardColorScheme scheme;
};

constexpr ColorSchemeOption kColorSchemeOptions[] = {
    {"Brown", BoardColorScheme::kBrown}, {"Grey", BoardColorScheme::kGrey},
    {"Green", BoardColorScheme::kGreen}, {"Blue", BoardColorScheme::kBlue},
    {"Red", BoardColorScheme::kRed},
};

struct PieceSetOption {
  const char* label;
  PieceSet set;
};

constexpr PieceSetOption kPieceSetOptions[] = {
    {"STIX Two Math", PieceSet::kStixTwoMath},
    {"FreeSerif", PieceSet::kFreeSerif},
    {"Nishiki-teki", PieceSet::kNishikiTeki},
    {"Noto Sans Symbols 2", PieceSet::kNotoSansSymbols2},
    {"DejaVu Sans", PieceSet::kDejaVuSans},
};

// Only options named in these groups are shown; other engine knobs (NumaPolicy,
// Move Overhead, Syzygy*, ...) are deliberately hidden.
struct OptionGroup {
  const char* title;
  std::vector<const char*> option_names;
};

// "Playing strength" is built by MakeStrengthGroupBox, not this mechanism.
const std::vector<OptionGroup> kEngineOptionGroups = {
    {"Search", {"Threads", "Hash", "Clear Hash"}},
    {"Analysis", {"Ponder", "UCI_ShowWDL", "MultiPV", "UCI_Chess960"}},
};

// Display labels for terse UCI names; the raw name is still what's sent and
// stored. Unlisted names show as-is.
const std::map<QString, const char*> kEngineOptionLabels = {
    {"Threads", "Threads"},
    {"Hash", "Hash size (MB)"},
    {"Clear Hash", "Clear hash table"},
    {"Ponder", "Enable pondering"},
    {"UCI_ShowWDL", "Show win/draw/loss stats"},
    {"MultiPV", "Number of lines to show"},
    {"UCI_Chess960", "Enable Chess960 (Fischer Random)"},
    {"UCI_LimitStrength", "Limit playing strength"},
    {"UCI_Elo", "Target Elo rating"},
    {"Skill Level", "Skill level"},
};

QString EngineOptionLabel(const QString& uci_name) {
  auto it = kEngineOptionLabels.find(uci_name);
  return it != kEngineOptionLabels.end() ? it->second : uci_name;
}

// Fills `combo` and selects the entry whose value matches `current` (the first
// if none do).
template <typename Option, size_t N, typename Value, typename LabelOf,
          typename ValueOf>
void PopulateComboFromOptions(QComboBox* combo, const Option (&options)[N],
                              const Value& current, LabelOf label_of,
                              ValueOf value_of) {
  for (const auto& option : options) combo->addItem(label_of(option));

  for (size_t i = 0; i < N; ++i) {
    if (value_of(options[i]) == current) {
      combo->setCurrentIndex(static_cast<int>(i));
      break;
    }
  }
}
}  // namespace

SettingsDialog::SettingsDialog(UciEngine& uci_engine, QWidget* parent)
    : QDialog(parent),
      uci_engine_(uci_engine),
      last_applied_engine_path_(
          AppConfig::Instance().EnginePath().toStdString()) {
  setWindowTitle("Settings");
  setModal(true);

  auto* tabs = new QTabWidget(this);
  tabs->addTab(MakeBoardTab(), "Board");
  tabs->addTab(MakeEngineTab(), "Engine");

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);

  auto* layout = new QVBoxLayout(this);
  layout->addWidget(tabs);
  layout->addWidget(buttons);
}

QWidget* SettingsDialog::MakeBoardTab() {
  AppConfig& config = AppConfig::Instance();

  auto* tab = new QWidget(this);

  theme_combo_ = new QComboBox(tab);
  PopulateComboFromOptions(
      theme_combo_, kThemeOptions, config.Theme(),
      [](const ThemeOption& o) { return o.label; },
      [](const ThemeOption& o) { return o.theme; });
  connect(theme_combo_, &QComboBox::currentIndexChanged, this,
          &SettingsDialog::OnThemeChanged);

  color_scheme_combo_ = new QComboBox(tab);
  PopulateComboFromOptions(
      color_scheme_combo_, kColorSchemeOptions, config.ColorScheme(),
      [](const ColorSchemeOption& o) { return o.label; },
      [](const ColorSchemeOption& o) { return o.scheme; });
  connect(color_scheme_combo_, &QComboBox::currentIndexChanged, this,
          &SettingsDialog::OnColorSchemeChanged);

  piece_set_combo_ = new QComboBox(tab);
  PopulateComboFromOptions(
      piece_set_combo_, kPieceSetOptions, config.Set(),
      [](const PieceSetOption& o) { return o.label; },
      [](const PieceSetOption& o) { return o.set; });
  connect(piece_set_combo_, &QComboBox::currentIndexChanged, this,
          &SettingsDialog::OnPieceSetChanged);

  auto* form = new QFormLayout(tab);
  form->addRow("Board theme", theme_combo_);
  form->addRow("Board color", color_scheme_combo_);
  form->addRow("Piece set", piece_set_combo_);

  return tab;
}

// Adds a form row for `option`, writing through to AppConfig (persist) and the
// live engine (immediate effect), and appends a resetter restoring the engine's
// default.
void SettingsDialog::AddOptionRow(QFormLayout* form, QWidget* parent,
                                  const UciOption& option,
                                  const QString& current) {
  QString name = QString::fromStdString(option.name);
  QString label = EngineOptionLabel(name);
  QString default_value = QString::fromStdString(option.default_value);

  QWidget* value_widget = nullptr;
  std::function<void()> reset_to_default;

  switch (option.type) {
    case UciOptionType::kCheck: {
      auto* check = new QCheckBox(parent);
      check->setChecked(current == "true");
      connect(check, &QCheckBox::toggled, this, [this, name](bool checked) {
        QString value = checked ? "true" : "false";
        AppConfig::Instance().SetUciOptionOverride(name, value);
        uci_engine_.SetOption(name.toStdString(), value.toStdString());
      });
      value_widget = check;
      reset_to_default = [check, default_value]() {
        check->setChecked(default_value == "true");
      };
      break;
    }

    case UciOptionType::kSpin: {
      auto* spin = new QSpinBox(parent);
      spin->setMinimum(option.min ? static_cast<int>(*option.min)
                                  : std::numeric_limits<int>::min());
      spin->setMaximum(option.max ? static_cast<int>(*option.max)
                                  : std::numeric_limits<int>::max());
      spin->setValue(current.toInt());
      connect(spin, &QSpinBox::valueChanged, this, [this, name](int value) {
        QString text = QString::number(value);
        AppConfig::Instance().SetUciOptionOverride(name, text);
        uci_engine_.SetOption(name.toStdString(), text.toStdString());
      });
      value_widget = spin;
      reset_to_default = [spin, default_value]() {
        spin->setValue(default_value.toInt());
      };
      break;
    }

    case UciOptionType::kCombo: {
      auto* combo = new QComboBox(parent);
      for (const std::string& value : option.combo_values)
        combo->addItem(QString::fromStdString(value));
      combo->setCurrentText(current);
      connect(combo, &QComboBox::currentTextChanged, this,
              [this, name](const QString& value) {
                AppConfig::Instance().SetUciOptionOverride(name, value);
                uci_engine_.SetOption(name.toStdString(), value.toStdString());
              });
      value_widget = combo;
      reset_to_default = [combo, default_value]() {
        combo->setCurrentText(default_value);
      };
      break;
    }

    case UciOptionType::kButton: {
      auto* button = new QPushButton(label, parent);
      connect(button, &QPushButton::clicked, this, [this, name]() {
        uci_engine_.SetOption(name.toStdString(), "");
      });
      form->addRow(button);
      return;
    }

    case UciOptionType::kString: {
      auto* edit = new QLineEdit(parent);
      edit->setText(current);
      connect(edit, &QLineEdit::editingFinished, this, [this, name, edit]() {
        QString value = edit->text();
        AppConfig::Instance().SetUciOptionOverride(name, value);
        uci_engine_.SetOption(name.toStdString(), value.toStdString());
      });
      value_widget = edit;
      reset_to_default = [edit, default_value]() {
        edit->setText(default_value);
        emit edit->editingFinished();
      };
      break;
    }
  }

  std::function<void()> reset_and_clear_override = [this, name,
                                                    reset_to_default]() {
    AppConfig::Instance().RemoveUciOptionOverride(name);
    reset_to_default();
  };

  auto* reset_button = new QToolButton(parent);
  reset_button->setIcon(
      parent->style()->standardIcon(QStyle::SP_DialogResetButton));
  reset_button->setToolTip("Reset to default");
  reset_button->setAutoRaise(true);
  connect(reset_button, &QToolButton::clicked, this, reset_and_clear_override);

  auto* row = new QWidget(parent);
  auto* row_layout = new QHBoxLayout(row);
  row_layout->setContentsMargins(0, 0, 0, 0);
  row_layout->addWidget(value_widget);
  row_layout->addWidget(reset_button);

  form->addRow(label, row);

  engine_option_resetters_.push_back(reset_and_clear_override);
}

// Builds the "Playing strength" group: nine named presets with editable Elo
// plus "Full strength", replacing the raw UCI_LimitStrength/UCI_Elo/Skill Level
// widgets. Returns nullptr if the engine lacks UCI_LimitStrength or UCI_Elo.
QWidget* SettingsDialog::MakeStrengthGroupBox(
    QWidget* parent, const std::vector<UciOption>& available) {
  auto find_option = [&](const char* name) {
    return std::find_if(available.begin(), available.end(),
                        [&](const UciOption& o) { return o.name == name; });
  };

  auto limit_it = find_option("UCI_LimitStrength");
  auto elo_it = find_option("UCI_Elo");
  if (limit_it == available.end() || elo_it == available.end()) return nullptr;

  int elo_min = elo_it->min ? static_cast<int>(*elo_it->min) : 1;
  int elo_max = elo_it->max ? static_cast<int>(*elo_it->max) : 4000;

  AppConfig& config = AppConfig::Instance();

  // Only the presets' Elo values are edited here. The checked button isn't
  // persisted (New Game owns that), so the starting selection comes from the
  // engine's defaults.
  bool limit_enabled = limit_it->default_value == "true";
  int current_elo = QString::fromStdString(elo_it->default_value).toInt();

  std::array<int, AppConfig::kStrengthPresetCount> preset_elos =
      config.StrengthPresetElos();

  auto* box = new QGroupBox("Playing strength", parent);
  auto* box_layout = new QVBoxLayout(box);

  StrengthButtonGrid strength_grid = BuildStrengthButtonGrid(box);
  QButtonGroup* group = strength_grid.group;
  box_layout->addWidget(strength_grid.grid_widget);

  auto* elo_row = new QWidget(box);
  auto* elo_layout = new QHBoxLayout(elo_row);
  elo_layout->setContentsMargins(0, 0, 0, 0);

  auto* elo_label = new QLabel("Target Elo", elo_row);
  auto* elo_spin = new QSpinBox(elo_row);
  elo_spin->setRange(elo_min, elo_max);

  elo_layout->addWidget(elo_label);
  elo_layout->addWidget(elo_spin);
  box_layout->addWidget(elo_row);

  // Picks the preset matching current_elo, else the closest (e.g. an Elo edited
  // outside this control).
  int default_preset_index = 0;
  int best_diff = std::numeric_limits<int>::max();
  for (int i = 0; i < AppConfig::kStrengthPresetCount; ++i) {
    int diff = std::abs(preset_elos[i] - current_elo);
    if (diff < best_diff) {
      best_diff = diff;
      default_preset_index = i;
    }
  }

  auto update_elo_spin_enabled = [elo_spin, elo_label](bool enabled) {
    elo_spin->setEnabled(enabled);
    elo_label->setEnabled(enabled);
  };

  if (limit_enabled) {
    group->button(default_preset_index)->setChecked(true);
    elo_spin->setValue(preset_elos[default_preset_index]);
    update_elo_spin_enabled(true);
  } else {
    group->button(kFullStrengthButtonId)->setChecked(true);
    elo_spin->setValue(current_elo);
    update_elo_spin_enabled(false);
  }

  // Clicking a preset only previews it on the live engine; editing the Elo
  // stores that preset's value. The checked button isn't persisted.
  connect(group, &QButtonGroup::idClicked, this,
          [this, elo_spin, preset_elos, update_elo_spin_enabled](int id) {
            if (id == kFullStrengthButtonId) {
              uci_engine_.SetOption("UCI_LimitStrength", "false");
              update_elo_spin_enabled(false);
              return;
            }

            int elo = preset_elos[id];
            uci_engine_.SetOption("UCI_LimitStrength", "true");
            uci_engine_.SetOption("UCI_Elo", std::to_string(elo));
            update_elo_spin_enabled(true);

            // Blocked to avoid a redundant SetOption round trip.
            QSignalBlocker blocker(elo_spin);
            elo_spin->setValue(elo);
          });

  connect(elo_spin, &QSpinBox::valueChanged, this, [this, group](int value) {
    int checked_id = group->checkedId();
    if (checked_id == kFullStrengthButtonId || checked_id < 0) return;

    AppConfig::Instance().SetStrengthPresetElo(checked_id, value);
    uci_engine_.SetOption("UCI_Elo", std::to_string(value));
  });

  engine_option_resetters_.push_back(
      [this, group, elo_spin, update_elo_spin_enabled]() {
        uci_engine_.SetOption("UCI_LimitStrength", "false");

        QSignalBlocker button_blocker(group);
        for (QAbstractButton* button : group->buttons())
          button->setChecked(false);
        group->button(kFullStrengthButtonId)->setChecked(true);
        update_elo_spin_enabled(false);
      });

  return box;
}

QWidget* SettingsDialog::MakeEngineTab() {
  AppConfig& config = AppConfig::Instance();

  auto* tab = new QWidget(this);

  engine_path_edit_ = new QLineEdit(tab);
  engine_path_edit_->setText(config.EnginePath());
  connect(engine_path_edit_, &QLineEdit::editingFinished, this,
          &SettingsDialog::OnEnginePathEdited);

  engine_status_label_ = new QLabel(tab);
  engine_status_label_->setWordWrap(true);

  auto* path_form = new QFormLayout;
  path_form->addRow("Engine path", engine_path_edit_);
  path_form->addRow(QString(), engine_status_label_);

  engine_tab_layout_ = new QVBoxLayout(tab);
  engine_tab_layout_->addLayout(path_form);
  engine_tab_layout_->addStretch();

  engine_options_container_ = nullptr;
  RebuildEngineOptions();

  return tab;
}

void SettingsDialog::RebuildEngineOptions() {
  int generation = ++engine_options_generation_;
  ClearEngineOptions();

  engine_status_label_->setText("Connecting to engine...");
  engine_status_label_->setVisible(true);

  // The engine starts lazily on this request. The reply arrives on its worker
  // thread and is marshalled back, guarded against the dialog having closed.
  QPointer<SettingsDialog> self(this);
  uci_engine_.RequestOptions(
      [self, generation](std::vector<UciOption> available) {
        QMetaObject::invokeMethod(
            qApp,
            [self, generation, available]() {
              if (!self || generation != self->engine_options_generation_)
                return;
              self->PopulateEngineOptions(available);
            },
            Qt::QueuedConnection);
      });
}

void SettingsDialog::ClearEngineOptions() {
  engine_option_resetters_.clear();
  if (!engine_options_container_) return;

  // May run from editingFinished while Qt moves focus, possibly into this very
  // container; deleting it synchronously would destroy the widget about to be
  // focused. Park focus on the path field and dispose of the container on the
  // next event-loop pass.
  engine_path_edit_->setFocus(Qt::OtherFocusReason);
  engine_options_container_->hide();
  engine_tab_layout_->removeWidget(engine_options_container_);
  engine_options_container_->setParent(nullptr);
  engine_options_container_->deleteLater();
  engine_options_container_ = nullptr;
}

void SettingsDialog::PopulateEngineOptions(
    const std::vector<UciOption>& available) {
  QWidget* tab = engine_path_edit_->parentWidget();
  engine_options_container_ = new QWidget(tab);
  auto* layout = new QVBoxLayout(engine_options_container_);
  layout->setContentsMargins(0, 0, 0, 0);

  QMap<QString, QString> overrides = AppConfig::Instance().UciOptionOverrides();

  for (const OptionGroup& group : kEngineOptionGroups) {
    auto* box = new QGroupBox(group.title, engine_options_container_);
    auto* form = new QFormLayout(box);
    bool any_rows = false;

    for (const char* option_name : group.option_names) {
      auto it = std::find_if(
          available.begin(), available.end(),
          [&](const UciOption& option) { return option.name == option_name; });
      if (it == available.end()) continue;

      QString name = QString::fromStdString(it->name);
      QString current =
          overrides.value(name, QString::fromStdString(it->default_value));
      if (it->type == UciOptionType::kSpin && name == "MultiPV") {
        current = QString::number(ValidSpinValue(*it, current.toStdString()));
      }
      AddOptionRow(form, box, *it, current);
      any_rows = true;
    }

    if (any_rows)
      layout->addWidget(box);
    else
      delete box;
  }

  if (QWidget* strength_box =
          MakeStrengthGroupBox(engine_options_container_, available))
    layout->addWidget(strength_box);

  if (!engine_option_resetters_.empty()) {
    auto* reset_all_button =
        new QPushButton("Reset all to defaults", engine_options_container_);
    connect(reset_all_button, &QPushButton::clicked, this, [this]() {
      for (const auto& reset : engine_option_resetters_) reset();
    });

    auto* reset_all_row = new QHBoxLayout;
    reset_all_row->addStretch();
    reset_all_row->addWidget(reset_all_button);
    layout->addLayout(reset_all_row);
  }

  // Index 0 is the path form; the trailing stretch stays last.
  engine_tab_layout_->insertWidget(1, engine_options_container_);

  RefreshEngineStatus();
  adjustSize();
}

void SettingsDialog::OnThemeChanged(int index) {
  AppConfig::Instance().SetTheme(kThemeOptions[index].theme);
  emit boardAppearanceChanged();
}

void SettingsDialog::OnColorSchemeChanged(int index) {
  AppConfig::Instance().SetColorScheme(kColorSchemeOptions[index].scheme);
  emit boardAppearanceChanged();
}

void SettingsDialog::OnPieceSetChanged(int index) {
  AppConfig::Instance().SetSet(kPieceSetOptions[index].set);
  emit pieceSetChanged();
}

// Saves the path, applies it to the live engine, and rebuilds the option rows.
void SettingsDialog::OnEnginePathEdited() {
  QString path = engine_path_edit_->text().trimmed();
  // editingFinished also fires on focus loss.
  if (path.toStdString() == last_applied_engine_path_) return;
  last_applied_engine_path_ = path.toStdString();

  AppConfig::Instance().SetEnginePath(path);
  uci_engine_.SetCommand(path.toStdString());
  RebuildEngineOptions();
}

void SettingsDialog::RefreshEngineStatus() {
  std::optional<std::string> error = uci_engine_.StartupError();
  engine_status_label_->setVisible(error.has_value());
  engine_status_label_->setText(error ? QString::fromStdString(*error)
                                      : QString());
}
}  // namespace chess
