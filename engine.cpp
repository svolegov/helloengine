#include "engine.h"

#include <assert.h>
#include <unordered_set>
#include <sstream>

#define SORT_MOVES 1

namespace chesseng {
namespace {
constexpr int32_t MAX_DEPTH = 6;
constexpr int32_t TRIM_TABLE_SIZE = 10000*1000;

constexpr int16_t PAWN_BONUS = 100;
constexpr int16_t ROOK_BONUS = 500;
constexpr int16_t KNIGHT_BONUS = 300;
constexpr int16_t BISHOP_BONUS = 300;
constexpr int16_t QUEEN_BONUS = 900;
constexpr int16_t KING_BONUS = 20000;

constexpr int16_t CAN_MOVE_BONUS = 5;
constexpr int16_t CENTER_BONUS = 20;
constexpr int16_t NEAR_CENTER_BONUS = 10;
constexpr int16_t PAWN_ROW_PROGRESS_BONUS = 20;
constexpr int16_t UNDEFENDED_PIECE_PENALTY_WITH_ATTACKERS = -75;
constexpr int16_t UNDEFENDED_PIECE_PENALTY_NO_ATTACKERS = -50;
constexpr int16_t NO_ATTACKERS_HAVE_DEFENDERS_BONUS = 20;
constexpr int16_t CHECK_PENALTY = -100;
constexpr int16_t DISTANT_CHECKMATE_DECAY = -5;

constexpr int16_t STALEMATE_SCORE = -300;
constexpr int16_t AFTER_CHECKMATE_SCORE = 10000;
constexpr int8_t EXACT_EVAL_DEPTH = 100;

struct HeuristicsContext {
  HeuristicsContext(){
    whiteAttackCount.fill(0);
    blackAttackCount.fill(0);
  }

  std::array<int8_t, 64> whiteAttackCount;
  std::array<int8_t, 64> blackAttackCount;
};

inline void countAttackerDefender(HeuristicsContext& evalContext, Position movePosition, Side pieceSide) {
  if(pieceSide == Side::WHITE) {
    evalContext.whiteAttackCount[movePosition.data] += 1;
  } else {
    evalContext.blackAttackCount[movePosition.data] += 1;
  }
}

inline void evaluateRayMove(const Board& board, HeuristicsContext& evalContext, Position fromPosition, int8_t moveRow, int8_t moveCol, EvalRecord& record, bool& rayEnds) {
  if(!rowcolok(moveRow) || !rowcolok(moveCol)) {
    rayEnds=true;
    return;
  }

  Position movePosition = Position(moveRow, moveCol);
  Square square = board.getSquare(fromPosition);
  Side pieceSide = Board::getSide(square.getSideBit());
  int8_t pieceSign = Board::getSideSign(pieceSide);
  Square moveSquare = board.getSquare(movePosition);
  PieceType movePieceType = moveSquare.getPieceType();
  Side movePieceSide = Board::getSide(moveSquare.getSideBit());
  if(movePieceType == PieceType::NO_PIECE || movePieceSide != pieceSide) {
    // move valid
    record.score+=CAN_MOVE_BONUS*pieceSign;

    // register move
    if(pieceSide == board.getMovingSide()) {
      record.moves.push_back(Move(fromPosition,movePosition, movePieceType==PieceType::NO_PIECE ? MoveType::MOVE : MoveType::CAPTURE));
    }
  }

  // count attackers, defenders
  countAttackerDefender(evalContext, movePosition, pieceSide);
  

  // ray ends on first not-empty square
  if(movePieceType != PieceType::NO_PIECE) {
    rayEnds=true;
  }
}

void setExactScore(EvalRecord& record, int16_t score) {
  record.score=score;
  record.evalStatus = EvalStatus::DONE_COMPLETE;
  record.evalDepth = EXACT_EVAL_DEPTH;
}
}

EvalRecord Engine::evaluateBoard(const Board& board) {
  EvalRecord record;
  record.moves.reserve(40);
  Side movingSide = board.getMovingSide();
  int8_t movingSideSign = Board::getSideSign(movingSide);
  SideBit movingSideBit = Board::getSideBit(movingSide);
  HeuristicsContext evalContext;
  
  std::array<Side,2> sideEvalOrder{Side::BLACK,Side::WHITE};
  if(movingSide == Side::BLACK) {
    sideEvalOrder = {Side::WHITE, Side::BLACK};
  }

  for(Side evalSide : sideEvalOrder) {
    for(size_t posIndex=0;posIndex<64;posIndex++) {
      Position pos(posIndex);
      Square square = board.getSquare(pos);
      PieceType pieceType = square.getPieceType();
      if(pieceType == PieceType::NO_PIECE) {
        continue;
      }
      SideBit pieceSideBit = square.getSideBit();
      Side pieceSide = Board::getSide(pieceSideBit);
      if(pieceSide != evalSide) {
        continue;
      }
      int8_t pieceSign = Board::getSideSign(pieceSide);
      int8_t row=pos.getRow();
      int8_t col=pos.getCol();

      if(square.getPieceType() == PieceType::PAWN_PIECE) {
        // PAWN
        // piece eval
        record.score+=PAWN_BONUS*pieceSign;

        int8_t pawnRowProgress = (pieceSide==Side::WHITE) ? row-1 : 6-row;
        record.score+=pawnRowProgress*PAWN_ROW_PROGRESS_BONUS*pieceSign;

        // moves eval
        int8_t forwardMoveRow = row + pieceSign;
        if(rowcolok(forwardMoveRow)) {
          Square forwardMoveSquare = board.getSquare(forwardMoveRow,col);
          if(forwardMoveSquare.getPieceType()==PieceType::NO_PIECE) {
            // forward move is valid
            record.score+=CAN_MOVE_BONUS*pieceSign;

            // TODO: blocked pawn penalty

            // register move
            if(square.getSideBit() == movingSideBit) {
              bool promotionPossible = (pieceSide == Side::WHITE && forwardMoveRow==7) || (pieceSide == Side::BLACK && forwardMoveRow == 0);
              record.moves.push_back(Move(pos,Position(forwardMoveRow,col), MoveType::MOVE, promotionPossible ? PieceType::QUEEN_PIECE : PieceType::NO_PIECE));
            }

          }
        }

        int8_t twiceForwardStartingRow = pieceSide == Side::WHITE ? 1 : 6;
        int8_t twiceForwardMoveRow = row +2*pieceSign;
        if(row==twiceForwardStartingRow && rowcolok(twiceForwardMoveRow)) {
          Square forwardMoveSquare = board.getSquare(forwardMoveRow,col);
          Square twiceForwardMoveSquare = board.getSquare(twiceForwardMoveRow,col);
          if(forwardMoveSquare.getPieceType()==PieceType::NO_PIECE && twiceForwardMoveSquare.getPieceType()==PieceType::NO_PIECE) {
            // twice forward move is valid
            record.score+=CAN_MOVE_BONUS*pieceSign;

            // register move
            if(square.getSideBit() == movingSideBit) {
              record.moves.push_back(Move(pos,Position(twiceForwardMoveRow,col), MoveType::MOVE));
            }
          }
        }

        for(int8_t colshift =-1;colshift<=1;colshift+=2){
          int8_t takesCol = col+colshift;
          if(rowcolok(takesCol)) {
            // regular capture
            {
              Position takesPos = Position(forwardMoveRow,takesCol);
              Square takesSquare = board.getSquare(takesPos);
              Side movePieceSide = Board::getSide(takesSquare.getSideBit());
              if(takesSquare.getPieceType()!=PieceType::NO_PIECE && takesSquare.getSideBit()!=pieceSideBit) {
                // move is valid
                record.score+=CAN_MOVE_BONUS*pieceSign;

                // register capture move
                if(square.getSideBit() == movingSideBit) {
                  bool promotionPossible = (pieceSide == Side::WHITE && forwardMoveRow==7) || (pieceSide == Side::BLACK && forwardMoveRow == 0);
                  record.moves.push_back(Move(pos,takesPos, MoveType::CAPTURE, promotionPossible ? PieceType::QUEEN_PIECE : PieceType::NO_PIECE));
                }
              }

              // count attackers, defenders
              countAttackerDefender(evalContext, takesPos, pieceSide);
            }
            // en passant capture
            {
              Position enpassantPawnPos = Position(row,takesCol);
              Square maybePawnSquare = board.getSquare(enpassantPawnPos);
              if(maybePawnSquare.getPieceType()==PieceType::PAWN_PIECE 
                && maybePawnSquare.getSideBit()!=pieceSideBit 
                && maybePawnSquare.getPawnMovedTwiceBit()==PawnMovedTwiceBit::YES) {
                  // move is valid
                  record.score+=CAN_MOVE_BONUS*pieceSign;
                  Position movePosition = Position(forwardMoveRow,takesCol);
                  record.moves.push_back(Move(pos,movePosition, MoveType::CAPTURE));
                }
            }
          }
        }

        
      } else if(square.getPieceType() == PieceType::ROOK_PIECE) {
        // ROOK
        // piece eval
        record.score += ROOK_BONUS*pieceSign;

        // moves eval
        bool rayEnds=false;
        for(int8_t moveRow = row+1;moveRow<8 && !rayEnds;moveRow++) {
          evaluateRayMove(board, evalContext, pos, moveRow, col, record, rayEnds);
        }
        rayEnds=false;
        for(int8_t moveRow = row-1;moveRow>=0 && !rayEnds;moveRow--) {
          evaluateRayMove(board, evalContext, pos, moveRow, col, record, rayEnds);
        }
        rayEnds=false;
        for(int8_t moveCol = col+1;moveCol<8 && !rayEnds;moveCol++) {
          evaluateRayMove(board, evalContext, pos, row, moveCol, record, rayEnds);
        }
        rayEnds=false;
        for(int8_t moveCol = col-1;moveCol>=0 && !rayEnds;moveCol--) {
          evaluateRayMove(board, evalContext, pos, row, moveCol, record, rayEnds);
        }
      } else if(square.getPieceType() == PieceType::BISHOP_PIECE) {
        // BISHOP
        // piece eval
        record.score += BISHOP_BONUS*pieceSign;

        // moves eval
        bool rayEnds=false;
        for(int8_t moveRow = row+1;moveRow<8 && !rayEnds;moveRow++) {
          int8_t moveCol = col + (moveRow-row);
          evaluateRayMove(board, evalContext, pos, moveRow, moveCol, record, rayEnds);
        }
        rayEnds=false;
        for(int8_t moveRow = row+1;moveRow<8 && !rayEnds;moveRow++) {
          int8_t moveCol = col + (row-moveRow);
          evaluateRayMove(board, evalContext, pos, moveRow, moveCol, record, rayEnds);
        }
        rayEnds=false;
        for(int8_t moveRow = row-1;moveRow>=0 && !rayEnds;moveRow--) {
          int8_t moveCol = col + (moveRow-row);
          evaluateRayMove(board, evalContext, pos, moveRow, moveCol, record, rayEnds);
        }
        rayEnds=false;
        for(int8_t moveRow = row-1;moveRow>=0 && !rayEnds;moveRow--) {
          int8_t moveCol = col + (row-moveRow);
          evaluateRayMove(board, evalContext, pos, moveRow, moveCol, record, rayEnds);
        }
      } else if(square.getPieceType() == PieceType::QUEEN_PIECE) {
        // QUEEN
        // piece eval
        record.score += QUEEN_BONUS*pieceSign;

        // moves eval
        // rook-like
        bool rayEnds=false;
        for(int8_t moveRow = row+1;moveRow<8 && !rayEnds;moveRow++) {
          evaluateRayMove(board, evalContext, pos, moveRow, col, record, rayEnds);
        }
        rayEnds=false;
        for(int8_t moveRow = row-1;moveRow>=0 && !rayEnds;moveRow--) {
          evaluateRayMove(board, evalContext, pos, moveRow, col, record, rayEnds);
        }
        rayEnds=false;
        for(int8_t moveCol = col+1;moveCol<8 && !rayEnds;moveCol++) {
          evaluateRayMove(board, evalContext, pos, row, moveCol, record, rayEnds);
        }
        rayEnds=false;
        for(int8_t moveCol = col-1;moveCol>=0 && !rayEnds;moveCol--) {
          evaluateRayMove(board, evalContext, pos, row, moveCol, record, rayEnds);
        }
        // bishop-like
        rayEnds=false;
        for(int8_t moveRow = row+1;moveRow<8 && !rayEnds;moveRow++) {
          int8_t moveCol = col + (moveRow-row);
          evaluateRayMove(board, evalContext, pos, moveRow, moveCol, record, rayEnds);
        }
        rayEnds=false;
        for(int8_t moveRow = row+1;moveRow<8 && !rayEnds;moveRow++) {
          int8_t moveCol = col + (row-moveRow);
          evaluateRayMove(board, evalContext, pos, moveRow, moveCol, record, rayEnds);
        }
        rayEnds=false;
        for(int8_t moveRow = row-1;moveRow>=0 && !rayEnds;moveRow--) {
          int8_t moveCol = col + (moveRow-row);
          evaluateRayMove(board, evalContext, pos, moveRow, moveCol, record, rayEnds);
        }
        rayEnds=false;
        for(int8_t moveRow = row-1;moveRow>=0 && !rayEnds;moveRow--) {
          int8_t moveCol = col + (row-moveRow);
          evaluateRayMove(board, evalContext, pos, moveRow, moveCol, record, rayEnds);
        }
      } else if(square.getPieceType() == PieceType::KNIGHT_PIECE) {
        // KNIGHT
        // piece eval
        record.score += KNIGHT_BONUS*pieceSign;
        
        // moves eval
        static std::array<std::pair<int8_t,int8_t>,8> knightDeltas = {std::pair<int8_t,int8_t>{-2,-1},{-2,1},{-1,-2},{-1,2},{1,-2},{1,2},{2,-1},{2,1}};
        bool dummyRayEnds = false;
        for(const auto& deltaPair : knightDeltas) {
          evaluateRayMove(board, evalContext, pos, row+deltaPair.first, col+deltaPair.second, record, dummyRayEnds);
        }
      } else if(square.getPieceType() == PieceType::KING_PIECE) {
        // KING
        // piece eval
        record.score += KING_BONUS*pieceSign;

        // moves eval
        for(int8_t rowDelta = -1; rowDelta<=1; rowDelta++) {
          for(int8_t colDelta = -1; colDelta<=1; colDelta++) {
            if(colDelta==0 && rowDelta==0) {
              continue;
            }

            if(!rowcolok(row+rowDelta) || !rowcolok(col+colDelta)) {
              continue;
            }

            Position movePos = Position(row+rowDelta, col+colDelta);
            Square moveSquare = board.getSquare(movePos);
            PieceType movePieceType = moveSquare.getPieceType();
            Side movePieceSide = Board::getSide(moveSquare.getSideBit());
            if(movePieceType == PieceType::NO_PIECE || movePieceSide != pieceSide) {             
              int8_t moveSquareAttackerCount = pieceSide == Side::WHITE ? evalContext.blackAttackCount[movePos.data] : evalContext.whiteAttackCount[movePos.data];
              if(moveSquareAttackerCount > 0) {
                // move to attacked square is invalid
                continue;
              }

              // move is valid
              record.score+=CAN_MOVE_BONUS*pieceSign;

              // register move
              if(pieceSide == board.getMovingSide()) {
                record.moves.push_back(Move(pos,movePos, movePieceType==PieceType::NO_PIECE ? MoveType::MOVE : MoveType::CAPTURE));
              }  
            }

            // count attackers, defenders
            countAttackerDefender(evalContext, movePos, pieceSide);
          }
        }

        // TODO: castling
        if(pos.getCol() == 4 && square.getMovedBit() == MovedBit::NO) {
          int8_t attackerCount =  pieceSide == Side::WHITE ? evalContext.blackAttackCount[pos.data] : evalContext.whiteAttackCount[pos.data];
          if(attackerCount ==0) {
            // short castling
            {
              Square expectRook = board.getSquare(row, 7);
              if(expectRook.getPieceType()==PieceType::ROOK_PIECE && expectRook.getMovedBit()==MovedBit::NO) {
                bool valid=true;
                for(int middleCol=5;middleCol<=6;middleCol++) {
                  Position middlePos = Position(row, middleCol);
                  int8_t attackerCount =  pieceSide == Side::WHITE ? evalContext.blackAttackCount[middlePos.data] : evalContext.whiteAttackCount[middlePos.data];
                  valid&=(board.getSquare(middlePos).getPieceType() == PieceType::NO_PIECE) && attackerCount==0;
                }

                if(valid) {
                  // move is valid
                  record.score+=CAN_MOVE_BONUS*pieceSign;
                  // register move
                  if(pieceSide == board.getMovingSide()) {
                    record.moves.push_back(Move(pos,Position(row, 6), MoveType::MOVE));
                  }  
                }
              }  
            }
            // long castling
            {
              Square expectRook = board.getSquare(row, 0);
              if(expectRook.getPieceType()==PieceType::ROOK_PIECE && expectRook.getMovedBit()==MovedBit::NO) {
                bool valid=true;
                for(int middleCol=1;middleCol<=3;middleCol++) {
                  Position middlePos = Position(row, middleCol);
                  int8_t attackerCount =  pieceSide == Side::WHITE ? evalContext.blackAttackCount[middlePos.data] : evalContext.whiteAttackCount[middlePos.data];
                  valid&=(board.getSquare(middlePos).getPieceType() == PieceType::NO_PIECE) && attackerCount==0;
                }

                if(valid) {
                  // move is valid
                  record.score+=CAN_MOVE_BONUS*pieceSign;
                  // register move
                  if(pieceSide == board.getMovingSide()) {
                    record.moves.push_back(Move(pos,Position(row, 2), MoveType::MOVE));
                  }  
                }
              }  
            }
            
          }
        }

      }

      // piece in center bonus
      if((row == 3 || row == 4)&&(col == 3 || col == 4)) {
        record.score+=CENTER_BONUS*pieceSign;
      } else if(row>=2 && row<=5 && col>=2 && col<=5) {
        record.score+=NEAR_CENTER_BONUS*pieceSign;
      }
    }
  }

  // attacker count eval
  for(size_t posIndex=0;posIndex<64;posIndex++) {    
    Square square = board.getSquare(Position(posIndex));
    PieceType pieceType = square.getPieceType();
    Side pieceSide = Board::getSide(square.getSideBit());
    int8_t pieceSign = Board::getSideSign(pieceSide);
    if(pieceType==PieceType::NO_PIECE) {
      continue;
    }
    int8_t whiteAC = evalContext.whiteAttackCount[posIndex];
    int8_t blackAC = evalContext.blackAttackCount[posIndex];
    if(whiteAC == 0 && blackAC == 0) {
      continue;
      record.score+=UNDEFENDED_PIECE_PENALTY_NO_ATTACKERS*pieceSign;
    }
    
    // there's a piece, attacker count > 0
    if(pieceType == PieceType::KING_PIECE && pieceSide!=movingSide && ((pieceSide == Side::WHITE && blackAC>0) || (pieceSide == Side::BLACK && whiteAC>0))) {
      // king is attacked on opponents move, after-checkmate
      // remove pseudo-legal moves
      record.moves.clear();
      setExactScore(record, AFTER_CHECKMATE_SCORE*movingSideSign);
      return record;
    }

    
    if(pieceType == PieceType::KING_PIECE) {
      if(pieceSide == Side::WHITE ? blackAC>0 : whiteAC>0) {
        // it's a check
        record.score+= CHECK_PENALTY*pieceSign;
        record.isQuietPosition = false;
      }
    } else {
      // more attackers than defenders
      // not-king piece
      if(pieceSide == Side::WHITE ? blackAC>whiteAC : whiteAC>blackAC) {
        record.score+=UNDEFENDED_PIECE_PENALTY_WITH_ATTACKERS*pieceSign;
      }

      // no attackers, 1+ defenders
      if(pieceSide == Side::WHITE ? (blackAC==0) && (whiteAC > 0) : (whiteAC==0)&&(blackAC>0)) {
        record.score+=NO_ATTACKERS_HAVE_DEFENDERS_BONUS*pieceSign;
      }
    }
  }

  if(record.moves.size() == 0) {
    setExactScore(record, STALEMATE_SCORE*movingSideSign);
    return record;
  }

  // TODO: doubled pawn penalty
  // TODO: positive/negative attack balance penalty/bonus ?

  record.evalStatus = EvalStatus::DONE_COMPLETE;
  return record;
}

inline void swapMoves(std::vector<Move>& moves, size_t from, size_t to) {
  if(from!=to) {
    Move tmp = moves[from];
    moves[from]=moves[to];
    moves[to] = tmp;
  }
}

#if SORT_MOVES == 1
struct MoveScore{
  MoveScore(){}
  MoveScore(Move move, Side movingSide):move(move),movingSide(movingSide) {}
  void setRecord(EvalRecord* record) {
    this->record = record;
  }
  
  Move move;
  Side movingSide;
  EvalRecord* record{nullptr};

  static bool bestScoreFirst(const MoveScore& lhs, const MoveScore& rhs) {
    if((lhs.record==nullptr) != (rhs.record==nullptr)){
      return lhs.record!=nullptr;
    }
    if((lhs.record==nullptr)&&(rhs.record==nullptr)) {
      int16_t ltypeScore = lhs.move.getMoveType()==MoveType::CAPTURE ? 1 : 0;
      int16_t rtypeScore = rhs.move.getMoveType()==MoveType::CAPTURE ? 1 : 0;
      if(ltypeScore!=rtypeScore) {
        return ltypeScore>rtypeScore;
      }
      return lhs.move.data<rhs.move.data;
    }

    // both moves have record
    if(lhs.record->evalStatus!=rhs.record->evalStatus) {
      return lhs.record->evalStatus==EvalStatus::DONE_COMPLETE;
    }

    int16_t lscore = lhs.record->evalStatus == EvalStatus::DONE_PARTIAL ? (lhs.movingSide == Side::WHITE ? lhs.record->minWhite : lhs.record->maxBlack) : lhs.record->score;
    int16_t rscore = rhs.record->evalStatus == EvalStatus::DONE_PARTIAL ? (rhs.movingSide == Side::WHITE ? rhs.record->minWhite : rhs.record->maxBlack) : rhs.record->score;

    if(lscore==rscore) {
      int16_t ltypeScore = lhs.move.getMoveType()==MoveType::CAPTURE ? 1 : 0;
      int16_t rtypeScore = rhs.move.getMoveType()==MoveType::CAPTURE ? 1 : 0;
      if(ltypeScore!=rtypeScore) {
        return ltypeScore>rtypeScore;
      }
      lscore = lhs.record->score;
      rscore = rhs.record->score;
    }

    // record scores are different
    if(lscore!=rscore) {
      return (lhs.movingSide==Side::WHITE) ? lscore>rscore : lscore<rscore;
    }

    // record scores are same
    return lhs.move.data<rhs.move.data;
  }
  
};

void sortMoveScores(std::vector<MoveScore>& moveScores, std::vector<Move>& moves, size_t sortBeginIndex, size_t sortEndIndex) {
  std::sort(moveScores.begin()+sortBeginIndex,moveScores.begin()+sortEndIndex, &MoveScore::bestScoreFirst);
  for(int i=sortBeginIndex;i<sortEndIndex;i++) {
    moves[i] = moveScores[i].move;
  }
}
#endif

enum class SearchMode:uint8_t{
  // search all moves
  REGULAR=0,
  // search capture moves and moves and end check
  QUIET=1
};

EvalResult Engine::evaluate(const Board& board, EvalContext& context, int16_t toDepth, int16_t minWhite, int16_t maxBlack, int16_t toQsDepth, bool fromQuietMove) {
  auto it = evals.find(board);
  EvalRecord* record;
  
  // Get eval record or create new record and run heuristics
  if(it != evals.end()) {
    record = &(it->second);
  } else {
    auto insertRes = evals.insert(std::pair<Board,EvalRecord>(board, Engine::evaluateBoard(board)));
    record = &(insertRes.first->second);
    context.nodesEvaluated++;
    if(context.nodesEvaluated % context.nodesEvaluatedCallbackInterval == 0) {
      context.nodesEvaluatedCallback();
      if(context.searchShouldTimeout()) {
        return EvalResult(nullptr, EvalResultCode::TIMEOUT, 0);
      }
    }
  }

  // Handle evaluation cycle
  if(record->evalStatus == EvalStatus::IN_EVALUATION) {
    return EvalResult(nullptr, EvalResultCode::LOOP, 0);
  }

  // Handle partial record: if depth is sufficient and min/max cutoff applies, return cutoff.
  // Else do regular move search
  if(record->evalStatus == EvalStatus::DONE_PARTIAL) {
    if(record->evalDepth >= toDepth && record->qsEvalDepth >= toQsDepth) {
      if(board.getMovingSide() == Side::WHITE && record->maxBlack >= maxBlack) {
        return EvalResult(record, EvalResultCode::SUCCESS, maxBlack);
      }
      if(board.getMovingSide() == Side::BLACK && record->minWhite <= minWhite) {
        return EvalResult(record, EvalResultCode::SUCCESS, minWhite);
      }
    }
    
    // Partial record is not useful to this search, do regular search
    record->evalDepth=0;
    record->qsEvalDepth=0;
    record->evalStatus = EvalStatus::DONE_COMPLETE;
  }


  // is eval for quiet position?
  bool quietSearchRequired = !record->isQuietPosition || !fromQuietMove;
  // regular search + qs search cases:
  // case #1: record depth < toDepth, do regular move search
  // case #2: record depth >= toDepth, position is quiet: return record
  // case #3: record depth >= toDepth, position is not quiet, record qsDepth < toQsDepth: do quiet search
  // case #4: record depth >= toDepth, position is not quiet, record qsDepth >= toQsDepth: return record

  // case #2
  bool quietAndDepthAchieved = record->evalDepth >= toDepth && !quietSearchRequired;
  // case #4
  bool notQuietAndDepthAchievedAndQsDepthAchived = record->evalDepth >= toDepth && record->qsEvalDepth>=toQsDepth;
  if(quietAndDepthAchieved || notQuietAndDepthAchievedAndQsDepthAchived) {
    return EvalResult(record, EvalResultCode::SUCCESS, record->score);
  }

  SearchMode searchMode = (record->evalDepth < toDepth) ? SearchMode::REGULAR : SearchMode::QUIET;

  int16_t newScore = board.getMovingSide() == Side::WHITE ? MIN_SCORE : MAX_SCORE;

  size_t bestMoveIndex=0;
  Move bestMove;

  #if SORT_MOVES == 1
  std::vector<MoveScore> moveScores(record->moves.size());
  for(int moveIndex = 0;moveIndex<record->moves.size();moveIndex++) {
    moveScores[moveIndex] = MoveScore(record->moves[moveIndex], board.getMovingSide());
  }
  #endif

  record->evalStatus = EvalStatus::IN_EVALUATION;
  for(int moveIndex = 0;moveIndex<record->moves.size();moveIndex++) {
    const Move& move = record->moves[moveIndex];
    bool examineMove = searchMode == SearchMode::REGULAR || !record->isQuietPosition || move.getMoveType()==MoveType::CAPTURE;
    if(!examineMove) {
      continue;
    }

    bool quietMove = record->isQuietPosition && move.getMoveType() == MoveType::MOVE;
    Board nextBoard = Board::makeMove(board, move.getFrom(), move.getTo(), move.getPromotionType());
    EvalResult nextEvalResult = evaluate(nextBoard, context, toDepth > 0 ? toDepth-1 : 0, minWhite, maxBlack, toDepth > 0 ? toQsDepth : toQsDepth-1, quietMove);
    if(nextEvalResult.result == EvalResultCode::TIMEOUT) {
      record->evalStatus = EvalStatus::DONE_COMPLETE;
      record->evalDepth=0;
      record->qsEvalDepth=0;
      return EvalResult(nullptr, EvalResultCode::TIMEOUT, 0);
    } else if(nextEvalResult.result == EvalResultCode::LOOP) {
      continue;
    } else if(nextEvalResult.result!= EvalResultCode::SUCCESS) {
      assert(nextEvalResult.result == EvalResultCode::SUCCESS);
    }

    #if SORT_MOVES == 1
    moveScores[moveIndex].setRecord(nextEvalResult.record);
    #endif

    if((board.getMovingSide() == Side::WHITE && newScore < nextEvalResult.score)
      || (board.getMovingSide() == Side::BLACK && newScore > nextEvalResult.score)) {
      newScore = nextEvalResult.score;
      bestMoveIndex = moveIndex;
      bestMove = move;
    }
    
    if(board.getMovingSide() == Side::WHITE) {
      // alphabeta max
      if(newScore >= maxBlack) {
        record->maxBlack = maxBlack;
        record->evalStatus = EvalStatus::DONE_PARTIAL;
        record->evalDepth = toDepth;
        record->qsEvalDepth = toQsDepth;
        record->bestMove=move;
        #if SORT_MOVES == 1
        sortMoveScores(moveScores, record->moves, 1, moveIndex+1);
        #else 
        swapMoves(record->moves, 0, moveIndex);
        #endif
        return EvalResult(record, EvalResultCode::SUCCESS, maxBlack);
      }
      if(newScore > minWhite) {
        minWhite = newScore;
      }
    } else {
      //alphabeta min
      if(newScore<=minWhite) {
        record->minWhite = minWhite;
        record->evalStatus = EvalStatus::DONE_PARTIAL;
        record->evalDepth = toDepth;
        record->qsEvalDepth = toQsDepth;
        record->bestMove=move;
        #if SORT_MOVES == 1
        sortMoveScores(moveScores, record->moves, 1, moveIndex + 1);
        #else
        swapMoves(record->moves, 0, moveIndex);
        #endif
        return EvalResult(record, EvalResultCode::SUCCESS, minWhite);
      }
      if(newScore<maxBlack) {
        maxBlack = newScore;
      }
    }
  }

  if(newScore != MIN_SCORE && newScore !=MAX_SCORE) {
    record->score = newScore;
  } else {
    // quiet search found no capture moves / post-check moves, return heuristic result
  }
  record->evalStatus = EvalStatus::DONE_COMPLETE;
  record->evalDepth = toDepth;
  record->qsEvalDepth = toQsDepth;
  record->bestMove=bestMove;
  #if SORT_MOVES == 1
  sortMoveScores(moveScores, record->moves, 0, record->moves.size());
  #else
  swapMoves(record->moves, 0, bestMoveIndex);
  #endif
  
  if(std::abs(record->score)>AFTER_CHECKMATE_SCORE/2) {
    // adjust score for mate distance
    record->score += (record->score>0) ? DISTANT_CHECKMATE_DECAY : -DISTANT_CHECKMATE_DECAY;
  }

  return EvalResult(record, EvalResultCode::SUCCESS, record->score);
}

EvalRecord* Engine::findRecord(const Board& board) {
  auto it = evals.find(board);
  if(it != evals.end()) {
    return &(it->second);
  }
  return nullptr;
}

Move Engine::findBestMove(const Board& board, int16_t toDepth, int16_t toQsDepth, int16_t allowedTimeMs) {
  toDepth = toDepth > MAX_DEPTH ? MAX_DEPTH : toDepth;
  EvalContext evalContext(true, allowedTimeMs, toDepth);

  // Trim evals table if needed
  if(evals.size()>TRIM_TABLE_SIZE) {
    evals = std::unordered_map<Board, EvalRecord, Hasher<Board>>();
  }
  
  std::stringstream ss;
  ss << "Started findBestMove to depth " << toDepth;
  Log::log(ss.str());
  
  bool haveTimeForMoreSearch = false;
  EvalRecord* record = nullptr;
  for(int depth=std::min(toDepth,(int16_t)3);depth<=toDepth || haveTimeForMoreSearch;depth++){
    EvalResult result = evaluate(board, evalContext, depth, MIN_SCORE, MAX_SCORE, toQsDepth, true);
    if(result.result==EvalResultCode::SUCCESS){
      record = result.record;
    } else {
      break;
    }

    evalContext.depthAchieved = depth;

    ss.str("");
    ss << "findBestMove at depth " << depth << " took " << evalContext.getMsSinceStartTime() << "ms. Evaluated boards: " << evalContext.nodesEvaluated << ". Eval result: " << (int16_t)result.result
      << ". Best move " << record->bestMove.print() << ", score "<<record->score;
    Log::log(ss.str());
    haveTimeForMoreSearch=(allowedTimeMs>0 && depth<toDepth*2 && evalContext.getMsSinceStartTime() < (allowedTimeMs / 6));
  }
  auto endTime = std::chrono::steady_clock::now();
  
  ss.str("");
  ss << "info score cp " << (board.getMovingSide() == Side::WHITE?1:-1) * record->score;
  loggedcoutline(ss.str());

  ss.str("");
  ss << "Done findBestMove in " << evalContext.getMsSinceStartTime() << "ms. Eval: " << (record->score/100.0);
  Log::log(ss.str());

  ss.str("");
  ss << "Best move sequence: ";
  auto moveSeq = getBestMoveSequence(board);
  for(const auto& move:moveSeq){
    ss << move.print() << " ";
  }
  std::unordered_set<Board, Hasher<Board>> seenBoards;
  Board curBoard = board;
  EvalRecord* curRecord = record;
  while(curRecord!=nullptr && curRecord->evalDepth > 0 && curRecord->moves.size()>0) {
    seenBoards.insert(curBoard);
    ss << curRecord->bestMove.print() << " ";
    curBoard = Board::makeMove(curBoard, curRecord->bestMove);
    curRecord = findRecord(curBoard);
    if(seenBoards.find(curBoard) != seenBoards.end()) {
      curRecord = nullptr;
    }
  }
  Log::log(ss.str());

  return record->bestMove;
}

std::vector<Move> Engine::getBestMoveSequence(const Board& board) {
  std::vector<Move> res;
  std::unordered_set<Board, Hasher<Board>> seenBoards;
  Board curBoard = board;
  EvalRecord* curRecord = findRecord(board);
  while(curRecord!=nullptr && curRecord->evalDepth > 0 && curRecord->moves.size()>0) {
    seenBoards.insert(curBoard);
    res.push_back(curRecord->bestMove);
    if(curRecord->moves.size() == 0) {
     break; 
    }
    curBoard = Board::makeMove(curBoard, curRecord->bestMove);
    curRecord = findRecord(curBoard);
    if(seenBoards.find(curBoard) != seenBoards.end()) {
      curRecord = nullptr;
    }
  }
  return res;
}

EvalContext::EvalContext(bool trackTime, int32_t allowedTimeMs, int16_t depthRequired) {
  if(trackTime) {
    startTime = std::chrono::steady_clock::now();
    lastReportTime = startTime;
  }
  this->allowedRunTimeMs = allowedTimeMs;
  this->depthRequired = depthRequired;
}

int32_t EvalContext::getMsSinceStartTime(){
  return (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-startTime)).count();
}

void EvalContext::nodesEvaluatedCallback() {
  auto now = std::chrono::steady_clock::now();
  if((now - lastReportTime) > std::chrono::milliseconds(1000)) {
    lastReportTime = now;
    std::stringstream ss;
    ss << getMsSinceStartTime() << "ms evaluated nodes: " << nodesEvaluated;
    Log::log(ss.str());
    ss.str("");
    ss << "info depth " << depthAchieved << " nodes " << nodesEvaluated << " nps " << (int32_t)(1000 * (double)nodesEvaluated/(getMsSinceStartTime()+1));
    loggedcoutline(ss.str());
  }
}

bool EvalContext::searchShouldTimeout() {
  return depthAchieved>=depthRequired && (allowedRunTimeMs > 0) &&  (getMsSinceStartTime() > 2 * allowedRunTimeMs);
}

}

