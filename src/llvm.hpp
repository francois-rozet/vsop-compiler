#ifndef LLVM_H
#define LLVM_H

#include "llvm/Support/raw_ostream.h"

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"

#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"

#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

static bool isUnit(llvm::Type* t) {
	return (not t) or t->isVoidTy();
}

static bool isInteger(llvm::Type* t) {
	return t and t->isIntegerTy(32);
}

static bool isReal(llvm::Type* t) {
	return t and t->isDoubleTy();
}

static bool isBoolean(llvm::Type* t) {
	return t and t->isIntegerTy(1);
}

static bool isString(llvm::Type* t) {
	return t and t->isPointerTy() and t->getPointerElementType()->isIntegerTy(8);
}

static bool isClass(llvm::Type* t) {
	return t and t->isPointerTy() and t->getPointerElementType()->isStructTy();
}

static bool isNumeric(llvm::Type* t) {
	return isInteger(t) or isReal(t);
}

static bool isPrimitive(llvm::Type* t) {
	return isNumeric(t) or isBoolean(t) or isString(t);
}

/// Compare two types
static bool isSameAs(llvm::Type* a, llvm::Type* b) {
	if (isUnit(a)) return isUnit(b);
	else if (isInteger(a)) return isInteger(b);
	else if (isReal(a)) return isReal(b);
	else if (isBoolean(a)) return isBoolean(b);
	else if (isString(a)) return isString(b);
	else if (isClass(a))
		return isClass(b) and a->getPointerElementType() == b->getPointerElementType();

	return false;
}

/// Convert a type into the associated string
static std::string asString(llvm::Type* type) {
	if (isInteger(type)) return "int32";
	else if (isReal(type)) return "double";
	else if (isBoolean(type)) return "bool";
	else if (isString(type)) return "string";
	else if (isClass(type)) {
		std::string str = type->getPointerElementType()->getStructName().str();
		return str.substr(str.find_first_of('.') + 1, std::string::npos);
	}

	return "unit";
}

/// Position wrapper
struct Position {
	int line;
	int column;
};

/// Error wrapper
struct Error {
	Position pos;
	std::string msg;
};

/**
 * LLVM C++ context, builder and module wrapper
 *
 * @note LLVMHelper includes a named values manager and a function pass manager
 */
class LLVMHelper {
	public:
		LLVMHelper(const std::string& name) {
			context = std::make_shared<llvm::LLVMContext>();
			builder = std::make_shared<llvm::IRBuilder<>>(*context);
			module = std::make_shared<llvm::Module>(name, *context);
		}

		std::shared_ptr<llvm::LLVMContext> context;
		std::shared_ptr<llvm::IRBuilder<>> builder;
		std::shared_ptr<llvm::Module> module;

		/// Stack of errorsw
		std::vector<Error> errors;

		/// Stack of innermost loop-exits
		std::vector<llvm::BasicBlock*> exits;

		/**
		 * Insert a named value
		 *
		 * @warning push does not allocate memory
		 * @see alloc
		 */
		llvm::Value* push(const std::string& name, llvm::Value* ptr) {
			if (this->contains(name))
				scope.at(name).push_back(ptr);
			else
				scope.insert({name, {ptr}});

			return this->getValue(name);
		}

		/// Remove a named value
		llvm::Value* pop(const std::string& name) {
			llvm::Value* ptr = nullptr;

			if (this->contains(name)) {
				ptr = scope.at(name).back();

				if (scope.at(name).size() > 1)
					scope.at(name).pop_back();
				else
					scope.erase(name);
			}

			return ptr;
		}

		/// Get a named value
		llvm::Value* getValue(const std::string& name) const {
			if (this->contains(name))
				return scope.at(name).back();
			return nullptr;
		}

		/// Get a named value type
		llvm::Type* getType(const std::string& name) const {
			if (llvm::Value* ptr = this->getValue(name))
				return ptr->getType()->getPointerElementType();
			return nullptr;
		}

		/// State whether a name is associated to a value
		bool contains(const std::string& name) const {
			return scope.find(name) != scope.end();
		}

		/// Allocate named memory on the stack
		llvm::Value* alloc(const std::string& name, llvm::Type* type) {
			return this->push(
				name,
				isUnit(type) ? nullptr : builder->CreateAlloca(type)
			);
		}

		/**
		 * Store a value in named memory space
		 *
		 * @warning should be preceeded by alloc
		 */
		llvm::Value* store(const std::string& name, llvm::Value* value) const {
			if (llvm::Value* ptr = this->getValue(name))
				return builder->CreateStore(value, ptr)->getOperand(0);
			return nullptr;
		}

		/**
		 * Load a value from named memory space
		 *
		 * @warning should be preceeded by store
		 */
		llvm::Value* load(const std::string& name) const {
			if (llvm::Value* ptr = this->getValue(name))
				return builder->CreateLoad(ptr);
			return nullptr;
		}

		/// Convert a string into the associated type
		llvm::Type* asType(const std::string& type) {
			if (type == "unit") return llvm::Type::getVoidTy(*context);
			else if (type == "int32") return llvm::Type::getInt32Ty(*context);
			else if (type == "double") return llvm::Type::getDoubleTy(*context);
			else if (type == "bool") return llvm::Type::getInt1Ty(*context);
			else if (type == "string") return llvm::Type::getInt8PtrTy(*context);

			llvm::StructType* st = module->getTypeByName("struct." + type);
			return st ? st->getPointerTo() : nullptr;
		}

		/// Default value of a type
		llvm::Value* defaultValue(llvm::Type* type) {
			if (isClass(type))
				return llvm::ConstantPointerNull::get((llvm::PointerType*) type);
			else if (isString(type))
				return builder->CreateGlobalStringPtr("", "str");
			else if (isPrimitive(type))
				return llvm::Constant::getNullValue(type);

			return nullptr;
		}

		llvm::Value* defaultValue(const std::string& type) {
			return this->defaultValue(this->asType(type));
		}

		/// Cast a numeric (int32 or double) into another numeric type
		llvm::Value* numericCast(llvm::Value* value, llvm::Type* type=nullptr) {
			llvm::Type* value_t = value ? value->getType() : nullptr;

			if (not type)
				type = this->asType("double");

			if (isInteger(value_t) and isReal(type))
				return builder->CreateSIToFP(value, type);
			else if (isReal(value_t) and isInteger(type))
				return builder->CreateFPToSI(value, type);
			else if (isSameAs(value_t, type))
				return value;

			return nullptr;
		}

		/// Optimize module functions
		int passes() {
			// Create function pass manager
			llvm::legacy::FunctionPassManager optimizer(module.get());

			optimizer.add(llvm::createInstructionCombiningPass()); // peephole and bit-twiddling optimizations
			optimizer.add(llvm::createReassociatePass()); // reassociate expressions
			optimizer.add(llvm::createGVNPass()); // eliminate common sub-expressions
			optimizer.add(llvm::createCFGSimplificationPass()); // simplify the control flow graph (deleting unreachable blocks, etc)

			optimizer.doInitialization();

			int errs = 0;

			// Error output
			std::string str;
			llvm::raw_string_ostream rso(str);

			// Pass over each functions
			for (auto it = module->begin(); it != module->end(); ++it) {
				// Validate the generated code
				if (llvm::verifyFunction(*it, &rso)) {
					rso << '\n';
					++errs;
					continue;
				}

				// Run the optimizer
				optimizer.run(*it);
			}

			if (errs)
				std::cerr << rso.str();

			return errs;
		}

		std::string dump() {
			std::string str;
			llvm::raw_string_ostream rso(str);
			rso << *module;
			return rso.str();
		}

	private:
		/**
		 * Storage for named values with O(1) insertion, removal and lookup
		 *
		 * @see push, pop, get, getType, contains, alloc, store and load
		 */
		std::unordered_map<std::string, std::vector<llvm::Value*>> scope;
};

#endif
