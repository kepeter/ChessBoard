/*
 * Copyright (c) 2026 Kornfeld Eliyahu Peter
 * License: MIT
 */

#ifndef CHESSBOARD_UI_MAIN_WINDOW_HPP_
#define CHESSBOARD_UI_MAIN_WINDOW_HPP_

#include <string>

#include <QFont>
#include <QMainWindow>
#include <QSoundEffect>

#include "engine/chess_engine.hpp"
#include "models/square.hpp"
#include "session/game_session.hpp"
#include "session/move_classifier.hpp"
#include "uci/uci_engine.hpp"

class QAction;
class QResizeEvent;

namespace chess {
class BoardWidget;
class SidebarWidget;
class AnalysisPanel;

class MainWindow : public QMainWindow {
  Q_OBJECT

 public:
  MainWindow(ChessEngine& engine, UciEngine& uci_engine, GameSession& session,
             QFont piece_font, QWidget* parent = nullptr);

  // Lets main() show the New Game dialog on startup.
  void PromptNewGame();

 private slots:
  void OnNewGameTriggered();
  void OnStopGameTriggered();
  void OnAboutTriggered();
  void OnSettingsTriggered();
  void ApplyAnalysisMultiPv();
  void OnPieceSetChanged();
  void OnPromotionNeeded(Square from, Square to, PieceColor color);
  void OnGameOverDetected();
  void UpdateGameActionsEnabled();
  void OnShowEvaluationBarToggled(bool checked);
  void OnShowBestLinesToggled(bool checked);
  void OnShowAnalysisArrowsToggled(bool checked);
  void OnShowArrowsDuringEngineMoveToggled(bool checked);
  void OnEngineMoveSoundToggled(bool checked);
  void OnShowMoveClassificationToggled(bool checked);
  void OnShowMoveClassificationOnBoardToggled(bool checked);
  void OnAnalysisUpdatedForArrows(AnalysisInfo info);
  void OnSessionStateChangedForArrows();
  void OnMoveClassifiedForBoard(MoveQuality quality);

 private:
  ChessEngine& engine_;
  UciEngine& uci_engine_;
  GameSession& session_;
  QSoundEffect engine_move_sound_;
  QFont piece_font_;

  BoardWidget* board_widget_;
  SidebarWidget* sidebar_widget_;
  AnalysisPanel* analysis_panel_;
  QAction* new_game_action_;
  QAction* stop_game_action_;
  QAction* settings_action_;

  // Second half of OnNewGameTriggered, run once the engine has started.
  void OpenNewGameDialog();
  // True while an engine start for New Game is pending, so a second click
  // doesn't queue a second prompt.
  bool new_game_prompt_pending_ = false;

  bool show_analysis_arrows_ = false;
  bool show_arrows_during_engine_move_ = false;
  bool engine_move_sound_enabled_ = false;
  QAction* arrows_engine_move_action_ = nullptr;
  std::string last_arrow_position_fen_;
  bool show_classification_on_board_ = false;
};

// Centers the board horizontally in the window; the sidebar fills the space to
// its right and the analysis panel the space to its left.
class MainWindowCentral : public QWidget {
 public:
  MainWindowCentral(BoardWidget* board, SidebarWidget* sidebar,
                    AnalysisPanel* analysis_panel, QWidget* parent = nullptr);

 protected:
  void resizeEvent(QResizeEvent* event) override;

 private:
  BoardWidget* board_;
  SidebarWidget* sidebar_;
  AnalysisPanel* analysis_panel_;
};
}  // namespace chess

#endif  // CHESSBOARD_UI_MAIN_WINDOW_HPP_
