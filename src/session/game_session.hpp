/*
 * Copyright (c) 2026 Kornfeld Eliyahu Peter
 * License: MIT
 */

#ifndef CHESSBOARD_SESSION_GAME_SESSION_HPP_
#define CHESSBOARD_SESSION_GAME_SESSION_HPP_

#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include <QObject>
#include <QString>
#include <QTimer>

#include "engine/chess_engine.hpp"
#include "models/piece.hpp"
#include "models/time_control.hpp"
#include "session/game_clock.hpp"
#include "uci/uci_engine.hpp"

namespace chess {
// Coordinates a running game: owns the clock, decides when it is the UCI
// engine's turn and requests its move, and emits the signals the UI listens to.
// ChessEngine knows nothing of clocks or which side is the computer.
class GameSession : public QObject {
  Q_OBJECT

 public:
  GameSession(ChessEngine& engine, UciEngine& uci_engine,
              QObject* parent = nullptr);

  bool IsEngineThinking() const { return is_engine_thinking_; }

  PieceColor PlayerColor() const { return Opposite(engine_color_); }

  std::optional<PieceColor> Flagged() const { return clock_.Flagged(); }
  bool Stopped() const { return stopped_; }
  std::chrono::milliseconds WhiteRemaining() const {
    return clock_.WhiteRemaining();
  }
  std::chrono::milliseconds BlackRemaining() const {
    return clock_.BlackRemaining();
  }
  PieceColor SideToMove() const { return engine_.SideToMove(); }

  // The position analysis updates refer to: the next human position while the
  // engine is still to move.
  const std::string& AnalysisFen() const { return analysis_fen_; }
  // Read from the FEN's side-to-move field.
  PieceColor AnalysisSideToMove() const {
    return analysis_fen_.find(" b ") != std::string::npos ? PieceColor::kBlack
                                                          : PieceColor::kWhite;
  }

  // Wall-clock time of each half-move, in the order of the engine's History().
  const std::vector<std::chrono::milliseconds>& MoveTimes() const {
    return move_times_;
  }

  bool IsGameOver() const;
  bool IsEnginesTurn() const { return engine_color_ == engine_.SideToMove(); }
  bool IsGameStarted() const { return game_started_; }

  // Set when ChessEngine rejects the engine's reported move (the two have
  // diverged). The clock stops and no further engine moves are requested until
  // a new game.
  bool HasEngineError() const { return has_engine_error_; }

  // With starting_fen the game begins there (Chess960 is detected from it and
  // the chess960 flag is ignored). Returns false, leaving the current game
  // untouched, if the FEN is invalid.
  bool StartGame(PieceColor engine_color, TimeControl time_control,
                 bool chess960 = false,
                 const std::optional<std::string>& starting_fen = std::nullopt);

  void NotifyMoveMade(PieceColor moved_by);

  // Ends the game with no result. UCI has no draw or resignation, so this is
  // the human's only way out before a natural ending. No-op if no game is
  // running.
  void Stop();

  // Clears the engine-error state and re-requests the engine's move; UciEngine
  // restarts a dead engine on that request. No-op unless an error is pending.
  void RetryEngineMove();

  bool TryApplyMove(Square from, Square to,
                    std::optional<PieceType> promotion = std::nullopt);

  // Turns background analysis on or off. It runs only when it is not the
  // engine's turn (or on the predicted position, see UpdateAnalysis) and
  // follows the position automatically.
  void SetAnalysisEnabled(bool enabled);
  int AnalysisMultiPv() const { return analysis_multipv_; }
  void SetAnalysisMultiPv(int multipv);

 signals:
  void stateChanged();
  // Emitted at the start of StartGame(), before any move, so per-game state can
  // reset.
  void gameStarted();
  void moveMade(PieceColor by);
  void engineThinkingChanged(bool thinking);

  // Emitted on the main thread; may fire repeatedly per position as the search
  // deepens. Consumers key by AnalysisInfo::multipv.
  void analysisUpdated(AnalysisInfo info);

  // Emitted once per human move when the engine's reply search has a reliable
  // score for the position after it (or when the search ends), from the mover's
  // perspective. nullopt if there was no usable score.
  void postMoveEvalReady(PieceColor mover, std::optional<int> score_cp,
                         std::optional<int> score_mate);

  void gameOverDetected();

  // The engine's move couldn't be applied; `detail` names the move and
  // position.
  void engineMoveRejected(QString detail);

  // The engine produced no move (exited, timed out or failed to start). The
  // game pauses with the clock frozen until RetryEngineMove() or a new game.
  void engineFailed(QString reason);

 private slots:
  void OnClockTick();

 private:
  ChessEngine& engine_;
  UciEngine& uci_engine_;
  GameClock clock_;
  QTimer clock_timer_;
  std::vector<std::chrono::milliseconds> move_times_;
  std::chrono::steady_clock::time_point turn_started_at_;

  PieceColor engine_color_ = PieceColor::kBlack;
  bool is_engine_thinking_ = false;
  // Makes gameOverDetected() fire once, on the transition.
  bool was_game_over_ = false;
  bool game_started_ = false;
  bool has_engine_error_ = false;
  bool stopped_ = false;
  // Set when the human moves with analysis on; the engine's next search
  // evaluates that move.
  std::optional<PieceColor> move_to_classify_;

  // Bumped by StartGame. Async callbacks capture it and are dropped if it
  // changed, so replies from a previous game never apply to this one.
  int request_generation_ = 0;

  bool analysis_enabled_ = false;
  int analysis_multipv_ = 3;
  // Position the analysis was last started on; later updates for other
  // positions are dropped.
  std::string analysis_fen_;
  // The engine's expected reply to predicted_for_fen_, as a FEN (empty if
  // none).
  std::string predicted_for_fen_;
  std::string predicted_fen_;

  void RaiseChange();
  void CheckGameOverTransition();
  void MaybeTriggerEngineMove();
  void RequestEngineMove();
  void ApplyEngineMove(std::optional<std::string> uci_move,
                       std::optional<std::string> ponder_move);
  void StartPondering(const std::string& ponder_move);

  void RecordMoveTime();
  void UpdateAnalysis();
  void ClearEngineThinking();
  void OnPredictionInfo(const std::string& fen, const AnalysisInfo& info);
};
}  // namespace chess

#endif  // CHESSBOARD_SESSION_GAME_SESSION_HPP_
