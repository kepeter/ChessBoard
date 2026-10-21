/*
 * Copyright (c) 2026 Kornfeld Eliyahu Peter
 * License: MIT
 */

#include "ui/sidebar_widget.hpp"

#include <utility>
#include <vector>

#include <QHBoxLayout>
#include <QHeaderView>
#include <QTableWidget>
#include <QVBoxLayout>

#include "ui/captured_pieces_widget.hpp"
#include "ui/clock_widget.hpp"

namespace chess {
SidebarWidget::SidebarWidget(ChessEngine& engine, GameSession& session,
                             QFont piece_font, QWidget* parent)
    : QWidget(parent),
      engine_(engine),
      session_(session),
      piece_font_(std::move(piece_font)) {
  setMinimumWidth(kMinWidth);

  top_clock_ = new ClockWidget(this);
  bottom_clock_ = new ClockWidget(this);
  top_captured_ = new CapturedPiecesWidget(piece_font_, this);
  bottom_captured_ = new CapturedPiecesWidget(piece_font_, this);

  move_table_ = new QTableWidget(0, 4, this);
  move_table_->setHorizontalHeaderLabels(
      {"#", "White", "Black", "Time (W - B)"});
  move_table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
  move_table_->horizontalHeader()->setSectionResizeMode(3,
                                                        QHeaderView::Stretch);
  move_table_->setColumnWidth(0, 32);
  move_table_->setColumnWidth(1, 64);
  move_table_->setColumnWidth(2, 64);
  move_table_->verticalHeader()->setVisible(false);
  move_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  move_table_->setSelectionMode(QAbstractItemView::NoSelection);
  move_table_->setShowGrid(true);

  auto* top_row = new QHBoxLayout();
  top_row->addWidget(top_clock_, 1);
  top_row->addWidget(top_captured_, 1);

  auto* bottom_row = new QHBoxLayout();
  bottom_row->addWidget(bottom_clock_, 1);
  bottom_row->addWidget(bottom_captured_, 1);

  auto* layout = new QVBoxLayout(this);
  layout->addLayout(top_row);
  layout->addSpacing(8);
  layout->addWidget(move_table_, 1);
  layout->addLayout(bottom_row);

  connect(&session_, &GameSession::stateChanged, this, &SidebarWidget::Refresh);
  connect(&session_, &GameSession::moveMade, this, &SidebarWidget::Refresh);

  Refresh();
}

void SidebarWidget::SetPieceFont(QFont piece_font) {
  piece_font_ = std::move(piece_font);
  top_captured_->SetPieceFont(piece_font_);
  bottom_captured_->SetPieceFont(piece_font_);
}

void SidebarWidget::Refresh() {
  // The human's pieces are at the bottom of the board, so the opponent's clock
  // is on top.
  PieceColor top = Opposite(session_.PlayerColor());
  PieceColor bottom = session_.PlayerColor();

  auto remaining = [&](PieceColor color) {
    return color == PieceColor::kWhite ? session_.WhiteRemaining()
                                       : session_.BlackRemaining();
  };

  top_clock_->SetState(session_.SideToMove() == top, session_.Flagged() == top,
                       remaining(top));
  bottom_clock_->SetState(session_.SideToMove() == bottom,
                          session_.Flagged() == bottom, remaining(bottom));

  // stateChanged fires on every clock tick; skip the O(history) table rebuild
  // unless a move was made.
  size_t history_size = engine_.History().size();
  if (history_size == last_refreshed_history_size_) return;
  last_refreshed_history_size_ = history_size;

  RefreshMoveHistory();
  RefreshCaptured();
}

void SidebarWidget::RefreshCaptured() {
  std::vector<PieceType> captured_by_white;
  std::vector<PieceType> captured_by_black;
  int white_points = 0;
  int black_points = 0;

  for (const Move& move : engine_.History()) {
    if (!move.piece_captured) continue;

    PieceType type = move.piece_captured->type;
    int value = PieceValue(type);

    if (move.piece_moved.color == PieceColor::kWhite) {
      captured_by_white.push_back(type);
      white_points += value;
    } else {
      captured_by_black.push_back(type);
      black_points += value;
    }
  }

  PieceColor top = Opposite(session_.PlayerColor());
  PieceColor bottom = session_.PlayerColor();

  auto captured_for = [&](PieceColor color) {
    return color == PieceColor::kWhite ? captured_by_white : captured_by_black;
  };
  auto lead_for = [&](PieceColor color) {
    int own = color == PieceColor::kWhite ? white_points : black_points;
    int other = color == PieceColor::kWhite ? black_points : white_points;
    return own - other;
  };

  // Captures are the opponent's pieces, drawn in the opponent's color.
  top_captured_->SetCaptured(captured_for(top), Opposite(top), lead_for(top));
  bottom_captured_->SetCaptured(captured_for(bottom), Opposite(bottom),
                                lead_for(bottom));
}

// History is a flat list of half-moves; each row takes two, except a last row
// where Black hasn't replied yet.
void SidebarWidget::RefreshMoveHistory() {
  const auto& history = engine_.History();
  int rows = static_cast<int>((history.size() + 1) / 2);
  move_table_->setRowCount(rows);

  const auto& times = session_.MoveTimes();
  auto time_text = [&](size_t index) {
    if (index >= times.size()) return QString();
    const qint64 seconds = times[index].count() / 1000;
    return QString("%1m %2s").arg(seconds / 60).arg(seconds % 60);
  };

  for (int row = 0; row < rows; row++) {
    size_t i = row * 2;
    move_table_->setItem(row, 0,
                         new QTableWidgetItem(QString::number(row + 1)));
    move_table_->setItem(
        row, 1,
        new QTableWidgetItem(QString::fromStdString(history[i].notation)));
    QString time = time_text(i);
    if (i + 1 < history.size()) time += " - " + time_text(i + 1);
    move_table_->setItem(row, 3, new QTableWidgetItem(time));
    if (i + 1 < history.size()) {
      move_table_->setItem(row, 2,
                           new QTableWidgetItem(QString::fromStdString(
                               history[i + 1].notation)));
    } else {
      move_table_->setItem(row, 2, new QTableWidgetItem());
    }
  }
}
}  // namespace chess
