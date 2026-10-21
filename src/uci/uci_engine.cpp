/*
 * Copyright (c) 2026 Kornfeld Eliyahu Peter
 * License: MIT
 */

#include "uci/uci_engine.hpp"

#include <algorithm>
#include <deque>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <QByteArray>
#include <QMetaObject>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QTimer>

namespace chess {
namespace {
constexpr int kHandshakeTimeoutMs = 5000;
// Grace added to a search's time budget before the engine is declared
// unresponsive.
constexpr int kSearchGraceMs = 5000;
// Shallower scores are too unstable to classify a move by.
constexpr int kEarlyEvalDepth = 14;
constexpr int kQuitTimeoutMs = 500;

std::vector<std::string> SplitWords(const std::string& line) {
  std::vector<std::string> words;
  std::istringstream iss(line);
  std::string word;
  while (iss >> word) words.push_back(word);
  return words;
}

// Parses "option name <name> type <type> [default <v>] [min <v>] [max <v>] [var
// <v>]*". Names and defaults may contain spaces, so words are scanned up to the
// next keyword.
std::optional<UciOption> ParseOptionLine(
    const std::vector<std::string>& words) {
  if (words.empty() || words[0] != "option") return std::nullopt;

  auto is_keyword = [](const std::string& w) {
    return w == "name" || w == "type" || w == "default" || w == "min" ||
           w == "max" || w == "var";
  };

  UciOption option;
  option.type = UciOptionType::kString;

  size_t i = 1;
  while (i < words.size()) {
    const std::string& keyword = words[i++];
    std::string value;
    while (i < words.size() && !is_keyword(words[i])) {
      if (!value.empty()) value += ' ';
      value += words[i++];
    }

    if (keyword == "name") {
      option.name = value;
    } else if (keyword == "type") {
      if (value == "check") {
        option.type = UciOptionType::kCheck;
      } else if (value == "spin") {
        option.type = UciOptionType::kSpin;
      } else if (value == "combo") {
        option.type = UciOptionType::kCombo;
      } else if (value == "button") {
        option.type = UciOptionType::kButton;
      } else {
        option.type = UciOptionType::kString;
      }
    } else if (keyword == "default") {
      option.default_value = value;
    } else if (keyword == "min") {
      option.min = ParseInt(value).value_or(0);
    } else if (keyword == "max") {
      option.max = ParseInt(value).value_or(0);
    } else if (keyword == "var") {
      option.combo_values.push_back(value);
    }
  }

  if (option.name.empty()) return std::nullopt;
  return option;
}

// Parses "info depth D multipv K score cp X|mate M pv ...". Returns nullopt for
// lines lacking a score and a pv (e.g. "info string").
std::optional<AnalysisInfo> ParseInfoLine(
    const std::vector<std::string>& words) {
  if (words.empty() || words[0] != "info") return std::nullopt;

  AnalysisInfo info;
  bool has_score = false;
  bool has_pv = false;

  // A bound is a partial result of a re-search, not a proven value.
  for (const std::string& word : words) {
    if (word == "lowerbound" || word == "upperbound") return std::nullopt;
  }

  for (size_t i = 1; i < words.size(); ++i) {
    const std::string& word = words[i];

    if (word == "depth" && i + 1 < words.size()) {
      info.depth = static_cast<int>(ParseInt(words[++i]).value_or(0));
    } else if (word == "multipv" && i + 1 < words.size()) {
      info.multipv = static_cast<int>(ParseInt(words[++i]).value_or(0));
    } else if (word == "score" && i + 1 < words.size()) {
      const std::string& kind = words[++i];
      if (kind == "cp" && i + 1 < words.size()) {
        info.score_cp = static_cast<int>(ParseInt(words[++i]).value_or(0));
        has_score = true;
      } else if (kind == "mate" && i + 1 < words.size()) {
        info.score_mate = static_cast<int>(ParseInt(words[++i]).value_or(0));
        has_score = true;
      }
    } else if (word == "pv") {
      for (size_t j = i + 1; j < words.size(); ++j)
        info.pv_uci_moves.push_back(words[j]);
      has_pv = true;
      break;
    }
  }

  if (!has_score || !has_pv) return std::nullopt;
  return info;
}
}  // namespace

// Everything below runs on the worker thread; UciEngine posts lambdas here and
// touches nothing else.
class UciEngine::Worker : public QObject {
 public:
  struct Request {
    enum class Kind {
      kStart,
      kOptions,
      kSetOption,
      kBestMove,
      kPonder,
      kAnalyze,
      kStopAnalysis
    };
    Kind kind;
    std::string fen;
    std::string
        ponder_move;  // Ponder only: the expected reply, played on top of `fen`
    std::string expected_fen;  // Ponder only: the position after `ponder_move`
    EngineClock clock{};
    int multipv = 1;
    std::string option_name, option_value;
    std::function<void(std::optional<std::string>)> start_callback;
    std::function<void(std::vector<UciOption>)> options_callback;
    std::function<void(std::optional<std::string>, std::optional<std::string>)>
        move_callback;  // move, ponder move
    // BestMove only: called once with the evaluation, as soon as a top line
    // reaches kEarlyEvalDepth or at "bestmove" if none did.
    std::function<void(std::optional<AnalysisInfo>)> early_eval_callback;
    bool early_eval_sent = false;
    std::function<void(AnalysisInfo)> info_callback;
  };

  Worker(std::string command, std::shared_ptr<SharedStatus> status)
      : command_(std::move(command)), status_(std::move(status)) {}

  void Enqueue(Request request) {
    switch (request.kind) {
      case Request::Kind::kAnalyze: {
        // Only the latest analysis matters; queued ones are for superseded
        // positions.
        RemoveQueued(Request::Kind::kAnalyze);
        break;
      }

      case Request::Kind::kStopAnalysis: {
        // A queued analysis is cancelled outright; the engine never hears of
        // it.
        bool removed = RemoveQueued(Request::Kind::kAnalyze);
        if (removed || state_ != State::kAnalyzing) return;
        break;
      }

      case Request::Kind::kPonder: {
        auto ponder = sent_options_.find("Ponder");
        if (ponder == sent_options_.end() || ponder->second != "true") return;
        RemoveQueued(Request::Kind::kPonder);
        break;
      }

      default:
        break;
    }

    queue_.push_back(std::move(request));
    Pump();
  }

  void AbortSearch() {
    RemoveQueued(Request::Kind::kPonder);
    if (state_ == State::kSearching)
      Write("stop");
    else if (state_ == State::kPondering)
      StopPondering();
  }

  void SetCommand(std::string command) {
    Teardown(std::nullopt, /*invoke_callbacks=*/true);
    command_ = std::move(command);
  }

  // Blocking; called from ~UciEngine. Drops pending callbacks since their
  // targets are being destroyed.
  void Shutdown() { Teardown(std::nullopt, /*invoke_callbacks=*/false); }

 private:
  // Pondering is a "go ponder" search that ends with "ponderhit" (it becomes
  // the Searching BestMove) or "stop". Stopping is the gap between sending
  // "stop" and the engine's "bestmove", during which no other command may be
  // sent.
  enum class State {
    kNotStarted,
    kStarting,
    kIdle,
    kSearching,
    kPondering,
    kAnalyzing,
    kStopping,
    kDead
  };
  enum class HandshakeStage { kSpawning, kWaitUciok, kWaitReadyok };

  std::string command_;
  std::shared_ptr<SharedStatus> status_;

  QProcess* process_ = nullptr;
  QByteArray buffer_;
  QTimer* timeout_timer_ = nullptr;

  State state_ = State::kNotStarted;
  HandshakeStage handshake_stage_ = HandshakeStage::kSpawning;
  std::vector<UciOption> options_;
  // Last value sent per option, replayed after every (re)start.
  std::map<std::string, std::string> sent_options_;
  std::string ponder_fen_;  // the position the current ponder search is for

  std::deque<Request> queue_;
  std::optional<Request> current_;  // the Searching/Analyzing request
  std::optional<AnalysisInfo>
      search_eval_best_;  // deepest multipv-1 info seen by the current BestMove

  bool RemoveQueued(Request::Kind kind) {
    bool removed = false;
    for (auto it = queue_.begin(); it != queue_.end();) {
      if (it->kind == kind) {
        it = queue_.erase(it);
        removed = true;
      } else {
        ++it;
      }
    }
    return removed;
  }

  void StopPondering() {
    Write("stop");
    state_ = State::kStopping;
    current_.reset();
  }

  void ResumePonderAsSearch(Request request) {
    Write("ponderhit");
    // Backstop against a hung engine, as in a normal search.
    const EngineClock& c = request.clock;
    Timer()->start(std::max(c.white_ms, c.black_ms) + kSearchGraceMs);
    state_ = State::kSearching;
    current_ = std::move(request);

    if (current_->early_eval_callback && search_eval_best_ &&
        search_eval_best_->depth >= kEarlyEvalDepth) {
      current_->early_eval_sent = true;
      current_->early_eval_callback(search_eval_best_);
    }
  }

  void SetError(std::optional<std::string> error) {
    std::lock_guard<std::mutex> lock(status_->mutex);
    status_->error = std::move(error);
  }

  void Write(const std::string& command) {
    if (process_ && process_->state() == QProcess::Running) {
      process_->write(QByteArray::fromStdString(command + "\n"));
    }
  }

  QTimer* Timer() {
    if (!timeout_timer_) {
      timeout_timer_ = new QTimer(this);
      timeout_timer_->setSingleShot(true);
      connect(timeout_timer_, &QTimer::timeout, this, &Worker::OnTimeout);
    }
    return timeout_timer_;
  }

  // Runs queued requests while the engine is free. A running analysis is
  // stopped first (the queue resumes once its "bestmove" arrives); a running
  // search is waited for.
  void Pump() {
    while (!queue_.empty()) {
      switch (state_) {
        case State::kStarting:
        case State::kSearching:
        case State::kStopping:
          return;

        case State::kPondering: {
          Request& next = queue_.front();
          if (next.kind == Request::Kind::kBestMove &&
              next.fen == ponder_fen_) {
            Request request = std::move(next);
            queue_.pop_front();
            ResumePonderAsSearch(std::move(request));
          } else {
            StopPondering();
          }
          return;
        }

        case State::kAnalyzing: {
          Write("stop");
          state_ = State::kStopping;
          current_.reset();  // no more info for this analysis
          return;
        }

        case State::kNotStarted:
        case State::kDead: {
          BeginStart();
          return;
        }

        case State::kIdle: {
          Request request = std::move(queue_.front());
          queue_.pop_front();
          Execute(std::move(request));
          break;
        }
      }
    }
  }

  void Execute(Request request) {
    switch (request.kind) {
      case Request::Kind::kStart: {
        if (request.start_callback) request.start_callback(std::nullopt);
        break;
      }

      case Request::Kind::kOptions: {
        if (request.options_callback) request.options_callback(options_);
        break;
      }

      case Request::Kind::kSetOption: {
        Write(request.option_value.empty()
                  ? "setoption name " + request.option_name
                  : "setoption name " + request.option_name + " value " +
                        request.option_value);
        if (!request.option_value.empty())
          sent_options_[request.option_name] = request.option_value;
        break;
      }

      case Request::Kind::kBestMove: {
        // A move search wants one line; a higher MultiPV left from analysis
        // would split its effort.
        Write("setoption name MultiPV value 1");
        Write("position fen " + request.fen);
        search_eval_best_.reset();
        const EngineClock& c = request.clock;
        Write("go wtime " + std::to_string(c.white_ms) + " btime " +
              std::to_string(c.black_ms) + " winc " +
              std::to_string(c.white_inc_ms) + " binc " +
              std::to_string(c.black_inc_ms));
        // Only a backstop against a hung engine.
        Timer()->start(std::max(c.white_ms, c.black_ms) + kSearchGraceMs);
        state_ = State::kSearching;
        current_ = std::move(request);
        break;
      }

      case Request::Kind::kPonder: {
        // No timeout: ponder time is the human's, and ends with ponderhit or
        // stop.
        Write("setoption name MultiPV value 1");
        Write("position fen " + request.fen + " moves " + request.ponder_move);
        search_eval_best_.reset();
        const EngineClock& c = request.clock;
        Write("go ponder wtime " + std::to_string(c.white_ms) + " btime " +
              std::to_string(c.black_ms) + " winc " +
              std::to_string(c.white_inc_ms) + " binc " +
              std::to_string(c.black_inc_ms));
        ponder_fen_ = request.expected_fen;
        state_ = State::kPondering;
        current_ = std::move(request);
        break;
      }

      case Request::Kind::kAnalyze: {
        Write("setoption name MultiPV value " +
              std::to_string(request.multipv));
        Write("position fen " + request.fen);
        Write("go infinite");
        state_ = State::kAnalyzing;
        current_ = std::move(request);
        break;
      }

      case Request::Kind::kStopAnalysis: {
        // Pump stops a live analysis before taking another request, so there is
        // nothing to stop here.
        break;
      }
    }
  }

  void BeginStart() {
    SetError(std::nullopt);
    options_.clear();
    buffer_.clear();

    process_ = new QProcess(this);
    // Engine stderr goes to ours, never into the parsed stdout stream.
    process_->setProcessChannelMode(QProcess::ForwardedErrorChannel);
    connect(process_, &QProcess::started, this, &Worker::OnStarted);
    connect(process_, &QProcess::readyReadStandardOutput, this,
            &Worker::OnReadyRead);
    connect(process_, &QProcess::finished, this, &Worker::OnFinished);
    connect(process_, &QProcess::errorOccurred, this, &Worker::OnError);

    state_ = State::kStarting;
    handshake_stage_ = HandshakeStage::kSpawning;
    process_->start(QString::fromStdString(command_), QStringList());
  }

  void OnStarted() {
    if (state_ != State::kStarting) return;
    handshake_stage_ = HandshakeStage::kWaitUciok;
    Write("uci");
    Timer()->start(kHandshakeTimeoutMs);
  }

  void OnReadyRead() {
    buffer_ += process_->readAllStandardOutput();

    int newline;
    while (process_ && (newline = buffer_.indexOf('\n')) >= 0) {
      QByteArray raw = buffer_.left(newline);
      buffer_.remove(0, newline + 1);
      if (raw.endsWith('\r')) raw.chop(1);
      OnLine(std::string(raw.constData(), static_cast<size_t>(raw.size())));
    }
  }

  void OnLine(const std::string& line) {
    std::vector<std::string> words = SplitWords(line);
    if (words.empty()) return;

    switch (state_) {
      case State::kStarting: {
        if (auto option = ParseOptionLine(words)) {
          // App default: the engine thinks on the human's time.
          if (option->name == "Ponder") option->default_value = "true";
          // Likewise three analysis lines, within the engine's range.
          if (option->name == "MultiPV" &&
              option->type == UciOptionType::kSpin) {
            int64_t lines = 3;
            if (option->min) lines = std::max(lines, *option->min);
            if (option->max) lines = std::min(lines, *option->max);
            option->default_value = std::to_string(lines);
          }
          options_.push_back(std::move(*option));
        } else if (handshake_stage_ == HandshakeStage::kWaitUciok &&
                   words[0] == "uciok") {
          handshake_stage_ = HandshakeStage::kWaitReadyok;
          Write("isready");
          Timer()->start(kHandshakeTimeoutMs);
        } else if (handshake_stage_ == HandshakeStage::kWaitReadyok &&
                   words[0] == "readyok") {
          Timer()->stop();
          // Applies the Ponder default unless the caller chose otherwise (that
          // choice is queued and sent afterwards).
          if (!sent_options_.count("Ponder")) sent_options_["Ponder"] = "true";
          for (const auto& [name, value] : sent_options_) {
            const bool known =
                std::any_of(options_.begin(), options_.end(),
                            [&](const UciOption& o) { return o.name == name; });
            if (known) Write("setoption name " + name + " value " + value);
          }
          state_ = State::kIdle;
          Pump();
        }
        break;
      }

      case State::kSearching: {
        if (words[0] == "bestmove") {
          Timer()->stop();
          Request finished = std::move(*current_);
          current_.reset();
          state_ = State::kIdle;

          if (finished.kind == Request::Kind::kBestMove) {
            if (finished.early_eval_callback && !finished.early_eval_sent) {
              finished.early_eval_callback(search_eval_best_);
            }
            std::optional<std::string> move, ponder_move;
            if (words.size() >= 2) move = words[1];
            if (words.size() >= 4 && words[2] == "ponder")
              ponder_move = words[3];
            if (finished.move_callback)
              finished.move_callback(move, ponder_move);
          }
          Pump();
        } else if (auto info = ParseInfoLine(words);
                   info && info->multipv == 1) {
          search_eval_best_ = info;
          if (current_->kind == Request::Kind::kBestMove &&
              current_->early_eval_callback && !current_->early_eval_sent &&
              info->depth >= kEarlyEvalDepth) {
            current_->early_eval_sent = true;
            current_->early_eval_callback(info);
          }
        }
        break;
      }

      case State::kPondering: {
        if (words[0] == "bestmove") {
          // The engine gave up pondering itself (e.g. terminal position).
          current_.reset();
          state_ = State::kIdle;
          Pump();
        } else if (auto info = ParseInfoLine(words);
                   info && info->multipv == 1) {
          search_eval_best_ = info;
        }
        break;
      }

      case State::kAnalyzing: {
        if (words[0] == "bestmove") {
          // The engine ended the infinite search itself (e.g. the position is
          // already mate).
          current_.reset();
          state_ = State::kIdle;
          Pump();
        } else if (auto info = ParseInfoLine(words)) {
          if (current_ && current_->info_callback)
            current_->info_callback(*info);
        }
        break;
      }

      case State::kStopping: {
        if (words[0] == "bestmove") {
          state_ = State::kIdle;
          Pump();
        }
        break;
      }

      case State::kNotStarted:
      case State::kIdle:
      case State::kDead:
        break;  // stray output; nothing is waiting for it
    }
  }

  void OnTimeout() {
    switch (state_) {
      case State::kStarting: {
        Teardown("\"" + command_ +
                     "\" did not respond to the UCI handshake within " +
                     std::to_string(kHandshakeTimeoutMs / 1000) +
                     " seconds. Is it a UCI chess engine?",
                 true);
        break;
      }

      case State::kSearching: {
        Teardown("\"" + command_ +
                     "\" did not reply to a search request in time; it was "
                     "shut down.",
                 true);
        break;
      }

      default:
        break;
    }
  }

  void OnFinished(int exit_code, QProcess::ExitStatus) {
    if (state_ == State::kNotStarted || state_ == State::kDead) return;

    if (state_ == State::kStarting) {
      Teardown("\"" + command_ +
                   "\" exited before completing the UCI handshake. Is it a UCI "
                   "chess engine?",
               true);
    } else {
      Teardown("\"" + command_ + "\" exited unexpectedly (exit code " +
                   std::to_string(exit_code) + ").",
               true);
    }
  }

  void OnError(QProcess::ProcessError error) {
    if (error == QProcess::FailedToStart) {
      Teardown(
          "Could not start the chess engine \"" + command_ +
              "\". Check that the path is correct and the file is executable.",
          true);
    }
  }

  // Ends the process and fails every pending request. A non-empty `error` marks
  // the engine Dead; empty is a deliberate reset to NotStarted. Callbacks get
  // "nothing" unless invoke_callbacks is false.
  void Teardown(std::optional<std::string> error, bool invoke_callbacks) {
    Timer()->stop();

    if (process_) {
      QProcess* dying = process_;
      process_ = nullptr;  // OnFinished/OnError for it are ignored from here on
      dying->disconnect(this);
      if (dying->state() != QProcess::NotRunning) {
        dying->write("quit\n");
        if (!dying->waitForFinished(kQuitTimeoutMs)) {
          dying->kill();
          dying->waitForFinished(1000);
        }
      }
      dying->deleteLater();
    }
    buffer_.clear();
    options_.clear();

    std::deque<Request> failed;
    if (current_) failed.push_back(std::move(*current_));
    current_.reset();
    failed.insert(failed.end(), std::make_move_iterator(queue_.begin()),
                  std::make_move_iterator(queue_.end()));
    queue_.clear();

    state_ = error ? State::kDead : State::kNotStarted;
    SetError(error);

    if (!invoke_callbacks) return;
    for (Request& request : failed) {
      switch (request.kind) {
        case Request::Kind::kStart:
          if (request.start_callback)
            request.start_callback(
                error.value_or("The chess engine was shut down."));
          break;
        case Request::Kind::kOptions:
          if (request.options_callback) request.options_callback({});
          break;
        case Request::Kind::kBestMove:
          if (request.move_callback)
            request.move_callback(std::nullopt, std::nullopt);
          break;
        default:
          break;
      }
    }
  }
};

UciEngine::UciEngine(std::string command)
    : status_(std::make_shared<SharedStatus>()),
      analysis_status_(std::make_shared<SharedStatus>()) {
  worker_ = new Worker(command, status_);
  worker_->moveToThread(&thread_);
  QObject::connect(&thread_, &QThread::finished, worker_,
                   &QObject::deleteLater);
  thread_.setObjectName("uci-engine");
  thread_.start();

  analysis_worker_ = new Worker(std::move(command), analysis_status_);
  analysis_worker_->moveToThread(&analysis_thread_);
  QObject::connect(&analysis_thread_, &QThread::finished, analysis_worker_,
                   &QObject::deleteLater);
  analysis_thread_.setObjectName("uci-analysis");
  analysis_thread_.start();
}

UciEngine::~UciEngine() {
  QMetaObject::invokeMethod(
      worker_, [worker = worker_]() { worker->Shutdown(); },
      Qt::BlockingQueuedConnection);
  QMetaObject::invokeMethod(
      analysis_worker_, [worker = analysis_worker_]() { worker->Shutdown(); },
      Qt::BlockingQueuedConnection);
  thread_.quit();
  analysis_thread_.quit();
  thread_.wait();
  analysis_thread_.wait();
}

namespace {
template <typename Fn>
void Post(QObject* worker, Fn fn) {
  QMetaObject::invokeMethod(worker, std::move(fn), Qt::QueuedConnection);
}
}  // namespace

void UciEngine::Start(
    std::function<void(std::optional<std::string>)> callback) {
  Worker::Request request;
  request.kind = Worker::Request::Kind::kStart;
  request.start_callback = std::move(callback);
  Post(worker_, [worker = worker_, request = std::move(request)]() mutable {
    worker->Enqueue(std::move(request));
  });
}

std::optional<std::string> UciEngine::StartupError() const {
  std::lock_guard<std::mutex> lock(status_->mutex);
  return status_->error;
}

void UciEngine::RequestOptions(
    std::function<void(std::vector<UciOption>)> callback) {
  Worker::Request request;
  request.kind = Worker::Request::Kind::kOptions;
  request.options_callback = std::move(callback);
  Post(worker_, [worker = worker_, request = std::move(request)]() mutable {
    worker->Enqueue(std::move(request));
  });
}

void UciEngine::SetCommand(const std::string& command) {
  Post(worker_, [worker = worker_, command]() { worker->SetCommand(command); });
  Post(analysis_worker_,
       [worker = analysis_worker_, command]() { worker->SetCommand(command); });
}

// Sent to both engine processes so analysis sees the same settings.
void UciEngine::SetOption(const std::string& name, const std::string& value) {
  Worker::Request request;
  request.kind = Worker::Request::Kind::kSetOption;
  request.option_name = name;
  request.option_value = value;
  Post(analysis_worker_, [worker = analysis_worker_, request]() mutable {
    worker->Enqueue(std::move(request));
  });
  Post(worker_, [worker = worker_, request = std::move(request)]() mutable {
    worker->Enqueue(std::move(request));
  });
}

void UciEngine::RequestBestMoveAsync(
    const std::string& fen, EngineClock clock,
    std::function<void(std::optional<AnalysisInfo>)> early_eval_callback,
    std::function<void(std::optional<std::string>, std::optional<std::string>)>
        callback) {
  Worker::Request request;
  request.kind = Worker::Request::Kind::kBestMove;
  request.fen = fen;
  request.clock = clock;
  request.early_eval_callback = std::move(early_eval_callback);
  request.move_callback = std::move(callback);
  Post(worker_, [worker = worker_, request = std::move(request)]() mutable {
    worker->Enqueue(std::move(request));
  });
}

void UciEngine::StartPonder(const std::string& fen,
                            const std::string& ponder_move,
                            const std::string& expected_fen,
                            EngineClock clock) {
  Worker::Request request;
  request.kind = Worker::Request::Kind::kPonder;
  request.fen = fen;
  request.ponder_move = ponder_move;
  request.expected_fen = expected_fen;
  request.clock = clock;
  Post(worker_, [worker = worker_, request = std::move(request)]() mutable {
    worker->Enqueue(std::move(request));
  });
}

void UciEngine::StartAnalysis(const std::string& fen, int multipv,
                              std::function<void(AnalysisInfo)> on_info) {
  Worker::Request request;
  request.kind = Worker::Request::Kind::kAnalyze;
  request.fen = fen;
  request.multipv = multipv;
  request.info_callback = std::move(on_info);
  Post(analysis_worker_,
       [worker = analysis_worker_, request = std::move(request)]() mutable {
         worker->Enqueue(std::move(request));
       });
}

void UciEngine::StopAnalysis() {
  Worker::Request request;
  request.kind = Worker::Request::Kind::kStopAnalysis;
  Post(analysis_worker_,
       [worker = analysis_worker_, request = std::move(request)]() mutable {
         worker->Enqueue(std::move(request));
       });
}

void UciEngine::AbortSearch() {
  Post(worker_, [worker = worker_]() { worker->AbortSearch(); });
}
}  // namespace chess
