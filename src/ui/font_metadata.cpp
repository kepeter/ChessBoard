/*
 * Copyright (c) 2026 Kornfeld Eliyahu Peter
 * License: MIT
 */

#include "ui/font_metadata.hpp"

#include <QFile>
#include <QVector>

namespace chess {
namespace {
// OpenType numbers are big-endian, so they are assembled byte by byte.
std::uint16_t ReadU16BE(const QByteArray& data, qsizetype offset) {
  return (static_cast<std::uint8_t>(data[offset]) << 8) |
         static_cast<std::uint8_t>(data[offset + 1]);
}

std::uint32_t ReadU32BE(const QByteArray& data, qsizetype offset) {
  return (static_cast<std::uint32_t>(ReadU16BE(data, offset)) << 16) |
         ReadU16BE(data, offset + 2);
}

// Platform 3 and platform 0 records are UTF-16BE; others (e.g. Mac Roman) are
// read as Latin-1, enough for ASCII license text.
QString DecodeNameRecord(const QByteArray& data, qsizetype offset, int length,
                         std::uint16_t platform_id) {
  if (platform_id == 3 || platform_id == 0) {
    QVector<char16_t> units(length / 2);
    for (int i = 0; i < length / 2; ++i) {
      units[i] = ReadU16BE(data, offset + i * 2);
    }
    return QString(reinterpret_cast<const QChar*>(units.constData()),
                   units.size());
  }
  return QString::fromLatin1(data.constData() + offset, length);
}

// Finds a name ID (0 = Copyright, 13 = License Description, 14 = License URL)
// in the "name" table, located via the sfnt table directory (16-byte entries:
// tag, offset, length).
QString FindNameString(const QByteArray& data, std::uint16_t wanted_name_id) {
  if (data.size() < 12) return QString();

  // The table directory starts at byte 12; the table count is at byte 4.
  std::uint16_t num_tables = ReadU16BE(data, 4);
  qint64 name_table_offset = -1;

  for (int i = 0; i < num_tables; ++i) {
    qsizetype entry_offset = 12 + i * 16;
    if (entry_offset + 16 > data.size()) break;

    QByteArray tag = data.mid(entry_offset, 4);
    if (tag == "name") {
      name_table_offset = ReadU32BE(data, entry_offset + 8);
      break;
    }
  }

  if (name_table_offset < 0 || name_table_offset + 6 > data.size())
    return QString();

  // "name" header: format (unused), record count, and the string storage
  // offset.
  std::uint16_t count = ReadU16BE(data, name_table_offset + 2);
  std::uint16_t string_offset_pos = ReadU16BE(data, name_table_offset + 4);
  qint64 records_start = name_table_offset + 6;
  qint64 strings_start = name_table_offset + string_offset_pos;

  // A name ID repeats per platform/language; prefer Windows US English, else
  // the first match.
  QString fallback;
  for (int i = 0; i < count; ++i) {
    qint64 record_offset = records_start + i * 12;
    if (record_offset + 12 > data.size()) break;

    std::uint16_t platform_id = ReadU16BE(data, record_offset);
    std::uint16_t encoding_id = ReadU16BE(data, record_offset + 2);
    std::uint16_t language_id = ReadU16BE(data, record_offset + 4);
    std::uint16_t name_id = ReadU16BE(data, record_offset + 6);
    std::uint16_t length = ReadU16BE(data, record_offset + 8);
    std::uint16_t string_offset = ReadU16BE(data, record_offset + 10);

    if (name_id != wanted_name_id) continue;

    qint64 abs_offset = strings_start + string_offset;
    if (abs_offset + length > data.size()) continue;

    QString text = DecodeNameRecord(data, abs_offset, length, platform_id);

    // 0x0409 is Windows US English; encodings 1 and 10 are UTF-16.
    bool is_preferred_windows_english =
        platform_id == 3 && (encoding_id == 1 || encoding_id == 10) &&
        language_id == 0x0409;
    if (is_preferred_windows_english) return text;
    if (fallback.isEmpty()) fallback = text;
  }

  return fallback;
}
}  // namespace

FontMetadata ReadFontMetadata(const QString& font_file_path) {
  FontMetadata metadata;

  QFile file(font_file_path);
  if (!file.open(QIODevice::ReadOnly)) return metadata;

  QByteArray data = file.readAll();
  metadata.copyright = FindNameString(data, 0);
  metadata.license_description = FindNameString(data, 13);
  metadata.license_url = FindNameString(data, 14);
  return metadata;
}
}  // namespace chess
