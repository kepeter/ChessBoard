/*
 * Copyright (c) 2026 Kornfeld Eliyahu Peter
 * License: MIT
 */

#ifndef CHESSBOARD_UI_BOARD_WIDGET_HPP_
#define CHESSBOARD_UI_BOARD_WIDGET_HPP_

#include <chrono>
#include <optional>
#include <utility>
#include <vector>

#include <QFont>
#include <QPoint>
#include <QRectF>
#include <QSizeF>
#include <QWidget>

#include "engine/chess_engine.hpp"
#include "models/piece.hpp"
#include "models/square.hpp"
#include "session/game_session.hpp"
#include "session/move_classifier.hpp"

class QResizeEvent;

namespace chess {
class BoardWidget : public QWidget {
  Q_OBJECT

 public:
  BoardWidget(ChessEngine& engine, GameSession& session, QFont piece_font,
              QWidget* parent = nullptr);

  void SetPieceFont(QFont piece_font);

  // Draws arrows for the best line's first plies (empty clears). The first ply
  // is the human's; plies alternate and each pair is lighter.
  void SetAnalysisArrows(std::vector<std::pair<Square, Square>> arrows);

  // Badge on `square` (the last human move's destination); nullopt clears it.
  void SetLastMoveBadge(std::optional<Square> square,
                        std::optional<MoveQuality> quality);

 protected:
  void paintEvent(QPaintEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void changeEvent(QEvent* event) override;
  bool event(QEvent* event) override;

 private:
  ChessEngine& engine_;
  GameSession& session_;
  QFont piece_font_;
  // Bold, enlarged copy of the font for rank/file labels, rebuilt only when the
  // base font changes.
  QFont label_font_;

  void RebuildLabelFont();
  // Recomputes the 8x8 grid's origin and square size (excluding label gutters).
  // Shared by paintEvent and resizeEvent so GridRect() is accurate immediately
  // after a resize.
  void RecomputeGridGeometry();

  std::optional<Square> selected_square_;
  std::vector<Square> legal_targets_;

  std::optional<Square> drag_source_;
  std::optional<Square> drag_over_square_;
  bool dragging_ = false;
  QPoint drag_pos_;

  // A press becomes a drag only after the pointer passes the OS drag threshold;
  // until then it may still be a click.
  std::optional<Square> press_candidate_;
  QPoint press_pos_;

  // Square flashed red briefly after an illegal move.
  std::optional<Square> invalid_target_square_;
  std::chrono::steady_clock::time_point invalid_target_started_at_;
  QTimer* invalid_flash_timer_;

  bool board_flipped_ = false;

  QPointF board_origin_{};
  float square_size_ = 0.0f;
  // Left edge of the rank-label gutter; board_origin_ sits kLabelGutterWidth to
  // its right.
  QPointF label_gutter_origin_{};

  std::vector<std::pair<Square, Square>> analysis_arrows_;

  void PaintAnalysisArrow(QPainter& painter, Square from, Square to,
                          const QColor& color) const;

  std::optional<Square> last_move_badge_square_;
  std::optional<MoveQuality> last_move_badge_quality_;

  void PaintLastMoveBadge(QPainter& painter, const QRectF& square_rect) const;

  bool CanHumanInteract() const;
  void OnSquareClicked(Square square);
  bool TrySelect(Square square);
  void ClearSelection();
  void FlashInvalidTarget(Square square);
  void OnDragStart(Square square);
  void OnDragEnd();
  void OnDrop(Square square);
  void TryMoveOrPromote(Square from, Square to);
  void MakeMove(Square from, Square to, std::optional<PieceType> promotion);

  std::optional<Square> SquareAt(QPointF local_pos) const;
  QPointF SquareCenter(Square square) const;

 public:
  QPoint SquareCenterGlobal(Square square) const;

  // The 8x8 grid in local coordinates, without the label gutters. Empty before
  // the first paint.
  QRectF GridRect() const {
    return QRectF(board_origin_,
                  QSizeF(square_size_ * 8.0f, square_size_ * 8.0f));
  }

 signals:
  void promotionNeeded(Square from, Square to, PieceColor color);

 public slots:
  void OnGameSessionChanged();
};
}  // namespace chess

#endif  // CHESSBOARD_UI_BOARD_WIDGET_HPP_
