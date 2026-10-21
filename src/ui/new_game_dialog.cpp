/*
 * Copyright (c) 2026 Kornfeld Eliyahu Peter
 * License: MIT
 */

#include "ui/new_game_dialog.hpp"

#include <algorithm>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include <QAbstractButton>
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMetaObject>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "config/app_config.hpp"
#include "engine/chess_engine.hpp"
#include "models/piece.hpp"
#include "uci/uci_engine.hpp"
#include "ui/glyph_rendering.hpp"
#include "ui/strength_selector.hpp"

namespace chess {
namespace {
// Renders a glyph centered in a square pixmap, the way BoardWidget centers
// pieces.
QPixmap RenderGlyphPixmap(const QString& glyph, const QFont& font, int size,
                          const QColor& fill,
                          const QColor& outline = Qt::transparent) {
  QPixmap pixmap(size, size);
  pixmap.fill(Qt::transparent);

  QPainterPath path = CenteredGlyphPath(font, glyph, QRectF(0, 0, size, size));

  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing, true);
  // A white glyph needs an outline against a light button.
  painter.setPen(outline == Qt::transparent ? Qt::NoPen
                                            : QPen(outline, size * 0.03));
  painter.setBrush(fill);
  painter.drawPath(path);

  return pixmap;
}

// The King glyph drawn twice, clipped to the left half in black and the right
// half in white, so "Random" needs no icon asset.
QPixmap RenderSplitKingPixmap(const QFont& font, int size) {
  QString glyph = Piece(PieceType::kKing, PieceColor::kBlack).Glyph();
  QPixmap black_half = RenderGlyphPixmap(glyph, font, size, Qt::black);
  QPixmap white_half =
      RenderGlyphPixmap(glyph, font, size, Qt::white, Qt::black);

  QPixmap combined(size, size);
  combined.fill(Qt::transparent);

  QPainter painter(&combined);
  painter.setClipRect(0, 0, size / 2, size);
  painter.drawPixmap(0, 0, black_half);
  painter.setClipRect(size / 2, 0, size - size / 2, size);
  painter.drawPixmap(0, 0, white_half);

  return combined;
}

QPushButton* MakeSideSegment(const QString& label, const QPixmap& glyph_pixmap,
                             QWidget* parent) {
  auto* button = new QPushButton(parent);
  button->setCheckable(true);
  button->setMinimumHeight(88);

  auto* icon_label = new QLabel(button);
  icon_label->setPixmap(glyph_pixmap);
  icon_label->setAlignment(Qt::AlignCenter);
  icon_label->setAttribute(Qt::WA_TransparentForMouseEvents);

  auto* text_label = new QLabel(label, button);
  text_label->setAlignment(Qt::AlignCenter);
  text_label->setAttribute(Qt::WA_TransparentForMouseEvents);

  auto* layout = new QVBoxLayout(button);
  layout->setContentsMargins(4, 2, 4, 2);
  layout->setSpacing(0);
  layout->addWidget(icon_label);
  layout->addWidget(text_label);

  return button;
}

QPushButton* MakeTextSegment(const QString& label, QWidget* parent) {
  auto* button = new QPushButton(label, parent);
  button->setCheckable(true);
  button->setMinimumHeight(36);

  return button;
}

// Builds an exclusive group of checkable segments in an evenly spaced row added
// to `layout`; returns the group.
QButtonGroup* AddSegmentedRow(
    QVBoxLayout* layout, QWidget* parent,
    const std::vector<std::pair<int, QString>>& segments) {
  auto* row = new QWidget(parent);

  auto* row_layout = new QHBoxLayout(row);
  row_layout->setContentsMargins(0, 0, 0, 0);

  auto* group = new QButtonGroup(parent);
  group->setExclusive(true);

  for (const auto& [id, label] : segments) {
    QPushButton* button = MakeTextSegment(label, row);
    group->addButton(button, id);
    row_layout->addWidget(button);
  }

  layout->addWidget(row);
  return group;
}

bool OptionDefaultsToTrue(const std::vector<UciOption>& available,
                          const char* name) {
  auto it = std::find_if(available.begin(), available.end(),
                         [&](const UciOption& o) { return o.name == name; });
  return it != available.end() && it->default_value == "true";
}
}  // namespace

NewGameDialog::NewGameDialog(UciEngine& uci_engine, QWidget* parent)
    : QDialog(parent) {
  setWindowTitle("New game");
  setModal(true);

  constexpr int kGlyphPixelSize = 56;

  // Always FreeSerif: some piece sets have outlines too intricate to fill
  // cleanly at this size.
  QFont glyph_font = LoadPieceFont(PieceSet::kFreeSerif);
  glyph_font.setPixelSize(kGlyphPixelSize);

  auto* side_row = new QWidget(this);

  auto* side_layout = new QHBoxLayout(side_row);
  side_layout->setContentsMargins(0, 0, 0, 0);

  side_group_ = new QButtonGroup(this);
  side_group_->setExclusive(true);

  QString king_glyph = Piece(PieceType::kKing, PieceColor::kBlack).Glyph();
  QPixmap black_king =
      RenderGlyphPixmap(king_glyph, glyph_font, kGlyphPixelSize, Qt::black);
  QPixmap white_king = RenderGlyphPixmap(king_glyph, glyph_font,
                                         kGlyphPixelSize, Qt::white, Qt::black);
  QPixmap split_king = RenderSplitKingPixmap(glyph_font, kGlyphPixelSize);

  QPushButton* black_button = MakeSideSegment("Black", black_king, side_row);
  QPushButton* random_button = MakeSideSegment("Random", split_king, side_row);
  QPushButton* white_button = MakeSideSegment("White", white_king, side_row);

  // Button ids 0/1/2 index kSideForButtonId in the accept handler.
  side_group_->addButton(black_button, 0);
  side_group_->addButton(random_button, 1);
  side_group_->addButton(white_button, 2);

  side_layout->addWidget(black_button);
  side_layout->addWidget(random_button);
  side_layout->addWidget(white_button);

  switch (AppConfig::Instance().LastNewGameSide()) {
    case NewGameSide::kBlack:
      black_button->setChecked(true);
      break;
    case NewGameSide::kWhite:
      white_button->setChecked(true);
      break;
    case NewGameSide::kRandom:
    default:
      random_button->setChecked(true);
      break;
  }

  auto* side_label = new QLabel("Side", this);
  side_label->setAlignment(Qt::AlignCenter);

  auto* time_label = new QLabel("Time control", this);
  time_label->setAlignment(Qt::AlignCenter);

  // Presets come grouped by category (TimeControl::Presets()); each category is
  // a page of preset_pages_. "Custom" is a fifth tab of spin boxes and isn't a
  // TimeControlCategory.
  static constexpr TimeControlCategory kCategories[] = {
      TimeControlCategory::kBullet,
      TimeControlCategory::kBlitz,
      TimeControlCategory::kRapid,
      TimeControlCategory::kClassical,
  };
  // QButtonGroup reserves -1 for "no button", so Custom takes the value one
  // past the last category.
  constexpr int kCustomCategoryId =
      static_cast<int>(TimeControlCategory::kClassical) + 1;
  const int kCustomPageIndex = static_cast<int>(std::size(kCategories));

  auto* category_layout = new QVBoxLayout;
  std::vector<std::pair<int, QString>> category_segments;
  for (TimeControlCategory category : kCategories) {
    category_segments.emplace_back(static_cast<int>(category),
                                   TimeControl::CategoryName(category));
  }
  category_segments.emplace_back(kCustomCategoryId, "Custom");
  category_group_ = AddSegmentedRow(category_layout, this, category_segments);

  preset_pages_ = new QStackedWidget(this);

  // custom_minutes_/custom_increment_ hold the selected time control (see
  // SelectedTimeControl()): every preset click writes into them, so the Custom
  // page always shows the last preset picked.
  custom_minutes_ = new QSpinBox(this);
  custom_minutes_->setRange(1, 180);
  custom_minutes_->setValue(AppConfig::Instance().LastTimeControlMinutes());
  custom_minutes_->setSuffix(" min");

  custom_increment_ = new QSpinBox(this);
  custom_increment_->setRange(0, 60);
  custom_increment_->setValue(
      AppConfig::Instance().LastTimeControlIncrementSeconds());
  custom_increment_->setSuffix(" sec");

  int saved_minutes = custom_minutes_->value();
  int saved_increment = custom_increment_->value();

  const auto& presets = TimeControl::Presets();
  int default_category_index = -1;
  for (int category_index = 0;
       category_index < static_cast<int>(std::size(kCategories));
       ++category_index) {
    TimeControlCategory category = kCategories[category_index];

    auto* page_layout = new QVBoxLayout;
    std::vector<std::pair<int, QString>> preset_segments;
    int default_preset_id = -1;
    for (int i = 0; i < static_cast<int>(presets.size()); ++i) {
      if (presets[i].Category() != category) continue;
      preset_segments.emplace_back(i,
                                   QString::fromStdString(presets[i].Label()));
      // The saved time control restores a preset if one matches exactly;
      // otherwise it falls through to Custom.
      if (presets[i].minutes == saved_minutes &&
          presets[i].increment_seconds == saved_increment) {
        default_category_index = category_index;
        default_preset_id = i;
      }
    }

    auto* page = new QWidget(preset_pages_);
    auto* page_outer_layout = new QVBoxLayout(page);
    page_outer_layout->setContentsMargins(0, 0, 0, 0);
    page_outer_layout->addLayout(page_layout);

    QButtonGroup* preset_group =
        AddSegmentedRow(page_layout, page, preset_segments);
    connect(preset_group, &QButtonGroup::idClicked, this,
            [this](int preset_id) {
              const TimeControl& tc = TimeControl::Presets()[preset_id];
              custom_minutes_->setValue(tc.minutes);
              custom_increment_->setValue(tc.increment_seconds);
            });
    // Every page needs a default preset, since switching category shows its
    // checked preset without a click.
    preset_group
        ->button(default_preset_id >= 0 ? default_preset_id
                                        : preset_segments.front().first)
        ->setChecked(true);

    preset_pages_->addWidget(page);
  }

  auto* custom_page = new QWidget(preset_pages_);
  auto* custom_form = new QFormLayout(custom_page);
  custom_form->addRow("Minutes", custom_minutes_);
  custom_form->addRow("Increment", custom_increment_);

  preset_pages_->addWidget(custom_page);

  // No preset matched the saved time control, so default to Custom with those
  // values.
  if (default_category_index < 0) {
    category_group_->button(kCustomCategoryId)->setChecked(true);
    preset_pages_->setCurrentIndex(kCustomPageIndex);
  } else {
    category_group_
        ->button(static_cast<int>(kCategories[default_category_index]))
        ->setChecked(true);
    preset_pages_->setCurrentIndex(default_category_index);
  }

  connect(category_group_, &QButtonGroup::idClicked, this,
          [this, kCustomCategoryId, kCustomPageIndex](int category_id) {
            if (category_id == kCustomCategoryId) {
              preset_pages_->setCurrentIndex(kCustomPageIndex);
              return;
            }
            for (int i = 0; i < static_cast<int>(std::size(kCategories)); ++i) {
              if (static_cast<int>(kCategories[i]) == category_id) {
                preset_pages_->setCurrentIndex(i);
                break;
              }
            }
          });

  auto* strength_label = new QLabel("Strength", this);
  strength_label->setAlignment(Qt::AlignCenter);

  StrengthButtonGrid strength_grid = BuildStrengthButtonGrid(this);
  strength_group_ = strength_grid.group;

  int last_strength_id = AppConfig::Instance().LastNewGameStrengthId();
  QAbstractButton* last_strength_button =
      strength_group_->button(last_strength_id);
  (last_strength_button ? last_strength_button
                        : strength_group_->button(kFullStrengthButtonId))
      ->setChecked(true);

  // One-game overrides, seeded from Settings (or the engine default). Never
  // written to AppConfig; accepting applies them to the live engine for this
  // game only.
  auto* engine_options_row = new QWidget(this);
  auto* engine_options_layout = new QHBoxLayout(engine_options_row);
  engine_options_layout->setContentsMargins(0, 0, 0, 0);

  ponder_check_ = new QCheckBox("Ponder", engine_options_row);
  chess960_check_ = new QCheckBox("Chess960", engine_options_row);

  // Seed each checkbox from a saved override, else from the engine's advertised
  // default. That arrives asynchronously and is applied only if the dialog is
  // still open and a FEN hasn't locked the Chess960 box.
  QMap<QString, QString> overrides = AppConfig::Instance().UciOptionOverrides();
  auto seed_from_override = [&](QCheckBox* box, const char* name) {
    if (!overrides.contains(name)) return false;
    box->setChecked(overrides.value(name) == "true");
    return true;
  };
  bool ponder_seeded = seed_from_override(ponder_check_, "Ponder");
  bool chess960_seeded = seed_from_override(chess960_check_, "UCI_Chess960");

  if (!ponder_seeded || !chess960_seeded) {
    QPointer<NewGameDialog> self(this);
    uci_engine.RequestOptions([self, ponder_seeded, chess960_seeded](
                                  std::vector<UciOption> available) {
      QMetaObject::invokeMethod(
          qApp,
          [self, ponder_seeded, chess960_seeded, available]() {
            if (!self) return;
            if (!ponder_seeded)
              self->ponder_check_->setChecked(
                  OptionDefaultsToTrue(available, "Ponder"));
            if (!chess960_seeded && self->chess960_check_->isEnabled()) {
              self->chess960_check_->setChecked(
                  OptionDefaultsToTrue(available, "UCI_Chess960"));
            }
          },
          Qt::QueuedConnection);
    });
  }

  engine_options_layout->addWidget(ponder_check_);
  engine_options_layout->addWidget(chess960_check_);

  // Optional starting position, validated live on a scratch engine so an
  // invalid FEN can't affect the game. Blank means the normal start.
  auto* starting_fen_label =
      new QLabel("Starting position (FEN, optional)", this);

  starting_fen_edit_ = new QLineEdit(this);
  starting_fen_edit_->setPlaceholderText(
      "Leave blank for the normal starting position");

  starting_fen_status_ = new QLabel(this);
  starting_fen_status_->setWordWrap(true);

  connect(starting_fen_edit_, &QLineEdit::textChanged, this,
          [this, chess960_check = chess960_check_](const QString& text) {
            if (text.trimmed().isEmpty()) {
              starting_fen_status_->clear();
              chess960_check->setEnabled(true);
              return;
            }

            ChessEngine scratch;
            if (!scratch.LoadFen(text.trimmed().toStdString())) {
              starting_fen_status_->setText("Not a valid FEN.");
              chess960_check->setEnabled(true);
              return;
            }

            starting_fen_status_->setText(scratch.IsChess960()
                                              ? "Valid Chess960 position."
                                              : "Valid position.");
            // The FEN decides Chess960-ness; the checkbox only mirrors it.
            QSignalBlocker blocker(chess960_check);
            chess960_check->setChecked(scratch.IsChess960());
            chess960_check->setEnabled(false);
          });

  auto* buttons = new QDialogButtonBox(
      QDialogButtonBox::Cancel | QDialogButtonBox::Ok, this);
  buttons->button(QDialogButtonBox::Ok)->setText("Start");
  connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
    // A non-empty FEN must be valid to accept; starting_fen_status_ already
    // reflects that.
    QString fen_text = starting_fen_edit_->text().trimmed();
    if (!fen_text.isEmpty()) {
      ChessEngine scratch;
      if (!scratch.LoadFen(fen_text.toStdString())) return;
    }

    static constexpr NewGameSide kSideForButtonId[] = {
        NewGameSide::kBlack,
        NewGameSide::kRandom,
        NewGameSide::kWhite,
    };
    AppConfig::Instance().SetLastNewGameSide(
        kSideForButtonId[side_group_->checkedId()]);

    TimeControl tc = SelectedTimeControl();
    AppConfig::Instance().SetLastTimeControl(tc.minutes, tc.increment_seconds);

    AppConfig::Instance().SetLastNewGameStrengthId(
        strength_group_->checkedId());

    accept();
  });
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

  auto* layout = new QVBoxLayout(this);
  layout->addWidget(side_label);
  layout->addWidget(side_row);
  layout->addWidget(time_label);
  layout->addLayout(category_layout);
  layout->addWidget(preset_pages_);
  layout->addWidget(strength_label);
  layout->addWidget(strength_grid.grid_widget);
  layout->addWidget(engine_options_row);
  layout->addWidget(starting_fen_label);
  layout->addWidget(starting_fen_edit_);
  layout->addWidget(starting_fen_status_);
  layout->addWidget(buttons);
}

PieceColor NewGameDialog::SelectedPlayerColor() const {
  int id = side_group_->checkedId();
  if (id == 0) return PieceColor::kBlack;
  if (id == 2) return PieceColor::kWhite;

  // static: seed the generator once.
  static std::mt19937 rng(std::random_device{}());
  std::uniform_int_distribution<int> dist(0, 1);
  return dist(rng) == 0 ? PieceColor::kWhite : PieceColor::kBlack;
}

TimeControl NewGameDialog::SelectedTimeControl() const {
  return TimeControl(custom_minutes_->value(), custom_increment_->value());
}

int NewGameDialog::SelectedStrengthId() const {
  return strength_group_->checkedId();
}

bool NewGameDialog::SelectedPonder() const {
  return ponder_check_->isChecked();
}
bool NewGameDialog::SelectedChess960() const {
  return chess960_check_->isChecked();
}

std::optional<std::string> NewGameDialog::SelectedStartingFen() const {
  QString text = starting_fen_edit_->text().trimmed();
  if (text.isEmpty()) return std::nullopt;
  return text.toStdString();
}
}  // namespace chess
