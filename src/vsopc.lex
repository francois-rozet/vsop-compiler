%{
	#include "define.hpp"

	#include <cstdlib>
	#include <iostream>
	#include <string>
	#include <unordered_map>

	extern int yylex();

	/* filename */
	std::string yyfilename;

	/* error */
	int yylerror = 0;

	/* token value */
	union {
		char* str;
		int num;
	} yylval;

	std::string yybuffer;

	std::string yylhex(char c) {
		std::string s = "\\x";

		char d = c / 16;
		s += d < 10 ? d + '0' : d + 'a' - 10;
		d = c % 16;
		s += d < 10 ? d + '0' : d + 'a' - 10;

		return s;
	}

	/* token location */
	struct {
		int fl;
		int fc;
		int ll;
		int lc;
	} yylloc = {1, 1, 1, 1};

	void catchup() {
		yylloc.fl = yylloc.ll;
		yylloc.fc = yylloc.lc;
	}

	void forward() {
		for (int i = 0; i < yyleng; i++)
			if (yytext[i] == '\n') {
				++yylloc.ll;
				yylloc.lc = 1;
			} else
				++yylloc.lc;
	}

	void renew() {
		catchup();
		forward();
	}

	void commentmatch() {
		int lev = 0, i;

		for (i = yybuffer.length() - 1; i > 0; --i)
			if (lev == 1)
				break;
			else if (yybuffer[i - 1] == '(' and yybuffer[i] == '*')
				++lev;
			else if (yybuffer[i - 1] == '*' and yybuffer[i] == ')')
				--lev;

		for (int j = 0; j < i; j++)
			if (yybuffer[j] == '\n') {
				++yylloc.fl;
				yylloc.fc = 1;
			} else
				++yylloc.fc;
	}

	/* nested comment level */
	int level = 0;

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

	int yylobject() {
		auto it = keywords.find(yylval.str);
		return it == keywords.end() ? T_OBJECT_IDENTIFIER : it->second;
	}

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

{whitespace}				renew();
{single_line_comment}		renew();
{type_identifier}			renew(); yylval.str = yytext; return T_TYPE_IDENTIFIER;
{object_identifier}			renew(); yylval.str = yytext; return yylobject();
{invalid_integer_literal}	{
								renew(); ++yylerror;
								std::cerr << yyfilename << ':' << yylloc.fl << ':' << yylloc.fc << ": lexical error: invalid integer literal " << yytext << std::endl;
							}
{base16_literal}			renew(); yylval.num = strtol(yytext, NULL, 16); return T_INTEGER_LITERAL;
{base10_literal}			renew(); yylval.num = strtol(yytext, NULL, 10); return T_INTEGER_LITERAL;
\"							renew(); yybuffer = yytext; BEGIN(STRING);
"(*"						renew(); yybuffer = yytext; ++level; BEGIN(COMMENT);
"{"							renew(); return T_LBRACE;
"}"							renew(); return T_RBRACE;
"("							renew(); return T_LPAR;
")"							renew(); return T_RPAR;
":"							renew(); return T_COLON;
";"							renew(); return T_SEMICOLON;
","							renew(); return T_COMMA;
"+"							renew(); return T_PLUS;
"-"							renew(); return T_MINUS;
"*"							renew(); return T_TIMES;
"/"							renew(); return T_DIV;
"^"							renew(); return T_POW;
"."							renew(); return T_DOT;
"="							renew(); return T_EQUAL;
"<="						renew(); return T_LOWER_EQUAL;
"<-"						renew(); return T_ASSIGN;
"<"							renew(); return T_LOWER;

<STRING>\"					forward(); yybuffer += yytext; yylval.str = &yybuffer[0]; BEGIN(INITIAL); return T_STRING_LITERAL;
<STRING>{regular_char}+		{
								forward();

								char d;

								for (int i = 0; i < yyleng; i++)
									if (yytext[i] >= 32 and yytext[i] <= 126)
										yybuffer += yytext[i];
									else
										yybuffer += yylhex(yytext[i]);
							}
<STRING>{escape_sequence}	{
								forward();

								switch(yytext[1]) {
									case 'b': yybuffer += yylhex('\b');
										break;
									case 't': yybuffer += yylhex('\t');
										break;
									case 'n': yybuffer += yylhex('\n');
										break;
									case 'r': yybuffer += yylhex('\r');
										break;
									case '\\': yybuffer += yylhex('\\');
										break;
									case '\"': yybuffer += yylhex('\"');
										break;
									case 'x': yybuffer += yytext;
										break;
								}
							}
<STRING><<EOF>>				{
								++yylerror;
								std::cerr << yyfilename << ':' << yylloc.fl << ':' << yylloc.fc << ": lexical error: unterminated string literal" << std::endl;
								return T_EOF;
							}

<COMMENT>"(*"				forward(); yybuffer += yytext; ++level;
<COMMENT>"*)"				forward(); if (--level == 0) { BEGIN(INITIAL); } else { yybuffer += yytext; }
<COMMENT>[^\0]				forward(); yybuffer += yytext;
<COMMENT><<EOF>>			{
								commentmatch(); ++yylerror;
								std::cerr << yyfilename << ':' << yylloc.fl << ':' << yylloc.fc << ": lexical error: unterminated comment" << std::endl;
								return T_EOF;
							}

<*>.|\n						{
								++yylerror;
								yybuffer += yylhex(yytext[0]);
								std::cerr << yyfilename << ':' << yylloc.ll << ':' << yylloc.lc << ": lexical error: invalid character " << yylhex(yytext[0]) << std::endl;
								forward();
							}

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
			std::cout << yylloc.fl << ',' << yylloc.fc;

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

	return yylerror;
}
