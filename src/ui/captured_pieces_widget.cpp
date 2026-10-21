/*
 * Copyright (c) 2026 Kornfeld Eliyahu Peter
 * License: MIT
 */

#include "ui/captured_pieces_widget.hpp"

#include <algorithm>
#include <optional>
#include <utility>

#include <QPainter>
#include <QPainterPath>
#include <QPen>

#include "ui/glyph_rendering.hpp"

namespace chess {
namespace {
constexpr int kIconSize = 40;
// Same-type icons overlap like a stack; a wider gap separates groups.
constexpr int kSameTypeAdvance = 5;
constexpr int kGroupGap = 22;
const QColor kLightPieceOutline(0x55, 0x55, 0x55);

// Most to least valuable, left to right, rather than capture order.
int SortRank(PieceType type) {
  switch (type) {
    case PieceType::kQueen:
      return 0;
    case PieceType::kRook:
      return 1;
    case PieceType::kBishop:
      return 2;
    case PieceType::kKnight:
      return 3;
    case PieceType::kPawn:
      return 4;
    default:
      return 5;
  }
}
}  // namespace

CapturedPiecesWidget::CapturedPiecesWidget(QFont piece_font, QWidget* parent)
    : QWidget(parent), piece_font_(std::move(piece_font)) {
  setMinimumHeight(kIconSize);
  icon_font_ =
      GlyphFontForSquareSize(piece_font_, static_cast<float>(kIconSize));
}

void CapturedPiecesWidget::SetPieceFont(QFont piece_font) {
  piece_font_ = std::move(piece_font);
  icon_font_ =
      GlyphFontForSquareSize(piece_font_, static_cast<float>(kIconSize));
  update();
}

void CapturedPiecesWidget::SetCaptured(std::vector<PieceType> captured,
                                       PieceColor captured_color,
                                       int material_lead) {
  std::sort(captured.begin(), captured.end(),
            [](PieceType a, PieceType b) { return SortRank(a) < SortRank(b); });
  captured_ = std::move(captured);
  captured_color_ = captured_color;
  material_lead_ = material_lead;
  update();
}

void CapturedPiecesWidget::paintEvent(QPaintEvent*) {
  QPainter painter(this);

  PieceColor captured_color = captured_color_;
  QColor color = PieceGlyphColor(captured_color);

  double x = 0;
  std::optional<PieceType> prev_type;
  for (PieceType type : captured_) {
    if (prev_type && *prev_type != type) x += kGroupGap;

    Piece piece(type, captured_color);
    QRectF rect(x, 0, kIconSize, height());

    // The light fill vanishes on a plain background, so light pieces get a dark
    // outline.
    if (captured_color == PieceColor::kWhite) {
      QPainterPath path = CenteredGlyphPath(icon_font_, piece.Glyph(), rect);
      painter.setRenderHint(QPainter::Antialiasing, true);

      QPen outline(kLightPieceOutline, 0.6);
      outline.setJoinStyle(Qt::RoundJoin);
      outline.setCapStyle(Qt::RoundCap);
      painter.setPen(outline);
      painter.setBrush(color);
      painter.drawPath(path);
      painter.setPen(Qt::NoPen);
      painter.setBrush(Qt::NoBrush);
    } else {
      DrawPieceGlyph(painter, icon_font_, rect, piece.Glyph(), color);
    }

    x += kSameTypeAdvance;
    prev_type = type;
  }

  if (!captured_.empty()) x += kIconSize - kSameTypeAdvance;

  if (material_lead_ > 0) {
    x += 2;
    QFontMetricsF metrics(font());
    QRectF text_rect(x, 0, metrics.horizontalAdvance(QStringLiteral("+00")),
                     height());
    painter.setPen(palette().color(QPalette::Text));
    painter.drawText(text_rect, Qt::AlignVCenter | Qt::AlignLeft,
                     QStringLiteral("+%1").arg(material_lead_));
  }
}
}  // namespace chess
