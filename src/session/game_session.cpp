/*
 * Copyright (c) 2026 Kornfeld Eliyahu Peter
 * License: MIT
 */

#include "session/game_session.hpp"

#include <string>
#include <utility>

#include <QCoreApplication>
#include <QMetaObject>
#include <QPointer>

#include "session/uci_move_parsing.hpp"

namespace chess {
GameSession::GameSession(ChessEngine& engine, UciEngine& uci_engine,
                         QObject* parent)
    : QObject(parent), engine_(engine), uci_engine_(uci_engine) {
  connect(&clock_timer_, &QTimer::timeout, this, &GameSession::OnClockTick);
  // About 15 Hz keeps the displayed tenths of a second fresh.
  clock_timer_.start(66);
}

void GameSession::RaiseChange() { emit stateChanged(); }

bool GameSession::IsGameOver() const {
  return engine_.IsCheckmate() || engine_.IsStalemate() || engine_.IsDraw() ||
         Flagged().has_value() || stopped_;
}

void GameSession::CheckGameOverTransition() {
  bool is_over = IsGameOver();
  bool newly_over = is_over && !was_game_over_;

  // Set before emitting: the connected slot opens a modal dialog that re-enters
  // the event loop, where the clock timer could fire and emit a second time.
  was_game_over_ = is_over;

  if (!newly_over) return;

  // The game may end while the engine is searching. Stop that search and clear
  // the thinking flag now; the late reply is discarded by ApplyEngineMove.
  ClearEngineThinking();
  // Also ends a ponder search.
  uci_engine_.AbortSearch();

  emit gameOverDetected();
}

void GameSession::ClearEngineThinking() {
  if (!is_engine_thinking_) return;
  is_engine_thinking_ = false;
  emit engineThinkingChanged(false);
}

bool GameSession::StartGame(PieceColor engine_color, TimeControl time_control,
                            bool chess960,
                            const std::optional<std::string>& starting_fen) {
  // Validated first so a rejected FEN leaves the current game untouched.
  if (starting_fen) {
    ChessEngine scratch;
    if (!scratch.LoadFen(*starting_fen)) return false;
  }

  engine_color_ = engine_color;
  was_game_over_ = false;
  game_started_ = true;
  has_engine_error_ = false;
  stopped_ = false;
  move_to_classify_.reset();
  move_times_.clear();
  predicted_for_fen_.clear();
  predicted_fen_.clear();
  turn_started_at_ = std::chrono::steady_clock::now();

  // Invalidates replies still in flight from the previous game and cuts that
  // search short so this game's first request isn't queued behind it.
  request_generation_++;
  ClearEngineThinking();
  uci_engine_.AbortSearch();

  if (starting_fen) {
    engine_.LoadFen(*starting_fen);
  } else {
    engine_.Reset(chess960);
  }
  clock_.Start(time_control);

  emit gameStarted();
  RaiseChange();
  MaybeTriggerEngineMove();
  UpdateAnalysis();
  return true;
}

// Time since the previous move (or the game start).
void GameSession::RecordMoveTime() {
  const auto now = std::chrono::steady_clock::now();
  move_times_.push_back(std::chrono::duration_cast<std::chrono::milliseconds>(
      now - turn_started_at_));
  turn_started_at_ = now;
}

void GameSession::NotifyMoveMade(PieceColor moved_by) {
  RecordMoveTime();
  clock_.EndTurn(moved_by);
  emit moveMade(moved_by);
  CheckGameOverTransition();

  // The engine's own move search evaluates this move (see RequestEngineMove).
  if (moved_by == PlayerColor() && analysis_enabled_ && !IsGameOver() &&
      !has_engine_error_) {
    move_to_classify_ = moved_by;
  }
  MaybeTriggerEngineMove();

  UpdateAnalysis();
}

void GameSession::Stop() {
  if (!game_started_ || IsGameOver()) return;

  stopped_ = true;
  CheckGameOverTransition();
  RaiseChange();
  UpdateAnalysis();
}

void GameSession::RetryEngineMove() {
  if (!has_engine_error_) return;
  has_engine_error_ = false;
  RaiseChange();
  MaybeTriggerEngineMove();
  UpdateAnalysis();
}

void GameSession::MaybeTriggerEngineMove() {
  if (has_engine_error_ || !IsEnginesTurn() || IsGameOver()) return;
  RequestEngineMove();
}

void GameSession::RequestEngineMove() {
  is_engine_thinking_ = true;
  emit engineThinkingChanged(true);
  RaiseChange();

  std::string fen = engine_.ToFen();
  // The callback runs on the worker thread; ApplyEngineMove must run on the
  // main thread, so it is queued there.
  int generation = request_generation_;
  QPointer<GameSession> self(this);
  const std::optional<PieceColor> mover = move_to_classify_;
  move_to_classify_.reset();
  const int increment_ms = static_cast<int>(clock_.Increment().count());
  uci_engine_.RequestBestMoveAsync(
      fen,
      {static_cast<int>(clock_.WhiteRemaining().count()),
       static_cast<int>(clock_.BlackRemaining().count()), increment_ms,
       increment_ms},
      [self, generation, fen, mover](std::optional<AnalysisInfo> info) {
        if (!mover) return;
        QMetaObject::invokeMethod(
            qApp,
            [self, generation, fen, mover, info]() {
              if (!self || generation != self->request_generation_) return;

              // The engine searched the position after the human's move, so its
              // side-to-move score is negated into the mover's perspective.
              if (self->IsGameOver() || fen != self->engine_.ToFen()) return;
              std::optional<int> cp, mate;
              if (info) {
                if (info->score_cp) cp = -*info->score_cp;
                if (info->score_mate) mate = -*info->score_mate;
              }
              emit self->postMoveEvalReady(*mover, cp, mate);
            },
            Qt::QueuedConnection);
      },
      [self, generation](std::optional<std::string> uci_move,
                         std::optional<std::string> ponder_move) {
        QMetaObject::invokeMethod(
            qApp,
            [self, generation, uci_move, ponder_move]() {
              // Reply for a replaced game.
              if (!self || generation != self->request_generation_) return;
              self->ApplyEngineMove(uci_move, ponder_move);
            },
            Qt::QueuedConnection);
      });
}

void GameSession::SetAnalysisEnabled(bool enabled) {
  analysis_enabled_ = enabled;
  UpdateAnalysis();
  RaiseChange();
}

void GameSession::SetAnalysisMultiPv(int multipv) {
  analysis_multipv_ = multipv;
  // A running analysis must restart to use the new MultiPV count.
  if (analysis_enabled_) UpdateAnalysis();
}

// Starts, restarts or stops analysis to match the current position and turn.
// No-op if nothing relevant changed.
void GameSession::UpdateAnalysis() {
  bool should_run =
      analysis_enabled_ && game_started_ && !IsGameOver() && !has_engine_error_;

  if (!should_run) {
    uci_engine_.StopAnalysis();
    analysis_fen_.clear();
    return;
  }

  const std::string current_fen = engine_.ToFen();
  std::string target_fen = current_fen;
  bool predicting = false;

  // While the engine is to move there is no human position yet: search the
  // current position briefly to learn the engine's likely reply, then analyse
  // the position after it. If the engine plays it, that analysis carries on.
  if (IsEnginesTurn()) {
    if (predicted_for_fen_ == current_fen) {
      if (predicted_fen_.empty()) {
        uci_engine_.StopAnalysis();
        analysis_fen_.clear();
        return;
      }
      target_fen = predicted_fen_;
    } else {
      predicting = true;
    }
  }

  if (target_fen == analysis_fen_)
    return;  // Already analyzing this exact position.

  analysis_fen_ = target_fen;
  QPointer<GameSession> self(this);
  uci_engine_.StartAnalysis(
      target_fen, analysis_multipv_,
      [self, target_fen, predicting](AnalysisInfo info) {
        QMetaObject::invokeMethod(
            qApp,
            [self, target_fen, predicting, info = std::move(info)]() {
              // Drop updates for a position analysis has moved on from.
              if (!self || target_fen != self->analysis_fen_) return;
              if (predicting) {
                self->OnPredictionInfo(target_fen, info);
                return;
              }
              emit self->analysisUpdated(info);
            },
            Qt::QueuedConnection);
      });
}

void GameSession::OnPredictionInfo(const std::string& fen,
                                   const AnalysisInfo& info) {
  // Deep enough that the first PV move is usually the engine's real choice.
  constexpr int kPredictionDepth = 12;
  if (info.multipv != 1 || info.depth < kPredictionDepth ||
      info.pv_uci_moves.empty())
    return;
  if (predicted_for_fen_ == fen) return;

  predicted_for_fen_ = fen;
  predicted_fen_.clear();
  ChessEngine scratch;
  if (scratch.LoadFen(fen) && ApplyUciMove(scratch, info.pv_uci_moves[0]) &&
      !scratch.IsCheckmate() && !scratch.IsStalemate() && !scratch.IsDraw()) {
    predicted_fen_ = scratch.ToFen();
  }
  UpdateAnalysis();
}

void GameSession::OnClockTick() {
  // The clock freezes on an engine error or once the game is over, or the side
  // to move would drain toward a spurious flag. CheckGameOverTransition() still
  // runs because a flag fall is detected inside Tick.
  if (!has_engine_error_ && !IsGameOver()) clock_.Tick(engine_.SideToMove());
  CheckGameOverTransition();
  RaiseChange();
}

bool GameSession::TryApplyMove(Square from, Square to,
                               std::optional<PieceType> promotion) {
  // The game may have ended (flag fall) while a promotion dialog was open.
  if (IsGameOver()) return false;

  PieceColor color = engine_.SideToMove();
  if (!engine_.TryMakeMove(from, to, promotion).has_value()) return false;

  NotifyMoveMade(color);
  return true;
}

void GameSession::ApplyEngineMove(std::optional<std::string> uci_move,
                                  std::optional<std::string> ponder_move) {
  ClearEngineThinking();

  // Reply for a game that ended while the engine searched.
  if (IsGameOver()) return;

  // No reply: the engine died, timed out or failed to start. Pause in the error
  // state; the UI offers RetryEngineMove(), which restarts the engine.
  if (!uci_move) {
    has_engine_error_ = true;
    emit engineFailed(
        QString::fromStdString(uci_engine_.StartupError().value_or(
            "The chess engine did not reply with a move.")));
    RaiseChange();
    return;
  }

  // "(none)" means no legal move.
  if (!uci_move->empty() && *uci_move != "(none)") {
    auto color = engine_.SideToMove();
    std::optional<Move> applied = ApplyUciMove(engine_, *uci_move);

    if (applied.has_value()) {
      RecordMoveTime();
      clock_.EndTurn(color);

      emit moveMade(color);
    } else {
      // The engine's own move was rejected: app and engine disagree about the
      // position, so play can't safely continue.
      has_engine_error_ = true;
      emit engineMoveRejected(
          QString("Engine reply \"%1\" was not a legal move here (FEN: %2).")
              .arg(QString::fromStdString(*uci_move),
                   QString::fromStdString(engine_.ToFen())));
    }
  }

  if (has_engine_error_) {
    RaiseChange();
    return;
  }

  CheckGameOverTransition();
  RaiseChange();
  UpdateAnalysis();
  if (ponder_move) StartPondering(*ponder_move);
}

// Lets the engine keep searching, on the human's time, the position after its
// predicted reply.
void GameSession::StartPondering(const std::string& ponder_move) {
  if (ponder_move.empty() || IsGameOver() || has_engine_error_ ||
      IsEnginesTurn())
    return;

  ChessEngine expected = engine_;
  if (!ApplyUciMove(expected, ponder_move) || expected.IsCheckmate() ||
      expected.IsStalemate())
    return;

  const int increment_ms = static_cast<int>(clock_.Increment().count());
  uci_engine_.StartPonder(engine_.ToFen(), ponder_move, expected.ToFen(),
                          {static_cast<int>(clock_.WhiteRemaining().count()),
                           static_cast<int>(clock_.BlackRemaining().count()),
                           increment_ms, increment_ms});
}
}  // namespace chess
