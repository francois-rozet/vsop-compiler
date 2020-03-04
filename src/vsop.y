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
%}

%locations // yylloc

%union // yylval
{
    int num;
    char* str;
}

/* Tokens */

%token END 0

%token <num> INTEGER_LITERAL
%token <str> STRING_LITERAL TYPE_IDENTIFIER OBJECT_IDENTIFIER

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
%token SSTRING "string"
%token THEN "then"
%token TRUE "true"
%token UNIT "unit"
%token WHILE "while"

%token LBRACE "{"
%token RBRACE "}"
%token LPAR "("
%token RPAR ")"
%token COLON ":"
%token SEMICOLON ";"
%token COMMA ","
%token PLUS "+"
%token MINUS "-"
%token TIMES "*"
%token DIV "/"
%token POW "^"
%token DOT "."
%token EQUAL "="
%token LOWER "<"
%token LOWER_EQUAL "<="
%token ASSIGN "<-"

%left "." "*" "/" "+" "-" "and"
%right "^" "isnull" "not" "<-"
%nonassoc "<" "<=" "="

%%

program:		class program-tail END;
program-tail:	/* */
				| class program-tail;

class:			"class" TYPE_IDENTIFIER class-parent "{" class-body "}";
class-parent:	/* */
				| "extends" TYPE_IDENTIFIER;
class-body:		/* */
				| field class-body
				| method class-body;

field:			OBJECT_IDENTIFIER ":" type option-assign ";";
option-assign:	/* */
				| "<-" expr;

method:			OBJECT_IDENTIFIER "(" formals ")" ":" type block;

type:			TYPE_IDENTIFIER
				| "int32"
				| "bool"
				| "string"
				| "unit";

formal:			OBJECT_IDENTIFIER ":" type;
formals:		/* */
				| formal formals-tail;
formals-tail:	/* */
				| "," formal formals-tail;

block:			"{" expr block-tail "}";
block-tail:		/* */
				| ";" expr block-tail;

literal:		INTEGER_LITERAL
				| STRING_LITERAL
				| "true"
				| "false";

args:			/* */
				| expr args-tail;
args-tail:		/* */
				| "," expr args-tail;

expr:			"if" expr "then" expr if-tail
				| "while" expr "do" expr
				| "let" OBJECT_IDENTIFIER ":" type option-assign "in" expr
				| "not" expr
				| "-" expr
				| "isnull" expr
				| "new" TYPE_IDENTIFIER
				| literal
				| block
				| expr expr-tail-0
				| OBJECT_IDENTIFIER expr-tail-1
				| "(" expr-tail-2;

if-tail:		/* */
				| "else" expr;

expr-tail-0:	"and" expr
				| "=" expr
				| "<" expr
				| "<=" expr
				| "+" expr
				| "-" expr
				| "*" expr
				| "/" expr
				| "^" expr
				| "." OBJECT_IDENTIFIER "(" args ")";

expr-tail-1:	/* */
				| "<-" expr
				| "(" args ")";

expr-tail-2:	")"
				| expr ")";

%%

int yyerror(const std::string& s) {
	std::cerr << yyfilename << ':' << yylloc.first_line << ':' << yylloc.first_column << ':';
	std::cerr << ' ' << s << std::endl;

	return ++yyerr;
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
			yyparse();

		fclose(yyin);
	}

	return yyerr;
}
