#ifndef DEFINE_H
#define DEFINE_H

#define T_EOF 0

#define T_LBRACE '{'
#define T_RBRACE '}'
#define T_LPAR '('
#define T_RPAR ')'
#define T_COLON ':'
#define T_SEMICOLON ';'
#define T_COMMA ','
#define T_PLUS '+'
#define T_MINUS '-'
#define T_TIMES '*'
#define T_DIV '/'
#define T_POW '^'
#define T_DOT '.'
#define T_EQUAL '='
#define T_LOWER '<'
#define T_LOWER_EQUAL 257 // <=
#define T_ASSIGN 258 // <-

#define T_INTEGER_LITERAL 1000
#define T_STRING_LITERAL 1001
#define T_TYPE_IDENTIFIER 1002
#define T_OBJECT_IDENTIFIER 1003

#define T_AND 2000
#define T_BOOL 2001
#define T_CLASS 2002
#define T_DO 2003
#define T_ELSE 2004
#define T_EXTENDS 2005
#define T_FALSE 2006
#define T_IF 2007
#define T_IN 2008
#define T_INT32 2009
#define T_ISNULL 2010
#define T_LET 2011
#define T_NEW 2012
#define T_NOT 2013
#define T_STRING 2014
#define T_THEN 2015
#define T_TRUE 2016
#define T_UNIT 2017
#define T_WHILE 2018

#define T_WHITESPACE 3000
#define T_COMMENT 3001

#endif
