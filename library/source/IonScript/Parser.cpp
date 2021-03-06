/*******************************************************************************
 * IonScript                                                                   *
 * (c) 2010-2011 Canio Massimo Tristano <massimo.tristano@gmail.com>           *
 *                                                                             *
 * This software is provided 'as-is', without any express or implied           *
 * warranty. In no event will the authors be held liable for any damages       *
 * arising from the use of this software.                                      *
 *                                                                             *
 * Permission is granted to anyone to use this software for any purpose,       *
 * including commercial applications, and to alter it and redistribute it      *
 * freely, subject to the following restrictions:                              *
 *                                                                             *
 * 1. The origin of this software must not be misrepresented; you must not     *
 * claim that you wrote the original software. If you use this software        *
 * in a product, an acknowledgment in the product documentation would be       *
 * appreciated but is not required.                                            *
 *                                                                             *
 * 2. Altered source versions must be plainly marked as such, and must not be  *
 * misrepresented as being the original software.                              *
 *                                                                             *
 * 3. This notice may not be removed or altered from any source                *
 * distribution.                                                               *
 ******************************************************************************/

#include "Parser.h"
#include "SyntaxTree.h"

#include <cassert>

using namespace std;
using namespace ionscript;

//

Parser::Parser(std::istream& source) : mLexer(source) { }

void Parser::parse(SyntaxTree& tree) {
   mTokenType = mLexer.nextToken();
   block(tree, false);
   expect(Lexer::T_EOS);
}

//

void Parser::error() const {
   throw SyntaxError(mLexer.getLine(), mLexer.getColumn(), mTokenType, mLexer.getString());
}

void Parser::expect(Lexer::TokenType token) {
   if (mTokenType != token)
      error();
   else
      nextToken();
}

bool Parser::accept(Lexer::TokenType token) {
   if (mTokenType == token) {
      nextToken();
      return true;
   } else
      return false;
}

void Parser::endOfStatement() {
   if (!accept(Lexer::T_NEWLINE))
      if (!accept(Lexer::T_SEMICOLON))
         expect(Lexer::T_EOS);
}
//

void Parser::block(SyntaxTree& tree, int state) {
   tree.type = SyntaxTree::TYPE_BLOCK;
   while (true) {
      while (mTokenType == Lexer::T_NEWLINE || mTokenType == Lexer::T_SEMICOLON)
         nextToken();
      if (mTokenType == Lexer::T_EOS || mTokenType == Lexer::T_ELSE || mTokenType == Lexer::T_END)
         break;
      statement(*tree.createChild(), state);
   }
}

void Parser::statement(SyntaxTree& tree, int state) {
   tree.sourceLineNumber = mLexer.getLine();
   switch (mTokenType) {
      case Lexer::T_IF:
         ifblock(tree, state);
         break;

      case Lexer::T_WHILE:
         whileblock(tree, state);
         break;

      case Lexer::T_FOR:
         forblock(tree, state);
         break;

      case Lexer::T_DEF:
         if (state & STATE_INSIDE_FUNCTION)
            error();
         functionDefinition(tree);
         break;

      case Lexer::T_RETURN:
         if (!(state & STATE_INSIDE_FUNCTION))
            error();
         nextToken();
         tree.type = SyntaxTree::TYPE_RETURN;
         if (mTokenType != Lexer::T_NEWLINE)
            expression(*tree.createChild());
         endOfStatement();
         break;

      case Lexer::T_CONTINUE:
         if (!(state & STATE_INSIDE_LOOP))
            error();
         nextToken();
         tree.type = SyntaxTree::TYPE_CONTINUE;
         break;

      case Lexer::T_BREAK:
         if (!(state & STATE_INSIDE_LOOP))
            error();
         nextToken();
         tree.type = SyntaxTree::TYPE_BREAK;
         break;

      default:
         expression(tree);
         endOfStatement();
         break;
   }
   tree.simplify();
}

void Parser::functionDefinition(SyntaxTree& tree) {
   expect(Lexer::T_DEF);
   tree.type = SyntaxTree::TYPE_FUNCTION_DEF;
   tree.str = mLexer.getString();
   expect(Lexer::T_IDENTIFIER);

   if (accept(Lexer::T_LEFT_ROUND_BRACKET)) {
      if (!accept(Lexer::T_RIGHT_ROUND_BRACKET))
         do {
            if (mTokenType == Lexer::T_IDENTIFIER) {
               SyntaxTree* pTree = tree.createChild();
               pTree->type = SyntaxTree::TYPE_ARGUMENT;
               pTree->str = mLexer.getString();
               nextToken();
            } else
               error();
            accept(Lexer::T_COMMA);
         } while (!accept(Lexer::T_RIGHT_ROUND_BRACKET));
   }

   if (accept(Lexer::T_NEWLINE)) {
      block(*tree.createChild(), STATE_INSIDE_FUNCTION);
      expect(Lexer::T_END);
      //endOfStatement();
   } else {
      expect(Lexer::T_COLON);
      SyntaxTree* blockTree = tree.createChild();
      blockTree->type = SyntaxTree::TYPE_BLOCK;
      statement(*blockTree->createChild(), STATE_INSIDE_FUNCTION);
   }
}

void Parser::ifblock(SyntaxTree& tree, int state) {
   expect(Lexer::T_IF);
   tree.type = SyntaxTree::TYPE_IF;
   expression(*tree.createChild());

   if (accept(Lexer::T_NEWLINE)) {
      block(*tree.createChild(), state);

      if (!accept(Lexer::T_END)) {
         if (mTokenType == Lexer::T_ELSE)
            elseblock(*tree.createChild(), state);
         else
            error();
      }
   } else {
      expect(Lexer::T_COLON);
      SyntaxTree* blockTree = tree.createChild();
      blockTree->type = SyntaxTree::TYPE_BLOCK;
      statement(*blockTree->createChild(), state);
      if (mTokenType == Lexer::T_ELSE)
         elseblock(*tree.createChild(), state);
   }
}

void Parser::elseblock(SyntaxTree& tree, int state) {
   expect(Lexer::T_ELSE);
   if (mTokenType == Lexer::T_IF)
      ifblock(tree, state); //else if
   else {

      if (accept(Lexer::T_NEWLINE)) {
         block(tree, state); // else
         expect(Lexer::T_END);
      } else {
         expect(Lexer::T_COLON);
         tree.type = SyntaxTree::TYPE_BLOCK;
         statement(*tree.createChild(), state);
      }
   }
}

void Parser::whileblock(SyntaxTree& tree, int state) {
   expect(Lexer::T_WHILE);
   tree.type = SyntaxTree::TYPE_WHILE;
   expression(*tree.createChild());

   if (accept(Lexer::T_NEWLINE)) {
      block(*tree.createChild(), state | STATE_INSIDE_LOOP);
      expect(Lexer::T_END);
   } else {
      expect(Lexer::T_COLON);
      SyntaxTree* blockTree = tree.createChild();
      blockTree->type = SyntaxTree::TYPE_BLOCK;
      statement(*blockTree->createChild(), state);
   }
}

void Parser::forblock(SyntaxTree& tree, int state) {
   expect(Lexer::T_FOR);
   tree.type = SyntaxTree::TYPE_FOR;

   expression(*tree.createChild());

   expect(Lexer::T_SEMICOLON);

   expression(*tree.createChild());

   expect(Lexer::T_SEMICOLON);

   expression(*tree.createChild());

   if (accept(Lexer::T_NEWLINE)) {
      block(*tree.createChild(), state | STATE_INSIDE_LOOP);
      expect(Lexer::T_END);
   } else {
      expect(Lexer::T_COLON);
      SyntaxTree* blockTree = tree.createChild();
      blockTree->type = SyntaxTree::TYPE_BLOCK;
      statement(*blockTree->createChild(), state);
   }
}

void Parser::expression(SyntaxTree& tree) {
   andExpression(tree);
   while (true) {
      switch (mTokenType) {
         case Lexer::T_ASSIGNEMENT:
         {
            nextToken();
            tree.copyOnNewChild();
            tree.type = SyntaxTree::TYPE_ASSIGNEMENT;
            expression(*tree.createChild());
            break;
         }

         case Lexer::T_PLUS_ASSIGNEMENT:
         {
            nextToken();
            SyntaxTree* pLeftTree = tree.copyOnNewChild();
            tree.type = SyntaxTree::TYPE_ASSIGNEMENT;
            SyntaxTree* pRightTree = tree.createChild();
            pRightTree->type = SyntaxTree::TYPE_SUM;
            pLeftTree->copyTo(*pRightTree->createChild());
            expression(*pRightTree->createChild());
            break;
         }
         case Lexer::T_MINUS_ASSIGNEMENT:
         {
            nextToken();
            SyntaxTree* pLeftTree = tree.copyOnNewChild();
            tree.type = SyntaxTree::TYPE_ASSIGNEMENT;
            SyntaxTree* pRightTree = tree.createChild();
            pRightTree->type = SyntaxTree::TYPE_DIFFERENCE;
            pLeftTree->copyTo(*pRightTree->createChild());
            expression(*pRightTree->createChild());
            break;
         }

         case Lexer::T_ASTERISK_ASSIGNEMENT:
         {
            nextToken();
            SyntaxTree* pLeftTree = tree.copyOnNewChild();
            tree.type = SyntaxTree::TYPE_ASSIGNEMENT;
            SyntaxTree* pRightTree = tree.createChild();
            pRightTree->type = SyntaxTree::TYPE_PRODUCT;
            pLeftTree->copyTo(*pRightTree->createChild());
            expression(*pRightTree->createChild());
            break;
         }

         case Lexer::T_SLASH_ASSIGNEMENT:
         {
            nextToken();
            SyntaxTree* pLeftTree = tree.copyOnNewChild();
            tree.type = SyntaxTree::TYPE_ASSIGNEMENT;
            SyntaxTree* pRightTree = tree.createChild();
            pRightTree->type = SyntaxTree::TYPE_DIVISION;
            pLeftTree->copyTo(*pRightTree->createChild());
            expression(*pRightTree->createChild());
            break;
         }

         default:
            tree.simplify();
            return;
      }
   }
}

void Parser::andExpression(SyntaxTree& tree) {
   orExpression(tree);
   while (true) {
      if (mTokenType == Lexer::T_AND) {
         tree.copyOnNewChild();
         tree.type = SyntaxTree::TYPE_AND;
         nextToken();
         orExpression(*tree.createChild());
      } else
         return;
   }
}

void Parser::orExpression(SyntaxTree& tree) {
   comparisonExpression(tree);
   while (true) {
      if (mTokenType == Lexer::T_OR) {
         tree.copyOnNewChild();
         tree.type = SyntaxTree::TYPE_OR;
         nextToken();
         comparisonExpression(*tree.createChild());
      } else
         return;
   }
}

void Parser::comparisonExpression(SyntaxTree& tree) {
   mathExpression(tree);
   while (true) {
      switch (mTokenType) {
         case Lexer::T_EQUALS:
            tree.copyOnNewChild();
            tree.type = SyntaxTree::TYPE_EQUALS;
            nextToken();
            mathExpression(*tree.createChild());
            break;
         case Lexer::T_NOTEQUALS:
            tree.copyOnNewChild();
            tree.type = SyntaxTree::TYPE_NOT_EQUALS;
            nextToken();
            mathExpression(*tree.createChild());
            break;
         case Lexer::T_GREATER:
            tree.copyOnNewChild();
            tree.type = SyntaxTree::TYPE_GREATER;
            nextToken();
            mathExpression(*tree.createChild());
            break;
         case Lexer::T_GREATER_EQUALS:
            tree.copyOnNewChild();
            tree.type = SyntaxTree::TYPE_GREATER_EQUALS;
            nextToken();
            mathExpression(*tree.createChild());
            break;
         case Lexer::T_LESSER:
            tree.copyOnNewChild();
            tree.type = SyntaxTree::TYPE_LESSER;
            nextToken();
            mathExpression(*tree.createChild());
            break;
         case Lexer::T_LESSER_EQUALS:
            tree.copyOnNewChild();
            tree.type = SyntaxTree::TYPE_LESSER_EQUALS;
            nextToken();
            mathExpression(*tree.createChild());
            break;
         default:
            return;
      }
   }
}

void Parser::mathExpression(SyntaxTree& tree) {
   term(tree);
   while (true)
      switch (mTokenType) {
         case Lexer::T_PLUS:
            tree.copyOnNewChild();
            tree.type = SyntaxTree::TYPE_SUM;
            nextToken();
            term(*tree.createChild());
            break;
         case Lexer::T_MINUS:
            tree.copyOnNewChild();
            tree.type = SyntaxTree::TYPE_DIFFERENCE;
            nextToken();
            term(*tree.createChild());
            break;
         default:
            return;
      }
}

void Parser::term(SyntaxTree& tree) {
   implicitFunctionCall(tree);
   while (true)
      switch (mTokenType) {
         case Lexer::T_ASTERISK:
            tree.copyOnNewChild();
            tree.type = SyntaxTree::TYPE_PRODUCT;
            nextToken();
            implicitFunctionCall(*tree.createChild());
            break;
         case Lexer::T_SLASH:
            tree.copyOnNewChild();
            tree.type = SyntaxTree::TYPE_DIVISION;
            nextToken();
            implicitFunctionCall(*tree.createChild());
            break;
         default:
            return;
      }
}

void Parser::implicitFunctionCall(SyntaxTree& tree) {
   dereference(tree);
   while (accept(Lexer::T_DOT)) {
      if (!mTokenType == Lexer::T_IDENTIFIER) // It may be improved adding arrays.
         error();
      tree.copyOnNewChild();
      tree.type = SyntaxTree::TYPE_FUNCTION_CALL;
      tree.str = mLexer.getString();
      nextToken();
      expect(Lexer::T_LEFT_ROUND_BRACKET);
      params(tree);
      expect(Lexer::T_RIGHT_ROUND_BRACKET);
   }
}

void Parser::dereference(SyntaxTree& tree) {
   factor(tree);
   while (accept(Lexer::T_LEFT_SQUARE_BRACKET)) {

      tree.copyOnNewChild();
      tree.type = SyntaxTree::TYPE_CONTAINER_ELEMENT;

      mathExpression(*tree.createChild());
      expect(Lexer::T_RIGHT_SQUARE_BRACKET);
   }
}

void Parser::factor(SyntaxTree& tree) {
   switch (mTokenType) {
      case Lexer::T_NIL:
         tree.type = SyntaxTree::TYPE_NIL;
         nextToken();
         return;

      case Lexer::T_IDENTIFIER:
         tree.type = SyntaxTree::TYPE_VARIABLE;
         tree.str = mLexer.getString();
         nextToken(); // id
         if (accept(Lexer::T_LEFT_ROUND_BRACKET)) {// Function call
            tree.type = SyntaxTree::TYPE_FUNCTION_CALL;
            params(tree);
            expect(Lexer::T_RIGHT_ROUND_BRACKET);
         }
         return;

      case Lexer::T_NUMBER:
         tree.type = SyntaxTree::TYPE_NUMBER;
         tree.number = mLexer.getTokenAsNumber();
         nextToken();
         return;

      case Lexer::T_LEFT_ROUND_BRACKET:
         nextToken(); // (
         expression(tree);
         expect(Lexer::T_RIGHT_ROUND_BRACKET); // )
         return;

      case Lexer::T_MINUS:
         nextToken(); // -
         tree.type = SyntaxTree::TYPE_NEGATION;
         factor(*tree.createChild());
         return;

      case Lexer::T_STRING:
         tree.type = SyntaxTree::TYPE_STRING;
         tree.str = mLexer.getString();
         nextToken();
         return;

      case Lexer::T_NOT:
         tree.type = SyntaxTree::TYPE_NOT;
         nextToken();
         mathExpression(*tree.createChild());
         break;


      case Lexer::T_TRUE:
         tree.type = SyntaxTree::TYPE_BOOLEAN;
         tree.boolean = true;
         tree.str = mLexer.getString();
         nextToken();
         return;

      case Lexer::T_FALSE:
         tree.type = SyntaxTree::TYPE_BOOLEAN;
         tree.boolean = false;
         tree.str = mLexer.getString();
         nextToken();
         return;

      case Lexer::T_LEFT_SQUARE_BRACKET: // lists
      {
         tree.type = SyntaxTree::TYPE_LIST;
         nextToken();
         if (accept(Lexer::T_RIGHT_SQUARE_BRACKET))
            return;
         expression(*tree.createChild());
         while (mTokenType != Lexer::T_RIGHT_SQUARE_BRACKET) {
            expect(Lexer::T_COMMA);
            expression(*tree.createChild());
         }
         nextToken();
         return;
      }

      case Lexer::T_LEFT_CURLY_BRACKET:
      {
         tree.type = SyntaxTree::TYPE_DICTIONARY;
         nextToken();
         stripNewLines();
         if (accept(Lexer::T_RIGHT_CURLY_BRACKET))
            return;
         SyntaxTree* pTree = tree.createChild();
         pTree->type = SyntaxTree::TYPE_PAIR;
         expression(*pTree->createChild());
         expect(Lexer::T_COLON);
         expression(*pTree->createChild());
         while (mTokenType != Lexer::T_RIGHT_CURLY_BRACKET) {
            expect(Lexer::T_COMMA);
            stripNewLines();
            SyntaxTree* pTree = tree.createChild();
            pTree->type = SyntaxTree::TYPE_PAIR;
            expression(*pTree->createChild());
            expect(Lexer::T_COLON);
            expression(*pTree->createChild());
            stripNewLines();
         }
         nextToken();
         return;
      }

      case Lexer::T_NEW:
         tree.type = SyntaxTree::TYPE_FUNCTION_CALL;
         nextToken();
         if (!mTokenType == Lexer::T_IDENTIFIER)
            error();
         tree.str = mLexer.getString() + "_new";
         nextToken();
         if (accept(Lexer::T_LEFT_ROUND_BRACKET)) {
            params(tree);
            expect(Lexer::T_RIGHT_ROUND_BRACKET);
         }
         return;
      default:
         error();
   }
}

void Parser::params(SyntaxTree& tree) {
   if (mTokenType == Lexer::T_RIGHT_ROUND_BRACKET)
      return;
   expression(*tree.createChild());
   while (mTokenType != Lexer::T_RIGHT_ROUND_BRACKET) {
      expect(Lexer::T_COMMA);
      expression(*tree.createChild());
   }
}
