/*
 * Copyright (c) 2026 Kornfeld Eliyahu Peter
 * License: MIT
 */

#include <algorithm>
#include <vector>

#include <QApplication>
#include <QFont>
#include <QMap>
#include <QMetaObject>
#include <QString>
#include <QTimer>

#include "config/app_config.hpp"
#include "engine/chess_engine.hpp"
#include "session/game_session.hpp"
#include "uci/uci_engine.hpp"
#include "ui/main_window.hpp"

int main(int argc, char** argv) {
  QCoreApplication::setApplicationName("ChessBoard");
  QApplication app(argc, argv);

  QFont piece_font = chess::LoadPieceFont(chess::AppConfig::Instance().Set());

  chess::ChessEngine engine;
  chess::UciEngine uci_engine(
      chess::AppConfig::Instance().EnginePath().toStdString());

  // Option ranges are known only after the engine starts; then invalid saved
  // values are dropped and the rest sent.
  chess::UciEngine* engine_ptr = &uci_engine;
  uci_engine.RequestOptions([engine_ptr](
                                std::vector<chess::UciOption> available) {
    QMetaObject::invokeMethod(
        qApp,
        [engine_ptr, available]() {
          chess::AppConfig& config = chess::AppConfig::Instance();
          config.RemoveInvalidUciOptionOverrides(
              [&available](const QString& name, const QString& value) {
                auto option =
                    std::find_if(available.begin(), available.end(),
                                 [&](const chess::UciOption& o) {
                                   return o.name == name.toStdString();
                                 });
                return option == available.end() ||
                       option->type != chess::UciOptionType::kSpin ||
                       chess::IsValidSpinValue(*option, value.toStdString());
              });
          QMap<QString, QString> overrides = config.UciOptionOverrides();
          for (auto it = overrides.cbegin(); it != overrides.cend(); ++it) {
            engine_ptr->SetOption(it.key().toStdString(),
                                  it.value().toStdString());
          }
        },
        Qt::QueuedConnection);
  });
  chess::GameSession session(engine, uci_engine);
  chess::MainWindow window(engine, uci_engine, session, piece_font);

  window.resize(1024, 768);
  window.showMaximized();

  // Deferred so the main window is shown first
  // and the dialog appears on top of it.
  QTimer::singleShot(0, &window, &chess::MainWindow::PromptNewGame);

  return app.exec();
}
