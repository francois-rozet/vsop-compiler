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
	List<Class>* classes;
	Class* clas;
	Class::Definition* decl;
	Field* field;
	Method* method;
	List<Formal>* formals;
	Formal* formal;
	List<Expr>* block;
	Expr* expr;
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
	/* main gloabl variables */
	extern int yymode;
	extern List<Class> yyclasses;

	/* flex global variables */
	extern FILE* yyin;

	/* bison variables */
	int yyerrs = 0;

	/* bison functions */
	int yylex(void);
	int yyparse(void);
	void yylocate(Node*, const YYLTYPE&);
	void yyrelocate(int, int);
	void yyrelocate(const YYLTYPE&);
	void yyprint(const std::string&);
	void yyerror(const std::string&);
	bool yyopen(char*);
	void yyclose();
%}

%define parse.error verbose

/* Tokens */

%token END 0 "end-of-file"

%token <num> INTEGER_LITERAL "integer-literal"
%token <id> STRING_LITERAL "string-literal"
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
%token <id> SELF "self"

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

%token START_LEXER START_PARSER;
%start start;

%nterm <id> type class-parent type_id object_id
%nterm <classes> program
%nterm <clas> class
%nterm <decl> class-aux
%nterm <field> field
%nterm <method> method
%nterm <formals> formals formals-aux
%nterm <formal> formal
%nterm <block> block block-aux args args-aux
%nterm <expr> expr expr-aux if while let unary binary call literal init

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

start:			START_LEXER token
				| START_PARSER program;

token:			/* */
				| token INTEGER_LITERAL
				{ yyprint("integer-literal," + Integer($2).to_string(false)); }
				| token STRING_LITERAL
				{ yyprint("string-literal," + String($2).to_string(false));  }
				| token TYPE_IDENTIFIER
				{ yyprint("type-identifier," + std::string($2)); }
				| token object
				{ yyprint("object-identifier," + std::string($<id>2)); }
				| token keyword
				{ yyprint($<id>2); };

object:			OBJECT_IDENTIFIER | "self";

keyword:		"and" | "bool" | "class" | "do" | "else" | "extends" | "false" | "if" | "in" | "int32" | "isnull" | "let" | "new" | "not" | "string" | "then" | "true" | "unit" | "while" | "{" | "}" | "(" | ")" | ":" | ";" | "," | "+" | "-" | "*" | "/" | "^" | "." | "=" | "<" | "<=" | "<-";

program:		class
				{ yyclasses.add($1); }
				| class program
				{ yyclasses.add($1); }
				| error
				{ yyerrok; }
				| error program
				{ yyerrok; };

class:			"class" type_id class-parent "{" class-aux
				{ $$ = new Class($2, $3, $5->fields, $5->methods); yylocate($$, @$); delete $5; };

class-parent:	/* */
				{ $$ = strdup("Object"); }
				| "extends" type_id
				{ $$ = $2; };

class-aux:		"}"
				{ $$ = new Class::Definition(); }
				| field class-aux
				{ $2->fields.add($1); $$ = $2;	}
				| method class-aux
				{ $2->methods.add($1); $$ = $2;	}
				| error "}"
				{ $$ = new Class::Definition(); yyerrok; }
				| error ";" class-aux
				{ $$ = $3; yyerrok; }
				| error block class-aux /* prevent unmatched { */
				{ $$ = $3; yyerrok; }
				| error END
				{ $$ = new Class::Definition();
					yyrelocate(@$);
					yyerror("syntax error, unexpected end-of-file, missing ending } of class declaration");
				};

field:			object_id ":" type init ";"
				{ $$ = new Field($1, $3, $4); yylocate($$, @$); };

method:			object_id formals ":" type block
				{ $$ = new Method($1, *$2, $4, new Block(*$5)); yylocate($$, @$); delete $2; };

formal:			object_id ":" type
				{ $$ = new Formal($1, $3); yylocate($$, @$); };

formals:		"(" ")"
				{ $$ = new List<Formal>(); }
				| "(" formals-aux
				{ $$ = $2; };
formals-aux:	formal ")"
				{ $$ = new List<Formal>(); $$->add($1); }
				| formal "," formals-aux
				{ $3->add($1); $$ = $3; }
				| error ")"
				{ $$ = new List<Formal>(); yyerrok; }
				| error "," formals-aux
				{ $$ = $3; yyerrok; };

object_id:		OBJECT_IDENTIFIER
				| TYPE_IDENTIFIER
				{ $$ = strdup(("my" + std::string($1)).c_str());
					yyrelocate(@$);
					yyerror("syntax error, unexpected type-identifier " + std::string($1) + ", replaced by " + std::string($$));
				};

type_id:		TYPE_IDENTIFIER
				| OBJECT_IDENTIFIER
				{ $$ = strdup($1); *$$ -= 'a' - 'A';
					yyrelocate(@$);
					yyerror("syntax error, unexpected object-identifier " + std::string($1) + ", replaced by " + std::string($$));
				};

type:			type_id
				| "int32"
				| "bool"
				| "string"
				| "unit";

block:			"{" block-aux
				{ $$ = $2; }
				| "{" "}"
				{ $$ = new List<Expr>();
					yyrelocate(@$);
					yyerror("syntax error, empty block");
				};
block-aux:		expr "}"
				{ $$ = new List<Expr>(); $$->add($1); }
				| expr ";" block-aux
				{ $3->add($1); $$ = $3; }
				| error "}"
				{ $$ = new List<Expr>(); yyerrok; }
				| error ";" block-aux
				{ $$ = $3; }
				| error block block-aux /* prevent unmatched { */
				{ $$ = $3; }
				| error END
				{ $$ = new List<Expr>();
					yyrelocate(@$);
					yyerror("syntax error, unexpected end-of-file, missing ending } of block");
				};

expr:			expr-aux
				{ $$ = $1; yylocate($$, @$); };
expr-aux:		if
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
				{ $$ = new Block(*$1); delete $1; }
				| "self"
				{ $$ = new Self(); };

if:				"if" expr "then" expr
				{ $$ = new If($2, $4, NULL); }
				| "if" expr "then" expr "else" expr
				{ $$ = new If($2, $4, $6); };

while:			"while" expr "do" expr
				{ $$ = new While($2, $4); };

let:			"let" object_id ":" type init "in" expr
				{ $$ = new Let($2, $4, $5, $7); };
init:			/* */
				{ $$ = NULL; }
				| "<-" expr
				{ $$ = $2; };

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
				{ $$ = new String($1); }
				| "true"
				{ $$ = new Boolean(true); }
				| "false"
				{ $$ = new Boolean(false); };

call:			expr "." object_id args
				{ $$ = new Call($1, $3, *$4); delete $4; }
				| object_id args
				{ $$ = new Call(new Self(), $1, *$2); delete $2; };

args:			"(" ")"
				{ $$ = new List<Expr>(); }
				| "(" args-aux
				{ $$ = $2; };
args-aux:		expr ")"
				{ $$ = new List<Expr>(); $$->add($1); }
				| expr "," args-aux
				{ $3->add($1); $$ = $3; }
				| error ")"
				{ $$ = new List<Expr>(); yyerrok; }
				| error "," args-aux
				{ $$ = $3; }
				| error END
				{ $$ = new List<Expr>();
					yyrelocate(@$);
					yyerror("syntax error, unexpected end-of-file, missing ending ) of argument list");
				};

%%

void yylocate(Node* n, const YYLTYPE& loc) {
	n->line = loc.first_line;
	n->column = loc.first_column;
}

void yyrelocate(int line, int column) {
	yylloc.first_line = line;
	yylloc.first_column = column;
}

void yyrelocate(const YYLTYPE& loc) {
	yylloc.first_line = loc.first_line;
	yylloc.first_column = loc.first_column;
}

void yyprint(const std::string& msg) {
	std::cout << yylloc.first_line << ',' << yylloc.first_column << ',';
	std::cout << msg << std::endl;
}

void yyerror(const std::string& msg) {
	std::cerr << yylloc.filename << ':' << yylloc.first_line << ':' << yylloc.first_column << ':';
	std::cerr << ' ' << msg << std::endl;
	++yyerrs;
}

bool yyopen(char* filename) {
	yylloc.filename = filename;
	yyin = fopen(yylloc.filename, "r");

	return yyin ? true : false;
}

void yyclose() {
	fclose(yyin);
}
