/*
 * Copyright (c) 2026 Kornfeld Eliyahu Peter
 * License: MIT
 */

#include "ui/best_lines_widget.hpp"

#include <string>
#include <vector>

#include <QHeaderView>
#include <QStringList>
#include <QTableWidget>
#include <QVBoxLayout>

#include "engine/chess_engine.hpp"
#include "session/uci_move_parsing.hpp"

namespace chess {
namespace {
QString FormatScore(const AnalysisInfo& info, PieceColor side_to_move) {
  // UCI scores are from the side to move; the display uses White's perspective,
  // so Black-to-move scores are negated.
  int sign = side_to_move == PieceColor::kWhite ? 1 : -1;

  if (info.score_mate) {
    int mate = *info.score_mate * sign;
    return mate >= 0 ? QString("M%1").arg(mate) : QString("-M%1").arg(-mate);
  }

  int cp = info.score_cp.value_or(0) * sign;
  double pawns = cp / 100.0;
  return (pawns >= 0 ? "+" : "") + QString::number(pawns, 'f', 2);
}
}  // namespace

BestLinesWidget::BestLinesWidget(QWidget* parent) : QWidget(parent) {
  table_ = new QTableWidget(0, 2, this);
  table_->setHorizontalHeaderLabels({"Eval", "Line"});
  table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
  table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
  table_->setColumnWidth(0, 56);
  table_->verticalHeader()->setVisible(false);
  table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  table_->setSelectionMode(QAbstractItemView::NoSelection);
  table_->setShowGrid(true);
  table_->setWordWrap(true);
  table_->setTextElideMode(Qt::ElideNone);
  table_->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(table_);
}

void BestLinesWidget::SetPosition(const std::string& fen,
                                  PieceColor side_to_move) {
  fen_ = fen;
  side_to_move_ = side_to_move;
}

void BestLinesWidget::ClearLines() { table_->setRowCount(0); }

QString BestLinesWidget::PvToSan(
    const std::vector<std::string>& pv_uci_moves) const {
  if (fen_.empty()) return QString();

  ChessEngine scratch;
  if (!scratch.LoadFen(fen_)) return QString();

  QStringList san_moves;
  // Replay on a scratch engine from the analysed position; stop at the first
  // move that does not apply.
  for (size_t i = 0; i < pv_uci_moves.size(); ++i) {
    auto applied = ApplyUciMove(scratch, pv_uci_moves[i]);
    if (!applied) break;
    san_moves << QString::fromStdString(applied->notation);
  }

  return san_moves.join(' ');
}

void BestLinesWidget::SetLine(const AnalysisInfo& info) {
  if (fen_.empty()) return;

  int row = info.multipv - 1;
  if (row < 0) return;
  if (row >= table_->rowCount()) table_->setRowCount(row + 1);

  table_->setItem(row, 0,
                  new QTableWidgetItem(FormatScore(info, side_to_move_)));
  table_->setItem(row, 1, new QTableWidgetItem(PvToSan(info.pv_uci_moves)));
}
}  // namespace chess
