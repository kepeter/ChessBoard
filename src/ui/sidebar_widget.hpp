/*
 * Copyright (c) 2026 Kornfeld Eliyahu Peter
 * License: MIT
 */

#ifndef CHESSBOARD_UI_SIDEBAR_WIDGET_HPP_
#define CHESSBOARD_UI_SIDEBAR_WIDGET_HPP_

#include <cstddef>

#include <QFont>
#include <QWidget>

#include "engine/chess_engine.hpp"
#include "session/game_session.hpp"

class QTableWidget;

namespace chess {
class ClockWidget;
class CapturedPiecesWidget;

class SidebarWidget : public QWidget {
  Q_OBJECT

 public:
  static constexpr int kMinWidth = 280;

  SidebarWidget(ChessEngine& engine, GameSession& session, QFont piece_font,
                QWidget* parent = nullptr);

  void SetPieceFont(QFont piece_font);

 public slots:
  void Refresh();

 private:
  ChessEngine& engine_;
  GameSession& session_;
  QFont piece_font_;

  ClockWidget* top_clock_;
  ClockWidget* bottom_clock_;
  CapturedPiecesWidget* top_captured_;
  CapturedPiecesWidget* bottom_captured_;
  QTableWidget* move_table_;

  // Lets Refresh() skip rebuilding the move table on clock ticks (stateChanged
  // fires ~15x/sec).
  size_t last_refreshed_history_size_ = 0;

  void RefreshMoveHistory();
  void RefreshCaptured();
};
}  // namespace chess

#endif  // CHESSBOARD_UI_SIDEBAR_WIDGET_HPP_
