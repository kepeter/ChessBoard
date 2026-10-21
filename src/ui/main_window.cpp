/*
 * Copyright (c) 2026 Kornfeld Eliyahu Peter
 * License: MIT
 */

#include "ui/main_window.hpp"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include <QAction>
#include <QApplication>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMetaObject>
#include <QPointer>
#include <QPushButton>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QWidget>

#include "config/app_config.hpp"
#include "models/move.hpp"
#include "session/uci_move_parsing.hpp"
#include "ui/about_dialog.hpp"
#include "ui/analysis_panel.hpp"
#include "ui/board_widget.hpp"
#include "ui/evaluation_bar_widget.hpp"
#include "ui/game_over_dialog.hpp"
#include "ui/new_game_dialog.hpp"
#include "ui/promotion_dialog.hpp"
#include "ui/settings_dialog.hpp"
#include "ui/sidebar_widget.hpp"
#include "ui/strength_selector.hpp"

namespace chess {
MainWindow::MainWindow(ChessEngine& engine, UciEngine& uci_engine,
                       GameSession& session, QFont piece_font, QWidget* parent)
    : QMainWindow(parent),
      engine_(engine),
      uci_engine_(uci_engine),
      session_(session),
      piece_font_(std::move(piece_font)) {
  setWindowTitle("ChessBoard");

  auto* game_menu = menuBar()->addMenu("&Game");
  new_game_action_ = game_menu->addAction("&New Game");
  connect(new_game_action_, &QAction::triggered, this,
          &MainWindow::OnNewGameTriggered);

  stop_game_action_ = game_menu->addAction("S&top Game");
  connect(stop_game_action_, &QAction::triggered, this,
          &MainWindow::OnStopGameTriggered);

  settings_action_ = game_menu->addAction("&Settings...");
  connect(settings_action_, &QAction::triggered, this,
          &MainWindow::OnSettingsTriggered);

  game_menu->addSeparator();
  QAction* exit_action = game_menu->addAction("E&xit");
  exit_action->setShortcut(QKeySequence::Quit);
  connect(exit_action, &QAction::triggered, qApp, &QApplication::quit);

  auto* view_menu = menuBar()->addMenu("&View");
  QAction* eval_bar_action = view_menu->addAction("Show &Evaluation Bar");
  eval_bar_action->setCheckable(true);
  eval_bar_action->setChecked(AppConfig::Instance().ShowEvaluationBar());
  connect(eval_bar_action, &QAction::toggled, this,
          &MainWindow::OnShowEvaluationBarToggled);

  QAction* best_lines_action = view_menu->addAction("Show &Best Lines");
  best_lines_action->setCheckable(true);
  best_lines_action->setChecked(AppConfig::Instance().ShowBestLines());
  connect(best_lines_action, &QAction::toggled, this,
          &MainWindow::OnShowBestLinesToggled);

  QAction* analysis_arrows_action =
      view_menu->addAction("Show Analysis &Arrows");
  analysis_arrows_action->setCheckable(true);
  analysis_arrows_action->setChecked(
      AppConfig::Instance().ShowAnalysisArrows());
  connect(analysis_arrows_action, &QAction::toggled, this,
          &MainWindow::OnShowAnalysisArrowsToggled);

  // Sub-option of the arrows toggle; settable only while arrows are on.
  arrows_engine_move_action_ =
      view_menu->addAction("      Arrows During &Engine Move");
  arrows_engine_move_action_->setCheckable(true);
  show_arrows_during_engine_move_ =
      AppConfig::Instance().ShowArrowsDuringEngineMove();
  arrows_engine_move_action_->setChecked(show_arrows_during_engine_move_ &&
                                         analysis_arrows_action->isChecked());
  arrows_engine_move_action_->setEnabled(analysis_arrows_action->isChecked());
  connect(arrows_engine_move_action_, &QAction::toggled, this,
          &MainWindow::OnShowArrowsDuringEngineMoveToggled);

  QAction* sound_action = view_menu->addAction("Play Engine Move &Sound");
  sound_action->setCheckable(true);
  engine_move_sound_enabled_ = AppConfig::Instance().EngineMoveSound();
  sound_action->setChecked(engine_move_sound_enabled_);
  connect(sound_action, &QAction::toggled, this,
          &MainWindow::OnEngineMoveSoundToggled);

  QAction* classification_action =
      view_menu->addAction("Show Move &Classification");
  classification_action->setCheckable(true);
  classification_action->setChecked(
      AppConfig::Instance().ShowMoveClassification());
  connect(classification_action, &QAction::toggled, this,
          &MainWindow::OnShowMoveClassificationToggled);

  QAction* classification_board_action =
      view_menu->addAction("Show Classification on &Board");
  classification_board_action->setCheckable(true);
  classification_board_action->setChecked(
      AppConfig::Instance().ShowMoveClassificationOnBoard());
  connect(classification_board_action, &QAction::toggled, this,
          &MainWindow::OnShowMoveClassificationOnBoardToggled);

  auto* help_menu = menuBar()->addMenu("&Help");
  QAction* about_action = help_menu->addAction("&About");
  connect(about_action, &QAction::triggered, this,
          &MainWindow::OnAboutTriggered);

  board_widget_ = new BoardWidget(engine_, session_, piece_font_, this);
  sidebar_widget_ = new SidebarWidget(engine_, session_, piece_font_, this);
  analysis_panel_ = new AnalysisPanel(engine_, session_, this);
  ApplyAnalysisMultiPv();
  show_analysis_arrows_ = analysis_arrows_action->isChecked();
  analysis_panel_->SetShowEvaluationBar(eval_bar_action->isChecked());
  analysis_panel_->SetShowBestLines(best_lines_action->isChecked());
  analysis_panel_->SetShowClassification(classification_action->isChecked());
  show_classification_on_board_ = classification_board_action->isChecked();
  analysis_panel_->SetClassifyForBoardBadge(show_classification_on_board_);

  connect(board_widget_, &BoardWidget::promotionNeeded, this,
          &MainWindow::OnPromotionNeeded);
  connect(&session_, &GameSession::gameOverDetected, this,
          &MainWindow::OnGameOverDetected);
  connect(&session_, &GameSession::stateChanged, this,
          &MainWindow::UpdateGameActionsEnabled);
  connect(&session_, &GameSession::engineMoveRejected, this,
          [this](const QString& detail) {
            QMessageBox::critical(this, "Game stopped",
                                  "The game could not continue because the "
                                  "engine's move could not be applied:\n\n" +
                                      detail +
                                      "\n\nStart a new game to keep playing.");
          });
  connect(&session_, &GameSession::engineFailed, this,
          [this](const QString& reason) {
            QMessageBox box(QMessageBox::Warning, "Chess engine stopped",
                            reason +
                                "\n\nThe game is paused. Retry restarts the "
                                "engine and asks it to move again.",
                            QMessageBox::NoButton, this);
            QPushButton* retry =
                box.addButton("Retry", QMessageBox::AcceptRole);
            box.addButton("Give up", QMessageBox::RejectRole);
            box.setDefaultButton(retry);
            box.exec();
            if (box.clickedButton() == retry) session_.RetryEngineMove();
          });

  // The panel's widgets and the board arrows are separate toggles sharing one
  // session analysis flag, so it is their OR.
  auto update_analysis_enabled = [this]() {
    session_.SetAnalysisEnabled(analysis_panel_->WantsAnalysis() ||
                                show_analysis_arrows_);
  };
  connect(analysis_panel_, &AnalysisPanel::wantsAnalysisChanged, this,
          update_analysis_enabled);
  update_analysis_enabled();

  connect(&session_, &GameSession::analysisUpdated, this,
          &MainWindow::OnAnalysisUpdatedForArrows);
  connect(&session_, &GameSession::stateChanged, this,
          &MainWindow::OnSessionStateChangedForArrows);
  connect(&session_, &GameSession::moveMade, this,
          &MainWindow::OnSessionStateChangedForArrows);
  engine_move_sound_.setSource(
      QUrl::fromLocalFile(AssetsDirPath() + "/engine_move.wav"));
  connect(&session_, &GameSession::moveMade, this, [this](PieceColor by) {
    if (engine_move_sound_enabled_ && by != session_.PlayerColor())
      engine_move_sound_.play();
  });
  connect(&session_, &GameSession::gameStarted, this, [this]() {
    board_widget_->SetLastMoveBadge(std::nullopt, std::nullopt);
  });
  connect(analysis_panel_, &AnalysisPanel::moveClassifiedForBoard, this,
          &MainWindow::OnMoveClassifiedForBoard);

  setCentralWidget(new MainWindowCentral(board_widget_, sidebar_widget_,
                                         analysis_panel_, this));
  UpdateGameActionsEnabled();
}

void MainWindow::PromptNewGame() { OnNewGameTriggered(); }

// Engine startup is asynchronous, so the dialog opens from its callback. On
// failure the user is told why and sent to Settings; the flow ends there. The
// callback arrives on the worker thread and is marshalled to the main thread,
// guarded by a QPointer.
void MainWindow::OnNewGameTriggered() {
  if (new_game_prompt_pending_) return;
  new_game_prompt_pending_ = true;

  QPointer<MainWindow> self(this);
  uci_engine_.Start([self](std::optional<std::string> error) {
    QMetaObject::invokeMethod(
        qApp,
        [self, error]() {
          if (!self) return;
          self->new_game_prompt_pending_ = false;

          if (error) {
            QMessageBox::warning(
                self, "Chess engine not available",
                QString::fromStdString(*error) +
                    "\n\nThe computer can't play moves or analyze positions "
                    "until the engine path is fixed. "
                    "The Settings dialog will open next so you can set it.");
            self->OnSettingsTriggered();
            return;
          }

          self->OpenNewGameDialog();
        },
        Qt::QueuedConnection);
  });
}

void MainWindow::OpenNewGameDialog() {
  NewGameDialog dialog(uci_engine_, this);
  if (dialog.exec() != QDialog::Accepted) return;

  PieceColor player_color = dialog.SelectedPlayerColor();
  TimeControl tc = dialog.SelectedTimeControl();

  ApplyStrengthToEngine(uci_engine_, dialog.SelectedStrengthId());
  // Per-game overrides: applied to the engine only, never saved to AppConfig.
  uci_engine_.SetOption("Ponder", dialog.SelectedPonder() ? "true" : "false");
  uci_engine_.SetOption("UCI_Chess960",
                        dialog.SelectedChess960() ? "true" : "false");

  if (!session_.StartGame(Opposite(player_color), tc, dialog.SelectedChess960(),
                          dialog.SelectedStartingFen())) {
    QMessageBox::warning(this, "Invalid starting position",
                         "The starting position could not be loaded, so no new "
                         "game was started.");
  }
}

void MainWindow::OnStopGameTriggered() {
  auto reply = QMessageBox::question(
      this, "Stop game", "Stop this game? It will end with no result.",
      QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
  if (reply != QMessageBox::Yes) return;

  session_.Stop();
}

// New Game and Settings would interrupt the engine mid-game, so they're only
// available between games. A game paused by an engine error counts as between
// games.
void MainWindow::UpdateGameActionsEnabled() {
  const bool running = session_.IsGameStarted() && !session_.IsGameOver();
  stop_game_action_->setEnabled(running);

  const bool in_progress = running && !session_.HasEngineError();
  new_game_action_->setEnabled(!in_progress);
  settings_action_->setEnabled(!in_progress);
}

void MainWindow::OnShowEvaluationBarToggled(bool checked) {
  AppConfig::Instance().SetShowEvaluationBar(checked);
  analysis_panel_->SetShowEvaluationBar(checked);
}

void MainWindow::OnShowBestLinesToggled(bool checked) {
  AppConfig::Instance().SetShowBestLines(checked);
  analysis_panel_->SetShowBestLines(checked);
}

void MainWindow::OnShowAnalysisArrowsToggled(bool checked) {
  AppConfig::Instance().SetShowAnalysisArrows(checked);
  show_analysis_arrows_ = checked;
  session_.SetAnalysisEnabled(analysis_panel_->WantsAnalysis() ||
                              show_analysis_arrows_);

  // Shown off and locked while arrows are off; its saved value returns with
  // them.
  QSignalBlocker blocker(arrows_engine_move_action_);
  arrows_engine_move_action_->setEnabled(checked);
  arrows_engine_move_action_->setChecked(checked &&
                                         show_arrows_during_engine_move_);

  if (!checked) board_widget_->SetAnalysisArrows({});
}

void MainWindow::OnShowArrowsDuringEngineMoveToggled(bool checked) {
  AppConfig::Instance().SetShowArrowsDuringEngineMove(checked);
  show_arrows_during_engine_move_ = checked;
  if (!checked && session_.IsEnginesTurn())
    board_widget_->SetAnalysisArrows({});
}

void MainWindow::OnEngineMoveSoundToggled(bool checked) {
  AppConfig::Instance().SetEngineMoveSound(checked);
  engine_move_sound_enabled_ = checked;
}

void MainWindow::OnShowMoveClassificationToggled(bool checked) {
  AppConfig::Instance().SetShowMoveClassification(checked);
  analysis_panel_->SetShowClassification(checked);
}

void MainWindow::OnShowMoveClassificationOnBoardToggled(bool checked) {
  AppConfig::Instance().SetShowMoveClassificationOnBoard(checked);
  show_classification_on_board_ = checked;
  analysis_panel_->SetClassifyForBoardBadge(checked);

  if (!checked) board_widget_->SetLastMoveBadge(std::nullopt, std::nullopt);
}

// The badge sits on the human's last move and stays through the engine's reply
// until the human moves again or a new game starts.
void MainWindow::OnMoveClassifiedForBoard(MoveQuality quality) {
  if (!show_classification_on_board_ || engine_.History().empty()) return;

  board_widget_->SetLastMoveBadge(
      ChessEngine::LandingSquare(engine_.History().back()), quality);
}

// Clear arrows for a position that is no longer on the board rather than leave
// them stale until new analysis arrives.
void MainWindow::OnSessionStateChangedForArrows() {
  std::string fen = engine_.ToFen();
  if (fen == last_arrow_position_fen_) return;
  last_arrow_position_fen_ = fen;

  // The engine played the predicted reply, so the drawn arrows still apply.
  if (session_.AnalysisFen() == fen) return;
  if (show_analysis_arrows_) board_widget_->SetAnalysisArrows({});
}

namespace {
// Arrows point at the squares the human drags to reproduce the move; for a
// castle that is the rook's square (the app's input convention), not the king's
// landing square.
std::optional<std::pair<Square, Square>> ApplyUciMoveInputSquares(
    ChessEngine& engine, const std::string& uci_move) {
  auto applied = ApplyUciMove(engine, uci_move);
  if (!applied) return std::nullopt;
  return std::make_pair(applied->from, applied->to);
}
}  // namespace

// Only the top line drives the arrows; its first six plies are replayed on a
// scratch engine to get their squares.
void MainWindow::OnAnalysisUpdatedForArrows(AnalysisInfo info) {
  if (!show_analysis_arrows_ || info.multipv != 1) return;
  if (!show_arrows_during_engine_move_ && session_.IsEnginesTurn()) return;

  ChessEngine scratch;
  // Arrows belong to the analysed position, which is the human's next position
  // while the engine thinks.
  if (!scratch.LoadFen(session_.AnalysisFen())) return;

  constexpr size_t kMaxArrows = 6;
  std::vector<std::pair<Square, Square>> arrows;
  for (size_t i = 0; i < info.pv_uci_moves.size() && i < kMaxArrows; ++i) {
    auto squares = ApplyUciMoveInputSquares(scratch, info.pv_uci_moves[i]);
    if (!squares) break;
    arrows.push_back(*squares);
  }

  board_widget_->SetAnalysisArrows(std::move(arrows));
}

void MainWindow::OnAboutTriggered() {
  AboutDialog dialog(this);

  dialog.exec();
}

void MainWindow::OnSettingsTriggered() {
  SettingsDialog dialog(uci_engine_, this);
  connect(&dialog, &SettingsDialog::boardAppearanceChanged, board_widget_,
          [this]() { board_widget_->update(); });
  connect(&dialog, &SettingsDialog::pieceSetChanged, this,
          &MainWindow::OnPieceSetChanged);

  dialog.exec();
  ApplyAnalysisMultiPv();
}

// The line count is the Settings MultiPV value, checked against the engine's
// range (3 if unset or invalid).
void MainWindow::ApplyAnalysisMultiPv() {
  QPointer<MainWindow> self(this);
  uci_engine_.RequestOptions([self](std::vector<UciOption> available) {
    QMetaObject::invokeMethod(
        qApp,
        [self, available]() {
          if (!self) return;
          int lines = 3;
          auto it = std::find_if(
              available.begin(), available.end(),
              [](const UciOption& o) { return o.name == "MultiPV"; });
          if (it != available.end() && it->type == UciOptionType::kSpin) {
            QString saved = AppConfig::Instance().UciOptionOverrides().value(
                "MultiPV", QString::fromStdString(it->default_value));
            lines = ValidSpinValue(*it, saved.toStdString());
          }
          if (lines != self->session_.AnalysisMultiPv())
            self->session_.SetAnalysisMultiPv(lines);
        },
        Qt::QueuedConnection);
  });
}

void MainWindow::OnPieceSetChanged() {
  piece_font_ = LoadPieceFont(AppConfig::Instance().Set());
  board_widget_->SetPieceFont(piece_font_);
  sidebar_widget_->SetPieceFont(piece_font_);
}

void MainWindow::OnPromotionNeeded(Square from, Square to, PieceColor color) {
  PromotionDialog dialog(color, piece_font_,
                         board_widget_->SquareCenterGlobal(to), this);

  if (dialog.exec() == QDialog::Accepted) {
    session_.TryApplyMove(from, to, dialog.SelectedType());
  }
}

namespace {
std::string ColorName(PieceColor color) {
  return color == PieceColor::kWhite ? "White" : "Black";
}
}  // namespace

void MainWindow::OnGameOverDetected() {
  if (session_.Stopped())
    return;  // the user ended it deliberately; no result to announce

  std::string message;

  // Checkmate and flag fall take priority over GetDrawReason() in the message.
  if (engine_.IsCheckmate()) {
    PieceColor winner = Opposite(engine_.SideToMove());
    message = ColorName(winner) + " wins by checkmate";
  } else if (session_.Flagged()) {
    PieceColor winner = Opposite(*session_.Flagged());
    message = ColorName(winner) + " wins on time";
  } else if (engine_.IsStalemate()) {
    message = "Draw by stalemate";
  } else {
    switch (engine_.GetDrawReason()) {
      case DrawReason::kFiftyMoveRule:
        message = "Draw by fifty-move rule";
        break;
      case DrawReason::kThreefoldRepetition:
        message = "Draw by threefold repetition";
        break;
      case DrawReason::kInsufficientMaterial:
        message = "Draw by insufficient material";
        break;
      default:
        message = "Game over";
        break;
    }
  }

  GameOverDialog dialog(QString::fromStdString(message), this);

  dialog.exec();
}

MainWindowCentral::MainWindowCentral(BoardWidget* board, SidebarWidget* sidebar,
                                     AnalysisPanel* analysis_panel,
                                     QWidget* parent)
    : QWidget(parent),
      board_(board),
      sidebar_(sidebar),
      analysis_panel_(analysis_panel) {
  board_->setParent(this);
  sidebar_->setParent(this);
  analysis_panel_->setParent(this);
}

void MainWindowCentral::resizeEvent(QResizeEvent* event) {
  QWidget::resizeEvent(event);

  int total_width = width();
  int total_height = height();

  // Equal margins left and right, sized for the wider of the sidebar and
  // analysis panel; the narrower side is aligned within its margin, not
  // stretched.
  int margin = std::max(sidebar_->minimumWidth(),
                        analysis_panel_->EffectiveMinimumWidth());

  int board_width =
      std::max(std::min(total_height, total_width - 2 * margin), 0);
  margin = (total_width - board_width) / 2;

  board_->setGeometry(margin, 0, board_width, total_height);
  sidebar_->setGeometry(total_width - margin, 0, margin, total_height);
  analysis_panel_->setGeometry(0, 0, margin, total_height);

  // The eval bar matches the board's 8x8 grid rect (not the label gutters).
  // board_'s geometry must be set first, since GridRect() depends on it.
  QRectF grid_rect = board_->GridRect();
  EvaluationBarWidget* eval_bar = analysis_panel_->EvalBar();
  int bar_x = analysis_panel_->width() - AnalysisPanel::kBarWidth;
  eval_bar->setGeometry(bar_x, static_cast<int>(grid_rect.top()),
                        AnalysisPanel::kBarWidth,
                        static_cast<int>(grid_rect.height()));
}
}  // namespace chess
