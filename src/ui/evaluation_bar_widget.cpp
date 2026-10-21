/*
 * Copyright (c) 2026 Kornfeld Eliyahu Peter
 * License: MIT
 */

#include "ui/evaluation_bar_widget.hpp"

#include <algorithm>
#include <cmath>

#include <QPainter>

namespace chess {
namespace {
constexpr int kMinWidth = 28;
const QColor kWhiteFill(245, 245, 245);
const QColor kBlackFill(40, 40, 40);
const QColor kNeutralFill(150, 150, 150);
const QColor kBorderColor(80, 80, 80);

// Centipawns at which the fill pins to the edge (+8 pawns).
constexpr double kSaturationCp = 800.0;
}  // namespace

EvaluationBarWidget::EvaluationBarWidget(QWidget* parent) : QWidget(parent) {
  setMinimumWidth(kMinWidth);
}

void EvaluationBarWidget::SetScoreCp(std::optional<int> score_cp) {
  score_cp_ = score_cp;
  score_mate_.reset();
  update();
}

void EvaluationBarWidget::SetScoreMate(
    std::optional<int> score_mate_white_pov) {
  score_mate_ = score_mate_white_pov;
  score_cp_.reset();
  update();
}

void EvaluationBarWidget::Clear() {
  score_cp_.reset();
  score_mate_.reset();
  update();
}

void EvaluationBarWidget::SetFlipped(bool flipped) {
  if (flipped_ == flipped) return;
  flipped_ = flipped;
  update();
}

double EvaluationBarWidget::WhiteFraction() const {
  if (score_mate_) return *score_mate_ > 0 ? 1.0 : 0.0;
  if (!score_cp_) return 0.5;

  double normalized = std::clamp(*score_cp_ / kSaturationCp, -1.0, 1.0);
  return 0.5 + 0.5 * normalized;
}

void EvaluationBarWidget::paintEvent(QPaintEvent*) {
  QPainter painter(this);
  QRectF rect(0, 0, width(), height());

  double white_fraction = WhiteFraction();
  bool has_score = score_cp_.has_value() || score_mate_.has_value();

  if (!has_score) {
    painter.fillRect(rect, kNeutralFill);
  } else {
    double white_height = rect.height() * white_fraction;
    // Unflipped, White fills from the bottom; flipped, from the top, so the
    // human's color is always at the bottom.
    QRectF white_rect = flipped_ ? QRectF(0, 0, rect.width(), white_height)
                                 : QRectF(0, rect.height() - white_height,
                                          rect.width(), white_height);
    QRectF black_rect =
        flipped_ ? QRectF(0, white_height, rect.width(),
                          rect.height() - white_height)
                 : QRectF(0, 0, rect.width(), rect.height() - white_height);

    painter.fillRect(black_rect, kBlackFill);
    painter.fillRect(white_rect, kWhiteFill);
  }

  painter.setPen(kBorderColor);
  painter.drawRect(rect.adjusted(0, 0, -1, -1));

  if (score_mate_) {
    // The mate label sits inside the mating side's fill, with the same flip.
    bool label_at_bottom = (*score_mate_ > 0) != flipped_;
    painter.setPen(label_at_bottom ? Qt::black : Qt::white);
    QRectF label_rect = label_at_bottom ? rect.adjusted(0, 0, 0, -4)
                                        : rect.adjusted(0, 4, 0, 0);
    painter.drawText(
        label_rect,
        Qt::AlignHCenter | (label_at_bottom ? Qt::AlignBottom : Qt::AlignTop),
        QString("M%1").arg(std::abs(*score_mate_)));
  }
}
}  // namespace chess
