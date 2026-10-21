/*
 * Copyright (c) 2026 Kornfeld Eliyahu Peter
 * License: MIT
 */

#include "ui/about_dialog.hpp"

#include <QFrame>
#include <QLabel>
#include <QLibraryInfo>
#include <QVBoxLayout>

#include "config/app_config.hpp"
#include "ui/clickable_row.hpp"
#include "ui/license_dialog.hpp"

namespace chess {
namespace {
void FillRow(QFrame* frame, const QString& text) {
  frame->setFrameStyle(QFrame::StyledPanel | QFrame::Plain);
  auto* layout = new QVBoxLayout(frame);
  layout->setContentsMargins(14, 10, 14, 10);
  layout->addWidget(new QLabel(text, frame));
}

QWidget* MakeRow(const QString& text, QWidget* parent) {
  auto* frame = new QFrame(parent);
  FillRow(frame, text);
  return frame;
}

void SetMuted(QLabel* label) {
  QPalette palette = label->palette();
  palette.setColor(QPalette::WindowText,
                   palette.color(QPalette::PlaceholderText));
  label->setPalette(palette);
}

// Clickable row opening a LicenseDialog whose text is read from the font file
// itself, so it can't drift from the bundled font.
QWidget* MakeFontRow(const QString& font_display_name,
                     const QString& font_file_name, QWidget* parent) {
  auto* frame = new ClickableRow(parent);
  FillRow(frame, font_display_name);
  frame->setToolTip("Click for details...");

  QString font_file_path = AssetsDirPath() + "/" + font_file_name;

  // frame->window() anchors the dialog to the About dialog so it centers
  // correctly.
  QObject::connect(frame, &ClickableRow::clicked, frame,
                   [font_display_name, font_file_path, frame]() {
                     LicenseDialog dialog(font_display_name, font_file_path,
                                          frame->window());
                     dialog.exec();
                   });

  return frame;
}
}  // namespace

AboutDialog::AboutDialog(QWidget* parent) : QDialog(parent) {
  setWindowTitle("About");
  setModal(true);

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(24, 24, 24, 24);

  auto* title = new QLabel("ChessBoard", this);
  QFont title_font = title->font();
  title_font.setPointSize(title_font.pointSize() + 8);
  title_font.setBold(true);
  title->setFont(title_font);
  title->setAlignment(Qt::AlignHCenter);
  layout->addWidget(title);

  // CHESSBOARD_VERSION is injected by CMake from the project version.
  auto* version = new QLabel("Version " CHESSBOARD_VERSION, this);
  version->setAlignment(Qt::AlignHCenter);
  SetMuted(version);
  layout->addWidget(version);

  auto* subtitle =
      new QLabel("A desktop chess board with UCI engine support.", this);
  subtitle->setAlignment(Qt::AlignHCenter);
  subtitle->setWordWrap(true);
  SetMuted(subtitle);
  layout->addWidget(subtitle);
  layout->addSpacing(12);

  auto* libraries_label = new QLabel("Libraries", this);
  libraries_label->setEnabled(false);
  layout->addWidget(libraries_label);
  // QLibraryInfo::version() is the Qt version linked at runtime, which may be
  // newer than the minimum required.
  layout->addWidget(MakeRow(
      "Qt " + QLibraryInfo::version().toString() + " (LGPL v3 License)", this));
  layout->addSpacing(8);

  auto* assets_label = new QLabel("Assets", this);
  assets_label->setEnabled(false);
  layout->addWidget(assets_label);
  layout->addWidget(MakeFontRow("DejaVu Sans", "DejaVuSans.ttf", this));
  layout->addWidget(MakeFontRow("FreeSerif", "FreeSerif.otf", this));
  layout->addWidget(MakeFontRow("Nishiki-teki", "NishikiTeki.ttf", this));
  layout->addWidget(
      MakeFontRow("Noto Sans Symbols 2", "NotoSansSymbols2.ttf", this));
  layout->addWidget(MakeFontRow("STIX Two Math", "STIXTwoMath.otf", this));

  adjustSize();

  setFixedWidth(geometry().size().width() + 200);
  setFixedHeight(geometry().size().height());
}
}  // namespace chess
