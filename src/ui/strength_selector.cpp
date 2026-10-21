/*
 * Copyright (c) 2026 Kornfeld Eliyahu Peter
 * License: MIT
 */

#include "ui/strength_selector.hpp"

#include <QButtonGroup>
#include <QGridLayout>
#include <QPushButton>
#include <QWidget>

#include "uci/uci_engine.hpp"

namespace chess {
const char* const kStrengthPresetNames[AppConfig::kStrengthPresetCount] = {
    "Novice",   "Beginner", "Casual", "Club",        "Intermediate",
    "Advanced", "Expert",   "Master", "Grandmaster",
};

namespace {
constexpr int kGridColumns = 3;
}  // namespace

StrengthButtonGrid BuildStrengthButtonGrid(QWidget* parent) {
  auto* grid_widget = new QWidget(parent);
  auto* grid_layout = new QGridLayout(grid_widget);
  grid_layout->setContentsMargins(0, 0, 0, 0);

  auto* group = new QButtonGroup(parent);
  group->setExclusive(true);

  for (int i = 0; i < AppConfig::kStrengthPresetCount; ++i) {
    auto* button = new QPushButton(kStrengthPresetNames[i], grid_widget);
    button->setCheckable(true);
    group->addButton(button, i);
    grid_layout->addWidget(button, i / kGridColumns, i % kGridColumns);
  }

  auto* full_strength_button = new QPushButton("Full strength", grid_widget);
  full_strength_button->setCheckable(true);
  group->addButton(full_strength_button, kFullStrengthButtonId);
  int full_strength_row =
      (AppConfig::kStrengthPresetCount + kGridColumns - 1) / kGridColumns;
  grid_layout->addWidget(full_strength_button, full_strength_row, 0, 1,
                         kGridColumns);

  return {grid_widget, group};
}

// Any id outside the presets (i.e. kFullStrengthButtonId) means full strength.
void ApplyStrengthToEngine(UciEngine& engine, int strength_id) {
  if (strength_id < 0 || strength_id >= AppConfig::kStrengthPresetCount) {
    engine.SetOption("UCI_LimitStrength", "false");
    return;
  }

  int elo = AppConfig::Instance().StrengthPresetElos()[strength_id];
  engine.SetOption("UCI_LimitStrength", "true");
  engine.SetOption("UCI_Elo", std::to_string(elo));
}
}  // namespace chess
