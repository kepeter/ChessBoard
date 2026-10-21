/*
 * Copyright (c) 2026 Kornfeld Eliyahu Peter
 * License: MIT
 */

#ifndef CHESSBOARD_UI_FONT_METADATA_HPP_
#define CHESSBOARD_UI_FONT_METADATA_HPP_

#include <QString>

namespace chess {
struct FontMetadata {
  QString copyright;
  QString license_description;
  QString license_url;
};

// Reads copyright/license text from the font's own OpenType "name" table, so it
// always matches the bundled font.
FontMetadata ReadFontMetadata(const QString& font_file_path);
}  // namespace chess

#endif  // CHESSBOARD_UI_FONT_METADATA_HPP_
