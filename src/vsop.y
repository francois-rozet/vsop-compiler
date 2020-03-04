%{
	#include <iostream>
	#include <string>

	/* flex global variables */
	extern FILE* yyin;

	/* flex global functions */
	extern int lex();

	/* bison variables */
	char* yyfilename;
	int yyerr = 0;

	/* bison functions */
	int yylex(void);
	int yyerror(const std::string& s);
	std::string yyname(int);
%}

%locations // yylloc

%union // yylval
{
    int num;
    char* str;
}

/* Tokens */

%token-table

%token END 0 "end-of-file"

%token <num> INTEGER_LITERAL "integer-literal"
%token <str> STRING_LITERAL "string-literal"
%token <str> TYPE_IDENTIFIER "type-identifier"
%token <str> OBJECT_IDENTIFIER "object-identifier"

%token AND "and"
%token BOOL "bool"
%token CLASS "class"
%token DO "do"
%token ELSE "else"
%token EXTENDS "extends"
%token FALSE "false"
%token IF "if"
%token IN "in"
%token INT32 "int32"
%token ISNULL "isnull"
%token LET "let"
%token NEW "new"
%token NOT "not"
%token STRING "string"
%token THEN "then"
%token TRUE "true"
%token UNIT "unit"
%token WHILE "while"

%token LBRACE "lbrace"
%token RBRACE "rbrace"
%token LPAR "lpar"
%token RPAR "rpar"
%token COLON "colon"
%token SEMICOLON "semicolon"
%token COMMA "comma"
%token PLUS "plus"
%token MINUS "minus"
%token TIMES "times"
%token DIV "div"
%token POW "pow"
%token DOT "dot"
%token EQUAL "equal"
%token LOWER "lower"
%token LOWER_EQUAL "lower-equal"
%token ASSIGN "assign"

%%

Exp	:	INTEGER_LITERAL { $<num>$ = $1; };

%%

int yyerror(const std::string& s) {
	std::cerr << yyfilename << ':' << yylloc.first_line << ':' << yylloc.first_column << ':';
	std::cerr << ' ' << s << std::endl;

	return ++yyerr;
}

std::string yyname(int type) {
	std::string s = yytname[yytranslate[type]];
	return s.substr(1, s.length() - 2);
}

int parse() {
	return 0;
}

int main (int argc, char* argv[]) {
	if (argc < 2)
		return 0;
	else if (argc < 3) {
		std::cerr << "vsopc " << argv[1] << ": error: no input file" << std::endl;
		return 1;
	} else {
		yyfilename = argv[2];
		yyin = fopen(yyfilename, "r");

		if (not yyin) {
			std::cerr << "vsopc: fatal-error: " << yyfilename << ": No such file or directory" << std::endl;
			return -1;
		}

		std::string action = argv[1];

		if (action == "-lex")
			lex();
		else if (action, "-parse")
			parse();

		fclose(yyin);
	}

	return yyerr;
}
