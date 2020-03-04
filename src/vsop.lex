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

std::string format(const char* s) {
	std::string str;

	for (; *s; ++s)
		switch (*s) {
			case '\"':
			case '\\': str += char2hex(*s); break;
			default:
				if (*s >= 32 and *s <= 126)
					str += *s;
				else
					str += char2hex(*s);
		}

	return "\"" + str + "\"";
}

int lex() {
	for (int type = yylex(); type; type = yylex()) {
		std::cout << yylloc.first_line << ',' << yylloc.first_column << ',';

		switch (type) {
			case INTEGER_LITERAL:	std::cout << "integer-literal," << yylval.num;			break;
			case STRING_LITERAL:	std::cout << "string-literal," << format(yylval.str);	break;
			case TYPE_IDENTIFIER:	std::cout << "type-identifier," << yylval.str;			break;
			case OBJECT_IDENTIFIER:	std::cout << "object-identifier," << yylval.str;		break;
			case AND:				std::cout << "and";										break;
			case BOOL:				std::cout << "bool";									break;
			case CLASS:				std::cout << "class";									break;
			case DO:				std::cout << "do";										break;
			case ELSE:				std::cout << "else";									break;
			case EXTENDS:			std::cout << "extends";									break;
			case FALSE:				std::cout << "false";									break;
			case IF:				std::cout << "if";										break;
			case IN:				std::cout << "in";										break;
			case INT32:				std::cout << "int32";									break;
			case ISNULL:			std::cout << "isnull";									break;
			case LET:				std::cout << "let";										break;
			case NEW:				std::cout << "new";										break;
			case NOT:				std::cout << "not";										break;
			case SSTRING:			std::cout << "string";									break;
			case THEN:				std::cout << "then";									break;
			case TRUE:				std::cout << "true";									break;
			case UNIT:				std::cout << "unit";									break;
			case WHILE:				std::cout << "while";									break;
			case LBRACE:			std::cout << "lbrace";									break;
			case RBRACE:			std::cout << "rbrace";									break;
			case LPAR:				std::cout << "lpar";									break;
			case RPAR:				std::cout << "rpar";									break;
			case COLON:				std::cout << "colon";									break;
			case SEMICOLON:			std::cout << "semicolon";								break;
			case COMMA:				std::cout << "comma";									break;
			case PLUS:				std::cout << "plus";									break;
			case MINUS:				std::cout << "minus";									break;
			case TIMES:				std::cout << "times";									break;
			case DIV:				std::cout << "div";										break;
			case POW:				std::cout << "pow";										break;
			case DOT:				std::cout << "dot";										break;
			case EQUAL:				std::cout << "equal";									break;
			case LOWER:				std::cout << "lower";									break;
			case LOWER_EQUAL:		std::cout << "lower-equal";								break;
			case ASSIGN:			std::cout << "assign";									break;
		}

		std::cout << std::endl;
	}

	return 0;
}
