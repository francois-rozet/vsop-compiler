%{
	#define YY_USER_ACTION updateloc();

	#include "define.hpp"

	#include <cstdlib>
	#include <iostream>
	#include <string>
	#include <vector>
	#include <unordered_map>

	extern int yylex();

	/* filename */
	std::string yyfilename;

	/* token value */
	union {
		char* str;
		int num;
	} yylval;

	std::string yybuffer;

	std::string yyhex(char c) {
		std::string s = "\\x";

		char d = c / 16;
		s += d + (d < 10 ? '0' : 'a' - 10);
		d = c % 16;
		s += d + (d < 10 ? '0' : 'a' - 10);

		return s;
	}

	/* token location */
	struct loc {
		int l; // line
		int c; // column
	};

	struct {
		loc f; // first
		loc l; // last
	} yylloc = {{1, 1}, {1, 1}};

	std::vector<loc> yystack;

	void updateloc() {
		yylloc.f = yylloc.l;

		for (int i = 0; i < yyleng; i++)
			if (yytext[i] == '\n') {
				++yylloc.l.l;
				yylloc.l.c = 1;
			} else
				++yylloc.l.c;
	}

	/* error */
	int yyerror = 0;

	void error(const std::string& s) {
		++yyerror;
		std::cerr << yyfilename << ':' << yylloc.f.l << ':' << yylloc.f.c << ": lexical error";
		std::cerr << ": " << s << std::endl;
	};

	/* Keywords */
	std::unordered_map<std::string, int> keywords = {
		{"and", T_AND},
		{"bool", T_BOOL},
		{"class", T_CLASS},
		{"do", T_DO},
		{"else", T_ELSE},
		{"extends", T_EXTENDS},
		{"false", T_FALSE},
		{"if", T_IF},
		{"in", T_IN},
		{"int32", T_INT32},
		{"isnull", T_ISNULL},
		{"let", T_LET},
		{"new", T_NEW},
		{"not", T_NOT},
		{"string", T_STRING},
		{"then", T_THEN},
		{"true", T_TRUE},
		{"unit", T_UNIT},
		{"while", T_WHILE}
	};

	/* Escape sequences */
	std::unordered_map<char, char> esequences = {
		{'b', '\b'},
		{'t', '\t'},
		{'n', '\n'},
		{'r', '\r'},
		{'\\', '\\'},
		{'\"', '\"'}
	};

	/* (Standard) output */
	std::unordered_map<int, std::string> output = {
		{T_LBRACE, "lbrace"},
		{T_RBRACE, "rbrace"},
		{T_LPAR, "lpar"},
		{T_RPAR, "rpar"},
		{T_COLON, "colon"},
		{T_SEMICOLON, "semicolon"},
		{T_COMMA, "comma"},
		{T_PLUS, "plus"},
		{T_MINUS, "minus"},
		{T_TIMES, "times"},
		{T_DIV, "div"},
		{T_POW, "pow"},
		{T_DOT, "dot"},
		{T_EQUAL, "equal"},
		{T_LOWER, "lower"},
		{T_LOWER_EQUAL, "lower-equal"},
		{T_ASSIGN, "assign"},
		{T_INTEGER_LITERAL, "integer-literal"},
		{T_STRING_LITERAL, "string-literal"},
		{T_TYPE_IDENTIFIER, "type-identifier"},
		{T_OBJECT_IDENTIFIER, "object-identifier"},
		{T_AND, "and"},
		{T_BOOL, "bool"},
		{T_CLASS, "class"},
		{T_DO, "do"},
		{T_ELSE, "else"},
		{T_EXTENDS, "extends"},
		{T_FALSE, "false"},
		{T_IF, "if"},
		{T_IN, "in"},
		{T_INT32, "int32"},
		{T_ISNULL, "isnull"},
		{T_LET, "let"},
		{T_NEW, "new"},
		{T_NOT, "not"},
		{T_STRING, "string"},
		{T_THEN, "then"},
		{T_TRUE, "true"},
		{T_UNIT, "unit"},
		{T_WHILE, "while"}
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
{type_identifier}			yylval.str = yytext; return T_TYPE_IDENTIFIER;
{object_identifier}			yylval.str = yytext; return keywords.find(yylval.str) == keywords.end() ? T_OBJECT_IDENTIFIER : keywords[yylval.str];
{invalid_integer_literal}	error("invalid integer-literal " + std::string(yytext));
{base16_literal}			yylval.num = strtol(yytext, NULL, 16); return T_INTEGER_LITERAL;
{base10_literal}			yylval.num = strtol(yytext, NULL, 10); return T_INTEGER_LITERAL;
\"							yystack.push_back(yylloc.f); yybuffer = yytext; BEGIN(STRING);
"(*"						yystack.push_back(yylloc.f); yybuffer = yytext; BEGIN(COMMENT);
"{"							return T_LBRACE;
"}"							return T_RBRACE;
"("							return T_LPAR;
")"							return T_RPAR;
":"							return T_COLON;
";"							return T_SEMICOLON;
","							return T_COMMA;
"+"							return T_PLUS;
"-"							return T_MINUS;
"*"							return T_TIMES;
"/"							return T_DIV;
"^"							return T_POW;
"."							return T_DOT;
"="							return T_EQUAL;
"<="						return T_LOWER_EQUAL;
"<-"						return T_ASSIGN;
"<"							return T_LOWER;

<STRING>\"					yylloc.f = yystack.back(); yystack.pop_back(); yybuffer += yytext; yylval.str = &yybuffer[0]; BEGIN(INITIAL); return T_STRING_LITERAL;
<STRING>{regular_char}		{
								if (yytext[0] >= 32 and yytext[0] <= 126)
									yybuffer += yytext[0];
								else
									yybuffer += yyhex(yytext[0]);
							}
<STRING>{escape_sequence}	{
								switch(yytext[1]) {
									case 'x': yybuffer += yytext;
									case '\n':
										break;
									default:
										yybuffer += yyhex(esequences[yytext[1]]);
								}
							}
<STRING><<EOF>>				yylloc.f = yystack.back(); error("unterminated string-literal"); return T_EOF;

<COMMENT>"(*"				yystack.push_back(yylloc.f);
<COMMENT>"*)"				yystack.pop_back(); if (yystack.empty()) BEGIN(INITIAL);
<COMMENT>[^\0]				/* */
<COMMENT><<EOF>>			yylloc.f = yystack.back(); error("unterminated comment"); return T_EOF;

<*>.|\n						error("invalid character " + yyhex(yytext[0]));

%%

int main (int argc, char* argv[]) {
	if (argc < 2)
		return 0;

	if (argc < 3) {
		std::cerr << "vsopc " << argv[1] << ": error: no input file" << std::endl;
		return -1;
	}

	if (std::string(argv[1]) == "-lex") {
		FILE* file = fopen(argv[2], "r");

		if (not file) {
			std::cerr << "-lex: error: " << argv[2] << ": No such file or directory" << std::endl;
			return -1;
		}

		yyfilename = argv[2];
		yyin = file;

		for (int type = yylex(); type; type = yylex()) {
			std::cout << yylloc.f.l << ',' << yylloc.f.c;

			auto it = output.find(type);
			std::cout << ',' << it->second;

			switch (type) {
				case T_INTEGER_LITERAL: std::cout << ',' << yylval.num;
					break;
				case T_TYPE_IDENTIFIER:
				case T_OBJECT_IDENTIFIER:
				case T_STRING_LITERAL: std::cout << ',' << yylval.str;
			}

			std::cout << std::endl;
		}
	}

	return yyerror;
}
