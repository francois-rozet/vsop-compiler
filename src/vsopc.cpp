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
extern void yyrelocate(const Node* n);
extern void yyprint(const string&);
extern void yyerror(const string&);
extern bool yyopen(char*);
extern void yyclose();

int lexer(char* filename) {
	if (not yyopen(filename))
		return 1;

	yymode = START_LEXER;
	yyparse();

	yyclose();
	return yyerrs;
}

int parser(char* filename) {
	if (not yyopen(filename))
		return 1;

	yymode = START_PARSER;
	yyparse();

	if (yyclasses.empty()) {
		yyclose();
		return yyerrs;
	}

	Program program = Program(yyclasses);
	cout << program.to_string() << endl;

	yyclose();
	return yyerrs;
}

int checker(char* filename) {
	if (not yyopen(filename))
		return 1;

	yymode = START_PARSER;
	yyparse();

	if (yyclasses.empty()) {
		yyclose();
		return yyerrs;
	}

	Program program = Program(yyclasses);
	Scope scope;
	vector<Error> errors;

	program.redefinition(errors);
	program.inheritance(errors);
	program.override(errors);
	program.main(errors);
	program.check(&program, scope, errors);

	for (Error& e: errors) {
		yyrelocate(e.node);
		yyerror("semantic error, " + e.msg);
	}

	cout << program.to_string() << endl;

	yyclose();
	return yyerrs;
}

int main (int argc, char* argv[]) {
	if (argc < 2)
		return 0;
	else if (argc < 3) {
		cerr << "vsopc " << argv[1] << ": error: no input file" << std::endl;
		return 1;
	}

	string action = argv[1];

	if (action == "-lex")
		return lexer(argv[2]);
	else if (action == "-parse")
		return parser(argv[2]);
	else if (action == "-check")
		return checker(argv[2]);

	return 0;
}
