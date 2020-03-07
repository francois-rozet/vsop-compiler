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

	/* bison variables */
	int yyerrs = 0;

	/* bison functions */
	int yylex(void);
	void yyrelocate(const YYLTYPE&);
	int yyerror(const std::string&);

	/* AST */
	Node* root;
%}

%define parse.error verbose

/* Tokens */

%token END 0 "end-of-file"

%token <num> INTEGER_LITERAL "integer-literal"
%token <node> STRING_LITERAL "string-literal"
%token <id> TYPE_IDENTIFIER "type-identifier"
%token <id> OBJECT_IDENTIFIER "object-identifier"

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

%nterm <id> type class-parent type_id object_id
%nterm <node> class field method formal expr if while let unary binary call literal
%nterm <args> block block-aux args args-aux
%nterm <decl> class-aux
%nterm <pgm> program
%nterm <forms> formals formals-aux

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

program:		class
				{ $$ = new List<Class>(); $$->push($1); root = $$; }
				| class program
				{ $2->push($1); $$ = $2; }
				| error
				{ $$ = new List<Class>(); root = $$; yyerrok; }
				| error program
				{ $$ = $2; yyerrok; };

class:			"class" type_id class-parent "{" class-aux
				{ $$ = new Class($2, $3, $5->fields, $5->methods); };

class-parent:	/* */
				{ $$ = strdup("Object"); }
				| "extends" type_id
				{ $$ = $2; };

class-aux:		"}"
				{ $$ = new Declaration(); }
				| field class-aux
				{ $2->fields.push($1); $$ = $2; }
				| method class-aux
				{ $2->methods.push($1); $$ = $2; }
				| error "}"
				{ $$ = new Declaration(); yyerrok; };

field:			object_id ":" type ";"
				{ $$ = new Field($1, $3, NULL); }
				| object_id ":" type "<-" expr ";"
				{ $$ = new Field($1, $3, $5); };

method:			object_id formals ":" type block
				{ $$ = new Method($1, *$2, $4, Block(*$5)); };

object_id:		OBJECT_IDENTIFIER
				| TYPE_IDENTIFIER
				{ $$ = strdup($1); *$$ += 'a' - 'A'; yyerror("syntax error, unexpected type-identifier " + std::string($1) + ", replaced by " + std::string($$)); };

type_id:		TYPE_IDENTIFIER
				| OBJECT_IDENTIFIER
				{ $$ = strdup($1); *$$ -= 'a' - 'A'; yyerror("syntax error, unexpected object-identifier " + std::string($1) + ", replaced by " + std::string($$)); };

type:			type_id
				| "int32"
				| "bool"
				| "string"
				| "unit";

formal:			object_id ":" type
				{ $$ = new Formal($1, $3); };

formals:		"(" ")"
				{ $$ = new List<Formal>(); }
				| "(" formals-aux
				{ $$ = $2; };
formals-aux:	formal ")"
				{ $$ = new List<Formal>(); $$->push($1); }
				| formal "," formals-aux
				{ $3->push($1); $$ = $3; }
				| error ")"
				{ $$ = new List<Formal>(); yyerrok; };

block:			"{" block-aux
				{ $$ = $2; };
block-aux:		expr "}"
				{ $$ = new Args(); $$->push($1); }
				| expr ";" block-aux
				{ $3->push($1); $$ = $3; }
				| error "}"
				{ $$ = new Args(); yyerrok; };

expr:			if
				| while
				| let
				| unary
				| binary
				| call
				| literal
				| "new" type_id
				{ $$ = new New($2); }
				| object_id
				{ $$ = new Identifier($1); }
				| object_id "<-" expr
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

let:			"let" object_id ":" type "in" expr
				{ $$ = new Let($2, $4, NULL, $6); }
				| "let" object_id ":" type "<-" expr "in" expr
				{ $$ = new Let($2, $4, $6, $8); };

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

literal:		INTEGER_LITERAL
				{ $$ = new Integer($1); }
				| STRING_LITERAL
				| "true"
				{ $$ = new Boolean(true); }
				| "false"
				{ $$ = new Boolean(false); };

call:			expr "." object_id args
				{ $$ = new Call($1, $3, *$4); }
				| object_id args
				{ $$ = new Call(NULL, $1, *$2); };

args:			"(" ")"
				{ $$ = new Args(); }
				| "(" args-aux
				{ $$ = $2; };
args-aux:		expr ")"
				{ $$ = new Args(); $$->push($1); }
				| expr "," args-aux
				{ $3->push($1); $$ = $3; }
				| error ")"
				{ $$ = new Args(); yyerrok; };

%%

void yyrelocate(const YYLTYPE& loc) {
	yylloc.first_line = loc.first_line;
	yylloc.first_column = loc.first_column;
}

int yyerror(const std::string& msg) {
	std::cerr << yylloc.filename << ':' << yylloc.first_line << ':' << yylloc.first_column << ':';
	std::cerr << ' ' << msg << std::endl;

	return ++yyerrs;
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

		if (root)
			std::cout << root->to_string() << std::endl;
	}

	fclose(yyin);

	return yyerrs;
}
