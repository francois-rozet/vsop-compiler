%code top {
	#include <iostream>
	#include <string.h>
}

%code requires {
	#include "ast.hpp"
}

%union // yylval
{
	int num;
	char* id;
	Node* node;
	Args* args;
	Declaration* decl;
	List<Class>* pgm;
	List<Formal>* forms;
}

%code requires {
	#define YYLTYPE yyltype

	typedef struct yyltype {
		int first_line = 1;
		int first_column = 1;
		int last_line = 1;
		int last_column = 1;
		char* filename = NULL;
	} yyltype;
}

%locations // yylloc

%{
	/* flex global variables */
	extern FILE* yyin;

	/* flex global functions */
	extern int lex();

	/* bison functions */
	int yylex(void);
	void yyrelocate(const YYLTYPE&);
	void yyerror(const std::string&);

	/* AST */
	Node* root;
%}

/* Tokens */

%token END 0

%token <num> INTEGER_LITERAL
%token <node> STRING_LITERAL
%token <id> TYPE_IDENTIFIER OBJECT_IDENTIFIER

%token <id> AND "and"
%token <id> BOOL "bool"
%token <id> CLASS "class"
%token <id> DO "do"
%token <id> ELSE "else"
%token <id> EXTENDS "extends"
%token <id> FALSE "false"
%token <id> IF "if"
%token <id> IN "in"
%token <id> INT32 "int32"
%token <id> ISNULL "isnull"
%token <id> LET "let"
%token <id> NEW "new"
%token <id> NOT "not"
%token <id> SSTRING "string"
%token <id> THEN "then"
%token <id> TRUE "true"
%token <id> UNIT "unit"
%token <id> WHILE "while"

%token <id> LBRACE "{"
%token <id> RBRACE "}"
%token <id> LPAR "("
%token <id> RPAR ")"
%token <id> COLON ":"
%token <id> SEMICOLON ";"
%token <id> COMMA ","
%token <id> PLUS "+"
%token <id> MINUS "-"
%token <id> TIMES "*"
%token <id> DIV "/"
%token <id> POW "^"
%token <id> DOT "."
%token <id> EQUAL "="
%token <id> LOWER "<"
%token <id> LOWER_EQUAL "<="
%token <id> ASSIGN "<-"

%nterm <id> type class-parent
%nterm <node> class field method formal expr if while let unary binary call literal assign-tail
%nterm <args> block block-tail args args-tail
%nterm <decl> class-body
%nterm <pgm> program program-tail
%nterm <forms> formals formals-tail

%precedence "if" "then" "while" "do" "let" "in"
%precedence "else"

%right "<-"
%left "and"
%right "not"
%nonassoc "<" "<=" "="
%left "+" "-"
%left "*" "/"
%right UMINUS "isnull"
%right "^"
%left "."

%%

program:		class program-tail
				{ $2->push($1); $$ = $2; root = $$; };
program-tail:	/* */
				{ $$ = new List<Class>(); }
				| class program-tail
				{ $2->push($1); $$ = $2; };

class:			"class" TYPE_IDENTIFIER class-parent "{" class-body "}"
				{ $$ = new Class($2, $3, $5->fields, $5->methods); };
class-parent:	/* */
				{ $$ = strdup("Object"); }
				| "extends" TYPE_IDENTIFIER
				{ $$ = $2; };
class-body:		/* */
				{ $$ = new Declaration(); }
				| field class-body
				{ $2->fields.push($1); $$ = $2; }
				| method class-body
				{ $2->methods.push($1); $$ = $2; };

field:			OBJECT_IDENTIFIER ":" type assign-tail ";"
				{ $$ = new Field($1, $3, $4); };

method:			OBJECT_IDENTIFIER "(" formals ")" ":" type block
				{ $$ = new Method($1, *$3, $6, Block(*$7)); };

type:			TYPE_IDENTIFIER
				| "int32"
				| "bool"
				| "string"
				| "unit";

formal:			OBJECT_IDENTIFIER ":" type
				{ $$ = new Formal($1, $3); };
formals:		/* */
				{ $$ = new List<Formal>(); }
				| formal formals-tail
				{ $2->push($1); $$ = $2; };
formals-tail:	/* */
				{ $$ = new List<Formal>(); }
				| "," formal formals-tail
				{ $3->push($2); $$ = $3; };

expr:			if
				| while
				| let
				| unary
				| binary
				| call
				| literal
				| "new" TYPE_IDENTIFIER
				{ $$ = new New($2); }
				| OBJECT_IDENTIFIER
				{ $$ = new Identifier($1); }
				| OBJECT_IDENTIFIER "<-" expr
				{ $$ = new Assign($1, $3); }
				| "(" ")"
				{ $$ = new Unit(); }
				| "(" expr ")"
				{ $$ = $2; }
				| block
				{ $$ = new Block(*$1); };

if:				"if" expr "then" expr
				{ $$ = new If($2, $4, NULL); }
				| "if" expr "then" expr "else" expr
				{ $$ = new If($2, $4, $6); };

while:			"while" expr "do" expr
				{ $$ = new While($2, $4); };

let:			"let" OBJECT_IDENTIFIER ":" type assign-tail "in" expr
				{ $$ = new Let($2, $4, $5, $7); };

unary:			"not" expr
				{ $$ = new Unary(Unary::NOT, $2); }
				| "-" expr %prec UMINUS
				{ $$ = new Unary(Unary::MINUS, $2); }
				| "isnull" expr
				{ $$ = new Unary(Unary::ISNULL, $2); };

binary:			expr "and" expr
				{ $$ = new Binary(Binary::AND, $1, $3); }
				| expr "=" expr
				{ $$ = new Binary(Binary::EQUAL, $1, $3); }
				| expr "<" expr
				{ $$ = new Binary(Binary::LOWER, $1, $3); }
				| expr "<=" expr
				{ $$ = new Binary(Binary::LOWER_EQUAL, $1, $3); }
				| expr "+" expr
				{ $$ = new Binary(Binary::PLUS, $1, $3); }
				| expr "-" expr
				{ $$ = new Binary(Binary::MINUS, $1, $3); }
				| expr "*" expr
				{ $$ = new Binary(Binary::TIMES, $1, $3); }
				| expr "/" expr
				{ $$ = new Binary(Binary::DIV, $1, $3); }
				| expr "^" expr
				{ $$ = new Binary(Binary::POW, $1, $3); };

call:			expr "." OBJECT_IDENTIFIER "(" args ")"
				{ $$ = new Call($1, $3, *$5); }
				| OBJECT_IDENTIFIER "(" args ")"
				{ $$ = new Call(NULL, $1, *$3); };

literal:		INTEGER_LITERAL
				{ $$ = new Integer($1); }
				| STRING_LITERAL
				| "true"
				{ $$ = new Boolean(true); }
				| "false"
				{ $$ = new Boolean(false); };

assign-tail:	/* */
				{ $$ = NULL; }
				| "<-" expr
				{ $$ = $2; };

block:			"{" expr block-tail "}"
				{ $3->push($2); $$ = $3; };
block-tail:		/* */
				{ $$ = new Args(); }
				| ";" expr block-tail
				{ $3->push($2); $$ = $3; };

args:			/* */
				{ $$ = new Args(); }
				| expr args-tail
				{ $2->push($1); $$ = $2; };
args-tail:		/* */
				{ $$ = new Args(); }
				| "," expr args-tail
				{ $3->push($2); $$ = $3; };

%%

void yyrelocate(const YYLTYPE& loc) {
	yylloc.first_line = loc.first_line;
	yylloc.first_column = loc.first_column;
}

void yyerror(const std::string& msg) {
	std::cerr << yylloc.filename << ':' << yylloc.first_line << ':' << yylloc.first_column << ':';
	std::cerr << ' ' << msg << std::endl;
}

int main (int argc, char* argv[]) {
	if (argc < 2)
		return 0;
	else if (argc < 3) {
		std::cerr << "vsopc " << argv[1] << ": error: no input file" << std::endl;
		return 1;
	}

	yylloc.filename = argv[2];
	yyin = fopen(yylloc.filename, "r");

	if (not yyin) {
		std::cerr << "vsopc: fatal-error: " << yylloc.filename << ": No such file or directory" << std::endl;
		return 1;
	}

	std::string action = argv[1];

	if (action == "-lex")
		lex();
	else if (action, "-parse") {
		yyparse();

		if (not yynerrs)
			std::cout << root->to_string() << std::endl;
	}

	fclose(yyin);

	return yynerrs;
}
