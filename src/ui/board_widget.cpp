/*
 * Copyright (c) 2026 Kornfeld Eliyahu Peter
 * License: MIT
 */

#include "ui/board_widget.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <unordered_map>
#include <utility>
#include <vector>

#include <QApplication>
#include <QHelpEvent>
#include <QImage>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QStyleHints>
#include <QTimer>
#include <QToolTip>

#include "config/app_config.hpp"
#include "ui/glyph_rendering.hpp"

namespace chess {
namespace {
constexpr float kInvalidFlashSeconds = 0.5f;

const QColor kLastMoveTint(255, 215, 0, static_cast<int>(255 * 0.25f));
const QColor kDragOverValidTint(47, 158, 68, static_cast<int>(255 * 0.35f));
const QColor kDragOverInvalidTint(210, 50, 50, static_cast<int>(255 * 0.35f));
const QColor kCheckKingTint(255, 140, 0, static_cast<int>(255 * 0.45f));
const QColor kCheckmateKingTint(200, 0, 0, static_cast<int>(255 * 0.6f));
const QColor kSelectedBg(60, 170, 90, static_cast<int>(255 * 0.55f));
const QColor kSelectedRing(47, 158, 68);
const QColor kSelectedInvalidBg(210, 50, 50, static_cast<int>(255 * 0.55f));
const QColor kSelectedInvalidRing(192, 39, 45);
const QColor kMoveHintDot(0, 0, 0, static_cast<int>(255 * 0.25f));
const QColor kCaptureHintRing(200, 0, 0, static_cast<int>(255 * 0.55f));
const QColor kLabelColor(0x22, 0x22, 0x22);
// Arrows alternate human (blue) and engine (orange); each successive pair is
// lighter, so the order reads without a legend.
const QColor kAnalysisArrowHuman(30, 120, 220);
const QColor kAnalysisArrowEngine(235, 130, 20);
const float kAnalysisArrowAlpha[] = {0.90f, 0.55f, 0.30f};
constexpr double kLastMoveBadgeFraction = 0.24;
constexpr double kLastMoveBadgeMarginFraction = 0.06;

// Tints a base texture toward `tint`. Multiply and Overlay blends lose the
// grain contrast when either the photo or the tint is light. Instead each
// pixel's lightness is averaged with the tint's, and only hue and saturation
// are replaced, so contrast survives.
QImage TintTexture(const QImage& base, const QColor& tint) {
  QImage source = base.convertToFormat(QImage::Format_RGB32);
  QImage tinted(source.size(), QImage::Format_RGB32);

  int tint_h, tint_s, tint_l;
  tint.getHsl(&tint_h, &tint_s, &tint_l);

  for (int y = 0; y < source.height(); y++) {
    const QRgb* src_row =
        reinterpret_cast<const QRgb*>(source.constScanLine(y));
    QRgb* dst_row = reinterpret_cast<QRgb*>(tinted.scanLine(y));

    for (int x = 0; x < source.width(); x++) {
      QColor pixel(src_row[x]);
      int pixel_l = pixel.lightness();
      int blended_l = (pixel_l + tint_l) / 2;

      QColor result;
      result.setHsl(tint_h, tint_s, blended_l);
      dst_row[x] = result.rgb();
    }
  }

  return tinted;
}

// Process-lifetime cache of a theme's raw and tinted textures, to avoid
// re-reading and re-tinting on every paint.
struct SquareTextures {
  QImage light_tinted;
  QImage dark_tinted;
};

const SquareTextures& GetSquareTextures(BoardTheme theme,
                                        const QColor& light_tint,
                                        const QColor& dark_tint) {
  struct CacheKey {
    BoardTheme theme;
    QRgb light_tint;
    QRgb dark_tint;

    bool operator==(const CacheKey& other) const {
      return theme == other.theme && light_tint == other.light_tint &&
             dark_tint == other.dark_tint;
    }
  };
  struct CacheKeyHash {
    size_t operator()(const CacheKey& k) const {
      return std::hash<int>()(static_cast<int>(k.theme)) ^
             std::hash<QRgb>()(k.light_tint) * 3 ^
             std::hash<QRgb>()(k.dark_tint) * 7;
    }
  };

  static std::unordered_map<CacheKey, SquareTextures, CacheKeyHash> cache;
  static std::unordered_map<BoardTheme, std::pair<QImage, QImage>> raw_textures;

  CacheKey key{theme, light_tint.rgba(), dark_tint.rgba()};
  auto cached = cache.find(key);
  if (cached != cache.end()) return cached->second;

  auto raw = raw_textures.find(theme);
  if (raw == raw_textures.end()) {
    BoardTextureFiles files = TextureFilesForTheme(theme);
    QString dir = AssetsDirPath();
    QImage light(dir + "/" + files.light_square);
    QImage dark(dir + "/" + files.dark_square);
    raw = raw_textures
              .emplace(theme, std::make_pair(std::move(light), std::move(dark)))
              .first;
  }

  SquareTextures textures{TintTexture(raw->second.first, light_tint),
                          TintTexture(raw->second.second, dark_tint)};
  return cache.emplace(key, std::move(textures)).first->second;
}

// Top-right corner of a square, clear of the centered piece glyph.
QRectF LastMoveBadgeRect(const QRectF& square_rect) {
  double diameter = square_rect.width() * kLastMoveBadgeFraction;
  double margin = square_rect.width() * kLastMoveBadgeMarginFraction;
  return QRectF(square_rect.right() - diameter - margin,
                square_rect.top() + margin, diameter, diameter);
}

// Soft diagonal highlight for the glass theme; a tint alone can't make it look
// glossy.
void PaintGlassGloss(QPainter& painter, const QRectF& rect) {
  QLinearGradient gloss(rect.topLeft(), rect.bottomRight());
  gloss.setColorAt(0.0, QColor(255, 255, 255, 70));
  gloss.setColorAt(0.28, QColor(255, 255, 255, 20));
  gloss.setColorAt(0.45, QColor(255, 255, 255, 0));
  gloss.setColorAt(1.0, QColor(255, 255, 255, 0));

  painter.save();
  painter.setClipRect(rect);
  painter.fillRect(rect, gloss);
  painter.restore();
}

void PaintSquarePattern(QPainter& painter, const QRectF& rect, BoardTheme theme,
                        const BoardColors& colors, bool is_dark) {
  const SquareTextures& textures =
      GetSquareTextures(theme, colors.light_square, colors.dark_square);
  const QImage& texture =
      is_dark ? textures.dark_tinted : textures.light_tinted;

  if (texture.isNull()) {
    painter.fillRect(rect, is_dark ? colors.dark_square : colors.light_square);
  } else {
    // `rect` is snapped to shared integer grid lines, so squares meet exactly;
    // antialiasing is off because drawTiledPixmap softens its edges otherwise.
    // Dark-square assets already have their own grain direction, so no rotation
    // is applied.
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setClipRect(rect);
    painter.drawTiledPixmap(rect, QPixmap::fromImage(texture));
    painter.restore();
  }

  if (theme == BoardTheme::kGlass) PaintGlassGloss(painter, rect);
}

}  // namespace

void BoardWidget::SetPieceFont(QFont piece_font) {
  piece_font_ = std::move(piece_font);
  update();
}

void BoardWidget::SetAnalysisArrows(
    std::vector<std::pair<Square, Square>> arrows) {
  analysis_arrows_ = std::move(arrows);
  update();
}

void BoardWidget::SetLastMoveBadge(std::optional<Square> square,
                                   std::optional<MoveQuality> quality) {
  last_move_badge_square_ = square;
  last_move_badge_quality_ = quality;
  update();
}

void BoardWidget::PaintLastMoveBadge(QPainter& painter,
                                     const QRectF& square_rect) const {
  if (!last_move_badge_quality_) return;

  QRectF badge = LastMoveBadgeRect(square_rect);
  double diameter = badge.width();

  painter.setPen(QPen(Qt::white, diameter * 0.08));
  painter.setBrush(MoveQualityColor(*last_move_badge_quality_));
  painter.drawEllipse(badge);

  QFont symbol_font = font();
  symbol_font.setPixelSize(static_cast<int>(diameter * 0.6));
  symbol_font.setBold(true);
  painter.setFont(symbol_font);
  painter.setPen(Qt::white);
  painter.drawText(badge, Qt::AlignCenter,
                   MoveQualitySymbol(*last_move_badge_quality_));
}

// The shaft stops short of `to`'s center so the head isn't hidden behind the
// piece there.
void BoardWidget::PaintAnalysisArrow(QPainter& painter, Square from, Square to,
                                     const QColor& color) const {
  QPointF start = SquareCenter(from);
  QPointF end = SquareCenter(to);

  QPointF delta = end - start;
  double length = std::hypot(delta.x(), delta.y());
  if (length < 1.0) return;
  QPointF dir = delta / length;

  float head_length = square_size_ * 0.35f;
  float shaft_width = square_size_ * 0.14f;
  float head_width = square_size_ * 0.26f;

  QPointF head_base = end - dir * head_length;
  QPointF normal(-dir.y(), dir.x());

  QPainterPath path;
  path.moveTo(start + normal * (shaft_width / 2.0));
  path.lineTo(head_base + normal * (shaft_width / 2.0));
  path.lineTo(head_base + normal * (head_width / 2.0));
  path.lineTo(end);
  path.lineTo(head_base - normal * (head_width / 2.0));
  path.lineTo(head_base - normal * (shaft_width / 2.0));
  path.lineTo(start - normal * (shaft_width / 2.0));
  path.closeSubpath();

  painter.setPen(Qt::NoPen);
  painter.setBrush(color);
  painter.drawPath(path);
  painter.setBrush(Qt::NoBrush);
}

bool BoardWidget::event(QEvent* ev) {
  if (ev->type() == QEvent::ToolTip) {
    auto* help_event = static_cast<QHelpEvent*>(ev);

    if (last_move_badge_square_ && last_move_badge_quality_ &&
        square_size_ > 0.0f) {
      QPointF center = SquareCenter(*last_move_badge_square_);
      QRectF badge = LastMoveBadgeRect(QRectF(center.x() - square_size_ / 2.0,
                                              center.y() - square_size_ / 2.0,
                                              square_size_, square_size_));

      if (badge.contains(help_event->pos())) {
        QToolTip::showText(help_event->globalPos(),
                           MoveQualityLabel(*last_move_badge_quality_), this);
        return true;
      }
    }

    QToolTip::hideText();
    return true;
  }

  return QWidget::event(ev);
}

BoardWidget::BoardWidget(ChessEngine& engine, GameSession& session,
                         QFont piece_font, QWidget* parent)
    : QWidget(parent),
      engine_(engine),
      session_(session),
      piece_font_(std::move(piece_font)) {
  setMouseTracking(true);
  setMinimumSize(240, 240);
  RebuildLabelFont();

  invalid_flash_timer_ = new QTimer(this);
  invalid_flash_timer_->setInterval(16);
  connect(invalid_flash_timer_, &QTimer::timeout, this, [this]() {
    float elapsed =
        std::chrono::duration<float>(std::chrono::steady_clock::now() -
                                     invalid_target_started_at_)
            .count();
    if (elapsed >= kInvalidFlashSeconds) {
      invalid_target_square_.reset();
      invalid_flash_timer_->stop();
    }
    update();
  });

  connect(&session_, &GameSession::stateChanged, this,
          &BoardWidget::OnGameSessionChanged);
}

void BoardWidget::OnGameSessionChanged() { update(); }

void BoardWidget::RebuildLabelFont() {
  label_font_ = font();
  label_font_.setBold(true);
  label_font_.setPointSize(label_font_.pointSize() + 2);
}

void BoardWidget::changeEvent(QEvent* event) {
  QWidget::changeEvent(event);
  if (event->type() == QEvent::FontChange) RebuildLabelFont();
}

// The board accepts input only while a game runs, it is the human's turn, the
// engine is idle, the game isn't over and there is no engine error.
bool BoardWidget::CanHumanInteract() const {
  return session_.IsGameStarted() && !session_.IsEngineThinking() &&
         !session_.IsEnginesTurn() && !session_.IsGameOver() &&
         !session_.HasEngineError();
}

void BoardWidget::OnSquareClicked(Square square) {
  if (!CanHumanInteract()) return;

  if (!selected_square_) {
    TrySelect(square);
    update();
    return;
  }

  if (*selected_square_ == square) {
    ClearSelection();
    update();
    return;
  }

  if (std::find(legal_targets_.begin(), legal_targets_.end(), square) !=
      legal_targets_.end()) {
    TryMoveOrPromote(*selected_square_, square);
  } else if (!TrySelect(square)) {
    FlashInvalidTarget(square);
    ClearSelection();
  }
  update();
}

bool BoardWidget::TrySelect(Square square) {
  auto piece = engine_.PieceAt(square);
  if (!piece || piece->color != engine_.SideToMove()) return false;

  selected_square_ = square;
  legal_targets_ = engine_.GetLegalMoves(square);
  return true;
}

void BoardWidget::ClearSelection() {
  selected_square_.reset();
  legal_targets_.clear();
}

void BoardWidget::FlashInvalidTarget(Square square) {
  invalid_target_square_ = square;
  invalid_target_started_at_ = std::chrono::steady_clock::now();
  invalid_flash_timer_->start();
}

void BoardWidget::OnDragStart(Square square) {
  if (!CanHumanInteract()) return;

  if (!TrySelect(square)) return;

  drag_source_ = square;
  dragging_ = true;
}

void BoardWidget::OnDragEnd() {
  drag_source_.reset();
  drag_over_square_.reset();
  dragging_ = false;
  ClearSelection();
}

void BoardWidget::OnDrop(Square square) {
  drag_over_square_.reset();
  if (!drag_source_) return;

  Square from = *drag_source_;
  drag_source_.reset();
  dragging_ = false;

  if (from == square) {
    ClearSelection();
    return;
  }

  if (std::find(legal_targets_.begin(), legal_targets_.end(), square) !=
      legal_targets_.end()) {
    TryMoveOrPromote(from, square);
  } else {
    FlashInvalidTarget(square);
    ClearSelection();
  }
}

void BoardWidget::TryMoveOrPromote(Square from, Square to) {
  auto moving_piece = engine_.PieceAt(from);

  // Only a pawn can promote. Check this first: a castle's `to` is the rook's
  // square on the home rank, so a rank-only test would misread every castle as
  // a promotion.
  if (!moving_piece || moving_piece->type != PieceType::kPawn) {
    MakeMove(from, to, std::nullopt);
    return;
  }

  bool is_promotion = to.rank == 0 || to.rank == 7;

  if (is_promotion) {
    ClearSelection();
    emit promotionNeeded(from, to, moving_piece->color);
    return;
  }

  MakeMove(from, to, std::nullopt);
}

void BoardWidget::MakeMove(Square from, Square to,
                           std::optional<PieceType> promotion) {
  session_.TryApplyMove(from, to, promotion);

  ClearSelection();
  update();
}

// `fi`/`ri` are the visual column/row (0 = top-left), flipped to file/rank when
// viewing from Black's side.
std::optional<Square> BoardWidget::SquareAt(QPointF local_pos) const {
  if (square_size_ <= 0.0f) return std::nullopt;

  float local_x = local_pos.x() - board_origin_.x();
  float local_y = local_pos.y() - board_origin_.y();
  float board_size = square_size_ * 8.0f;
  if (local_x < 0 || local_y < 0 || local_x >= board_size ||
      local_y >= board_size)
    return std::nullopt;

  int fi = std::min(static_cast<int>(local_x / square_size_), 7);
  int ri = std::min(static_cast<int>(local_y / square_size_), 7);
  int rank = board_flipped_ ? ri : 7 - ri;
  int file = board_flipped_ ? 7 - fi : fi;
  return Square(file, rank);
}

QPointF BoardWidget::SquareCenter(Square square) const {
  int fi = board_flipped_ ? 7 - square.file : square.file;
  int ri = board_flipped_ ? square.rank : 7 - square.rank;
  return QPointF(board_origin_.x() + (fi + 0.5f) * square_size_,
                 board_origin_.y() + (ri + 0.5f) * square_size_);
}

QPoint BoardWidget::SquareCenterGlobal(Square square) const {
  return mapToGlobal(SquareCenter(square).toPoint());
}

namespace {
constexpr float kLabelGutterWidth = 24.0f;
constexpr float kMinBoardSize = 240.0f;
}  // namespace

void BoardWidget::RecomputeGridGeometry() {
  QFontMetricsF label_metrics(label_font_);
  float file_label_height = label_metrics.height() + 4.0f;

  float outer_w = width();
  float outer_h = height();
  float width_budget = outer_w - kLabelGutterWidth;
  float height_budget = outer_h - file_label_height;
  float board_size =
      std::max(std::min(width_budget, height_budget), kMinBoardSize);
  float square_size = board_size / 8.0f;
  float child_width = board_size + kLabelGutterWidth;
  float child_height = board_size + file_label_height;

  QPointF origin(std::max((outer_w - child_width) / 2.0f, 0.0f),
                 std::max((outer_h - child_height) / 2.0f, 0.0f));

  label_gutter_origin_ = origin;
  board_origin_ = QPointF(origin.x() + kLabelGutterWidth, origin.y());
  square_size_ = square_size;
}

void BoardWidget::resizeEvent(QResizeEvent* event) {
  QWidget::resizeEvent(event);
  RecomputeGridGeometry();
}

void BoardWidget::paintEvent(QPaintEvent*) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  BoardColors colors = AppConfig::Instance().Colors();
  BoardTheme theme = AppConfig::Instance().Theme();

  bool flipped = session_.PlayerColor() == PieceColor::kBlack;

  // Stored so SquareAt() and SquareCenter() use the orientation last painted.
  RecomputeGridGeometry();
  board_flipped_ = flipped;

  QPointF board_origin = board_origin_;
  float square_size = square_size_;
  float board_size = square_size * 8.0f;
  float file_label_height = QFontMetricsF(label_font_).height() + 4.0f;

  // Grid-line pixel positions shared by all squares, so neighbors meet on the
  // same integer boundary; rounding each square separately can open a one-pixel
  // seam.
  std::array<int, 9> grid_x{}, grid_y{};
  for (int i = 0; i <= 8; i++) {
    grid_x[i] =
        static_cast<int>(std::round(board_origin.x() + i * square_size));
    grid_y[i] =
        static_cast<int>(std::round(board_origin.y() + i * square_size));
  }

  painter.setFont(label_font_);
  painter.setPen(kLabelColor);
  for (int ri = 0; ri < 8; ri++) {
    int rank = flipped ? ri : 7 - ri;
    QString text = QString::number(rank + 1);
    QRectF label_rect(label_gutter_origin_.x(),
                      board_origin.y() + ri * square_size, kLabelGutterWidth,
                      square_size);
    painter.drawText(label_rect, Qt::AlignCenter, text);
  }

  std::optional<Square> last_move_from, last_move_to;
  if (!engine_.History().empty()) {
    const Move& last = engine_.History().back();
    last_move_from = last.from;
    last_move_to = ChessEngine::LandingSquare(last);
  }

  for (int ri = 0; ri < 8; ri++) {
    int rank = flipped ? ri : 7 - ri;
    for (int fi = 0; fi < 8; fi++) {
      int file = flipped ? 7 - fi : fi;
      Square sq(file, rank);

      QRectF rect(grid_x[fi], grid_y[ri], grid_x[fi + 1] - grid_x[fi],
                  grid_y[ri + 1] - grid_y[ri]);

      // a1 is dark; file+rank parity alternates from there.
      bool is_dark = (file + rank) % 2 == 0;
      PaintSquarePattern(painter, rect, theme, colors, is_dark);

      bool is_last_move = (last_move_from && *last_move_from == sq) ||
                          (last_move_to && *last_move_to == sq);
      if (is_last_move) painter.fillRect(rect, kLastMoveTint);

      bool is_legal_target =
          std::find(legal_targets_.begin(), legal_targets_.end(), sq) !=
          legal_targets_.end();

      bool is_drag_over =
          dragging_ && drag_over_square_ && *drag_over_square_ == sq;
      if (is_drag_over) {
        painter.fillRect(
            rect, is_legal_target ? kDragOverValidTint : kDragOverInvalidTint);
      }

      bool is_checked_king = (engine_.IsCheck() || engine_.IsCheckmate()) &&
                             engine_.FindKing(engine_.SideToMove()) == sq;
      if (is_checked_king) {
        painter.fillRect(
            rect, engine_.IsCheckmate() ? kCheckmateKingTint : kCheckKingTint);
      }

      if (invalid_target_square_ && *invalid_target_square_ == sq) {
        float elapsed =
            std::chrono::duration<float>(std::chrono::steady_clock::now() -
                                         invalid_target_started_at_)
                .count();
        if (elapsed < kInvalidFlashSeconds) {
          float t = elapsed / kInvalidFlashSeconds;
          QColor flash(210, 50, 50, static_cast<int>(255 * 0.55f * (1.0f - t)));
          painter.fillRect(rect, flash);
        }
      }

      auto piece = engine_.PieceAt(sq);
      bool is_own_turn_piece = piece && piece->color == engine_.SideToMove();
      bool has_moves = is_own_turn_piece && !engine_.GetLegalMoves(sq).empty();
      bool is_drag_source = drag_source_ && *drag_source_ == sq;
      bool is_selected = selected_square_ && *selected_square_ == sq;

      bool show_valid_highlight = false, show_invalid_highlight = false;
      if (is_drag_source) {
        if (!drag_over_square_) {
          show_valid_highlight = true;
        } else if (std::find(legal_targets_.begin(), legal_targets_.end(),
                             *drag_over_square_) != legal_targets_.end()) {
          show_valid_highlight = true;
        } else {
          show_invalid_highlight = true;
        }
      } else if (is_selected) {
        show_valid_highlight = has_moves;
        show_invalid_highlight = !has_moves;
      }

      QPointF center = rect.center();
      float piece_radius = square_size * 0.41f;

      if (show_valid_highlight) {
        painter.setPen(QPen(kSelectedRing, 3.0));
        painter.setBrush(kSelectedBg);
        painter.drawEllipse(center, piece_radius, piece_radius);
      } else if (show_invalid_highlight) {
        painter.setPen(QPen(kSelectedInvalidRing, 3.0));
        painter.setBrush(kSelectedInvalidBg);
        painter.drawEllipse(center, piece_radius, piece_radius);
      }
      painter.setBrush(Qt::NoBrush);

      if (piece && !(is_drag_source && dragging_)) {
        DrawPieceGlyph(painter,
                       GlyphFontForSquareSize(piece_font_, square_size), rect,
                       piece->Glyph(), PieceGlyphColor(piece->color));
      }

      if (last_move_badge_square_ && *last_move_badge_square_ == sq)
        PaintLastMoveBadge(painter, rect);

      if (is_legal_target) {
        if (piece) {
          painter.setPen(QPen(kCaptureHintRing, 4.0));
          painter.setBrush(Qt::NoBrush);
          float r = square_size * 0.45f;
          painter.drawEllipse(center, r, r);
        } else {
          painter.setPen(Qt::NoPen);
          painter.setBrush(kMoveHintDot);
          float r = square_size * 0.15f;
          painter.drawEllipse(center, r, r);
        }
      }
    }
  }

  painter.setFont(label_font_);
  painter.setPen(kLabelColor);
  for (int fi = 0; fi < 8; fi++) {
    int file = flipped ? 7 - fi : fi;
    QString text(QChar('a' + file));
    QRectF label_rect(board_origin.x() + fi * square_size,
                      board_origin.y() + board_size + 4, square_size,
                      file_label_height);
    painter.drawText(label_rect, Qt::AlignHCenter | Qt::AlignTop, text);
  }

  if (dragging_ && drag_source_) {
    auto piece = engine_.PieceAt(*drag_source_);
    if (piece) {
      QRectF drag_rect(drag_pos_.x() - square_size / 2,
                       drag_pos_.y() - square_size / 2, square_size,
                       square_size);
      DrawPieceGlyph(painter, GlyphFontForSquareSize(piece_font_, square_size),
                     drag_rect, piece->Glyph(), PieceGlyphColor(piece->color));
    }
  }

  // Plies alternate human/engine; the lighter, later arrows are painted first
  // so stronger ones stay on top.
  for (size_t i = analysis_arrows_.size(); i-- > 0;) {
    const size_t pair = i / 2;
    if (pair >= std::size(kAnalysisArrowAlpha)) continue;
    QColor color = i % 2 == 0 ? kAnalysisArrowHuman : kAnalysisArrowEngine;
    color.setAlphaF(kAnalysisArrowAlpha[pair]);
    PaintAnalysisArrow(painter, analysis_arrows_[i].first,
                       analysis_arrows_[i].second, color);
  }
}

void BoardWidget::mousePressEvent(QMouseEvent* event) {
  if (event->button() != Qt::LeftButton) return;

  auto sq = SquareAt(event->position());
  if (!sq) return;

  // Only a candidate: it becomes a drag once the mouse moves past the system
  // drag distance, and a click if it is released first.
  auto piece = engine_.PieceAt(*sq);
  if (piece && piece->color == engine_.SideToMove() && CanHumanInteract()) {
    press_candidate_ = sq;
    press_pos_ = event->pos();
  }
}

void BoardWidget::mouseMoveEvent(QMouseEvent* event) {
  if (dragging_) {
    drag_pos_ = event->pos();
    drag_over_square_ = SquareAt(event->position());
    update();
    return;
  }

  if (press_candidate_ && (event->buttons() & Qt::LeftButton)) {
    int dx = event->pos().x() - press_pos_.x();
    int dy = event->pos().y() - press_pos_.y();
    int threshold = QApplication::styleHints()->startDragDistance();
    if (dx * dx + dy * dy >= threshold * threshold) {
      OnDragStart(*press_candidate_);
      press_candidate_.reset();
      drag_pos_ = event->pos();
      update();
    }
  }
}

void BoardWidget::mouseReleaseEvent(QMouseEvent* event) {
  if (event->button() != Qt::LeftButton) return;

  if (dragging_) {
    auto sq = SquareAt(event->position());
    if (sq) {
      OnDrop(*sq);
    } else {
      OnDragEnd();
    }
    update();
    return;
  }

  if (press_candidate_) {
    Square clicked = *press_candidate_;
    press_candidate_.reset();
    OnSquareClicked(clicked);
    return;
  }

  auto sq = SquareAt(event->position());
  if (sq) OnSquareClicked(*sq);
}
}  // namespace chess
