#include "vsop.tab.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace std;

/* bison */

extern int yyerrs;

extern int yyparse(void);
extern void yyrelocate(int, int);
extern void yyprint(const string&);
extern void yyerror(const string&);
extern bool yyopen(const char*);
extern void yyclose();

/* vsopc */

int yymode = START_PARSER;

List<Class> yyclasses;
List<Method> yyfunctions;

Program* program;
LLVMHelper helper("VSOP");

void lexer() {
	if (yymode == START_EXT_PARSER)
		yymode = START_EXT_LEXER;
	else
		yymode = START_LEXER;

	yyparse();
}

void parser() {
	yyparse();

	program = new Program(yyclasses, yyfunctions);
}

void checker() {
	program->declaration(helper);
	program->codegen(*program, helper);

	for (Error& e: helper.errors) {
		yyrelocate(e.pos.line, e.pos.column);
		yyerror("semantic error, " + e.msg);
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
			case llvmir: llvmflag = true; // falltrought
			case check: checkflag = true;
			case parse: parseflag = true;
			case lex: lexflag = true; execflag = false; break;
			case ext: yymode = START_EXT_PARSER; break;
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

	helper.module->setSourceFileName(filename);

	if (lexflag) { // if -lex or higher
		if (parseflag) { // if -parse or higher
			parser();

			if (checkflag) { // if -check or higher
				checker();

				if (llvmflag) { // if -llvm or higher
					if (optflag)
						yyerrs += helper.passes();

					if (yyerrs == 0) { // no errors
						if (execflag and system(NULL)) {
							// Get basename
							string basename = filename.substr(0, filename.find_last_of('.'));
							string object = "resources/runtime/object";

							// Dump LLVM IR code
							ofstream out(basename + ".ll");
							out << helper.dump();
							out.close();

							// Compile basename.ll to assembly
							sys("llc-9 " + basename + ".ll -O2");

							// Bind with object.s and create executable
							sys("clang " + basename + ".s /usr/local/lib/vsopc/object.s -lm -o " + basename);
						} else
							cout << helper.dump();
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
