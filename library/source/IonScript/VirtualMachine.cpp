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

#include "VirtualMachine.h"
#include "Exceptions.h"
#include "Parser.h"
#include "SyntaxTree.h"
#include "OpCode.h"
#include "Bytecode.h"
#include "Compiler.h"

#include <vector>
#include <map>

using namespace ionscript;
using namespace std;

enum
{
	BFID_PRINT,
	BFID_POST,
	BFID_GET,
	BFID_LEN,
	BFID_APPEND,
	BFID_REMOVE,
	BFID_ASSERT,
	BFID_DUMP,
	BFID_STR,
	BFID_JOIN,
	BFID_ERROR,
};

VirtualMachine::VirtualMachine() : mpProgram(0)
{
	HostFunctionGroupID hfgID = registerHostFunctionGroup(builtinsGroup);
	setFunction("print", hfgID, BFID_PRINT, 0, -1);
	setFunction("post", hfgID, BFID_POST, 2);
	setFunction("get", hfgID, BFID_GET, 1);
	setFunction("len", hfgID, BFID_LEN, 1);
	setFunction("append", hfgID, BFID_APPEND, 2);
	setFunction("remove", hfgID, BFID_REMOVE, 2);
	setFunction("assert", hfgID, BFID_ASSERT, 1, 2);
	setFunction("dump", hfgID, BFID_DUMP); // no arguments
	setFunction("str", hfgID, BFID_STR, 1);
	setFunction("join", hfgID, BFID_JOIN, 2, -1);
	setFunction("error", hfgID, BFID_ERROR, 1);
}

VirtualMachine::~VirtualMachine()
{
	if (mpProgram)
		delete mpProgram;
}

HostFunctionGroupID VirtualMachine::registerHostFunctionGroup(HostFunction function)
{
	mHostFunctionGroups.push_back(function);
	return mHostFunctionGroups.size() - 1;
}

void VirtualMachine::setFunction(const std::string& name, HostFunctionGroupID hostFunctionID, FunctionID functionID, int minArgumentsCount, int maxArgumentsCount)
{
	if (maxArgumentsCount == -2 || (maxArgumentsCount != -1 && maxArgumentsCount < minArgumentsCount))
		maxArgumentsCount = minArgumentsCount;

	FunctionInfo info;
	info.hfgID = hostFunctionID;
	info.fID = functionID;
	info.minArgumentsCount = minArgumentsCount;
	info.maxArgumentsCount = maxArgumentsCount;

	mHostFunctionsMap[name] = info;
}

void VirtualMachine::post(const std::string& name, const Value& value)
{
	mGlobalVariables[name] = value;
}

bool VirtualMachine::hasGlobalVariable(const std::string& name) const
{
	return mGlobalVariables.find(name) != mGlobalVariables.end();
}

void VirtualMachine::undefineVariable(const std::string& name)
{
	mGlobalVariables.erase(name);
}

Value& VirtualMachine::get(const std::string& name)
{
	map<string, Value>::iterator it = mGlobalVariables.find(name);
	if (it == mGlobalVariables.end())
		throw UndefinedGlobalVariableException(name);
	else
		return it->second;
}

void VirtualMachine::compile(std::istream& source, std::vector<char>& output)
{
	SyntaxTree tree;
	compile(source, output, tree);
}

void VirtualMachine::compile(std::istream& source, std::vector<char>& output, SyntaxTree& tree)
{
	Parser parser(source);
	parser.parse(tree);
	BytecodeWriter writer(output);
	Compiler compiler(mHostFunctionsMap);
	compiler.compile(tree, writer);
}

void VirtualMachine::run(char* program)
{
	delete mpProgram;
	mpProgram = new BytecodeReader(program);

	unsigned int magicNumber, version;
	size_t size;
	*mpProgram >> magicNumber >> version >> size;

	if (magicNumber != kMagicNumber)
		throw RuntimeError("Given bytes do not form a valid bytecode.");
	if (version > kVersion)
		throw RuntimeError("Given bytecode has version higher than this Virtual Machine one.");

	mValues.clear();
	mValues.reserve(40);

	mActivations.clear();
	mActivations.push_back(ActivationRecord());

	mState = STATE_RUNNING;

	while (mpProgram->continues() && mState == STATE_RUNNING)
		executeInstruction();

	// The loop exited because we executed the whole program
	if (mState == STATE_RUNNING)
		mState = STATE_FINISHED;
}

void VirtualMachine::compileAndRun(const std::string& filename)
{
	ifstream source(filename.c_str());
	compileAndRun(source);
}

void VirtualMachine::compileAndRun(std::istream& source)
{
	std::vector<char> bytecode;
	SyntaxTree tree;
	compile(source, bytecode, tree);
	run(&bytecode[0]);
}

void VirtualMachine::pause()
{
	if (mState == STATE_RUNNING)
		mState = STATE_PAUSED;
}

void VirtualMachine::goOn()
{
	if (mState != STATE_PAUSED)
		return;

	mState = STATE_RUNNING;

	while (mpProgram->continues() && mState == STATE_RUNNING)
		executeInstruction();

	// The loop exited because we executed the whole program
	if (mState == STATE_RUNNING)
		mState = STATE_FINISHED;
}

Value VirtualMachine::callScriptFunction(const Value& function, const Value& argument)
{
	const Value * arguments[] = {&argument};
	return callScriptFunction(function, arguments, 1);
}

Value VirtualMachine::callScriptFunction(const Value& function, const Value& argument1, const Value& argument2)
{
	const Value * arguments[] = {&argument1, &argument2};
	return callScriptFunction(function, arguments, 2);
}

Value VirtualMachine::callScriptFunction(const Value& function, const Value** argument, size_t nArguments)
{
	function.assertType(Value::TYPE_SCRIPT_FUNCTION);

	// Check whether the required number of arguments corresponds to the one given.
	if (function.mnArguments != nArguments)
	{
		stringstream ss;
		ss << "wrong number of arguments given (" << (int) nArguments << " instead of " << (int) function.mnArguments << ").";
		error(ss.str());
	}
	index_t oldIP = mpProgram->getCursorPosition();

	// Push registers
	for (size_t i = 0; i < function.mnFunctionRegisters; i++)
		mValues.push_back(Value());

	// Push arguments
	for (size_t i = 0; i < nArguments; i++)
		mValues.push_back(*argument[i]);

	// Finally set the current IP
	mpProgram->setCursorPosition(function.mFunctionIndex);

	ActivationRecord record(0, mValues.size() - function.mnFunctionRegisters - nArguments, mValues.size() - nArguments);
	mActivations.push_back(record);

	while (mpProgram->getCursorPosition() != 0)
		executeInstruction();

	mpProgram->setCursorPosition(oldIP);

	// Pop the result and return it
	Value result = mValues.back();
	mValues.pop_back();

	return result;
}

void VirtualMachine::dump(std::ostream & output)
{
	output << "Values-Stack:\n";
	for (size_t i = 0; i < mValues.size(); ++i)
		output << "   " << i << ", " << (long) i - (long) mActivations.back().firstVariableLocation << ") " << mValues[i].toString() << "\n";

	output << "Activations-Stack:\n";
	list<ActivationRecord>::const_iterator it = mActivations.begin();
	size_t i = 0;
	for (; it != mActivations.end(); ++it)
	{
		output << "   " << i << ") " <<
			"return-index : " << it->returnIndex << ";" <<
			"stack-size: " << it->stackSize << ";" <<
			"first-variable-loc: " << it->firstVariableLocation << "\n";
		i++;
	}
}
//

void VirtualMachine::executeInstruction()
{
	OpCode op;
	*mpProgram >> op;

	switch (op)
	{
		case OP_REG:
		{
			small_size_t nRegisters;
			*mpProgram >> nRegisters;
			for (size_t i = 0; i < nRegisters; i++)
				mValues.push_back(Value());
			mActivations.back().firstVariableLocation += nRegisters;
			return;
		}

		case OP_PCALL_SF_L:
		case OP_PCALL_SF_G:
		{
			location_t functionLoc;
			*mpProgram >> functionLoc;

			Value functionValue;
			if (op == OP_PCALL_SF_G)
				functionValue = mValues[mActivations.front().firstVariableLocation + functionLoc]; // global
			else
				functionValue = mValues[mActivations.back().firstVariableLocation + functionLoc]; //local

			for (size_t i = 0; i < functionValue.mnFunctionRegisters; i++)
				mValues.push_back(Value());
			return;
		}

		case OP_CALL_SF_L:
		case OP_CALL_SF_G:
		{
			location_t functionLoc;
			small_size_t nArguments;
			*mpProgram >> functionLoc >> nArguments;

			Value functionValue;
			if (op == OP_CALL_SF_G)
				functionValue = mValues[mActivations.front().firstVariableLocation + functionLoc]; // global
			else
				functionValue = mValues[mActivations.back().firstVariableLocation + functionLoc]; //local

			if (functionValue.mType == Value::TYPE_SCRIPT_FUNCTION)
			{
				// Check whether the required number of arguments corresponds to the one given.
				if (functionValue.mnArguments != nArguments)
				{
					stringstream ss;
					ss << "wrong number of arguments given (" << (int) nArguments << " instead of " << (int) functionValue.mnArguments << ").";
					error(ss.str());
				}

				ActivationRecord record(mpProgram->getCursorPosition(), mValues.size() - functionValue.mnFunctionRegisters - nArguments, mValues.size() - nArguments);
				mActivations.push_back(record);

				// Finally set the current IP
				mpProgram->setCursorPosition(functionValue.mFunctionIndex);
			} else
				throw RuntimeError("object " + functionValue.toString() + " is not callable.");
			return;
		}


		case OP_CALL_HF:
		{
			HostFunctionGroupID hfgID;
			FunctionID fID;
			small_size_t nArguments;
			*mpProgram >> hfgID >> fID >> nArguments;

			FunctionCallManager manager(*this, fID, &mValues[mValues.size() - nArguments], nArguments);

			// Set the number of arguments
			mHostFunctionArgumentsCount = nArguments;

			// Pause the machine
			mState = STATE_WAITING_FOR_RETURN;

			// Call the host function group.
			mHostFunctionGroups[hfgID](manager);

			// If the state is PAUSED it means that the function already returned a value so we can continue
			if (mState == STATE_PAUSED)
				mState = STATE_RUNNING;

			break;
		}

		case OP_RETURN_NIL:
		{
			// Restore the stack as it was before
			while (mValues.size() > mActivations.back().stackSize)
				mValues.pop_back();

			// Return a nil value
			mValues.push_back(Value());

			// Set the Instruction Pointer
			mpProgram->setCursorPosition(mActivations.back().returnIndex);

			mActivations.pop_back();

			return;
		}

		case OP_RETURN:
		{
			location_t loc;
			*mpProgram >> loc;

			Value returnValue = getLocalValue(loc);

			// Restore the stack as it was before
			while (mValues.size() > mActivations.back().stackSize)
				mValues.pop_back();

			// Return a nil value
			mValues.push_back(returnValue);

			// Set the Instruction Pointer
			mpProgram->setCursorPosition(mActivations.back().returnIndex);

			mActivations.pop_back();

			return;
		}

		case OP_PUSH:
			mValues.push_back(Value());
			return;

		case OP_POP:
			mValues.pop_back();
			return;

		case OP_POP_N:
		{
			small_size_t n;
			*mpProgram >> n;
			for (size_t i = 0; i < n; i++)
				mValues.pop_back();
			return;
		}

		case OP_POP_TO:
		{
			location_t loc;
			*mpProgram >> loc;
			getLocalValue(loc) = mValues.back();
			mValues.pop_back();
			return;
		}

		case OP_PUSH_VAL:
		{
			location_t loc;
			*mpProgram >> loc;
			mValues.push_back(getLocalValue(loc));
			return;
		}

		case OP_PUSH_N:
		{
			double value;
			*mpProgram >> value;
			mValues.push_back(Value(value));
			return;
		}

		case OP_PUSH_S:
		{
			string value;
			*mpProgram >> value;
			mValues.push_back(Value(value));
			return;
		}

		case OP_PUSH_B:
		{
			bool value;
			*mpProgram >> value;
			mValues.push_back(Value(value));
			break;
		}

		case OP_STORE_AT_NIL:
		{
			location_t loc;
			*mpProgram >> loc;
			getLocalValue(loc).setNil();
			break;
		}

		case OP_STORE_AT_F:
		{
			index_t index;
			location_t loc;
			small_size_t nArguments, nRegisters;
			*mpProgram >> loc >> index >> nArguments >> nRegisters;
			getLocalValue(loc).setFunctionValue(index, nArguments, nRegisters);
			return;
		}

		case OP_LIST_NEW:
		{
			location_t listLoc;
			*mpProgram >> listLoc;
			getLocalValue(listLoc).setEmptyList();
			return;
		}

		case OP_LIST_ADD:
		{
			location_t listLoc, indexLoc;
			*mpProgram >> listLoc >> indexLoc;
			getLocalValue(listLoc).getList().push_back(getLocalValue(indexLoc));
			return;
		}

		case OP_DICTIONARY_NEW:
		{
			location_t dictLoc;
			*mpProgram >> dictLoc;
			getLocalValue(dictLoc).setEmptyDictionary();
			return;
		}

		case OP_DICTIONARY_ADD:
		{
			location_t dictLoc, keyLoc, valueLoc;
			*mpProgram >> dictLoc >> keyLoc >> valueLoc;
			getLocalValue(dictLoc).getDictionary()[ getLocalValue(keyLoc)] = getLocalValue(valueLoc);
			return;
		}

		case OP_GET:
		{
			location_t targetLoc, contLoc, indexLoc;
			*mpProgram >> targetLoc >> contLoc >> indexLoc;
			Value& cont = getLocalValue(contLoc);

			cont.assertType(Value::TYPE_LIST | Value::TYPE_DICTIONARY);

			if (cont.isList())
			{

				getLocalValue(indexLoc).assertIsPositiveInteger();

				size_t index = static_cast<size_t> (getLocalValue(indexLoc).getNumber());

				if (index >= cont.getList().size())
					throw RuntimeError("index out of list boundaries.");

				getLocalValue(targetLoc) = cont.getListElement(index);

			} else
			{

				Dictionary::const_iterator it = cont.getDictionary().find(getLocalValue(indexLoc));
				if (it == cont.getDictionary().end())
					throw RuntimeError("key not found in dictionary.");
				else
					getLocalValue(targetLoc) = it->second;
			}

			return;
		}

		case OP_SET:
		{
			location_t valueLoc, contLoc, indexLoc;
			*mpProgram >> valueLoc >> contLoc >> indexLoc;
			Value& cont = getLocalValue(contLoc);

			cont.assertType(Value::TYPE_LIST | Value::TYPE_DICTIONARY);

			if (cont.isList())
			{

				getLocalValue(indexLoc).assertIsPositiveInteger();

				size_t index = static_cast<size_t> (getLocalValue(indexLoc).getNumber());

				if (index >= cont.getList().size())
					throw RuntimeError("index out of list boundaries.");

				cont.getList()[index] = getLocalValue(valueLoc);

			} else
				cont.getDictionary()[getLocalValue(indexLoc)] = getLocalValue(valueLoc);

			return;
		}



		case OP_MOVE:
		{
			location_t loc1, loc2;
			*mpProgram >> loc1 >> loc2;
			getLocalValue(loc1) = getLocalValue(loc2);
			return;
		}

		case OP_ADD:
		{
			location_t loc1, loc2, loc3;
			*mpProgram >> loc1 >> loc2 >> loc3;
			getLocalValue(loc1) = getLocalValue(loc2) + getLocalValue(loc3);
			return;
		}
		case OP_SUB:
		{
			location_t loc1, loc2, loc3;
			*mpProgram >> loc1 >> loc2 >> loc3;
			getLocalValue(loc1) = getLocalValue(loc2) - getLocalValue(loc3);
			return;
		}

		case OP_MUL:
		{
			location_t loc1, loc2, loc3;
			*mpProgram >> loc1 >> loc2 >> loc3;
			getLocalValue(loc1) = getLocalValue(loc2) * getLocalValue(loc3);
			return;
		}

		case OP_DIV:
		{
			location_t loc1, loc2, loc3;
			*mpProgram >> loc1 >> loc2 >> loc3;
			getLocalValue(loc1) = getLocalValue(loc2) / getLocalValue(loc3);
			return;
		}

		case OP_JUMP:
		{
			index_t index;
			*mpProgram >> index;
			mpProgram->setCursorPosition(index);
			return;
		}
		case OP_JUMP_COND:
		{
			location_t loc;
			index_t index;
			*mpProgram >> loc >> index;
			if (!getLocalValue(loc).toBoolean())
				mpProgram->setCursorPosition(index);
			return;
		}

		case OP_NOT:
		{
			location_t loc1, loc2;
			*mpProgram >> loc1 >> loc2;
			getLocalValue(loc1) = !getLocalValue(loc2);
			return;
		}

		case OP_AND:
		{
			location_t loc1, loc2, loc3;
			*mpProgram >> loc1 >> loc2 >> loc3;
			getLocalValue(loc1) = getLocalValue(loc2) && getLocalValue(loc3);
			return;
		}

		case OP_OR:
		{
			location_t loc1, loc2, loc3;
			*mpProgram >> loc1 >> loc2 >> loc3;
			getLocalValue(loc1) = getLocalValue(loc2) || getLocalValue(loc3);
			return;
		}

		case OP_EQ:
		{
			location_t loc1, loc2, loc3;
			*mpProgram >> loc1 >> loc2 >> loc3;
			getLocalValue(loc1) = getLocalValue(loc2) == getLocalValue(loc3);
			return;
		}

		case OP_NEQ:
		{
			location_t loc1, loc2, loc3;
			*mpProgram >> loc1 >> loc2 >> loc3;
			getLocalValue(loc1) = getLocalValue(loc2) != getLocalValue(loc3);
			return;
		}

		case OP_GR:
		{
			location_t loc1, loc2, loc3;
			*mpProgram >> loc1 >> loc2 >> loc3;
			getLocalValue(loc1) = getLocalValue(loc2) > getLocalValue(loc3);
			return;
		}

		case OP_GRE:
		{
			location_t loc1, loc2, loc3;
			*mpProgram >> loc1 >> loc2 >> loc3;
			getLocalValue(loc1) = getLocalValue(loc2) >= getLocalValue(loc3);
			return;
		}

		case OP_LS:
		{
			location_t loc1, loc2, loc3;
			*mpProgram >> loc1 >> loc2 >> loc3;
			getLocalValue(loc1) = getLocalValue(loc2) < getLocalValue(loc3);
			return;
		}

		case OP_LSE:
		{
			location_t loc1, loc2, loc3;
			*mpProgram >> loc1 >> loc2 >> loc3;
			getLocalValue(loc1) = getLocalValue(loc2) <= getLocalValue(loc3);
			return;
		}

		default:
			error("Unsupported op-code.");
			return;
	}
}

void VirtualMachine::error(const std::string & message) const
{
	throw RuntimeError(message);
}

void VirtualMachine::returnValue(const Value & value)
{
	if (mState != STATE_WAITING_FOR_RETURN)
		throw RuntimeError("cannot return a value if a host function has not been called.");

	for (size_t i = 0; i < mHostFunctionArgumentsCount; i++)
		mValues.pop_back();
	mValues.push_back(value);

	mState = STATE_PAUSED;
}

void VirtualMachine::builtinsGroup(const FunctionCallManager & manager)
{
	switch (manager.getFunctionID())
	{
		case BFID_PRINT:
			for (size_t i = 0; i < manager.getArgumentsCount(); i++)
			{
				cout << manager.getArgument(i).toString();
				if (i != manager.getArgumentsCount() - 1)
					cout << " ";
			}
			cout << endl;
			manager.returnNil();
			return;

		case BFID_POST:
			manager.assertArgumentType(0, Value::TYPE_STRING);
			manager.mVM.mGlobalVariables[manager.getArgument(0).getString()] = manager.getArgument(1);
			manager.returnValue(manager.getArgument(1));
			return;

		case BFID_GET:
		{
			manager.assertArgumentType(0, Value::TYPE_STRING);
			map<string, Value>::iterator it = manager.mVM.mGlobalVariables.find(manager.getArgument(0).getString());
			if (it == manager.mVM.mGlobalVariables.end())
				manager.returnNil();
			else
				manager.returnValue(it->second);
			return;
		}

		case BFID_LEN:
			manager.assertArgumentType(0, Value::TYPE_STRING | Value::TYPE_LIST | Value::TYPE_DICTIONARY);
			switch (manager.getArgument(0).getType())
			{
				case Value::TYPE_STRING:
					manager.returnNumber(manager.getArgument(0).getString().size());
					return;
				case Value::TYPE_LIST:
					manager.returnNumber(manager.getArgument(0).getList().size());
					return;
				case Value::TYPE_DICTIONARY:
					manager.returnNumber(manager.getArgument(0).getDictionary().size());
					return;
				default: // Never executed
					return;
			}


		case BFID_APPEND:
			manager.assertArgumentType(0, Value::TYPE_LIST);
			manager.getArgument(0).getList().push_back(manager.getArgument(1));
			manager.returnValue(manager.getArgument(0));
			return;

		case BFID_REMOVE:
			manager.assertArgumentType(0, Value::TYPE_LIST);
			manager.assertArgumentType(1, Value::TYPE_NUMBER);
			if (!manager.getArgument(1).isInteger() || manager.getArgument(1).getNumber() < 0)
				throw RuntimeError("the index of the element to remove must be a positive integer.");
			manager.getArgument(0).getList().erase(manager.getArgument(0).getList().begin() + static_cast<size_t> (manager.getArgument(1).getNumber()));
			manager.returnValue(manager.getArgument(0));
			return;

		case BFID_ASSERT:
			if (!manager.getArgument(0).toBoolean())
			{
				if (manager.getArgumentsCount() == 2)
				{
					manager.assertArgumentType(1, Value::TYPE_STRING);
					throw RuntimeError("user assertion failed: " + manager.getArgument(1).getString() + ".");
				} else
					throw RuntimeError("user assertion failed.");
			}
			manager.returnNil();
			return;

		case BFID_DUMP:
			manager.mVM.dump(std::cout);
			manager.returnNil();
			return;

		case BFID_STR:
			manager.returnString(manager.getArgument(0).toString());
			return;

		case BFID_JOIN:
		{
			manager.assertArgumentType(0, Value::TYPE_STRING);
			string result = "";
			const string& separator = manager.getArgument(0).getString();
			if (manager.getArgument(1).isList())
			{
				List& list = manager.getArgument(1).getList();
				for (size_t i = 0; i < list.size(); i++)
				{
					result += list[i].toString();
					if (i != list.size() - 1)
						result += separator;
				}
			} else
			{
				for (size_t i = 1; i < manager.getArgumentsCount(); i++)
				{
					result += manager.getArgument(i).toString();
					if (i != manager.getArgumentsCount() - 1)
						result += separator;
				}
			}
			manager.returnString(result);
			return;
		}

		case BFID_ERROR:
			manager.assertArgumentType(0, Value::TYPE_STRING);
			throw RuntimeError(manager.getArgument(0).getString());
			return;

		default:
			manager.returnNil();
	}
}
