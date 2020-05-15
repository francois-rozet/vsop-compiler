# VSOP Compiler

The *Very Simple Object-oriented Programming* (VSOP) language is a simple object-oriented language inspired from [COOL](https://dl.acm.org/doi/abs/10.1145/381841.381847), the *Classroom Object-Oriented Language* designed by **Alex Aiken**. More information is provided in the [VSOP manual](resources/pdf/vsop.pdf).

This project consists in the realization of a VSOP compiler. It has been realized under the direction of **Cyril Soldani** as part of the course *Compilers* given by **Pascale Fontaine** to graduate computer science students at the [University of Liège](https://www.uliege.be/) during the academic year 2019-2020.

> A few test files are provided in the [`resources/vsop/`](resources/vsop/) directory.

## Installation

The compiler has been developped in standard `C++14`, using [`flex`](https://github.com/westes/flex/), [`bison`](https://github.com/akimd/bison) and [`LLVM`](https://llvm.org/) version `9.0.0`.

One can install these dependencies with

```bash
make install-tools
```

and then build the compiler executable with

```bash
make vsopc
```

## Implementation

The project was divided into four parts.

1. The [lexical analysis](resources/pdf/lexical-analysis.pdf) consists in converting the input VSOP file into a sequence of tokens.

	```bash
	./vsopc -lex resources/vsop/functional/002-hello-world.vsop
	```

	```
	1,1,class
	1,7,type-identifier,Main
	1,12,lbrace
	2,5,object-identifier,main
	2,9,lpar
	2,10,rpar
	2,12,colon
	2,14,int32
	2,20,lbrace
	3,9,object-identifier,print
	3,14,lpar
	3,15,string-literal,"Hello, world!\x0a"
	3,32,rpar
	3,33,semicolon
	4,9,integer-literal,0
	5,5,rbrace
	6,1,rbrace
	```

2. The [syntax analysis](resources/pdf/syntax-analysis.pdf) uses the tokens to generate an *Abstract Syntax Tree* of the input.

	```bash
	./vsopc -parse resources/vsop/functional/002-hello-world.vsop
	```

	```
	[Class(Main, Object, [], [
		Method(main, [], int32, [
			Call(self, print, ["Hello, world!\x0a"]),
			0
		])
	])]
	```

3. The [semantic analysis](resources/pdf/semantic-analysis.pdf) performs the type and scope checking while traveling through the AST.

	```bash
	./vsopc -parse resources/vsop/functional/002-hello-world.vsop
	```

	```
	[Class(Main, Object, [], [
		Method(main, [], int32, [
			Call(self:Main, print, ["Hello, world!\x0a":string]):Object,
			0:int32
		])
	]:int32)]
	```

4. The [code generation](resources/pdf/code-generation.pdf) translates the input to an `LLVM` intermediate representation and, finally, compiles it.

	```bash
	./vsopc resources/vsop/functional/002-hello-world.vsop
	./resources/vsop/functional/002-hello-world
	```

	```
	Hello, world!
	```

Some explanations about the implementation can be found in the [project report](latex/main.pdf) as well as in the code itself, that has been documented to some extent.

### Extensions

A few extensions have been added to the vanilla VSOP language, such as floating point arithmetic, top-level functions, foreign function interface, etc.

Their description is available in the [project report](latex/main.pdf) at the section *Extensions*.
