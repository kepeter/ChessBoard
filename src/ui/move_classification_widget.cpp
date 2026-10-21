/*
 * Copyright (c) 2026 Kornfeld Eliyahu Peter
 * License: MIT
 */

#include "ui/move_classification_widget.hpp"

#include <algorithm>

#include <QHelpEvent>
#include <QPainter>
#include <QToolTip>

namespace chess {
namespace {
constexpr double kBadgeDiameterFraction = 0.85;
constexpr int kAccuracyColumnWidth = 44;
}  // namespace

MoveClassificationWidget::MoveClassificationWidget(QWidget* parent)
    : QWidget(parent) {
  setFixedHeight(28);
}

void MoveClassificationWidget::AddClassification(MoveQuality quality,
                                                 double accuracy_percent) {
  counts_[static_cast<int>(quality)]++;
  accuracy_percent_ = accuracy_percent;
  update();
}

void MoveClassificationWidget::Reset() {
  counts_.fill(0);
  accuracy_percent_.reset();
  update();
}

double MoveClassificationWidget::BadgesAreaWidth() const {
  return accuracy_percent_ ? width() - kAccuracyColumnWidth : width();
}

bool MoveClassificationWidget::event(QEvent* ev) {
  if (ev->type() == QEvent::ToolTip) {
    auto* help_event = static_cast<QHelpEvent*>(ev);

    double cell_width = BadgesAreaWidth() / kQualityCount;
    int index = cell_width > 0
                    ? static_cast<int>(help_event->pos().x() / cell_width)
                    : -1;

    if (index >= 0 && index < kQualityCount) {
      MoveQuality quality = static_cast<MoveQuality>(index);
      QToolTip::showText(
          help_event->globalPos(),
          QString("%1: %2").arg(MoveQualityLabel(quality)).arg(counts_[index]),
          this);
    } else {
      QToolTip::hideText();
    }
    return true;
  }

  return QWidget::event(ev);
}

void MoveClassificationWidget::paintEvent(QPaintEvent*) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  double badges_width = BadgesAreaWidth();
  double cell_width = badges_width / kQualityCount;
  double diameter = std::min(cell_width, static_cast<double>(height())) *
                    kBadgeDiameterFraction;

  for (int i = 0; i < kQualityCount; i++) {
    MoveQuality quality = static_cast<MoveQuality>(i);
    QRectF cell(i * cell_width, 0, cell_width, height());
    QRectF badge(cell.center().x() - diameter / 2.0,
                 cell.center().y() - diameter / 2.0, diameter, diameter);

    painter.setPen(Qt::NoPen);
    painter.setBrush(MoveQualityColor(quality));
    painter.drawEllipse(badge);

    painter.setPen(Qt::white);
    painter.drawText(badge, Qt::AlignCenter, QString::number(counts_[i]));
  }

  if (accuracy_percent_) {
    QRectF accuracy_rect(badges_width, 0, kAccuracyColumnWidth, height());
    painter.setPen(palette().color(QPalette::Text));
    painter.drawText(accuracy_rect, Qt::AlignVCenter | Qt::AlignRight,
                     QString("%1%").arg(*accuracy_percent_, 0, 'f', 0));
  }
}
}  // namespace chess
