#include "vsop.tab.h"

#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>

using namespace std;

/* global variables */
int yymode;
List<Class> yyclasses;

/* bison global variables */
extern int yyerrs;

/* bison global functions */
extern int yyparse(void);
extern void yyrelocate(int, int);
extern void yyprint(const string&);
extern void yyerror(const string&);
extern bool yyopen(char*);
extern void yyclose();

int lexer() {
	yymode = START_LEXER;
	yyparse();

	return 0;
}

int parser() {
	yymode = START_PARSER;
	yyparse();

	if (yyclasses.empty())
		return 1;

	Program program = Program(yyclasses);
	cout << program.to_string() << endl;

	return 0;
}

int checker() {
	yymode = START_PARSER;
	yyparse();

	if (yyclasses.empty())
		return 1;

	Program program = Program(yyclasses);
	Scope scope;
	vector<Error> errors;

	program.augment(errors);
	program.check(&program, scope, errors);

	for (Error& e: errors) {
		yyrelocate(e.line, e.column);
		yyerror("semantic error, " + e.msg);
	}

	cout << program.to_string() << endl;

	return 0;
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

	string action = argv[1];

	if (action == "-lex")
		lexer();
	else if (action == "-parse")
		parser();
	else if (action == "-check")
		checker();

	yyclose();

	return yyerrs;
}
