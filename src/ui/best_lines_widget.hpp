/*
 * Copyright (c) 2026 Kornfeld Eliyahu Peter
 * License: MIT
 */

#ifndef CHESSBOARD_UI_BEST_LINES_WIDGET_HPP_
#define CHESSBOARD_UI_BEST_LINES_WIDGET_HPP_

#include <string>
#include <vector>

#include <QWidget>

#include "models/piece.hpp"
#include "uci/uci_engine.hpp"

class QTableWidget;

namespace chess {
class BestLinesWidget : public QWidget {
  Q_OBJECT

 public:
  explicit BestLinesWidget(QWidget* parent = nullptr);

  // The position the PVs are relative to; set before SetLine so they can be
  // replayed into SAN.
  void SetPosition(const std::string& fen, PieceColor side_to_move);

  void SetLine(const AnalysisInfo& info);

  // Removes all rows but keeps the position.
  void ClearLines();

 private:
  QTableWidget* table_;
  std::string fen_;
  PieceColor side_to_move_ = PieceColor::kWhite;

  // Replays UCI moves from fen_ into space-separated SAN, stopping at the first
  // move that can't be applied.
  QString PvToSan(const std::vector<std::string>& pv_uci_moves) const;
};
}  // namespace chess

#endif  // CHESSBOARD_UI_BEST_LINES_WIDGET_HPP_
