#ifndef AST_H
#define AST_H

#include "tools.hpp"

#include <algorithm>
#include <string>
#include <vector>
#include <unordered_map>

class Node {
	public:
		/* Constructors */
		Node() {}

		/* Fields */
		int line = 0, column = 0;

		/* Methods */
		virtual std::string to_string() const = 0;
		// virtual std::string type() const = 0;
};

template <typename T>
class List: public std::vector<T*> {
	public:
		/* Methods */
		List& add(T* t) {
			this->push_back(t);
			return *this;
		}

		std::string to_string() const {
			if (this->empty())
				return "[]";

			std::string str = "[" + this->front()->to_string();

			for (auto it = this->begin() + 1; it != this->end(); ++it)
				str += "," + (*it)->to_string();

			return str + "]";
		}
};

class Expr: public Node {};

class Block: public Expr {
	public:
		/* Constructors */
		Block(const List<Expr>& exprs): exprs(exprs) {
			std::reverse(this->exprs.begin(), this->exprs.end());
		}

		/* Fields */
		List<Expr> exprs;

		/* Methods */
		virtual std::string to_string() const {
			if (exprs.size() == 1)
				return exprs.front()->to_string();
			return exprs.to_string();
		}
};

class Field: public Node {
	public:
		/* Constructors */
		Field(const std::string& name, const std::string& type, Expr* init):
			name(name), type(type), init(init) {}

		/* Fields */
		std::string name, type;
		Expr* init;

		/* Methods */
		virtual std::string to_string() const {
			std::string str = "Field(" + name + "," + type;
			if (init)
				str += "," + init->to_string();
			return str + ")";
		}
};

class Formal: public Node {
	public:
		/* Constructors */
		Formal(const std::string& name, const std::string& type): name(name), type(type) {}

		/* Fields */
		std::string name, type;

		/* Methods */
		virtual std::string to_string() const {
			return name + ":" + type;
		}
};

class Method: public Node {
	public:
		/* Constructors */
		Method(const std::string& name, const List<Formal>& formals, const std::string& type, const Block& block):
			name(name), formals(formals), type(type), block(block) {
				std::reverse(this->formals.begin(), this->formals.end());
			}

		/* Fields */
		std::string name, type;

		List<Formal> formals;
		std::unordered_map<std::string, Formal*> formals_table;

		Block block;

		/* Methods */
		virtual std::string to_string() const {
			return "Method(" + name + "," + formals.to_string() + "," + type + "," + block.to_string() + ")";
		}
};

struct Declaration {
	List<Field> fields;
	List<Method> methods;
};

class Class: public Node {
	public:
		/* Constructors */
		Class(const std::string& name, const std::string& parent, const List<Field>& fields, const List<Method>& methods):
			name(name), parent(parent), fields(fields), methods(methods) {
				std::reverse(this->fields.begin(), this->fields.end());
				std::reverse(this->methods.begin(), this->methods.end());
			}

		/* Fields */
		std::string name, parent;

		List<Field> fields;
		std::unordered_map<std::string, Field*> fields_table;
		List<Method> methods;
		std::unordered_map<std::string, Method*> methods_table;

		/* Methods */
		virtual std::string to_string() const {
			return "Class(" + name + "," + parent + "," + fields.to_string() + "," + methods.to_string() + ")";
		}
};

class Program: public Node {
	public:
		/* Constructors */
		Program(const List<Class>& classes): classes(classes) {
			std::reverse(this->classes.begin(), this->classes.end());
		}

		/* Fields */
		List<Class> classes;
		std::unordered_map<std::string, Class*> classes_table;

		/* Methods */
		virtual std::string to_string() const {
			return classes.to_string();
		}
};

class If: public Expr {
	public:
		/* Constructors */
		If(Expr* cond, Expr* then, Expr* els): cond(cond), then(then), els(els) {}

		/* Fields */
		Expr* cond, * then, * els;

		/* Methods */
		virtual std::string to_string() const {
			std::string str = "If(" + cond->to_string() + "," + then->to_string();
			if (els)
				str += "," + els->to_string();
			return str + ")";
		}
};

class While: public Expr {
	public:
		/* Constructors */
		While(Expr* cond, Expr* body): cond(cond), body(body) {}

		/* Fields */
		Expr* cond, * body;

		/* Methods */
		virtual std::string to_string() const {
			return "While(" + cond->to_string() + "," + body->to_string() + ")";
		}
};

class Let: public Expr {
	public:
		/* Constructors */
		Let(const std::string& name, const std::string& type, Expr* init, Expr* scope):
			name(name), type(type), init(init), scope(scope) {}

		/* Fields */
		std::string name, type;
		Expr* init, * scope;

		/* Methods */
		virtual std::string to_string() const {
			std::string str = "Let(" + name + "," + type + ",";
			if (init)
				str += init->to_string() + ",";
			return str + scope->to_string() + ")";
		}
};

class Assign: public Expr {
	public:
		/* Constructors */
		Assign(std::string name, Expr* value):
			name(name), value(value) {}

		/* Fields */
		std::string name;
		Expr* value;

		/* Methods */
		virtual std::string to_string() const {
			return "Assign(" + name + "," + value->to_string() + ")";
		}
};

class Unary: public Expr {
	public:
		enum Type { NOT, MINUS, ISNULL };

		/* Constructors */
		Unary(Type type, Expr* value): type(type), value(value) {}

		/* Fields */
		Type type;
		Expr* value;

		/* Methods */
		virtual std::string to_string() const {
			std::string str = "UnOp(";
			switch (type) {
				case NOT: str += "not"; break;
				case MINUS: str += "-"; break;
				case ISNULL: str += "isnull"; break;
			}
			return str + "," + value->to_string() + ")";
		}
};

class Binary: public Expr {
	public:
		enum Type { AND, EQUAL, LOWER, LOWER_EQUAL, PLUS, MINUS, TIMES, DIV, POW };

		/* Constructors */
		Binary(Type type, Expr* left, Expr* right): type(type), left(left), right(right) {}

		/* Fields */
		Type type;
		Expr* left, * right;

		/* Methods */
		virtual std::string to_string() const {
			std::string str = "BinOp(";
			switch (type) {
				case AND: str += "and"; break;
				case EQUAL: str += "="; break;
				case LOWER: str += "<"; break;
				case LOWER_EQUAL: str += "<="; break;
				case PLUS: str += "+"; break;
				case MINUS: str += "-"; break;
				case TIMES: str += "*"; break;
				case DIV: str += "/"; break;
				case POW: str += "^"; break;
			}
			return str + "," + left->to_string() + "," + right->to_string() + ")";
		}
};

class Call: public Expr {
	public:
		/* Constructors */
		Call(Expr* scope, const std::string& name, const List<Expr>& args):
			scope(scope), name(name), args(args) {
				std::reverse(this->args.begin(), this->args.end());
			}

		/* Fields */
		Expr* scope;
		std::string name;
		List<Expr> args;

		/* Methods */
		virtual std::string to_string() const {
			return "Call(" + scope->to_string() + "," + name + "," + args.to_string() + ")";
		}
};

class New: public Expr {
	public:
		/* Constructors */
		New(const std::string& type): type(type) {}

		/* Fields */
		std::string type;

		/* Methods */
		virtual std::string to_string() const {
			return "New(" + type + ")";
		}
};

class Identifier: public Expr {
	public:
		/* Constructors */
		Identifier(const std::string& id): id(id) {}

		/* Fields */
		std::string id;

		/* Methods */
		virtual std::string to_string() const {
			return id;
		}
};

class Integer: public Expr {
	public:
		/* Constructors */
		Integer(int value): value(value) {}

		/* Fields */
		int value;

		/* Methods */
		virtual std::string to_string() const {
			return std::to_string(value);
		}
};

class String: public Expr {
	public:
		/* Constructors */
		String(const std::string& str): str(str) {}

		/* Fields */
		std::string str;

		/* Methods */
		virtual std::string to_string() const {
			std::string temp;
			for (const char& c: str)
				switch (c) {
					case '\"':
					case '\\': temp += char2hex(c); break;
					default:
						if (c >= 32 and c <= 126)
							temp += c;
						else
							temp += char2hex(c);
				}
			return "\"" + temp + "\"";
		}
};

class Boolean: public Expr {
	public:
		/* Constructors */
		Boolean(bool b): b(b) {}

		/* Fields */
		bool b;

		/* Methods */
		virtual std::string to_string() const {
			return b ? "true" : "false";
		}
};

class Unit: public Expr {
	public:
		/* Methods */
		virtual std::string to_string() const {
			return "()";
		}
};

#endif
