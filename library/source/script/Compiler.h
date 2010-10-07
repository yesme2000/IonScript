/*
 * Ion-Engine
 * Copyright Canio Massimo Tristano <massimo.tristano@gmail.com>
 * All rights reserved.
 */

#ifndef ION_COMPILER_H
#define	ION_COMPILER_H

#include "Exceptions.h"
#include "Typedefs.h"
#include "OpCode.h"

#include <exception>
#include <iostream>
#include <vector>
#include <stack>
#include <map>

namespace ion {
   namespace script {

      /**
       * Compiles the input syntax tree into executable bytecode.
       * @param hostFunctionsMap the map that contains the callable host functions.
       */
      class Compiler {
      public:
         /**
          * Constructs a new compiler specifing the callable host functions with the given HostFunctionMap.
          * @param hostFunctionsMap the map that contains the callable host functions.
          */
         Compiler (const HostFunctionsMap& hostFunctionsMap);

         /**
          * Compiles target SyntaxTree into an executable bytecode.
          * @param tree input syntax tree generated by the Parser.
          * @param output output bytecode.
          */
         void compile (const SyntaxTree& tree, BytecodeWriter& output);

      private:

         std::vector<std::string> mNamesStack;

         std::map<std::string, location_t> mScriptFunctionsLocations;
         const HostFunctionsMap& mHostFunctionsMap;

         /* STATE */
         std::stack<size_t> mActivationFramePointer;
         std::stack<small_size_t> mnRequiredRegisters;
         std::stack<small_size_t> mnDeclaredValues;
         std::stack<bool> mDeclareOnly;
         std::stack<std::vector<index_t>* > mContinues;
         std::stack<std::vector<index_t>* > mBreaks;

         int compile (const SyntaxTree& tree, BytecodeWriter& output, location_t target);
         void compileExpressionNodeChildren (const SyntaxTree& node, BytecodeWriter& output, location_t target, OpCode op);

         bool findLocalName (const std::string& name, location_t& outLocation) const;

         /* Auxiliary control functions*/
         void checkVariablesDefinition (const SyntaxTree& tree) const;
         void checkComparisonConsistency (const SyntaxTree& tree) const;

         void error (size_t line, const std::string& error) const;

      };
   }
}
#endif	/* ION_COMPILER_H */

