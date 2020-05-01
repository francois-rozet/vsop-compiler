#ifndef AST_H
#define AST_H

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

struct LLVMHelper {
	llvm::LLVMContext& context;
	llvm::IRBuilder<>& builder;
	llvm::Module& module;
	std::vector<llvm::BasicBlock*> exits;
};

class Scope: std::unordered_map<std::string, std::vector<llvm::Value*>> {
	public:
		/* Methods */
		llvm::Value* push(const std::string&, llvm::Value*);
		llvm::Value* pop(const std::string&);
		llvm::Value* alloc(LLVMHelper& h, const std::string&, llvm::Type*);
		llvm::Value* store(LLVMHelper& h, const std::string&, llvm::Value*);
		bool contains(const std::string&) const;
		llvm::Value* get(const std::string&) const;
		llvm::Type* getType(const std::string&) const;
		llvm::Value* load(LLVMHelper& h, const std::string&) const;
};

struct Error {
	int line;
	int column;
	std::string msg;
};

class Program; // forward declaration

class Node {
	public:
		/* Constructors */
		Node() {}
		virtual ~Node() {}

		/* Fields */
		int line = 1, column = 1;

		/* Methods */
		virtual std::string toString(bool with_t=false) const = 0;
		virtual void codegen(Program* p, LLVMHelper& h, Scope& s, std::vector<Error>& e) {}
};

template <typename T>
class List: public std::vector<std::shared_ptr<T>>, public Node {
	public:
		/* Constructors */
		List() {}
		List(std::initializer_list<T*> init) {
			for(T* t: init)
				this->push_back(std::shared_ptr<T>(t));
		}
		List(std::initializer_list<std::shared_ptr<T>> init) {
			for(std::shared_ptr<T> t: init)
				this->push_back(t);
		}

		/* Methods */
		List& add(T* t) {
			return this->add(std::shared_ptr<T>(t));
		}
		List& add(std::shared_ptr<T> t) {
			this->insert(this->begin(), t);
			return *this;
		}

		virtual std::string toString(bool with_t=false) const {
			if (this->empty())
				return "[]";

			std::string str = "[" + this->front()->toString(with_t);

			for (auto it = this->begin() + 1; it != this->end(); ++it)
				str += "," + (*it)->toString(with_t);

			return str + "]";
		}

		virtual void codegen(Program* p, LLVMHelper& h, Scope& s, std::vector<Error>& errors) {
			for (std::shared_ptr<T> t: *this)
				t->codegen(p, h, s, errors);
		}
};

class Expr: public Node {
	public:
		/* Fields */
		llvm::Value* val;

		/* Methods */
		virtual std::string toString_aux(bool with_t=false) const = 0;
		virtual std::string toString(bool with_t=false) const;

		virtual llvm::Value* codegen_aux(Program*, LLVMHelper&, Scope&, std::vector<Error>&) = 0;
		virtual void codegen(Program* p, LLVMHelper& h, Scope& s, std::vector<Error>& errors) {
			val = this->codegen_aux(p, h, s, errors);
		}
};

class Block: public Expr {
	public:
		/* Constructors */
		Block() {}
		Block(const List<Expr>& exprs): exprs(exprs) {}

		/* Fields */
		List<Expr> exprs;

		/* Methods */
		virtual std::string toString_aux(bool) const;
		virtual llvm::Value* codegen_aux(Program*, LLVMHelper&, Scope&, std::vector<Error>&);
};

class Field: public Node {
	public:
		/* Constructors */
		Field(const std::string& name, const std::string& type, Expr* init):
			name(name), type(type), init(init) {}
		Field(const std::string& name, const std::string& type, std::shared_ptr<Expr> init):
			name(name), type(type), init(init) {}

		/* Fields */
		std::string name, type;
		std::shared_ptr<Expr> init;

		llvm::Value* val;

		unsigned idx;

		/* Methods */
		virtual std::string toString(bool with_t=false) const;

		llvm::Value* codegen_aux(Program*, LLVMHelper&, Scope&, std::vector<Error>&);
		virtual void codegen(Program* p, LLVMHelper& h, Scope& s, std::vector<Error>& errors) {
			val = this->codegen_aux(p, h, s, errors);
		}

		void declaration(LLVMHelper&, std::vector<Error>&);

		llvm::Type* getType(LLVMHelper&) const;
};

class Formal: public Node {
	public:
		/* Constructors */
		Formal(const std::string& name, const std::string& type): name(name), type(type) {}

		/* Fields */
		std::string name, type;

		/* Methods */
		virtual std::string toString(bool with_t=false) const;

		void declaration(LLVMHelper&, std::vector<Error>&);

		llvm::Type* getType(LLVMHelper&) const;
};

class Class; // forward declaration

class Method: public Node {
	public:
		/* Constructors */
		Method(const std::string& name, const List<Formal>& formals, const std::string& type, Block* block):
			name(name), formals(formals), type(type), block(block) {}
		Method(const std::string& name, const List<Formal>& formals, const std::string& type, std::shared_ptr<Block> block):
			name(name), formals(formals), type(type), block(block) {}

		/* Fields */
		std::string name, type;

		List<Formal> formals;
		std::unordered_map<std::string, std::shared_ptr<Formal>> formals_table;

		std::shared_ptr<Block> block;

		Class* parent;

		unsigned idx;

		/* Methods */
		virtual std::string toString(bool with_t=false) const;
		virtual void codegen(Program*, LLVMHelper&, Scope&, std::vector<Error>&);

		void declaration(LLVMHelper&, std::vector<Error>&);

		std::string getName(bool cpp=false) const;
		llvm::Function* getFunction(LLVMHelper&) const;
		llvm::FunctionType* getType(LLVMHelper&) const;
};

class Class: public Node {
	public:
		struct Definition {
			List<Field> fields;
			List<Method> methods;
		};

		/* Constructors */
		Class(const std::string& name, const std::string& parent, const List<Field>& fields, const List<Method>& methods):
			name(name), parent_name(parent), fields(fields), methods(methods) {}

		/* Fields */
		std::string name, parent_name;

		List<Field> fields;
		std::unordered_map<std::string, std::shared_ptr<Field>> fields_table;
		List<Method> methods;
		std::unordered_map<std::string, std::shared_ptr<Method>> methods_table;

		Class* parent;

		/* Methods */
		virtual std::string toString(bool with_t=false) const;
		virtual void codegen(Program*, LLVMHelper&, Scope&, std::vector<Error>&);

		void declaration(LLVMHelper&, std::vector<Error>&);

		std::string getName() const;
		bool isDeclared(LLVMHelper&) const;
		llvm::StructType* getType(LLVMHelper&) const;
};

class Program: public Node {
	public:
		/* Constructors */
		Program(const List<Class>& classes): classes(classes) {}
		Program(const List<Class>& classes, const List<Method>& functions):
			classes(classes), functions(functions) {}

		/* Fields */
		List<Class> classes;
		std::unordered_map<std::string, std::shared_ptr<Class>> classes_table;

		List<Method> functions;
		std::unordered_map<std::string, std::shared_ptr<Method>> functions_table;

		/* Methods */
		virtual std::string toString(bool with_t=false) const;
		virtual void codegen(Program*, LLVMHelper&, Scope&, std::vector<Error>&);

		void declaration(LLVMHelper&, std::vector<Error>&);
};

class If: public Expr {
	public:
		/* Constructors */
		If(Expr* cond, Expr* then, Expr* els): cond(cond), then(then), els(els) {}
		If(std::shared_ptr<Expr> cond, std::shared_ptr<Expr> then, std::shared_ptr<Expr> els):
			cond(cond), then(then), els(els) {}

		/* Fields */
		std::shared_ptr<Expr> cond, then, els;

		/* Methods */
		virtual std::string toString_aux(bool) const;
		virtual llvm::Value* codegen_aux(Program*, LLVMHelper&, Scope&, std::vector<Error>&);
};

class While: public Expr {
	public:
		/* Constructors */
		While(Expr* cond, Expr* body): cond(cond), body(body) {}
		While(std::shared_ptr<Expr> cond, std::shared_ptr<Expr> body): cond(cond), body(body) {}

		/* Fields */
		std::shared_ptr<Expr> cond, body;

		/* Methods */
		virtual std::string toString_aux(bool) const;
		virtual llvm::Value* codegen_aux(Program*, LLVMHelper&, Scope&, std::vector<Error>&);
};

class Break: public Expr {
	public:
		/* Methods */
		virtual std::string toString_aux(bool) const;
		virtual llvm::Value* codegen_aux(Program*, LLVMHelper&, Scope&, std::vector<Error>&);
};

class For: public Expr { // -ext
	public:
		/* Constructors */
		For(const std::string& name, Expr* first, Expr* last, Expr* body):
			name(name), first(first), last(last), body(body) {}
		For(const std::string& name, std::shared_ptr<Expr> first, std::shared_ptr<Expr> last, std::shared_ptr<Expr> body):
			name(name), first(first), last(last), body(body) {}

		/* Fields */
		std::string name;
		std::shared_ptr<Expr> first, last, body;

		/* Methods */
		virtual std::string toString_aux(bool) const;
		virtual llvm::Value* codegen_aux(Program*, LLVMHelper&, Scope&, std::vector<Error>&);
};

class Let: public Expr {
	public:
		/* Constructors */
		Let(const std::string& name, const std::string& type, Expr* init, Expr* scope):
			name(name), type(type), init(init), scope(scope) {}
		Let(const std::string& name, const std::string& type, std::shared_ptr<Expr> init, std::shared_ptr<Expr> scope):
			name(name), type(type), init(init), scope(scope) {}

		/* Fields */
		std::string name, type;
		std::shared_ptr<Expr> init, scope;

		/* Methods */
		virtual std::string toString_aux(bool) const;
		virtual llvm::Value* codegen_aux(Program*, LLVMHelper&, Scope&, std::vector<Error>&);
};

class Lets: public Expr { // -ext
	public:
		/* Constructors */
		Lets(const List<Field>& fields, Expr* scope): fields(fields), scope(scope) {}
		Lets(const List<Field>& fields, std::shared_ptr<Expr> scope): fields(fields), scope(scope) {}

		/* Fields */
		List<Field> fields;
		std::shared_ptr<Expr> scope;

		/* Methods */
		virtual std::string toString_aux(bool) const;
		virtual llvm::Value* codegen_aux(Program*, LLVMHelper&, Scope&, std::vector<Error>&);
};

class Assign: public Expr {
	public:
		/* Constructors */
		Assign(std::string name, Expr* value): name(name), value(value) {}
		Assign(std::string name, std::shared_ptr<Expr> value): name(name), value(value) {}

		/* Fields */
		std::string name;
		std::shared_ptr<Expr> value;

		/* Methods */
		virtual std::string toString_aux(bool) const;
		virtual llvm::Value* codegen_aux(Program*, LLVMHelper&, Scope&, std::vector<Error>&);
};

class Unary: public Expr {
	public:
		enum Type { NOT, MINUS, ISNULL };

		/* Constructors */
		Unary(Type type, Expr* value): type(type), value(value) {}
		Unary(Type type, std::shared_ptr<Expr> value): type(type), value(value) {}

		/* Fields */
		Type type;
		std::shared_ptr<Expr> value;

		/* Methods */
		virtual std::string toString_aux(bool) const;
		virtual llvm::Value* codegen_aux(Program*, LLVMHelper&, Scope&, std::vector<Error>&);
};

class Binary: public Expr {
	public:
		enum Type {AND, OR, EQUAL, NEQUAL, LOWER, LOWER_EQUAL, GREATER, GREATER_EQUAL, PLUS, MINUS, TIMES, DIV, POW, MOD };

		/* Constructors */
		Binary(Type type, Expr* left, Expr* right): type(type), left(left), right(right) {}
		Binary(Type type, std::shared_ptr<Expr> left, std::shared_ptr<Expr> right):
			type(type), left(left), right(right) {}

		/* Fields */
		Type type;
		std::shared_ptr<Expr> left, right;

		/* Methods */
		virtual std::string toString_aux(bool) const;
		virtual llvm::Value* codegen_aux(Program*, LLVMHelper&, Scope&, std::vector<Error>&);
};

class Call: public Expr {
	public:
		/* Constructors */
		Call(Expr* scope, const std::string& name, const List<Expr>& args):
			scope(scope), name(name), args(args) {}
		Call(std::shared_ptr<Expr> scope, const std::string& name, const List<Expr>& args):
			scope(scope), name(name), args(args) {}

		/* Fields */
		std::shared_ptr<Expr> scope;
		std::string name;
		List<Expr> args;

		/* Methods */
		virtual std::string toString_aux(bool) const;
		virtual llvm::Value* codegen_aux(Program*, LLVMHelper&, Scope&, std::vector<Error>&);
};

class New: public Expr {
	public:
		/* Constructors */
		New(const std::string& type): type(type) {}

		/* Fields */
		std::string type;

		/* Methods */
		virtual std::string toString_aux(bool) const;
		virtual llvm::Value* codegen_aux(Program*, LLVMHelper&, Scope&, std::vector<Error>&);
};

class Identifier: public Expr {
	public:
		/* Constructors */
		Identifier(const std::string& id): id(id) {}

		/* Fields */
		std::string id;

		/* Methods */
		virtual std::string toString_aux(bool) const;
		virtual llvm::Value* codegen_aux(Program*, LLVMHelper&, Scope&, std::vector<Error>&);
};

class Self: public Identifier {
	public:
		/* Constructors */
		Self(): Identifier("self") {}
};

class Integer: public Expr {
	public:
		/* Constructors */
		Integer(int value): value(value) {}

		/* Fields */
		int value;

		/* Methods */
		virtual std::string toString_aux(bool) const;
		virtual llvm::Value* codegen_aux(Program*, LLVMHelper&, Scope&, std::vector<Error>&);
};

class String: public Expr {
	public:
		/* Constructors */
		String(const std::string& str): str(str) {}

		/* Fields */
		std::string str;

		/* Methods */
		virtual std::string toString_aux(bool) const;
		virtual llvm::Value* codegen_aux(Program*, LLVMHelper&, Scope&, std::vector<Error>&);
};

class Boolean: public Expr {
	public:
		/* Constructors */
		Boolean(bool b): b(b) {}

		/* Fields */
		bool b;

		/* Methods */
		virtual std::string toString_aux(bool) const;
		virtual llvm::Value* codegen_aux(Program*, LLVMHelper&, Scope&, std::vector<Error>&);
};

class Unit: public Expr {
	public:
		/* Methods */
		virtual std::string toString_aux(bool) const;
		virtual llvm::Value* codegen_aux(Program*, LLVMHelper&, Scope&, std::vector<Error>&);
};

#endif
