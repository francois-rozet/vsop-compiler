%{
	#define YY_USER_ACTION yyupdate();

	#include "tools.hpp"
	#include "vsop.tab.h"

	#include <iostream>
	#include <string>
	#include <vector>
	#include <unordered_map>

	/* flex variables */
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

	YYLTYPE yypop() {
		YYLTYPE loc = yystack.back();
		yystack.pop_back();

		loc.last_line = yylloc.last_line;
		loc.last_column = yylloc.last_column;

		return loc;
	}

	/* bison global functions */
	extern int yylex();
	extern int yyerror(const std::string&);
	extern std::string yyname(int);

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
		{"string", STRING},
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

{whitespace}				/* */
{single_line_comment}		/* */
{type_identifier}			yylval.str = yytext; return TYPE_IDENTIFIER;
{object_identifier}			yylval.str = yytext; return keywords.find(yytext) == keywords.end() ? OBJECT_IDENTIFIER : keywords[yytext];
{invalid_integer_literal}	yyerror("lexical error, invalid integer-literal " + std::string(yytext));
{base16_literal}			yylval.num = str2num(yytext, 16); return INTEGER_LITERAL;
{base10_literal}			yylval.num = str2num(yytext, 10); return INTEGER_LITERAL;
\"							yypush(); yybuffer = ""; BEGIN(STRING);
"(*"						yypush(); BEGIN(COMMENT);
"{"							return LBRACE;
"}"							return RBRACE;
"("							return LPAR;
")"							return RPAR;
":"							return COLON;
";"							return SEMICOLON;
","							return COMMA;
"+"							return PLUS;
"-"							return MINUS;
"*"							return TIMES;
"/"							return DIV;
"^"							return POW;
"."							return DOT;
"="							return EQUAL;
"<="						return LOWER_EQUAL;
"<-"						return ASSIGN;
"<"							return LOWER;

<STRING>\"					yylloc = yypop(); yylval.str = &yybuffer[0]; BEGIN(INITIAL); return STRING_LITERAL;
<STRING>{regular_char}+		yybuffer += yytext;
<STRING>{escape_sequence}	if (yytext[1] != '\n') yybuffer += esc2char(yytext);

<COMMENT>"(*"				yypush();
<COMMENT>"*)"				yypop(); if (yystack.empty()) BEGIN(INITIAL);
<COMMENT>[^\0]				/* */

<STRING,COMMENT><<EOF>>		yylloc = yypop(); yyerror("lexical error, unterminated encapsulated environment"); return END;

<*>.|\n						yyerror("lexical error, invalid character " + char2hex(yytext[0]));

%%

int lex() {
	for (int type = yylex(); type; type = yylex()) {
		std::cout << yylloc.first_line << ',' << yylloc.first_column;

		std::cout << ',' << yyname(type);

		switch (type) {
			case INTEGER_LITERAL: std::cout << ',' << yylval.num;
				break;
			case TYPE_IDENTIFIER:
			case OBJECT_IDENTIFIER: std::cout << ',' << yylval.str;
				break;
			case STRING_LITERAL:
				std::cout << ',' << '\"';
				for (char* s = yylval.str; *s; ++s)
					switch (*s) {
						case '\"':
						case '\\': std::cout << char2hex(*s);
							break;
						default:
							if (*s >= 32 and *s <= 126)
								std::cout << *s;
							else
								std::cout << char2hex(*s);
					}
				std::cout << '\"';
		}

		std::cout << std::endl;
	}

	return 0;
}
