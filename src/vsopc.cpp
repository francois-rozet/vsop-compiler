#include "vsop.tab.h"

#include "llvm/Support/raw_os_ostream.h"

#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>

using namespace std;

/* global variables */
int yymode = START_PARSER;

List<Class> yyclasses;
List<Method> yyfunctions;

Program* program;

llvm::LLVMContext context;
llvm::IRBuilder<> builder(context);
llvm::Module module("VSOP", context);

/* bison global variables */
extern int yyerrs;

/* bison global functions */
extern int yyparse(void);
extern void yyrelocate(int, int);
extern void yyprint(const string&);
extern void yyerror(const string&);
extern bool yyopen(char*);
extern void yyclose();

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

enum flags {
	lex,
	parse,
	check,
	llvmir,
	ext,
	none
};

flags hashflag(std::string const& str) {
	if (str == "-lex") return lex;
	if (str == "-parse") return parse;
	if (str == "-check") return check;
	if (str == "-llvm") return llvmir;
	if (str == "-ext") return ext;
	return none;
}

int main (int argc, char* argv[]) {
	bool lexflag = false, parseflag = false, checkflag = false, llvmflag = false, execflag = true;
	char* filename;

	for (int i = 1; i < argc; ++i)
		switch (hashflag(argv[i])) {
			case llvmir: llvmflag = true;
			case check: checkflag = true;
			case parse: parseflag = true;
			case lex: lexflag = true; execflag = false; break;
			case ext: yymode = START_EXTENDED; break;
			default: filename = argv[i];
		}

	if (execflag)
		lexflag = parseflag = checkflag = llvmflag = true;

	if (not filename) {
		cerr << "vsopc : error: no input file" << endl;
		return 1;
	} else if (not yyopen(filename)) {
		cerr << "vsopc: fatal-error: " << filename << ": No such file or directory" << endl;
		return 1;
	}

	module.setSourceFileName(filename);

	if (lexflag) {
		if (parseflag) {
			parser();

			if (checkflag) {
				checker();

				if (llvmflag) {
					if (execflag) {
						cout << "todo" << endl;
					} else {
						llvm::raw_os_ostream roo(cout);
						roo << module;
					}
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
