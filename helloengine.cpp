#include <iostream>
#include <vector>
#include <sstream>
#include <string>

#include "board.h"
#include "log.h"
#include "test.h"

using namespace chesseng;

std::vector<std::string> split(const std::string& input, char delimiter) {
  std::vector<std::string> res;
  std::istringstream sstream(input);
  std::string entry;
  while (std::getline(sstream, entry, delimiter)) {
    res.push_back(entry);
  }
  return res;
}

std::string EvalStatusToShortString(EvalStatus status){
  if(status==EvalStatus::DONE_PARTIAL) {
    return "AB";
  } else if (status == EvalStatus::DONE_COMPLETE) {
    return "DC";
  }
  return "NA";
}

void handle_uci(){
  loggedcoutline("id name HelloEngine 1 64");
  loggedcoutline("id author lego");
  loggedcoutline("uciok");
}
void handle_isready(){
  loggedcoutline("readyok");
}
void handle_ucinewgame(){
}
void handle_position(const std::string& input, Board& board){
  static std::string startPositionCommand = "position startpos";
  static std::string startPositionMovesPrefix = "position startpos moves ";
  if (input.rfind(startPositionCommand, 0) != 0) {
    Log::log("Unexpected position input: "+input);
    return;
  }
  board = Board();
  board.startingPosition();
  if(input.rfind(startPositionMovesPrefix, 0) == 0) {
    std::string movesString = input.substr(startPositionMovesPrefix.size());
    std::istringstream sstream(movesString);
    std::string move;
    while (std::getline(sstream, move, ' ')) {
      board = Board::makeMove(board, move);
    }
    std::string boardStr = board.logBoard();
    Log::log("Board after move "+move+":");
    Log::log(boardStr);
  }
}
void handle_go(const std::string& input, Engine& engine, const Board& board) {
  static std::string prefix = "go ";
  int16_t toDepth = 4;
  int16_t toQsDepth = 2;
  int32_t allowedTimeMs = 5000;

  if(input.size()>prefix.size()) {
    std::string paramsStr = input.substr(prefix.size());
    std::vector<std::string> params = split(paramsStr, ' ');
    for(int i=0;i<params.size();i++){
      if(params[i] == "depth") {
        toDepth = atoi(params[i+1].c_str());
      }
    }
  }

  Move bestMove = engine.findBestMove(board, toDepth, toQsDepth, allowedTimeMs);
  std::string bestMoveStr = bestMove.print();
  // loggedcoutline("bestmove e2e4 ponder e7e6");
  loggedcoutline("bestmove " + bestMoveStr);
}
void handle_printboard(const Board& board) {
  Log::logAndPrint(board.logBoard());
}
void handle_printmovedetails(const Board& board, Engine& engine) {
  Log::logAndPrint("Moves from current position:");
  EvalRecord* record = engine.findRecord(board);
  for(const auto& move:record->moves){
    std::stringstream ss;
    Board nextBoard = Board::makeMove(board, move);
    EvalRecord* nextEvalRecord = engine.findRecord(nextBoard);
    ss<<"- "<<move.print()<< " " << EvalStatusToShortString(nextEvalRecord->evalStatus) <<" score "<< (nextEvalRecord->score/100.0) << " ("<<(nextEvalRecord->minWhite/100.0)<<", "<<(nextEvalRecord->maxBlack/100.0) <<") D"<<(int16_t)nextEvalRecord->evalDepth<< " M"<< nextEvalRecord->moves.size() << " (";
    for(const auto& seqMove: engine.getBestMoveSequence(nextBoard)){
      ss<<seqMove.print()<<" ";
    }
    ss<<")";
    Log::logAndPrint(ss.str());
  }
  
}

void handle_unknown(const std::string& s) {
  loggedcoutline("Unknown command: " + s);
}

int main(int argc, char *argv[])
{
  loggedcoutline("HelloEngine 0");

  std::string verb;
  if(argc>1) {
    verb = argv[1];
    std::cout << "Verb " << verb << std::endl;
  }
  if(verb == "test") {
    test_all();
    return 0;
  }

  Engine engine;
  Board board;
  
  std::string input;
  for (; std::getline(std::cin, input);) {
    // std::cin >> input;
    Log::log("Got input: [" + input + "]");
    if(input=="uci") {
      handle_uci();
    } else if (input=="isready") {
      handle_isready();
    } else if (input=="ucinewgame") {
      handle_ucinewgame();
    } else if(input.rfind("position ", 0) == 0) {
      handle_position(input, board);
    } else if(input == "go" || input.rfind("go ", 0) == 0) {
      handle_go(input, engine, board);
    } else if (input == "stop" || input=="xboard") {
      // do nothing
    } else if (input == "pb") {
      handle_printboard(board);
    } else if (input == "pmd") {
      handle_printmovedetails(board, engine);
    } else {
      handle_unknown(input);
    }
    
  }
}