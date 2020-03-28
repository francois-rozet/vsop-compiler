#ifndef AST_H
#define AST_H

#include "tools.hpp"

#include <string>
#include <vector>

class Node {
	public:
		/* Constructors */
		Node() {}

		/* Methods */
		virtual std::string to_string() const = 0;
};

template <typename T>
class List: public Node {
	public:
		/* Constructors */
		List() {}
		List(std::vector<T*> stack): stack(stack) {}

		/* Fields */
		std::vector<T*> stack;

		/* Methods */
		List& push(Node* t) { if (t) stack.push_back(static_cast<T*>(t)); return *this; }

		virtual std::string to_string() const {
			if (stack.empty())
				return "[]";

			std::string s = stack.back()->to_string();

			for (auto it = stack.rbegin() + 1; it != stack.rend(); ++it)
				s += "," + (*it)->to_string();

			return "[" + s + "]";
		}

		/* Accessors */
		size_t size() const { return stack.size(); }
		const T* back() const { return stack.back(); }
};

class Expr: public Node {};

class Block: public Expr {
	public:
		/* Constructors */
		Block() {}
		Block(const List<Expr>& exprs): exprs(exprs) {}

		/* Fields */
		List<Expr> exprs;

		/* Methods */
		virtual std::string to_string() const {
			if (exprs.size() == 1)
				return exprs.back()->to_string();
			else
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
			name(name), formals(formals), type(type), block(block) {}

		/* Fields */
		std::string name, type;
		List<Formal> formals;
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
			name(name), parent(parent), fields(fields), methods(methods) {}

		/* Fields */
		std::string name, parent;
		List<Field> fields;
		List<Method> methods;

		/* Methods */
		virtual std::string to_string() const {
			return "Class(" + name + "," + parent + "," + fields.to_string() + "," + methods.to_string() + ")";
		}
};

class If: public Expr {
	public:
		/* Constructors */
		If(Expr* cond, Expr* then, Expr* els):
			cond(cond), then(then), els(els) {}

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
		Assign(const std::string& name, Expr* value):
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
			std::string op;

			switch (type) {
				case NOT: op = "not"; break;
				case MINUS: op = "-"; break;
				case ISNULL: op = "isnull"; break;
			}

			return "UnOp(" + op + "," + value->to_string() + ")";
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
			std::string op;

			switch (type) {
				case AND: op = "and"; break;
				case EQUAL: op = "="; break;
				case LOWER: op = "<"; break;
				case LOWER_EQUAL: op = "<="; break;
				case PLUS: op = "+"; break;
				case MINUS: op = "-"; break;
				case TIMES: op = "*"; break;
				case DIV: op = "/"; break;
				case POW: op = "^"; break;
			}

			return "BinOp(" + op + "," + left->to_string() + "," + right->to_string() + ")";
		}
};

class Call: public Expr {
	public:
		/* Constructors */
		Call(Expr* scope, const std::string& name, const List<Expr>& args):
			scope(scope), name(name), args(args) {}

		/* Fields */
		Node* scope;
		std::string name;
		List<Expr> args;

		/* Methods */
		virtual std::string to_string() const {
			std::string str = "Call(";
			str += scope ? scope->to_string() : "self";
			return str + "," + name + "," + args.to_string() + ")";
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
