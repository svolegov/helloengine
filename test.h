#pragma once

#include <array>
#include <assert.h>
#include <iostream>
#include <vector>
#include <string>

#include "board.h"
#include "engine.h"
#include "log.h"

namespace chesseng {
void test_boardEvalPawnRook() {
  Board board;
  board.setSquare(Position(1,3),Square(PieceType::PAWN_PIECE, SideBit::WHITE));
  board.setSquare(Position(3,7),Square(PieceType::PAWN_PIECE, SideBit::WHITE));
  board.setSquare(Position(3,2),Square(PieceType::ROOK_PIECE, SideBit::WHITE));
  board.setSquare(Position(2,2),Square(PieceType::PAWN_PIECE, SideBit::BLACK));
  board.setMovingSide(Side::WHITE);
  EvalRecord record = Engine::evaluateBoard(board);
  assert(record.moves.size() == 3 + 1 + 11 + 0);
  assert(record.score>500 && record.score<1000);
  assert(record.evalStatus==EvalStatus::DONE_COMPLETE);
}

void test_boardEvalStalemate() {
  Board board;
  board.setSquare(Position(1,1),Square(PieceType::ROOK_PIECE, SideBit::BLACK));
  board.setSquare(Position(7,1),Square(PieceType::ROOK_PIECE, SideBit::BLACK));
  board.setSquare(Position(0,0),Square(PieceType::KING_PIECE, SideBit::WHITE));
  board.setMovingSide(Side::WHITE);
  EvalRecord record = Engine::evaluateBoard(board);
  assert(record.moves.size() == 0);
  assert(record.score>-400 && record.score<=0);
  assert(record.evalStatus==EvalStatus::DONE_COMPLETE);
}

void test_boardEvalAfterCheckmate() {
  Board board;
  board.setSquare(Position(1,1),Square(PieceType::ROOK_PIECE, SideBit::BLACK));
  board.setSquare(Position(0,1),Square(PieceType::ROOK_PIECE, SideBit::BLACK));
  board.setSquare(Position(0,0),Square(PieceType::KING_PIECE, SideBit::WHITE));
  board.setMovingSide(Side::BLACK);
  EvalRecord record = Engine::evaluateBoard(board);
  assert(record.score<-2000);
  assert(record.evalStatus==EvalStatus::DONE_COMPLETE);
}

void test_boardEvalPawnBishop() {
  Board board;
  board.setSquare(Position(0,0),Square(PieceType::PAWN_PIECE, SideBit::WHITE));
  board.setSquare(Position(1,5),Square(PieceType::PAWN_PIECE, SideBit::WHITE));
  board.setSquare(Position(6,6),Square(PieceType::PAWN_PIECE, SideBit::BLACK));
  board.setSquare(Position(3,3),Square(PieceType::BISHOP_PIECE, SideBit::BLACK));
  board.setMovingSide(Side::BLACK);
  EvalRecord record = Engine::evaluateBoard(board);
  assert(record.moves.size() == 2 + 10);
  assert(record.score<-200 && record.score>-1000);
  assert(record.evalStatus==EvalStatus::DONE_COMPLETE);
}

void test_boardEvalPawnKnightQueen() {
  Board board;
  board.setSquare(Position(3,4),Square(PieceType::KNIGHT_PIECE, SideBit::BLACK));
  board.setSquare(Position(6,2),Square(PieceType::PAWN_PIECE, SideBit::BLACK));
  board.setSquare(Position(2,6),Square(PieceType::PAWN_PIECE, SideBit::BLACK));
  board.setSquare(Position(4,6),Square(PieceType::PAWN_PIECE, SideBit::WHITE));
  board.setSquare(Position(6,4),Square(PieceType::QUEEN_PIECE, SideBit::BLACK));
  board.setMovingSide(Side::BLACK);
  EvalRecord record = Engine::evaluateBoard(board);
  assert(record.moves.size() == 2 + 1 + 7 + 7 + 8);
  assert(record.score<-1300 && record.score>-2000);
  assert(record.evalStatus==EvalStatus::DONE_COMPLETE);
}

void test_moveCastling(){
  Board board;
  board.setSquare(Position(0,0),Square(PieceType::ROOK_PIECE, SideBit::WHITE));
  board.setSquare(Position(0,7),Square(PieceType::ROOK_PIECE, SideBit::WHITE));
  board.setSquare(Position(0,4),Square(PieceType::KING_PIECE, SideBit::WHITE));
  board.setMovingSide(Side::WHITE);
  EvalRecord record = Engine::evaluateBoard(board);
  // castling valid both long and short
  assert(record.moves.size() == 10 + 9 + 5 + 2);

  // rook moved
  Board rookMovedBoard = Board::makeMove(board, Move(Position(0,0),Position(1,0), MoveType::MOVE));
  rookMovedBoard = Board::makeMove(rookMovedBoard, Move(Position(0,0),Position(0,0), MoveType::MOVE));
  rookMovedBoard = Board::makeMove(rookMovedBoard, Move(Position(1,0),Position(0,0), MoveType::MOVE));
  rookMovedBoard = Board::makeMove(rookMovedBoard, Move(Position(0,0),Position(0,0), MoveType::MOVE));
  assert(board.getMovingSide() == Side::WHITE);
  record = Engine::evaluateBoard(rookMovedBoard);
  assert(record.moves.size() == 10 + 9 + 5 + 1);

  // blocking piece
  board.setSquare(Position(0,6),Square(PieceType::PAWN_PIECE,SideBit::WHITE));
  record = Engine::evaluateBoard(board);
  assert(record.moves.size() == 10 + 7 + 5 + 1 + 1);

  // attacking piece
  board.setSquare(Position(7,2), Square(PieceType::ROOK_PIECE,SideBit::BLACK));
  record = Engine::evaluateBoard(board);
  assert(record.moves.size() == 10 + 7 + 5 + 1 + 0);
}

void test_moveEnpassant(){
  Board board;
  board.setSquare(Position(1,0),Square(PieceType::PAWN_PIECE, SideBit::WHITE));
  board.setSquare(Position(1,1),Square(PieceType::PAWN_PIECE, SideBit::WHITE));
  board.setSquare(Position(3,2),Square(PieceType::PAWN_PIECE, SideBit::BLACK));
  board.setSquare(Position(3,6),Square(PieceType::PAWN_PIECE, SideBit::BLACK));
  board.setMovingSide(Side::WHITE);

  Board boardEP = Board::makeMove(board, Move(Position(1,1),Position(3,1),MoveType::MOVE));
  EvalRecord record = Engine::evaluateBoard(boardEP);
  assert(record.moves.size() == 3);

  // black move, ep clears on col 1
  Board boardPastEP1 = Board::makeMove(boardEP,Move(Position(3,6),Position(2,6),MoveType::MOVE));
  // white move, ep not reachable on col 0
  Board boardPastEP2 = Board::makeMove(boardPastEP1,Move(Position(1,0),Position(3,0),MoveType::MOVE));
  record = Engine::evaluateBoard(boardPastEP2);
  assert(record.moves.size() == 2);
  
}

void test_quietSearch(){
  Engine engine;
  Board board;
  EvalContext context(true);
  board.setSquare(Position(5,2),Square(PieceType::PAWN_PIECE, SideBit::BLACK));
  board.setSquare(Position(2,6),Square(PieceType::PAWN_PIECE, SideBit::BLACK));
  board.setSquare(Position(0,7),Square(PieceType::KING_PIECE, SideBit::WHITE));
  board.setMovingSide(Side::BLACK);
  EvalResult result = engine.evaluate(board, context, 1, MIN_SCORE, MAX_SCORE, 2, true);
  assert(context.nodesEvaluated == 6);

  board = Board();
  context = EvalContext(true);  
  for(int8_t row=5;row<=7;row++){
    for(int8_t col=0;col<=1;col++){
      board.setSquare(Position(row,col),Square(PieceType::PAWN_PIECE,SideBit::WHITE));
    }
  }
  board.setSquare(Position(7,0),Square(PieceType::QUEEN_PIECE,SideBit::BLACK));
  board.setMovingSide(Side::BLACK);
  result = engine.evaluate(board, context, 1, MIN_SCORE, MAX_SCORE, 2, true);
  assert(context.nodesEvaluated > 5);
  assert(result.score > 100);

}

void test_all() {
  test_boardEvalPawnRook();
  test_boardEvalPawnBishop();
  test_boardEvalStalemate();
  test_boardEvalAfterCheckmate();
  test_boardEvalPawnKnightQueen();
  test_moveCastling();
  test_moveEnpassant();
  test_quietSearch();
  std::cout << "Tests passed";
}

}
