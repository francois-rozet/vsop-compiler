%{
	#define YY_USER_ACTION yyupdate(); // execute yyupdate() after each matched rule

	#include "tools.hpp"
	#include "vsop.tab.h"

	#include <string.h>
	#include <vector>
	#include <unordered_map>

	/* vsopc */

	extern int yymode;

	/* bison */

	extern int yyerror(const std::string&);

	/* flex */

	/// Extensions trigger
	bool yyext = false;

	/**
	 * String buffer
	 *
	 * It is used for tokens parsed in several rule matches, like strings.
	 */
	std::string yybuffer;

	/**
	 * Location stack
	 *
	 * It is used to remember nested encapsulated environment (e.g. comments)
	 * starting locations. When the end of an environment is reached, its
	 * starting location is poped, so that only unterminated environments remain.
	 *
	 * @see yypush and yypop
	 */
	std::vector<YYLTYPE> yystack;

	/**
	 * Update the location window according to token length and content
	 */
	void yyupdate() {
		yylloc.first_line = yylloc.last_line;
		yylloc.first_column = yylloc.last_column;

		for (int i = 0; i < yyleng; i++)
			if (yytext[i] == '\n') {
				++yylloc.last_line;
				yylloc.last_column = 1;
			} else
				++yylloc.last_column;
	}

	/// Push a new location in 'yystack'
	void yypush() {
		yystack.push_back(yylloc);
	}

	/**
	 * Pop the last stored location in 'yystack'
	 *
	 * @remark Also updates the location window to include the environment.
	 */
	void yypop() {
		YYLTYPE back = yystack.back();
		yystack.pop_back();

		yylloc.first_line = back.first_line;
		yylloc.first_column = back.first_column;
	}

	/**
	 * Keywords of the VSOP language
	 *
	 * @note Hash-tables allows O(1) lookups.
	 */
	std::unordered_map<std::string, int> keywords = {
		{"and", AND},
		{"bool", BOOL},
		{"class", CLASS},
		{"do", DO},
		{"else", ELSE},
		{"extends", EXTENDS},
		{"false", FALSE},
		{"if", IF},
		{"in", IN},
		{"int32", INT32},
		{"isnull", ISNULL},
		{"let", LET},
		{"new", NEW},
		{"not", NOT},
		{"string", SSTRING},
		{"then", THEN},
		{"true", TRUE},
		{"unit", UNIT},
		{"while", WHILE},
		{"self", SELF}
	};

	/// Additional keywords of the extended VSOP language
	std::unordered_map<std::string, int> extensions = {
		{"break", BREAK},
		{"double", DOUBLE},
		{"extern", EXTERN},
		{"for", FOR},
		{"lets", LETS},
		{"mod", MOD},
		{"or", OR},
		{"to", TO},
		{"vararg", VARARG}
	};

	/**
	 * Operators of the VSOP language
	 *
	 * @note Both legacy and extended operators are in this table.
	 */
	struct op { int i; std::string s; };
	std::unordered_map<std::string, op> operators = {
		{"{", {LBRACE, "lbrace"}},
		{"}", {RBRACE, "rbrace"}},
		{"(", {LPAR, "lpar"}},
		{")", {RPAR, "rpar"}},
		{":", {COLON, "colon"}},
		{";", {SEMICOLON, "semicolon"}},
		{",", {COMMA, "comma"}},
		{"+", {PLUS, "plus"}},
		{"-", {MINUS, "minus"}},
		{"*", {TIMES, "times"}},
		{"/", {DIV, "div"}},
		{"^", {POW, "pow"}},
		{".", {DOT, "dot"}},
		{"=", {EQUAL, "equal"}},
		{"<=", {LOWER_EQUAL, "lower-equal"}},
		{"<-", {ASSIGN, "assign"}},
		{"<", {LOWER, "lower"}},
		{">=", {GREATER_EQUAL, "greater-equal"}}, // -ext
		{">", {GREATER, "greater"}}, // -ext
		{"!=", {NEQUAL, "not-equal"}} // -ext
	};

	/* /!\ copy paste at line 224 to support doubles parsing
	{real_literal}{base_identifier}* {
								yylval.doubl = str2maybedouble(yytext);
								if (yylval.doubl < 0)
									yyerror("lexical error, invalid real-literal " + std::string(yytext));
								else
									return REAL_LITERAL;
							}
	*/
%}

%option noyywrap

blankspace					[ \t\n\r]
whitespace					{blankspace}+

lowercase_letter			[a-z]
uppercase_letter			[A-Z]
letter						{lowercase_letter}|{uppercase_letter}

digit						[0-9]
hex_digit					{digit}|[a-f]|[A-F]
hex_prefix					"0x"

base10_literal				{digit}+
base16_literal				{hex_prefix}{hex_digit}+
integer_literal				{base10_literal}|{base16_literal}

real_literal				({digit}+"."{digit}*)|("."{digit}+)

base_identifier				{letter}|{digit}|_
type_identifier				{uppercase_letter}{base_identifier}*
object_identifier			{lowercase_letter}{base_identifier}*

regular_char				[^\0\n\"\\]
escape_char					[btnr\"\\]|(x{hex_digit}{2})|(\n[ \t]*)
escape_sequence				\\{escape_char}

single_line_comment			"//"[^\0\n]*

base_operator				"{"|"}"|"("|")"|":"|";"|","|"+"|"-"|"*"|"/"|"^"|"."|"="|"<="|"<-"|"<"
ext_operator				">="|">"|"!="

%x STRING COMMENT
%%

%{
	// switch bison parsing mode (-lex, -parse, -ext, ...)
	switch(yymode) {
		case START_LEXER:
			yymode = 0;
			return START_LEXER;
		case START_PARSER:
			yymode = 0;
			return START_PARSER;
		case START_EXT_LEXER:
			yymode = 0;
			yyext = true;
			return START_EXT_LEXER;
		case START_EXT_PARSER:
			yymode = 0;
			yyext = true;
			return START_EXT_PARSER;
		default: break;
	}
%}

{whitespace}				/* */
{single_line_comment}		/* */
{type_identifier}			yylval.id = strdup(yytext); return TYPE_IDENTIFIER;
{object_identifier}			{
								yylval.id = strdup(yytext);
								if (keywords.find(yytext) != keywords.end())
									return keywords[yytext];
								else if (yyext and extensions.find(yytext) != extensions.end())
									return extensions[yytext];
								else
									return OBJECT_IDENTIFIER;
							}

{integer_literal}{base_identifier}* {
								yylval.int32 = str2maybeint(yytext);
								if (yylval.int32 < 0)
									yyerror("lexical error, invalid integer-literal " + std::string(yytext));
								else
									return INTEGER_LITERAL;
							}

\"							yypush(); yybuffer = ""; BEGIN(STRING);
"(*"						yypush(); BEGIN(COMMENT);

{base_operator}				yylval.id = strdup(operators[yytext].s.c_str()); return operators[yytext].i;
{ext_operator}				{
								if (yyext) {
									yylval.id = strdup(operators[yytext].s.c_str()); return operators[yytext].i;
								} else
									yyerror("lexical error, invalid operator " + std::string(yytext));
							}

<STRING>\"					yypop(); yylval.id = &yybuffer[0]; BEGIN(INITIAL); return STRING_LITERAL;
<STRING>{regular_char}+		yybuffer += yytext;
<STRING>{escape_sequence}	if (yytext[1] != '\n') yybuffer += esc2char(yytext);

<COMMENT>"(*"				yypush();
<COMMENT>"*)"				yypop(); if (yystack.empty()) BEGIN(INITIAL);
<COMMENT>[^\0]				/* */

<STRING,COMMENT><<EOF>>		yypop(); yyerror("lexical error, unterminated encapsulated environment"); return END;

<*>.|\n						yyerror("lexical error, invalid character " + char2hex(yytext[0]));
