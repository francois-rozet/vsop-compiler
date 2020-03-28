%{
	#define YY_USER_ACTION yyupdate();

	#include "tools.hpp"
	#include "vsop.tab.h"

	#include <string.h>
	#include <vector>
	#include <unordered_map>

	/* bison global functions */
	extern int yyerror(const std::string&);

	/* flex global variables */
	int yymode;
	std::string yybuffer;
	std::vector<YYLTYPE> yystack;

	/* flex functions */
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

	void yypush() {
		yystack.push_back(yylloc);
	}

	void yypop() {
		YYLTYPE back = yystack.back();
		yystack.pop_back();

		yylloc.first_line = back.first_line;
		yylloc.first_column = back.first_column;
	}

	/* keywords */
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
		{"while", WHILE}
	};
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
invalid_integer_literal		{integer_literal}([g-z]|[G-Z]|_)

base_identifier				{letter}|{digit}|_
type_identifier				{uppercase_letter}{base_identifier}*
object_identifier			{lowercase_letter}{base_identifier}*

regular_char				[^\0\n\"\\]
escape_char					[btnr\"\\]|(x{hex_digit}{2})|(\n[ \t]*)
escape_sequence				\\{escape_char}

single_line_comment			"//"[^\0\n]*

%x STRING COMMENT
%%

%{
	if (yymode) {
		int temp = yymode;
		yymode = 0;
		return temp;
	}
%}

{whitespace}				/* */
{single_line_comment}		/* */
{type_identifier}			yylval.id = strdup(yytext); return TYPE_IDENTIFIER;
{object_identifier}			yylval.id = strdup(yytext); return keywords.find(yytext) == keywords.end() ? OBJECT_IDENTIFIER : keywords[yytext];
{invalid_integer_literal}	yyerror("lexical error, invalid integer-literal " + std::string(yytext));
{base16_literal}			yylval.num = str2num(yytext, 16); return INTEGER_LITERAL;
{base10_literal}			yylval.num = str2num(yytext, 10); return INTEGER_LITERAL;
\"							yypush(); yybuffer = ""; BEGIN(STRING);
"(*"						yypush(); BEGIN(COMMENT);
"{"							yylval.id = strdup("lbrace"); return LBRACE;
"}"							yylval.id = strdup("rbrace"); return RBRACE;
"("							yylval.id = strdup("lpar"); return LPAR;
")"							yylval.id = strdup("rpar"); return RPAR;
":"							yylval.id = strdup("colon"); return COLON;
";"							yylval.id = strdup("semicolon"); return SEMICOLON;
","							yylval.id = strdup("comma"); return COMMA;
"+"							yylval.id = strdup("plus"); return PLUS;
"-"							yylval.id = strdup("minus"); return MINUS;
"*"							yylval.id = strdup("times"); return TIMES;
"/"							yylval.id = strdup("div"); return DIV;
"^"							yylval.id = strdup("pow"); return POW;
"."							yylval.id = strdup("dot"); return DOT;
"="							yylval.id = strdup("equal"); return EQUAL;
"<="						yylval.id = strdup("lower-equal"); return LOWER_EQUAL;
"<-"						yylval.id = strdup("assign"); return ASSIGN;
"<"							yylval.id = strdup("lower"); return LOWER;

<STRING>\"					yypop(); yylval.id = &yybuffer[0]; BEGIN(INITIAL); return STRING_LITERAL;
<STRING>{regular_char}+		yybuffer += yytext;
<STRING>{escape_sequence}	if (yytext[1] != '\n') yybuffer += esc2char(yytext);

<COMMENT>"(*"				yypush();
<COMMENT>"*)"				yypop(); if (yystack.empty()) BEGIN(INITIAL);
<COMMENT>[^\0]				/* */

<STRING,COMMENT><<EOF>>		yypop(); yyerror("lexical error, unterminated encapsulated environment"); return END;

<*>.|\n						yyerror("lexical error, invalid character " + char2hex(yytext[0]));
