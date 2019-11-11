#pragma once

#include <array>
#include <iostream>
#include <vector>
#include <string>

#include "log.h"

namespace chesseng {

// piece struct:
// 0-3 bit: piece type
// 4 bit: side
constexpr uint8_t PIECE_MASK=0b111; // 7
constexpr uint8_t SIDE_BIT=(1<<3);
constexpr uint8_t MOVED_BIT=(1<<4);
constexpr uint8_t PAWN_MOVED_TWICE_BIT=(1<<5);

// gamestate struct:
// 0 bit: whose move
//
constexpr uint8_t WHOSE_TURN_BIT = 1;
enum class WhoseTurnBit: uint8_t {
  WHITE = 0,
  BLACK = WHOSE_TURN_BIT
};

inline bool rowcolok(int8_t rowcol) {
  return rowcol>=0 && rowcol<8;
}

enum class Side: uint8_t {
  WHITE=0,
  BLACK=1
};

enum class SideBit: uint8_t {
  WHITE=0,
  BLACK=SIDE_BIT
};

enum class PieceType: uint8_t {
  NO_PIECE=0,
  PAWN_PIECE=1,
  ROOK_PIECE=2,
  KNIGHT_PIECE=3,
  BISHOP_PIECE=4,
  QUEEN_PIECE=5,
  KING_PIECE=6
};

enum class MovedBit: uint8_t {
  NO=0,
  YES=MOVED_BIT
};

enum class PawnMovedTwiceBit: uint8_t {
  NO=0,
  YES=PAWN_MOVED_TWICE_BIT
};

struct Square {
  public:
  inline Square():data(0){}
  explicit inline Square(uint8_t data):data(data){}

  inline Square(PieceType piece, SideBit sideBit, MovedBit movedBit=MovedBit::NO, PawnMovedTwiceBit movedTwiceBit=PawnMovedTwiceBit::NO) {
    data = static_cast<uint8_t>(piece) | static_cast<uint8_t>(sideBit) | static_cast<uint8_t>(movedBit) |static_cast<uint8_t>(movedTwiceBit);
  }

  inline PieceType getPieceType(){
    return static_cast<PieceType>(data & PIECE_MASK);
  }

  inline SideBit getSideBit() {
    return static_cast<SideBit>(data & SIDE_BIT);
  }

  inline MovedBit getMovedBit() {
    return static_cast<MovedBit>(data & MOVED_BIT);
  }

  inline PawnMovedTwiceBit getPawnMovedTwiceBit() {
    return static_cast<PawnMovedTwiceBit>(data & PAWN_MOVED_TWICE_BIT);
  }

  uint8_t data;  
};

struct Position {
  public:
  Position(uint8_t row, uint8_t col) {
    data = (row << 3) + col;
  }
  explicit Position(uint8_t data):data(data) {
  }

  uint8_t getRow(){
    return data >> 3;
  }

  uint8_t getCol() {
    return data & 0b111;
  }

  std::string print() {
    std::string res("00");
    res[0]='a'+getCol();
    res[1]='1'+getRow();
    return res;
  }

  uint8_t data;
};

enum class MoveType: uint8_t {
  MOVE=0,
  CAPTURE=1
};

struct Move {
  Move(Position from, Position to, MoveType type, PieceType promotionType = PieceType::NO_PIECE){
    data = (static_cast<uint8_t>(promotionType)<<18) + (static_cast<uint8_t>(type)<<12) + (from.data<<6) + to.data;
  }
  Move():data(0){}


  Position getFrom() const {
    return Position((data >> 6) & 0b111111);
  }
  Position getTo() const {
    return Position(data & 0b111111);
  }
  MoveType getMoveType() const {
    return static_cast<MoveType>((data >> 12) & 0b111111);
  }
  PieceType getPromotionType() const {
    return static_cast<PieceType>((data>>18) & 0b111111);
  }
  std::string print() const;

  uint32_t data;
};

struct Board{
  public:
  Board();
  std::string logBoard() const;
  void startingPosition();
  inline void setSquare(Position pos, Square square) {
    squares[pos.data] = square;
  }
  inline Square getSquare(Position pos) const {
    return squares[pos.data];
  }
  inline Square getSquare(int8_t row, int8_t col) const {
    return squares[Position(row,col).data];
  }

  inline Side getMovingSide() const {
    WhoseTurnBit whoseSide = static_cast<WhoseTurnBit>(gamestate & WHOSE_TURN_BIT);
    return whoseSide == WhoseTurnBit::WHITE ? Side::WHITE : Side::BLACK;
  }

  inline void setMovingSide(Side side) {
    if(side == Side::WHITE) {
      gamestate &= ~(WHOSE_TURN_BIT);
    } else {
      gamestate |= WHOSE_TURN_BIT;
    }
  }

  static Board makeMove(const Board& board, std::string move);
  static Board makeMove(const Board& board, Move move);
  static Board makeMove(const Board& board, Position fromPos, Position toPos, PieceType promoteType);

  static inline Side getSide(SideBit sideBit) {
    return sideBit == SideBit::WHITE ? Side::WHITE : Side::BLACK;
  }
  static inline SideBit getSideBit(Side side) {
    return side == Side::WHITE ? SideBit::WHITE : SideBit::BLACK;
  }
  static inline int8_t getSideSign(Side side) {
    return side == Side::WHITE ? 1 : -1;
  }

  std::array<Square,64> squares;
  uint8_t gamestate{0};
};

inline bool operator==(const Board& lhs, const Board& rhs){
  for(int i=0;i<lhs.squares.size();i++) {
    if(lhs.squares[i].data!=rhs.squares[i].data) {
      return false;
    }
  }
  return lhs.gamestate == rhs.gamestate;
}

template <class T>
class Hasher;

template<>
struct Hasher<Board>
{
    std::size_t operator()(const Board& board) const 
    {
      size_t res = 0;
      for(int i=0;i<board.squares.size();i+=4) {
        res^= (size_t)board.squares[i].data
          + (((size_t)board.squares[i+1].data)<<8)
          + (((size_t)board.squares[i+2].data)<<16) 
          + (((size_t)board.squares[i+3].data)<<24)
          + 0x9e3779b9 + (res<< 6) + (res>> 2);
      }
      
      res += board.gamestate;
      return res;
    }
};

}

