/*
 * Copyright (c) 2026 Kornfeld Eliyahu Peter
 * License: MIT
 */

#include "ui/glyph_rendering.hpp"

#include <algorithm>

namespace chess {
namespace {
const QColor kLightPieceColor(0xfe, 0xfe, 0xfe);
const QColor kDarkPieceColor(0x1a, 0x1a, 0x1a);
}  // namespace

QPainterPath CenteredGlyphPath(const QFont& font, const QString& glyph,
                               const QRectF& target) {
  QPainterPath path;
  path.addText(0, 0, font, glyph);
  QRectF bounds = path.boundingRect();
  QPointF offset(target.center().x() - bounds.center().x(),
                 target.center().y() - bounds.center().y());
  path.translate(offset);

  return path;
}

QColor PieceGlyphColor(PieceColor color) {
  return color == PieceColor::kWhite ? kLightPieceColor : kDarkPieceColor;
}

QFont GlyphFontForSquareSize(const QFont& base_font, float square_size) {
  // Scaled below the square size so the piece leaves a margin.
  QFont glyph_font = base_font;
  glyph_font.setPixelSize(std::max(1, static_cast<int>(square_size * 0.82f)));

  return glyph_font;
}

void DrawPieceGlyph(QPainter& painter, const QFont& font, const QRectF& rect,
                    const QString& glyph, const QColor& color) {
  QPainterPath path = CenteredGlyphPath(font, glyph, rect);

  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setPen(Qt::NoPen);
  painter.setBrush(color);
  painter.drawPath(path);
  painter.setBrush(Qt::NoBrush);
}
}  // namespace chess
