#ifndef AST_H
#define AST_H

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

class Scope: std::unordered_map<std::string, std::vector<llvm::Value*>> {
	public:
		/* Methods */
		Scope& push(const std::string&, llvm::Value*);
		Scope& pop(const std::string&);
		Scope& replace(const std::string&, llvm::Value*);
		bool contains(const std::string&) const;
		llvm::Value* get(const std::string&) const;
};

struct Error {
	int line;
	int column;
	std::string msg;
};

struct LLVMHelper {
	llvm::LLVMContext& context;
	llvm::IRBuilder<>& builder;
	llvm::Module& module;
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
		virtual std::string to_string(bool) const = 0;
		virtual void codegen(Program* p, LLVMHelper& h, Scope& s, std::vector<Error>& e) {}
};

template <typename T>
class List: public std::vector<std::shared_ptr<T>>, public Node {
	public:
		/* Constructors */
		List() {}
		List(std::initializer_list<T*> init) {
			for(T* t: init)
				this->add(t);
		}

		/* Methods */
		List& add(T* t) {
			this->push_back(std::shared_ptr<T>(t));
			return *this;
		}

		virtual std::string to_string(bool with_type) const {
			if (this->empty())
				return "[]";

			std::string str = "[" + this->front()->to_string(with_type);

			for (auto it = this->begin() + 1; it != this->end(); ++it)
				str += "," + (*it)->to_string(with_type);

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
		virtual std::string to_string_aux(bool) const = 0;
		virtual std::string to_string(bool) const;

		virtual llvm::Value* codegen_aux(Program*, LLVMHelper&, Scope&, std::vector<Error>&) = 0;
		virtual void codegen(Program* p, LLVMHelper& h, Scope& s, std::vector<Error>& errors) {
			val = this->codegen_aux(p, h, s, errors);
		}
};

class Block: public Expr {
	public:
		/* Constructors */
		Block() {}
		Block(const List<Expr>&);

		/* Fields */
		List<Expr> exprs;

		/* Methods */
		virtual std::string to_string_aux(bool) const;
		virtual llvm::Value* codegen_aux(Program*, LLVMHelper&, Scope&, std::vector<Error>&);
};

class Field: public Node {
	public:
		/* Constructors */
		Field(const std::string& name, const std::string& type, Expr* init):
			name(name), type(type), init(init) {}

		/* Fields */
		std::string name, type;
		std::shared_ptr<Expr> init;

		llvm::Value* val;

		unsigned idx;

		/* Methods */
		virtual std::string to_string(bool) const;

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
		virtual std::string to_string(bool) const;

		void declaration(LLVMHelper&, std::vector<Error>&);

		llvm::Type* getType(LLVMHelper&) const;
};

class Class; // forward declaration

class Method: public Node {
	public:
		/* Constructors */
		Method(const std::string&, const List<Formal>&, const std::string&, Block*);

		/* Fields */
		std::string name, type;

		List<Formal> formals;
		std::unordered_map<std::string, std::shared_ptr<Formal>> formals_table;

		std::shared_ptr<Block> block;

		Class* parent;

		unsigned idx;

		/* Methods */
		virtual std::string to_string(bool) const;
		virtual void codegen(Program*, LLVMHelper&, Scope&, std::vector<Error>&);

		void declaration(LLVMHelper&, std::vector<Error>&);

		std::string getName() const;
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
		Class(const std::string&, const std::string&, const List<Field>&, const List<Method>&);

		/* Fields */
		std::string name, parent_name;

		List<Field> fields;
		std::unordered_map<std::string, std::shared_ptr<Field>> fields_table;
		List<Method> methods;
		std::unordered_map<std::string, std::shared_ptr<Method>> methods_table;

		Class* parent;

		/* Methods */
		virtual std::string to_string(bool) const;
		virtual void codegen(Program*, LLVMHelper&, Scope&, std::vector<Error>&);

		void declaration(LLVMHelper&, std::vector<Error>&);

		std::string getName() const;
		bool isDeclared(LLVMHelper&) const;
		llvm::StructType* getType(LLVMHelper&) const;
};

class Program: public Node {
	public:
		/* Constructors */
		Program(const List<Class>&);

		/* Fields */
		List<Class> classes;
		std::unordered_map<std::string, std::shared_ptr<Class>> classes_table;

		/* Methods */
		virtual std::string to_string(bool) const;
		virtual void codegen(Program*, LLVMHelper&, Scope&, std::vector<Error>&);

		void declaration(LLVMHelper&, std::vector<Error>&);
};

class If: public Expr {
	public:
		/* Constructors */
		If(Expr* cond, Expr* then, Expr* els): cond(cond), then(then), els(els) {}
		If(std::shared_ptr<Expr> cond, std::shared_ptr<Expr> then, std::shared_ptr<Expr> els): cond(cond), then(then), els(els) {}

		/* Fields */
		std::shared_ptr<Expr> cond, then, els;

		/* Methods */
		virtual std::string to_string_aux(bool) const;
		virtual llvm::Value* codegen_aux(Program*, LLVMHelper&, Scope&, std::vector<Error>&);
};

class While: public Expr {
	public:
		/* Constructors */
		While(Expr* cond, Expr* body): cond(cond), body(body) {}

		/* Fields */
		std::shared_ptr<Expr> cond, body;

		/* Methods */
		virtual std::string to_string_aux(bool) const;
		virtual llvm::Value* codegen_aux(Program*, LLVMHelper&, Scope&, std::vector<Error>&);
};

class Let: public Expr {
	public:
		/* Constructors */
		Let(const std::string& name, const std::string& type, Expr* init, Expr* scope):
			name(name), type(type), init(init), scope(scope) {}

		/* Fields */
		std::string name, type;
		std::shared_ptr<Expr> init, scope;

		/* Methods */
		virtual std::string to_string_aux(bool) const;
		virtual llvm::Value* codegen_aux(Program*, LLVMHelper&, Scope&, std::vector<Error>&);
		Scope& associate(Scope&) const;
		Scope& dissociate(Scope&) const;
};

class Assign: public Expr {
	public:
		/* Constructors */
		Assign(std::string name, Expr* value): name(name), value(value) {}

		/* Fields */
		std::string name;
		std::shared_ptr<Expr> value;

		/* Methods */
		virtual std::string to_string_aux(bool) const;
		virtual llvm::Value* codegen_aux(Program*, LLVMHelper&, Scope&, std::vector<Error>&);
};

class Unary: public Expr {
	public:
		enum Type { NOT, MINUS, ISNULL };

		/* Constructors */
		Unary(Type type, Expr* value): type(type), value(value) {}

		/* Fields */
		Type type;
		std::shared_ptr<Expr> value;

		/* Methods */
		virtual std::string to_string_aux(bool) const;
		virtual llvm::Value* codegen_aux(Program*, LLVMHelper&, Scope&, std::vector<Error>&);
};

class Binary: public Expr {
	public:
		enum Type { AND, EQUAL, LOWER, LOWER_EQUAL, PLUS, MINUS, TIMES, DIV, POW };

		/* Constructors */
		Binary(Type type, Expr* left, Expr* right): type(type), left(left), right(right) {}

		/* Fields */
		Type type;
		std::shared_ptr<Expr> left, right;

		/* Methods */
		virtual std::string to_string_aux(bool) const;
		virtual llvm::Value* codegen_aux(Program*, LLVMHelper&, Scope&, std::vector<Error>&);
};

class Call: public Expr {
	public:
		/* Constructors */
		Call(Expr*, const std::string&, const List<Expr>&);

		/* Fields */
		std::shared_ptr<Expr> scope;
		std::string name;
		List<Expr> args;

		/* Methods */
		virtual std::string to_string_aux(bool) const;
		virtual llvm::Value* codegen_aux(Program*, LLVMHelper&, Scope&, std::vector<Error>&);
};

class New: public Expr {
	public:
		/* Constructors */
		New(const std::string& type): type(type) {}

		/* Fields */
		std::string type;

		/* Methods */
		virtual std::string to_string_aux(bool) const;
		virtual llvm::Value* codegen_aux(Program*, LLVMHelper&, Scope&, std::vector<Error>&);
};

class Identifier: public Expr {
	public:
		/* Constructors */
		Identifier(const std::string& id): id(id) {}

		/* Fields */
		std::string id;

		/* Methods */
		virtual std::string to_string_aux(bool) const;
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
		virtual std::string to_string_aux(bool) const;
		virtual llvm::Value* codegen_aux(Program*, LLVMHelper&, Scope&, std::vector<Error>&);
};

class String: public Expr {
	public:
		/* Constructors */
		String(const std::string& str): str(str) {}

		/* Fields */
		std::string str;

		/* Methods */
		virtual std::string to_string_aux(bool) const;
		virtual llvm::Value* codegen_aux(Program*, LLVMHelper&, Scope&, std::vector<Error>&);
};

class Boolean: public Expr {
	public:
		/* Constructors */
		Boolean(bool b): b(b) {}

		/* Fields */
		bool b;

		/* Methods */
		virtual std::string to_string_aux(bool) const;
		virtual llvm::Value* codegen_aux(Program*, LLVMHelper&, Scope&, std::vector<Error>&);
};

class Unit: public Expr {
	public:
		/* Methods */
		virtual std::string to_string_aux(bool) const;
		virtual llvm::Value* codegen_aux(Program*, LLVMHelper&, Scope&, std::vector<Error>&);
};

#endif
