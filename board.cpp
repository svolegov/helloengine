#include <assert.h>
#include <sstream>

#include "board.h"

namespace chesseng {

Board::Board() {
  squares.fill(Square(0));
}

std::string Board::logBoard() const {
  std::stringstream out;
  std::array<std::string, 8> boardPrint;
  for(size_t row=0;row<8;row++) {
    boardPrint[row]=std::string(8, '.');
    for(size_t col=0;col<8;col++){
      Square square = getSquare(Position(row,col));
      PieceType pieceType = square.getPieceType();
      char pchar = '?';
      if(pieceType==PieceType::NO_PIECE) {
        pchar = '.';
      } else if(pieceType==PieceType::PAWN_PIECE) {
        pchar = square.getSideBit() == SideBit::WHITE ? 'p':'P';
      } else if(pieceType==PieceType::ROOK_PIECE) {
        pchar = square.getSideBit() == SideBit::WHITE ? 'r':'R';
      } else if(pieceType==PieceType::KNIGHT_PIECE) {
        pchar = square.getSideBit() == SideBit::WHITE ? 'n':'N';
      } else if(pieceType==PieceType::BISHOP_PIECE) {
        pchar = square.getSideBit() == SideBit::WHITE ? 'b':'B';
      } else if(pieceType==PieceType::QUEEN_PIECE) {
        pchar = square.getSideBit() == SideBit::WHITE ? 'q':'Q';
      } else if(pieceType==PieceType::KING_PIECE) {
        pchar = square.getSideBit() == SideBit::WHITE ? 'k':'K';
      }
      boardPrint[row][col]=pchar;
    }
  }

  out << "Move:" << ((gamestate&WHOSE_TURN_BIT)?"BLACK":"WHITE")<<std::endl;
  for(int8_t row=7;row>=0;row--) {
    out << boardPrint[row] << std::endl;
  }
  return out.str();
}

void Board::startingPosition() {
  for(size_t col=0;col<8;col++) {
    setSquare(Position(1,col), Square(PieceType::PAWN_PIECE, SideBit::WHITE));
    setSquare(Position(6,col), Square(PieceType::PAWN_PIECE, SideBit::BLACK));
  }
  setSquare(Position(0,0), Square(PieceType::ROOK_PIECE, SideBit::WHITE));
  setSquare(Position(0,7), Square(PieceType::ROOK_PIECE, SideBit::WHITE));
  setSquare(Position(7,0), Square(PieceType::ROOK_PIECE, SideBit::BLACK));
  setSquare(Position(7,7), Square(PieceType::ROOK_PIECE, SideBit::BLACK));
  setSquare(Position(0,1), Square(PieceType::KNIGHT_PIECE, SideBit::WHITE));
  setSquare(Position(0,6), Square(PieceType::KNIGHT_PIECE, SideBit::WHITE));
  setSquare(Position(7,1), Square(PieceType::KNIGHT_PIECE, SideBit::BLACK));
  setSquare(Position(7,6), Square(PieceType::KNIGHT_PIECE, SideBit::BLACK));
  setSquare(Position(0,2), Square(PieceType::BISHOP_PIECE, SideBit::WHITE));
  setSquare(Position(0,5), Square(PieceType::BISHOP_PIECE, SideBit::WHITE));
  setSquare(Position(7,2), Square(PieceType::BISHOP_PIECE, SideBit::BLACK));
  setSquare(Position(7,5), Square(PieceType::BISHOP_PIECE, SideBit::BLACK));
  setSquare(Position(0,3), Square(PieceType::QUEEN_PIECE, SideBit::WHITE));
  setSquare(Position(7,3), Square(PieceType::QUEEN_PIECE, SideBit::BLACK));
  setSquare(Position(0,4), Square(PieceType::KING_PIECE, SideBit::WHITE));
  setSquare(Position(7,4), Square(PieceType::KING_PIECE, SideBit::BLACK));

  setMovingSide(Side::WHITE);
}

Board Board::makeMove(const Board& board, Move move) {
  return Board::makeMove(board, move.getFrom(), move.getTo(), move.getPromotionType());
}
Board Board::makeMove(const Board& board, Position fromPos, Position toPos, PieceType promotionType) {
  Board result(board);

  // clear moved twice bit
  for(int8_t row=3;row<=4;row++){
    for(int8_t col=0;col<8;col++) {
      Square maybePawn = result.getSquare(row,col);
      if(maybePawn.getPawnMovedTwiceBit()==PawnMovedTwiceBit::YES && maybePawn.getPieceType()==PieceType::PAWN_PIECE) {
        result.setSquare(Position(row,col),Square(maybePawn.getPieceType(),maybePawn.getSideBit(),maybePawn.getMovedBit()));
      }
    }
  }
  
  result.setMovingSide(result.getMovingSide() == Side::WHITE ? Side::BLACK : Side::WHITE);
  Square movingPiece = result.getSquare(fromPos);

  result.setSquare(fromPos, Square(PieceType::NO_PIECE, SideBit::WHITE));
  result.setSquare(toPos, Square(movingPiece.getPieceType(), movingPiece.getSideBit(), MovedBit::YES));

  
  if(movingPiece.getPieceType() == PieceType::PAWN_PIECE) {
    // en passant
    if(fromPos.getCol() != toPos.getCol() && board.getSquare(toPos).getPieceType() == PieceType::NO_PIECE) {
      int8_t enpassantCaptureRow = fromPos.getRow();
      int8_t enpassantCaptureCol = toPos.getCol();
      result.setSquare(Position(enpassantCaptureRow, enpassantCaptureCol), Square(PieceType::NO_PIECE, SideBit::WHITE));
    }
    // moved twice
    int16_t rowDelta =(int16_t)fromPos.getRow()-(int16_t)toPos.getRow();
    if(std::abs(rowDelta) == 2) {
      result.setSquare(toPos, Square(movingPiece.getPieceType(), movingPiece.getSideBit(), MovedBit::YES, PawnMovedTwiceBit::YES));
    }
  }

  // castling
  if(movingPiece.getPieceType() == PieceType::KING_PIECE) {
    if(fromPos.getCol() == 4 && toPos.getCol() == 6) {
      // short castling: K col 4=>6
      Square expectRook = result.getSquare(fromPos.getRow(), 7);
      result.setSquare(Position(fromPos.getRow(),7), Square(PieceType::NO_PIECE, SideBit::WHITE));
      result.setSquare(Position(fromPos.getRow(),5), Square(expectRook.getPieceType(),expectRook.getSideBit(),MovedBit::YES));
    } else if (fromPos.getCol() == 4 && toPos.getCol() == 2) {
      // long castling: K col 4=>2
      Square expectRook = result.getSquare(fromPos.getRow(), 0);
      result.setSquare(Position(fromPos.getRow(),0), Square(PieceType::NO_PIECE, SideBit::WHITE));
      result.setSquare(Position(fromPos.getRow(),3),  Square(expectRook.getPieceType(),expectRook.getSideBit(),MovedBit::YES));
    }
  }

  // promotion
  if(promotionType != PieceType::NO_PIECE) {
    result.setSquare(toPos, Square(promotionType, movingPiece.getSideBit(), MovedBit::YES));
  }

  return result;
}
Board Board::makeMove(const Board& board, std::string move) {
  int8_t fromCol = move.at(0) - 'a';
  int8_t fromRow = move.at(1) - '1';
  int8_t toCol = move.at(2) - 'a';
  int8_t toRow = move.at(3) - '1';
  Position fromPos = Position(fromRow,fromCol);
  Position toPos = Position(toRow,toCol);
  PieceType promotionType = PieceType::NO_PIECE;

  // promotion
  if(move.size()>4) {
    char newPieceType = move.at(4);
    promotionType = PieceType::PAWN_PIECE;
    if(newPieceType=='q') {
      promotionType = PieceType::QUEEN_PIECE;
    } else if(newPieceType=='r') {
      promotionType = PieceType::ROOK_PIECE;
    } else if(newPieceType=='b') {
      promotionType = PieceType::BISHOP_PIECE;
    } else if(newPieceType=='n') {
      promotionType = PieceType::KNIGHT_PIECE;
    } else {
      assert(false);
    }
  }

  return makeMove(board, fromPos, toPos,promotionType);
}

std::string Move::print() const {
  std::string res(4,'0');
  res[0] = 'a' + getFrom().getCol();
  res[1] = '1' + getFrom().getRow();
  res[2] = 'a' + getTo().getCol();
  res[3] = '1' + getTo().getRow();
  PieceType promotionType = getPromotionType();
  if(promotionType!=PieceType::NO_PIECE) {
    if(promotionType==PieceType::PAWN_PIECE) {
      res+="p";
    } else if(promotionType==PieceType::ROOK_PIECE) {
      res+="r";
    } else if(promotionType==PieceType::BISHOP_PIECE) {
      res+="b";
    } else if(promotionType==PieceType::KNIGHT_PIECE) {
      res+="n";
    } else if(promotionType==PieceType::QUEEN_PIECE) {
      res+="q";
    }
  }
  return res;
}
  
}

