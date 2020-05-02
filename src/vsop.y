%code top {
	#include <iostream>
	#include <string.h>
}

%code requires {
	#include "ast.hpp"
}

%union // yylval
{
	int int32;
	double doubl;
	char* id;
	Class* clas;
	Class::Definition* defn;
	Field* field;
	List<Field>* fields;
	Method* method;
	Formal* formal;
	List<Formal>* formals;
	Expr* expr;
	List<Expr>* block;
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
	extern List<Method> yyfunctions;

	/* flex global variables */
	extern FILE* yyin;
	extern bool yyext;

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
	bool yyopen(const char*);
	void yyclose();
%}

%define parse.error verbose

/* Tokens */

%token END 0 "end-of-file"

%token <int32> INTEGER_LITERAL "integer-literal"
%token <doubl> REAL_LITERAL "real-literal" // /!\ tweak lexer to support
%token <id> STRING_LITERAL "string-literal"
%token <id> TYPE_IDENTIFIER "type-identifier"
%token <id> OBJECT_IDENTIFIER "object-identifier"

%token <id> AND "and"
%token <id> BOOL "bool"
%token <id> BREAK "break" // -ext
%token <id> CLASS "class"
%token <id> DO "do"
%token <id> DOUBLE "double" // -ext
%token <id> ELSE "else"
%token <id> EXTENDS "extends"
%token <id> EXTERN "extern" // -ext
%token <id> FALSE "false"
%token <id> FOR "for" // -ext
%token <id> IF "if"
%token <id> IN "in"
%token <id> INT32 "int32"
%token <id> ISNULL "isnull"
%token <id> LET "let"
%token <id> LETS "lets" // -ext
%token <id> NEW "new"
%token <id> NOT "not"
%token <id> MOD "mod" // -ext
%token <id> OR "or" // -ext
%token <id> SELF "self"
%token <id> SSTRING "string"
%token <id> THEN "then"
%token <id> TO "to" // -ext
%token <id> TRUE "true"
%token <id> UNIT "unit"
%token <id> WHILE "while"
%token <id> VARARG "vararg"

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
%token <id> NEQUAL "!=" // -ext
%token <id> LOWER "<"
%token <id> LOWER_EQUAL "<="
%token <id> GREATER ">" // -ext
%token <id> GREATER_EQUAL ">=" // -ext
%token <id> ASSIGN "<-"

%token START_LEXER START_PARSER START_EXTENDED;
%start start;

%nterm <id> type class-parent type_id object_id
%nterm <clas> class
%nterm <defn> class-aux
%nterm <field> field
%nterm <fields> fields fields-aux // -ext
%nterm <method> method prototype
%nterm <formals> formals formals-aux
%nterm <formal> formal
%nterm <block> block block-aux args args-aux
%nterm <expr> expr expr-aux if while for let lets unary binary call literal init

%precedence "if" "then" "while" "do" "for" "to" "let" "lets" "in"
%precedence "else"

%right "<-"
%left "and" "or"
%right "not"
%nonassoc "<" "<=" ">" ">=" "=" "!="
%left "+" "-"
%left "*" "/"
%right UMINUS "isnull"
%right "mod" "^"
%left "."

%%

start:			START_LEXER token
				| START_PARSER program
				| START_EXTENDED extended;

token:			/* */
				| token INTEGER_LITERAL
				{ yyprint("integer-literal," + Integer($2).toString()); }
				| token STRING_LITERAL
				{ yyprint("string-literal," + String($2).toString());  }
				| token TYPE_IDENTIFIER
				{ yyprint("type-identifier," + std::string($2)); }
				| token object
				{ yyprint("object-identifier," + std::string($<id>2)); }
				| token keyword
				{ yyprint($<id>2); };

object:			OBJECT_IDENTIFIER | "self";

keyword:		"and" | "bool" | "break" | "class" | "do" | "double" | "else" | "extends" | "extern" | "false" | "for" | "if" | "in" | "int32" | "isnull" | "let" | "lets" | "new" | "not" | "mod" | "or" | "string" | "then" | "to" | "true" | "unit" | "while" | "vararg" | "{" | "}" | "(" | ")" | ":" | ";" | "," | "+" | "-" | "*" | "/" | "^" | "." | "=" | "!=" | "<" | "<=" | ">" | ">=" | "<-";

program:		class
				{ yyclasses.add($1); }
				| class program
				{ yyclasses.add($1); }
				| error
				{ yyerrok; }
				| error program
				{ yyerrok; };

extended:		extended-aux
				| extended-aux extended
				| error
				{ yyerrok; }
				| error extended
				{ yyerrok; };

extended-aux:	class
				{ yyclasses.add($1); }
				| method
				{ yyfunctions.add($1); };

class:			"class" type_id class-parent "{" class-aux
				{ $$ = new Class($2, $3, $5->fields, $5->methods); yylocate($$, @$); delete $5; };

class-parent:	/* */
				{ $$ = strdup("Object"); }
				| "extends" type_id
				{ $$ = $2; };

class-aux:		"}"
				{ $$ = new Class::Definition(); }
				| field ";" class-aux
				{ $3->fields.add($1); $$ = $3;	}
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

field:			object_id ":" type init
				{ $$ = new Field($1, $3, $4); yylocate($$, @$); };

fields:			"(" ")"
				{ $$ = new List<Field>(); }
				| "(" fields-aux
				{ $$ = $2; };
fields-aux:		field ")"
				{ $$ = new List<Field>(); $$->add($1); }
				| field "," fields-aux
				{ $3->add($1); $$ = $3; }
				| error ")"
				{ $$ = new List<Field>(); yyerrok; }
				| error "," fields-aux
				{ $$ = $3; yyerrok; };

prototype:		object_id formals ":" type
				{ $$ = new Method($1, *$2, $4, NULL); yylocate($$, @$); delete $2; };

method:			prototype block
				{ $1->block = std::make_shared<Block>(*$2); $$ = $1; yylocate($$->block.get(), @2); delete $2; }
				| "extern" prototype ";"
				{ $$ = $2; }
				| "extern" "vararg" prototype ";"
				{ $$ = $3; $$->variadic = true; };

formal:			object_id ":" type // possible improvement -> merge field and formal
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
				| "double"
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
				| for
				| "break"
				{ $$ = new Break(); }
				| let
				| lets
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

for:			"for" object_id "<-" expr "to" expr "do" expr
				{ $$ = new For($2, $4, $6, $8); };

let:			"let" object_id ":" type init "in" expr
				{ $$ = new Let($2, $4, $5, $7); };

lets:			"lets" fields "in" expr
				{ $$ = new Lets(*$2, $4); };

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
				| expr "or" expr
				{ $$ = new Binary(Binary::OR, $1, $3); }
				| expr "=" expr
				{ $$ = new Binary(Binary::EQUAL, $1, $3); }
				| expr "!=" expr
				{ $$ = new Binary(Binary::NEQUAL, $1, $3); }
				| expr "<" expr
				{ $$ = new Binary(Binary::LOWER, $1, $3); }
				| expr "<=" expr
				{ $$ = new Binary(Binary::LOWER_EQUAL, $1, $3); }
				| expr ">" expr
				{ $$ = new Binary(Binary::GREATER, $1, $3); }
				| expr ">=" expr
				{ $$ = new Binary(Binary::GREATER_EQUAL, $1, $3); }
				| expr "+" expr
				{ $$ = new Binary(Binary::PLUS, $1, $3); }
				| expr "-" expr
				{ $$ = new Binary(Binary::MINUS, $1, $3); }
				| expr "*" expr
				{ $$ = new Binary(Binary::TIMES, $1, $3); }
				| expr "/" expr
				{ $$ = new Binary(Binary::DIV, $1, $3); }
				| expr "^" expr
				{ $$ = new Binary(Binary::POW, $1, $3); }
				| expr "mod" expr
				{ $$ = new Binary(Binary::MOD, $1, $3); };

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
				{ $$ = new Call(yyext ? (Expr*) new Unit() : (Expr*) new Self(), $1, *$2); delete $2; };

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

bool yyopen(const char* filename) {
	yylloc.filename = strdup(filename);
	yyin = fopen(yylloc.filename, "r");

	return yyin ? true : false;
}

void yyclose() {
	fclose(yyin);
}
