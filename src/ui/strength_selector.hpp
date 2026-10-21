/*
 * Copyright (c) 2026 Kornfeld Eliyahu Peter
 * License: MIT
 */

#ifndef CHESSBOARD_UI_STRENGTH_SELECTOR_HPP_
#define CHESSBOARD_UI_STRENGTH_SELECTOR_HPP_

#include "config/app_config.hpp"

class QButtonGroup;
class QWidget;

namespace chess {
class UciEngine;

// Names of the nine presets, weakest first; index matches
// AppConfig::StrengthPresetElos().
extern const char* const kStrengthPresetNames[AppConfig::kStrengthPresetCount];

// QButtonGroup reserves -1 for "no button", so "Full strength" takes the id
// after the presets.
constexpr int kFullStrengthButtonId = AppConfig::kStrengthPresetCount;

// A 3-column grid of the nine preset buttons plus a full-width "Full strength"
// button, in one exclusive QButtonGroup (parented to `parent`).
struct StrengthButtonGrid {
  QWidget* grid_widget;
  QButtonGroup* group;
};
StrengthButtonGrid BuildStrengthButtonGrid(QWidget* parent);

// Sends UCI_LimitStrength/UCI_Elo for strength_id without touching the saved
// UciOptionOverrides, so a one-game choice doesn't change what Settings shows.
void ApplyStrengthToEngine(UciEngine& engine, int strength_id);
}  // namespace chess

#endif  // CHESSBOARD_UI_STRENGTH_SELECTOR_HPP_
