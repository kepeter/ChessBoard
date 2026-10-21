/*
 * Copyright (c) 2026 Kornfeld Eliyahu Peter
 * License: MIT
 */

#include "ui/clock_widget.hpp"

#include <cstdint>
#include <cstdio>

#include <QEvent>
#include <QPainter>

namespace chess {
namespace {
const QColor kClockActiveBg(47, 158, 68);
const QColor kClockInactiveBg(210, 214, 218);
const QColor kClockFlaggedBg(248, 215, 218);
const QColor kClockFlaggedText(0x84, 0x20, 0x29);
}  // namespace

ClockWidget::ClockWidget(QWidget* parent) : QWidget(parent) {
  setFixedHeight(40);
  RebuildTimeFont();
}

void ClockWidget::RebuildTimeFont() {
  time_font_ = font();
  time_font_.setBold(true);
  time_font_.setPointSize(time_font_.pointSize() + 2);
}

// The cached bold font derives from the widget font, so refresh on change.
void ClockWidget::changeEvent(QEvent* event) {
  QWidget::changeEvent(event);
  if (event->type() == QEvent::FontChange) RebuildTimeFont();
}

void ClockWidget::SetState(bool active, bool flagged,
                           std::chrono::milliseconds remaining) {
  active_ = active;
  flagged_ = flagged;
  remaining_ = remaining;
  update();
}

QString ClockWidget::FormatTime(std::chrono::milliseconds t) {
  if (t < std::chrono::milliseconds::zero())
    t = std::chrono::milliseconds::zero();
  int64_t total_seconds = t.count() / 1000;
  int64_t hours = total_seconds / 3600;
  int64_t minutes = (total_seconds % 3600) / 60;
  int64_t seconds = total_seconds % 60;

  char buf[32];
  // Hours appear only for long time controls.
  if (hours > 0) {
    std::snprintf(buf, sizeof(buf), "%lld:%02lld:%02lld", hours, minutes,
                  seconds);
  } else {
    std::snprintf(buf, sizeof(buf), "%lld:%02lld", minutes, seconds);
  }
  return QString::fromLatin1(buf);
}

void ClockWidget::paintEvent(QPaintEvent*) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  QColor bg = flagged_ ? kClockFlaggedBg
                       : (active_ ? kClockActiveBg : kClockInactiveBg);
  QColor text_color =
      flagged_ ? kClockFlaggedText
               : (active_ ? QColor(255, 255, 255) : QColor(51, 51, 51));

  QRectF rect(0, 0, width(), height());
  painter.setPen(Qt::NoPen);
  painter.setBrush(bg);
  painter.drawRoundedRect(rect, 6.0, 6.0);

  painter.setFont(time_font_);

  painter.setPen(text_color);
  painter.drawText(rect, Qt::AlignCenter, FormatTime(remaining_));
}
}  // namespace chess
