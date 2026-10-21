/*
 * Copyright (c) 2026 Kornfeld Eliyahu Peter
 * License: MIT
 */

#ifndef CHESSBOARD_UI_GLYPH_RENDERING_HPP_
#define CHESSBOARD_UI_GLYPH_RENDERING_HPP_

#include <QColor>
#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include <QRectF>
#include <QString>

#include "models/piece.hpp"

namespace chess {
// Traces `glyph` and shifts it so its bounding box is centered on `target`;
// font ascent/descent vary too much to center by metrics.
QPainterPath CenteredGlyphPath(const QFont& font, const QString& glyph,
                               const QRectF& target);

QColor PieceGlyphColor(PieceColor color);

QFont GlyphFontForSquareSize(const QFont& base_font, float square_size);

void DrawPieceGlyph(QPainter& painter, const QFont& font, const QRectF& rect,
                    const QString& glyph, const QColor& color);
}  // namespace chess

#endif  // CHESSBOARD_UI_GLYPH_RENDERING_HPP_
