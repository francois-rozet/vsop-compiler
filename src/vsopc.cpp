#include "vsop.tab.h"

#include "llvm/Support/raw_os_ostream.h"

#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>

using namespace std;

/* global variables */
int yymode;
List<Class> yyclasses;
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
	yymode = START_PARSER;
	yyparse();

	program = new Program(yyclasses);
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

int main (int argc, char* argv[]) {
	if (argc < 2)
		return 0;
	else if (argc < 3) {
		cerr << "vsopc " << argv[1] << ": error: no input file" << endl;
		return 1;
	} else if (not yyopen(argv[2])) {
		cerr << "vsopc: fatal-error: " << argv[2] << ": No such file or directory" << endl;
		return 1;
	}

	module.setSourceFileName(argv[2]);

	string action = argv[1];

	if (action == "-lex")
		lexer();
	else {
		parser();

		if (action == "-parse")
			cout << program->to_string(false) << endl;
		else {
			checker();

			if (action == "-check")
				cout << program->to_string(true) << endl;
			else {
				if (action == "-llvm") {
					llvm::raw_os_ostream roo(cout);
					roo << module;
				}
			}
		}

		delete program;
	}

	yyclose();

	return yyerrs;
}
