#ifndef AST_H
#define AST_H

#include "llvm.hpp"

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

class Program; // forward declaration

/**
 * AST abstract node
 */
class Node {
	public:
		Node() {}
		virtual ~Node() {}

		/// Position in the parsed file
		Position pos = { 1, 1 };

		/// Produce string representation, with or without type augmentation
		virtual std::string toString(bool with_t=false) const = 0;

		/// Generate code
		virtual void codegen(Program& p, LLVMHelper& h) {}
};

/**
 * AST list node
 *
 * @tparam T the type of stored nodes
 *
 * @remark Nodes are stored with smart pointers, so that memory is automatically managed. Therefore, one should avoid to deallocate the nodes manually.
 */
template <typename T>
class List: public std::vector<std::shared_ptr<T>>, public Node {
	public:
		List() {}
		List(std::initializer_list<std::shared_ptr<T>> init) {
			for(std::shared_ptr<T> t: init)
				this->push(t);
		}
		List(std::initializer_list<T*> init) {
			for(T* t: init)
				this->push(t);
		}

		/// Push node at the back of the list
		void push(std::shared_ptr<T> t) {
			return this->push_back(t);
		}
		void push(T* t) {
			return this->push(std::shared_ptr<T>(t));
		}

		/// Reverse the list
		List& reverse() {
			std::reverse(this->begin(), this->end());
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

		virtual void codegen(Program& p, LLVMHelper& h) {
			for (std::shared_ptr<T> t: *this)
				t->codegen(p, h);
		}
};

/**
 * AST expression node
 */
class Expr: public Node {
	public:
		virtual std::string toString(bool with_t=false) const {
			std::string str = this->_toString(with_t);
			if (with_t)
				str += ":" + asString(this->getType());
			return str;
		}

		virtual void codegen(Program& p, LLVMHelper& h) {
			_value = this->_codegen(p, h);
		}

		/**
		 * Auxilary function for toString
		 *
		 * @see toString
		 */
		virtual std::string _toString(bool with_t=false) const = 0;

		/**
		 * Auxilary function for codegen
		 *
		 * @see codegen
		 */
		virtual llvm::Value* _codegen(Program&, LLVMHelper&) = 0;

		llvm::Value* getValue() const { return _value; }
		llvm::Type* getType() const { return _value ? _value->getType() : nullptr; }

	protected:
		/// LLVM value
		llvm::Value* _value = nullptr;
};

/**
 * AST block node
 */
class Block: public Expr {
	public:
		Block() {}
		Block(const List<Expr>& exprs): exprs(exprs) {}

		/// List of expressions
		List<Expr> exprs;

		virtual std::string _toString(bool) const;
		virtual llvm::Value* _codegen(Program&, LLVMHelper&);
};

class Field; // forward declaration
class Method; // forward declaration

struct ClassDefinition {
	List<Field> fields;
	List<Method> methods;
};

/**
 * AST class node
 */
class Class: public Node {
	public:
		Class(const std::string& name, const std::string& parent, const List<Field>& fields, const List<Method>& methods):
			name(name), parent_name(parent), fields(fields), methods(methods) {}

		std::string name, parent_name;

		List<Field> fields;
		std::unordered_map<std::string, std::shared_ptr<Field>> fields_table;
		List<Method> methods;
		std::unordered_map<std::string, std::shared_ptr<Method>> methods_table;

		Class* parent = nullptr; // parent pointer

		virtual std::string toString(bool with_t=false) const;
		virtual void codegen(Program&, LLVMHelper&);

		/**
		 * Declare and define the class structure
		 *
		 * @note The class vtable is also builded.
		 */
		void declaration(LLVMHelper&);

		std::string getStructName() const {
			return "struct." + name;
		}
		bool isDeclared(LLVMHelper& h) const {
			return h.module->getFunction(name + "__new"); // double _ for collision concerns
		}
		llvm::StructType* getType(LLVMHelper& h) const {
			llvm::StructType* st = h.module->getTypeByName(this->getStructName());
			return st ? st : llvm::StructType::create(*h.context, this->getStructName());
		}

		static bool isSubclassOf(Class* a, Class* b) {
			for (; a; a = a->parent)
				if (a == b)
					return true;

			return false;
		}

		static Class* commonAncestor(Class* a, Class* b) {
			for (; a; a = a->parent)
				if (isSubclassOf(b, a))
					break;

			return a;
		}
};

/**
 * AST field node
 */
class Field: public Expr {
	public:
		Field(const std::string& name, const std::string& type, Expr* init):
			name(name), type(type), init(init) {}
		Field(const std::string& name, const std::string& type, std::shared_ptr<Expr> init):
			name(name), type(type), init(init) {}

		std::string name, type;
		std::shared_ptr<Expr> init; // possibly null
		unsigned idx; // index in parent structure

		virtual std::string _toString(bool with_t=false) const { return ""; }
		virtual std::string toString(bool with_t=false) const;

		virtual llvm::Value* _codegen(Program&, LLVMHelper&);
};

/**
 * AST formal node
 *
 * @note One could consider merging Field and Formal classes, especially if default arguments for methods were added.
 */
class Formal: public Node {
	public:
		Formal(const std::string& name, const std::string& type): name(name), type(type) {}

		std::string name, type;

		virtual std::string toString(bool with_t=false) const;

		llvm::Type* getType(LLVMHelper& h) const {
			return h.asType(type);
		}
};

/**
 * AST method/function node
 */
class Method: public Node {
	public:
		Method(const std::string& name, const List<Formal>& formals, const std::string& type, Block* block, bool variadic=false):
			name(name), formals(formals), type(type), block(block), variadic(variadic) {}
		Method(const std::string& name, const List<Formal>& formals, const std::string& type, std::shared_ptr<Block> block, bool variadic=false):
			name(name), formals(formals), type(type), block(block), variadic(variadic) {}

		std::string name, type;
		bool variadic; // variable number of args

		List<Formal> formals;
		std::unordered_map<std::string, std::shared_ptr<Formal>> formals_table;

		std::shared_ptr<Block> block;

		Class* parent = nullptr; // parent pointer
		unsigned idx; // index in parent vtable

		virtual std::string toString(bool with_t=false) const;
		virtual void codegen(Program&, LLVMHelper&);

		/**
		 * Declare the method prototype in the module
		 *
		 * @note If invalid, the prototype is not declared.
		 */
		void declaration(LLVMHelper&);

		std::string getName(bool colons=false) const {
			return (parent ? parent->name + (colons ? "::" : "_") : "") + name;
		}
		llvm::Function* getFunction(LLVMHelper& h) const {
			return h.module->getFunction(this->getName());
		}
		llvm::FunctionType* getType(LLVMHelper& h) const {
			llvm::Function* f = this->getFunction(h);
			return f ? f->getFunctionType() : nullptr;
		}
};

/**
 * AST program node
 */
class Program: public Node {
	public:
		Program(const List<Class>& classes): classes(classes) {}
		Program(const List<Class>& classes, const List<Method>& functions):
			classes(classes), functions(functions) {}

		List<Class> classes;
		std::unordered_map<std::string, std::shared_ptr<Class>> classes_table;

		List<Method> functions;
		std::unordered_map<std::string, std::shared_ptr<Method>> functions_table;

		virtual std::string toString(bool with_t=false) const;
		virtual void codegen(Program&, LLVMHelper&);

		/// Declare and all classes and functions
		void declaration(LLVMHelper&);

		bool isSubclassOf(const std::string& a, const std::string& b) {
			auto ita = classes_table.find(a);
			auto itb = classes_table.find(b);

			if (ita != classes_table.end() and itb != classes_table.end())
				return Class::isSubclassOf(ita->second.get(), itb->second.get());

			return false;
		}

		Class* commonAncestor(const std::string& a, const std::string& b) {
			auto ita = classes_table.find(a);
			auto itb = classes_table.find(b);

			if (ita != classes_table.end() and itb != classes_table.end())
				return Class::commonAncestor(ita->second.get(), itb->second.get());

			return nullptr;
		}
};

class If: public Expr {
	public:
		If(Expr* cond, Expr* then, Expr* els): cond(cond), then(then), els(els) {}
		If(std::shared_ptr<Expr> cond, std::shared_ptr<Expr> then, std::shared_ptr<Expr> els):
			cond(cond), then(then), els(els) {}

		std::shared_ptr<Expr> cond, then, els;

		virtual std::string _toString(bool) const;
		virtual llvm::Value* _codegen(Program&, LLVMHelper&);
};

class While: public Expr {
	public:
		While(Expr* cond, Expr* body): cond(cond), body(body) {}
		While(std::shared_ptr<Expr> cond, std::shared_ptr<Expr> body): cond(cond), body(body) {}

		std::shared_ptr<Expr> cond, body;

		virtual std::string _toString(bool) const;
		virtual llvm::Value* _codegen(Program&, LLVMHelper&);
};

class Break: public Expr {
	public:
		virtual std::string toString(bool) const { return "break"; }
		virtual std::string _toString(bool) const { return "break"; }
		virtual llvm::Value* _codegen(Program&, LLVMHelper&);
};

class For: public Expr { // -ext
	public:
		For(const std::string& name, Expr* first, Expr* last, Expr* body):
			name(name), first(first), last(last), body(body) {}
		For(const std::string& name, std::shared_ptr<Expr> first, std::shared_ptr<Expr> last, std::shared_ptr<Expr> body):
			name(name), first(first), last(last), body(body) {}

		std::string name;
		std::shared_ptr<Expr> first, last, body;

		virtual std::string _toString(bool) const;
		virtual llvm::Value* _codegen(Program&, LLVMHelper&);
};

class Let: public Expr {
	public:
		Let(const std::string& name, const std::string& type, Expr* init, Expr* scope):
			name(name), type(type), init(init), scope(scope) {}
		Let(const std::string& name, const std::string& type, std::shared_ptr<Expr> init, std::shared_ptr<Expr> scope):
			name(name), type(type), init(init), scope(scope) {}

		std::string name, type;
		std::shared_ptr<Expr> init, scope;

		virtual std::string _toString(bool) const;
		virtual llvm::Value* _codegen(Program&, LLVMHelper&);
};

class Lets: public Expr { // -ext
	public:
		Lets(const List<Field>& fields, Expr* scope): fields(fields), scope(scope) {}
		Lets(const List<Field>& fields, std::shared_ptr<Expr> scope): fields(fields), scope(scope) {}

		List<Field> fields;
		std::shared_ptr<Expr> scope;

		virtual std::string _toString(bool) const;
		virtual llvm::Value* _codegen(Program&, LLVMHelper&);
};

class Assign: public Expr {
	public:
		Assign(std::string name, Expr* value): name(name), value(value) {}
		Assign(std::string name, std::shared_ptr<Expr> value): name(name), value(value) {}

		std::string name;
		std::shared_ptr<Expr> value;

		virtual std::string _toString(bool) const;
		virtual llvm::Value* _codegen(Program&, LLVMHelper&);
};

class Unary: public Expr {
	public:
		enum Type { NOT, MINUS, ISNULL };

		Unary(Type type, Expr* value): type(type), value(value) {}
		Unary(Type type, std::shared_ptr<Expr> value): type(type), value(value) {}

		Type type;
		std::shared_ptr<Expr> value;

		virtual std::string _toString(bool) const;
		virtual llvm::Value* _codegen(Program&, LLVMHelper&);
};

class Binary: public Expr {
	public:
		enum Type {AND, OR, EQUAL, NEQUAL, LOWER, LOWER_EQUAL, GREATER, GREATER_EQUAL, PLUS, MINUS, TIMES, DIV, POW, MOD };

		Binary(Type type, Expr* left, Expr* right): type(type), left(left), right(right) {}
		Binary(Type type, std::shared_ptr<Expr> left, std::shared_ptr<Expr> right):
			type(type), left(left), right(right) {}

		Type type;
		std::shared_ptr<Expr> left, right;

		virtual std::string _toString(bool) const;
		virtual llvm::Value* _codegen(Program&, LLVMHelper&);
};

class Call: public Expr {
	public:
		Call(Expr* scope, const std::string& name, const List<Expr>& args):
			scope(scope), name(name), args(args) {}
		Call(std::shared_ptr<Expr> scope, const std::string& name, const List<Expr>& args):
			scope(scope), name(name), args(args) {}

		std::shared_ptr<Expr> scope;
		std::string name;
		List<Expr> args;

		virtual std::string _toString(bool) const;
		virtual llvm::Value* _codegen(Program&, LLVMHelper&);
};

class New: public Expr {
	public:
		New(const std::string& type): type(type) {}

		std::string type;

		virtual std::string _toString(bool) const;
		virtual llvm::Value* _codegen(Program&, LLVMHelper&);
};

class Identifier: public Expr {
	public:
		Identifier(const std::string& id): id(id) {}

		std::string id;

		virtual std::string _toString(bool) const;
		virtual llvm::Value* _codegen(Program&, LLVMHelper&);
};

class Self: public Identifier {
	public:
		Self(): Identifier("self") {}
};

class Integer: public Expr {
	public:
		Integer(int value): value(value) {}

		int value;

		virtual std::string _toString(bool) const;
		virtual llvm::Value* _codegen(Program&, LLVMHelper&);
};

class Real: public Expr {
	public:
		Real(double value): value(value) {}

		double value;

		virtual std::string _toString(bool) const;
		virtual llvm::Value* _codegen(Program&, LLVMHelper&);
};

class Boolean: public Expr {
	public:
		Boolean(bool b): b(b) {}

		bool b;

		virtual std::string _toString(bool) const;
		virtual llvm::Value* _codegen(Program&, LLVMHelper&);
};


class String: public Expr {
	public:
		String(const std::string& str): str(str) {}

		std::string str;

		virtual std::string _toString(bool) const;
		virtual llvm::Value* _codegen(Program&, LLVMHelper&);
};

class Unit: public Expr {
	public:
		virtual std::string _toString(bool) const;
		virtual llvm::Value* _codegen(Program&, LLVMHelper&);
};

#endif
