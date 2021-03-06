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

#ifndef ION_SCRIPT_SYNTAXTREE_H
#define	ION_SCRIPT_SYNTAXTREE_H

#include <list>
#include <string>
#include <iostream>

namespace ionscript {

   /**
    * A syntax tree contains the whole program structure and definition in a tree-like abstraction for easy manipulation. The SyntaxTree allows simplification
    * of certain structures that can be computed statically (imagine 1 + 1 = 2) ensuring both bytecode additional size and slower performance. The SyntaxTree
    * is the output of the parsification process operated by the Parser.
    */
   class SyntaxTree {
   public:

      enum Type {
         TYPE_AND,
         TYPE_ARGUMENT,
         TYPE_ASSIGNEMENT,
         TYPE_BLOCK,
         TYPE_BOOLEAN,
         TYPE_BREAK,
         TYPE_CONTAINER_ELEMENT,
         TYPE_CONTINUE,
         TYPE_DICTIONARY,
         TYPE_DIFFERENCE,
         TYPE_DIVISION,
         TYPE_EQUALS,
         TYPE_FOR,
         TYPE_FUNCTION_CALL,
         TYPE_FUNCTION_DEF,
         TYPE_GREATER,
         TYPE_GREATER_EQUALS,
         TYPE_IF,
         TYPE_LESSER,
         TYPE_LESSER_EQUALS,
         TYPE_LIST,
         TYPE_NEGATION,
         TYPE_NIL,
         TYPE_NOT,
         TYPE_NOT_EQUALS,
         TYPE_NUMBER,
         TYPE_OR,
         TYPE_PAIR,
         TYPE_PRODUCT,
         TYPE_RETURN,
         TYPE_STRING,
         TYPE_SUM,
         TYPE_UNKNOWN,
         TYPE_VARIABLE,
         TYPE_WHILE,
      } type;

      std::string str;

      union {
         bool boolean;
         double number;
      };

      size_t sourceLineNumber;
      std::string sourceLine;

      SyntaxTree();
      virtual ~SyntaxTree();
      inline SyntaxTree* getParent() const {
         return mpParent;
      }
      inline bool hasChildren() const {
         return mChildren.size() > 0;
      }
      inline const std::list<SyntaxTree*>& getChildren() const {
         return mChildren;
      }
      SyntaxTree* left() const {
         return mChildren.front();
      }
      SyntaxTree* right() const {
         return mChildren.back();
      }
      bool isInteger() const {
         return type == TYPE_NUMBER && (int) number == number;
      }
      SyntaxTree* createChild();
      SyntaxTree* copyOnNewChild();
      void replaceByChild(SyntaxTree& pChild);
      void copyTo(SyntaxTree& tree) const;
      void remove();
      void simplify();
      void dump(std::ostream & targetStream = std::cout, const std::string& spacing = "") const;

   private:
      std::list<SyntaxTree*> mChildren;
      SyntaxTree* mpParent;

      void deleteChildren();

      static void convertToBoolean(SyntaxTree& tree);
   };
}

#endif	/* ION_SCRIPT_SYNTAXTREE_H */

