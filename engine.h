#pragma once

#include <array>
#include <chrono>
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>

#include "board.h"
#include "log.h"

namespace chesseng {

constexpr int16_t MIN_SCORE = -30000;
constexpr int16_t MAX_SCORE = 30000;

enum class EvalStatus: uint8_t {
  NOT_EVALUATED=0,
  IN_EVALUATION=1,
  DONE_PARTIAL=2,
  DONE_COMPLETE=3
};

struct EvalRecord {
  EvalRecord():bestMove(){}
  std::vector<Move> moves;
  Move bestMove;
  int16_t score{0};
  int16_t minWhite{MIN_SCORE};
  int16_t maxBlack{MAX_SCORE};
  EvalStatus evalStatus{EvalStatus::NOT_EVALUATED};
  uint8_t evalDepth{0};
  uint8_t qsEvalDepth{0};

  // position is quiet if there is no check
  bool isQuietPosition{true};
};

struct EvalContext {
  EvalContext(bool trackTime, int32_t allowedTimeMs = 0, int16_t depthRequired = 1);
  void nodesEvaluatedCallback();
  int32_t getMsSinceStartTime();
  bool searchShouldTimeout();
  
  int32_t nodesEvaluated{0};
  int16_t depthAchieved{0};
  int16_t depthRequired{0};
  int32_t nodesEvaluatedCallbackInterval{1000};
  std::chrono::time_point<std::chrono::steady_clock> startTime{std::chrono::time_point<std::chrono::steady_clock>::min()};
  int32_t allowedRunTimeMs{0};
  std::chrono::time_point<std::chrono::steady_clock> lastReportTime{std::chrono::time_point<std::chrono::steady_clock>::min()};
};

enum class EvalResultCode: uint8_t {
  SUCCESS=0,
  TIMEOUT=1,
  LOOP=2
};

struct EvalResult {
  EvalResult(EvalRecord* record, EvalResultCode result, int16_t score): record(record),result(result), score(score){}
  EvalRecord* record;
  int16_t score;
  EvalResultCode result;
};

struct Engine {
  public:
  static EvalRecord evaluateBoard(const Board& board);
  EvalResult evaluate(const Board& board, EvalContext& evalContext, int16_t toDepth, int16_t minWhite, int16_t maxBlack, int16_t toQsDepth, bool fromQuietMove);
  EvalRecord* findRecord(const Board& board);
  Move findBestMove(const Board& board, int16_t toDepth, int16_t toQsDepth=2, int16_t allowedTimeMs=0);
  std::vector<Move> Engine::getBestMoveSequence(const Board& board);

  private:
  std::unordered_map<Board, EvalRecord, Hasher<Board>> evals;
};
}