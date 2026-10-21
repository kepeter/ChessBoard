/*
 * Copyright (c) 2026 Kornfeld Eliyahu Peter
 * License: MIT
 */

#ifndef CHESSBOARD_UCI_UCI_ENGINE_HPP_
#define CHESSBOARD_UCI_UCI_ENGINE_HPP_

#include <charconv>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include <QThread>

namespace chess {
enum class UciOptionType {
  kCheck,
  kSpin,
  kCombo,
  kButton,
  kString,
};

// Both clocks, from which the engine budgets its own thinking time.
struct EngineClock {
  int white_ms;
  int black_ms;
  int white_inc_ms;
  int black_inc_ms;
};

// An engine option advertised in the handshake. Fields not applicable to `type`
// keep their defaults.
struct UciOption {
  std::string name;
  UciOptionType type;
  std::string default_value;
  std::optional<int64_t> min;
  std::optional<int64_t> max;
  // Only populated for Combo: the allowed values.
  std::vector<std::string> combo_values;
};

// Parses a whole string as a decimal integer; nullopt if it is anything else.
inline std::optional<int64_t> ParseInt(const std::string& text) {
  int64_t value = 0;
  const char* end = text.data() + text.size();
  auto [ptr, ec] = std::from_chars(text.data(), end, value);
  if (ec != std::errc() || ptr != end) return std::nullopt;
  return value;
}

inline bool IsValidSpinValue(const UciOption& option,
                             const std::string& saved) {
  std::optional<int64_t> value = ParseInt(saved);
  return value && (!option.min || *value >= *option.min) &&
         (!option.max || *value <= *option.max);
}

// The saved value, or the option's default when it isn't valid.
inline int ValidSpinValue(const UciOption& option, const std::string& saved) {
  return static_cast<int>(
      ParseInt(IsValidSpinValue(option, saved) ? saved : option.default_value)
          .value_or(0));
}

// One "info" line of a search: depth, MultiPV rank (1-based), score and the
// principal variation in UCI notation ("e2e4").
struct AnalysisInfo {
  int depth = 0;
  int multipv = 1;
  // Exactly one is set: centipawns (positive favors the side to move) or
  // mate-in-N (negative: the side to move is being mated).
  std::optional<int> score_cp;
  std::optional<int> score_mate;
  std::vector<std::string> pv_uci_moves;
};

// Talks to an external UCI engine (e.g. Stockfish) over stdin/stdout.
//
// Each engine process (the game engine and a separate one for analysis) is
// owned by one worker thread, the only code that touches it. Public methods
// just post a request and return; the worker runs requests strictly in order
// and ends any running analysis or ponder search first, so "stop" is only sent
// after a "go" and a request never reads another's output.
//
// Callbacks run on the worker thread; UI callers must marshal back to their
// own thread. Callbacks pending at destruction are dropped. The engine starts
// lazily on the first request. If it fails to start, stops responding or
// exits, pending requests complete with "nothing" (nullopt/empty),
// StartupError() says why, and the next request tries a fresh start.
class UciEngine {
 public:
  explicit UciEngine(std::string command = "stockfish");
  ~UciEngine();

  // Not copyable: owns a live child process and thread.
  UciEngine(const UciEngine&) = delete;
  UciEngine& operator=(const UciEngine&) = delete;

  // Starts the engine if needed. The callback gets nullopt on success,
  // otherwise the reason (also returned by StartupError()).
  void Start(std::function<void(std::optional<std::string> error)> callback);

  // Why the last start failed or the engine died; nullopt if it is running or
  // was never tried. Callable from any thread.
  std::optional<std::string> StartupError() const;

  // Delivers the advertised options, starting the engine first if needed; empty
  // if it can't start.
  void RequestOptions(std::function<void(std::vector<UciOption>)> callback);

  // Replaces the engine command; the next request spawns the new process.
  // Options it also advertises are re-applied.
  void SetCommand(const std::string& command);

  // Sends "setoption"; an empty value presses a button option.
  void SetOption(const std::string& name, const std::string& value);

  // Clock-driven search of `fen`. Delivers the move and the expected reply, or
  // nullopt on failure or timeout. A running StartPonder for exactly `fen`
  // becomes this search. `early_eval_callback` is called once before the move
  // with an evaluation from the side to move: the first top line deep enough to
  // be reliable, or the last one seen if the search ends sooner.
  void RequestBestMoveAsync(
      const std::string& fen, EngineClock clock,
      std::function<void(std::optional<AnalysisInfo>)> early_eval_callback,
      std::function<void(std::optional<std::string> move,
                         std::optional<std::string> ponder_move)>
          callback);

  // Searches `fen` + `ponder_move` while the human thinks; `expected_fen` is
  // the resulting position. A later RequestBestMoveAsync for exactly
  // `expected_fen` continues the search ("ponderhit"); anything else cancels
  // it. No-op unless the Ponder option is "true".
  void StartPonder(const std::string& fen, const std::string& ponder_move,
                   const std::string& expected_fen, EngineClock clock);

  // Runs on the separate analysis engine, alongside any game search. Starts an
  // infinite analysis with `multipv` lines; `on_info` fires per scored line.
  // Replaces any analysis already running; nothing is delivered after it ends.
  void StartAnalysis(const std::string& fen, int multipv,
                     std::function<void(AnalysisInfo)> on_info);

  void StopAnalysis();

  // Cuts a running search short so its callback completes early (the callback
  // is not cancelled; callers must drop unwanted results). Also cancels
  // pondering.
  void AbortSearch();

 private:
  class Worker;

  // Startup status shared with StartupError() callers on other threads.
  struct SharedStatus {
    mutable std::mutex mutex;
    std::optional<std::string> error;
  };

  std::shared_ptr<SharedStatus> status_;
  QThread thread_;
  Worker* worker_;  // owned by thread_'s event loop

  // Second engine process used only for live analysis, so it never competes
  // with the game engine. Gets the same command and options.
  std::shared_ptr<SharedStatus> analysis_status_;
  QThread analysis_thread_;
  Worker* analysis_worker_;
};
}  // namespace chess

#endif  // CHESSBOARD_UCI_UCI_ENGINE_HPP_
