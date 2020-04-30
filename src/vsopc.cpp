#include "vsop.tab.h"

#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace std;

/* global variables */
int yymode = START_PARSER;

List<Class> yyclasses;
List<Method> yyfunctions;

/* static variables */

static Program* program;

static llvm::LLVMContext context;
static llvm::IRBuilder<> builder(context);
static llvm::Module module("VSOP", context);

static string rso_string;
static llvm::raw_string_ostream rso(rso_string);

/* bison global variables */
extern int yyerrs;

/* bison global functions */
extern int yyparse(void);
extern void yyrelocate(int, int);
extern void yyprint(const string&);
extern void yyerror(const string&);
extern bool yyopen(const char*);
extern void yyclose();

/* functions */

void lexer() {
	yymode = START_LEXER;
	yyparse();
}

void parser() {
	yyparse();

	program = new Program(yyclasses, yyfunctions);
}

void checker() {
	LLVMHelper helper = {
		context,
		builder,
		module
	};

	Scope scope;
	vector<Error> errors;

	program->declaration(helper, errors);
	program->codegen(program, helper, scope, errors);

	for (Error& e: errors) {
		yyrelocate(e.line, e.column);
		yyerror("semantic error, " + e.msg);
	}
}

void llvmer() {
	// Create function pass manager
	llvm::legacy::FunctionPassManager optimizer(&module);

	optimizer.add(llvm::createInstructionCombiningPass()); // peephole and bit-twiddling optimizations
	optimizer.add(llvm::createReassociatePass()); // reassociate expressions
	optimizer.add(llvm::createGVNPass()); // eliminate common sub-expressions
	optimizer.add(llvm::createCFGSimplificationPass()); // simplify the control flow graph (deleting unreachable blocks, etc)

	optimizer.doInitialization();

	// Pass over each functions
	for (auto it = module.begin(); it != module.end(); ++it) {
		// Validate the generated code
		if (llvm::verifyFunction(*it, &rso)) {
			rso << '\n';
			++yyerrs;
			continue;
		}

		// Run the optimizer
		optimizer.run(*it);
	}
}

enum flags {
	lex,
	parse,
	check,
	llvmir,
	ext,
	nopt,
	none
};

flags hashflag(const string& str) {
	if (str == "-lex") return lex;
	if (str == "-parse") return parse;
	if (str == "-check") return check;
	if (str == "-llvm") return llvmir;
	if (str == "-ext") return ext;
	if (str == "-nopt") return nopt;
	return none;
}

int sys(const string& cmd) {
	return system(cmd.c_str());
}

int main (int argc, char* argv[]) {
	bool lexflag = false, parseflag = false, checkflag = false, llvmflag = false, execflag = true, optflag = true;
	string filename;

	for (int i = 1; i < argc; ++i)
		switch (hashflag(argv[i])) {
			case llvmir: llvmflag = true;
			case check: checkflag = true;
			case parse: parseflag = true;
			case lex: lexflag = true; execflag = false; break;
			case ext: yymode = START_EXTENDED; break;
			case nopt: optflag = false; break;
			default: filename = argv[i];
		}

	if (execflag)
		lexflag = parseflag = checkflag = llvmflag = true;

	if (filename.empty()) {
		cerr << "vsopc : error: no input file" << endl;
		return 1;
	} else if (not yyopen(filename.c_str())) {
		cerr << "vsopc: fatal-error: " << filename << ": No such file or directory" << endl;
		return 1;
	}

	module.setSourceFileName(filename);

	if (lexflag) {
		if (parseflag) {
			parser();

			if (checkflag) {
				checker();

				if (yyerrs == 0 and llvmflag) {
					if (optflag)
						llvmer();

					rso << module;

					if (yyerrs == 0 and execflag and system(NULL)) {
						// Get basename
						string basename = filename.substr(0, filename.find_last_of('.'));
						string object = "resources/runtime/object";

						// Dump LLVM IR code
						ofstream out(basename + ".ll");
						out << rso.str();
						out.close();

						// Compile basename.ll to assembly
						sys("llc-9 " + basename + ".ll -O2");

						// Bind with object.s and create executable
						sys("clang " + basename + ".s /usr/local/lib/vsopc/object.s -o " + basename);
					} else
						cout << rso.str() << endl;
				} else
					cout << program->toString(true) << endl;
			} else
				cout << program->toString(false) << endl;

			delete program;
		} else
			lexer();
	}

	yyclose();

	return yyerrs;
}
