#ifndef AST_H
#define AST_H

#include "tools.hpp"

#include <algorithm>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

/* Forward Declarations */

class Node;
class Program;

/* Declarations */

struct Error {
	Node* node;
	std::string msg;
};

class Node {
	public:
		/* Constructors */
		Node() {}

		/* Fields */
		int line = 0, column = 0;

		/* Methods */
		virtual std::string to_string() const = 0;

		virtual void check(Program*, std::unordered_map<std::string, std::string>, std::vector<Error>&) = 0;
		virtual std::string get_type(Program*, std::unordered_map<std::string, std::string>) = 0;
};

static bool is_class(Program* p, const std::string& type);
static bool is_type(Program* p, const std::string& type);
static bool conforms_to(Program* p, std::string a, std::string b);
static std::string common_ancestor(Program* p, std::string a, std::string b);

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

class Expr: public Node {
	public:
		/* Methods */
		virtual std::string to_string_aux() const = 0;
		virtual std::string to_string() const {
			std::string str = this->to_string_aux();
			if (not type_.empty())
				str += ":" + type_;
			return str;
		};

	protected:
		/* Fields */
		std::string type_;
};

class Block: public Expr {
	public:
		/* Constructors */
		Block() {}
		Block(const List<Expr>& exprs): exprs(exprs) {
			std::reverse(this->exprs.begin(), this->exprs.end());
		}

		/* Fields */
		List<Expr> exprs;

		/* Methods */
		virtual std::string to_string_aux() const {
			if (exprs.size() == 1)
				return exprs.front()->to_string_aux();
			return exprs.to_string();
		}

		virtual void check(Program* p, std::unordered_map<std::string, std::string> scope, std::vector<Error>& errors) {
			for (Expr* e: exprs)
				e->check(p, scope, errors);

			this->get_type(p, scope);
		}

		virtual std::string get_type(Program* p, std::unordered_map<std::string, std::string> scope) {
			type_ = exprs.empty() ? "error" : exprs.back()->get_type(p, scope);
			return type_;
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

		virtual void check(Program* p, std::unordered_map<std::string, std::string> scope, std::vector<Error>& errors) {
			if (init)
				init->check(p, scope, errors);

			if (not is_type(p, type) and not is_class(p, type))
				errors.push_back({this, "unknown type " + type});
			else if (init and not conforms_to(p, init->get_type(p, scope), type))
				errors.push_back({this, "expected type " + type + ", but got type " + init->get_type(p, scope)});
		}

		virtual std::string get_type(Program* p, std::unordered_map<std::string, std::string> scope) {
			return type;
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

		virtual void check(Program* p, std::unordered_map<std::string, std::string> scope, std::vector<Error>& errors) {
			if (not is_type(p, type) and not is_class(p, type))
				errors.push_back({this, "unknown type " + type});
		}

		virtual std::string get_type(Program* p, std::unordered_map<std::string, std::string> scope) {
			return type;
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

		virtual void check(Program* p, std::unordered_map<std::string, std::string> scope, std::vector<Error>& errors) {
			for (Formal* f: formals) {
				f->check(p, scope, errors);
				scope[f->name] = f->type;
			}

			block.check(p, scope, errors);

			if (not is_type(p, type) and not is_class(p, type))
				errors.push_back({this, "unknown type " + type});
			else if (not conforms_to(p, block.get_type(p, scope), type))
				errors.push_back({this, "expected type " + type + ", but got type " + block.get_type(p, scope)});
		}

		virtual std::string get_type(Program* p, std::unordered_map<std::string, std::string> scope) {
			return type;
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

		virtual void check(Program* p, std::unordered_map<std::string, std::string> scope, std::vector<Error>& errors) {
			for (Field* f: fields) {
				f->check(p, scope, errors);
				scope[f->name] = f->type;
			}

			scope["self"] = name;

			for (Method* m: methods)
				m->check(p, scope, errors);
		}

		virtual std::string get_type(Program* p, std::unordered_map<std::string, std::string> scope) {
			return name;
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

		virtual void check(Program* p, std::unordered_map<std::string, std::string> scope, std::vector<Error>& errors) {
			for (Class* c: classes)
				c->check(p, scope, errors);
		}

		virtual std::string get_type(Program* p, std::unordered_map<std::string, std::string> scope) {
			return "error";
		}
};

class If: public Expr {
	public:
		/* Constructors */
		If(Expr* cond, Expr* then, Expr* els): cond(cond), then(then), els(els) {}

		/* Fields */
		Expr* cond, * then, * els;

		/* Methods */
		virtual std::string to_string_aux() const {
			std::string str = "If(" + cond->to_string() + "," + then->to_string();
			if (els)
				str += "," + els->to_string();
			return str + ")";
		}

		virtual void check(Program* p, std::unordered_map<std::string, std::string> scope, std::vector<Error>& errors) {
			cond->check(p, scope, errors);
			then->check(p, scope, errors);
			if (els)
				els->check(p, scope, errors);

			if (cond->get_type(p, scope) != "bool")
				errors.push_back({this, "expected type bool, but got type " + cond->get_type(p, scope)});

			if (this->get_type(p, scope) == "error")
				errors.push_back({this, "types " + then->get_type(p, scope) + " and " + els->get_type(p, scope) + " don't agree"});

			this->get_type(p, scope);
		}

		virtual std::string get_type(Program* p, std::unordered_map<std::string, std::string> scope) {
			std::string then_t = then->get_type(p, scope);
			std::string els_t = els ? els->get_type(p, scope) : "unit";

			if (then_t == "unit" or els_t == "unit")
				type_ = "unit";
			else if (is_type(p, then_t) and then_t == els_t)
				type_ = then_t;
			else if (is_class(p, then_t) and is_class(p, els_t))
				type_ = common_ancestor(p, then_t, els_t);
			else
				type_ = "error";

			return type_;
		}
};

class While: public Expr {
	public:
		/* Constructors */
		While(Expr* cond, Expr* body): cond(cond), body(body) {}

		/* Fields */
		Expr* cond, * body;

		/* Methods */
		virtual std::string to_string_aux() const {
			return "While(" + cond->to_string() + "," + body->to_string() + ")";
		}

		virtual void check(Program* p, std::unordered_map<std::string, std::string> scope, std::vector<Error>& errors) {
			cond->check(p, scope, errors);
			body->check(p, scope, errors);

			if (cond->get_type(p, scope) != "bool")
				errors.push_back({this, "expected type bool, but got type " + cond->get_type(p, scope)});

			this->get_type(p, scope);
		}

		virtual std::string get_type(Program* p, std::unordered_map<std::string, std::string> scope) {
			type_ = "unit";
			return type_;
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
		virtual std::string to_string_aux() const {
			std::string str = "Let(" + name + "," + type + ",";
			if (init)
				str += init->to_string() + ",";
			return str + scope->to_string() + ")";
		}

		virtual void check(Program* p, std::unordered_map<std::string, std::string> scope, std::vector<Error>& errors) {
			if (name == "self")
				errors.push_back({this, "trying to assign to self"});

			if (not is_type(p, type) and not is_class(p, type))
				errors.push_back({this, "unknown type " + type});

			if (init)
				init->check(p, scope, errors);

			if (init and not conforms_to(p, init->get_type(p, scope), type))
				errors.push_back({this, "expected type " + type + ", but got type " + init->get_type(p, scope)});

			if (name != "self")
				scope[name] = type;

			this->scope->check(p, scope, errors);

			this->get_type(p, scope);
		}

		virtual std::string get_type(Program* p, std::unordered_map<std::string, std::string> scope) {
			if (name != "self")
				scope[name] = type;

			type_ = this->scope->get_type(p, scope);
			return type_;
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
		virtual std::string to_string_aux() const {
			return "Assign(" + name + "," + value->to_string() + ")";
		}

		virtual void check(Program* p, std::unordered_map<std::string, std::string> scope, std::vector<Error>& errors) {
			if (name == "self")
				errors.push_back({this, "trying to assign to self"});

			if (scope.find(name) == scope.end())
				errors.push_back({this, "trying to assign to undefined " + name});
			else if (not conforms_to(p, value->get_type(p, scope), scope[name]))
				errors.push_back({this, "expected type " + scope[name] + ", but got type " + value->get_type(p, scope)});

			this->get_type(p, scope);
		}

		virtual std::string get_type(Program* p, std::unordered_map<std::string, std::string> scope) {
			type_ = value->get_type(p, scope);
			return type_;
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
		virtual std::string to_string_aux() const {
			std::string str = "UnOp(";
			switch (type) {
				case NOT: str += "not"; break;
				case MINUS: str += "-"; break;
				case ISNULL: str += "isnull"; break;
			}
			return str + "," + value->to_string() + ")";
		}

		virtual void check(Program* p, std::unordered_map<std::string, std::string> scope, std::vector<Error>& errors) {
			value->check(p, scope, errors);

			std::string expected;

			switch (type) {
				case NOT: expected = "bool"; break;
				case MINUS: expected = "int32"; break;
				case ISNULL: expected = "int32"; break;
			}

			if (value->get_type(p, scope) != expected)
				errors.push_back({this, "expected type " + expected + ", but got type " + value->get_type(p, scope)});

			this->get_type(p, scope);
		}

		virtual std::string get_type(Program* p, std::unordered_map<std::string, std::string> scope) {
			switch (type) {
				case NOT: type_ = "bool"; break;
				case MINUS: type_ = "int32"; break;
				case ISNULL: type_ = "bool"; break;
				default: type_ = "error";
			}

			return type_;
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
		virtual std::string to_string_aux() const {
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

		virtual void check(Program* p, std::unordered_map<std::string, std::string> scope, std::vector<Error>& errors) {
			left->check(p, scope, errors);
			right->check(p, scope, errors);

			if (left->get_type(p, scope) != right->get_type(p, scope))
				errors.push_back({this, "types " + left->get_type(p, scope) + " and " + right->get_type(p, scope) + " don't agree"});

			std::string expected;

			switch (type) {
				case AND: expected = "bool"; break;
				case EQUAL: break;
				case LOWER:
				case LOWER_EQUAL:
				case PLUS:
				case MINUS:
				case TIMES:
				case DIV:
				case POW: expected = "int32"; break;
			}

			if (not expected.empty() and left->get_type(p, scope) != expected)
				errors.push_back({this, "expected type " + expected + ", but got type " + left->get_type(p, scope)});

			this->get_type(p, scope);
		}

		virtual std::string get_type(Program* p, std::unordered_map<std::string, std::string> scope) {
			switch (type) {
				case AND:
				case EQUAL:
				case LOWER:
				case LOWER_EQUAL: type_ = "bool"; break;
				case PLUS:
				case MINUS:
				case TIMES:
				case DIV:
				case POW: type_ = "int32"; break;
				default: type_ = "error";
			}

			return type_;
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
		virtual std::string to_string_aux() const {
			return "Call(" + scope->to_string() + "," + name + "," + args.to_string() + ")";
		}

		virtual void check(Program* p, std::unordered_map<std::string, std::string> scope, std::vector<Error>& errors) {
			this->scope->check(p, scope, errors);

			for (Expr* e: args)
				e->check(p, scope, errors);

			std::string type = this->scope->get_type(p, scope);

			if (p->classes_table.find(type) == p->classes_table.end())
				errors.push_back({this, "unknown type " + type});
			else {
				Class* c = p->classes_table[type];

				for (; c->name != "Object"; c = p->classes_table[c->parent])
					if (c->methods_table.find(name) != c->methods_table.end())
						break;

				if (c->methods_table.find(name) == c->methods_table.end())
					errors.push_back({this, "unknown method " + name});
				else {
					Method* m = c->methods_table[name];

					if (args.size() != m->formals.size())
						errors.push_back({this, "call of method " + m->name + " without enough arguments"});
					else {
						for (int i = 0; i < args.size(); ++i)
							if (not conforms_to(p, args[i]->get_type(p, scope), m->formals[i]->type))
								errors.push_back({args[i], "expected type " + m->formals[i]->type + ", but got type " + args[i]->get_type(p, scope)});
					}
				}
			}

			this->get_type(p, scope);
		}

		virtual std::string get_type(Program* p, std::unordered_map<std::string, std::string> scope) {
			type_ = "error";

			std::string type = this->scope->get_type(p, scope);

			if (p->classes_table.find(type) != p->classes_table.end()) {
				Class* c = p->classes_table[type];

				for (; c->name != "Object"; c = p->classes_table[c->parent])
					if (c->methods_table.find(name) != c->methods_table.end())
						break;

				if (c->methods_table.find(name) != c->methods_table.end())
					type_ = c->methods_table[name]->type;
			}

			return type_;
		}
};

class New: public Expr {
	public:
		/* Constructors */
		New(const std::string& type): type(type) {}

		/* Fields */
		std::string type;

		/* Methods */
		virtual std::string to_string_aux() const {
			return "New(" + type + ")";
		}

		virtual void check(Program* p, std::unordered_map<std::string, std::string> scope, std::vector<Error>& errors) {
			if (not is_type(p, type) and not is_class(p, type))
				errors.push_back({this, "unknown type " + type});

			this->get_type(p, scope);
		}

		virtual std::string get_type(Program* p, std::unordered_map<std::string, std::string> scope) {
			type_ = type;
			return type_;
		}
};

class Identifier: public Expr {
	public:
		/* Constructors */
		Identifier(const std::string& id): id(id) {}

		/* Fields */
		std::string id;

		/* Methods */
		virtual std::string to_string_aux() const {
			return id;
		}

		virtual void check(Program* p, std::unordered_map<std::string, std::string> scope, std::vector<Error>& errors) {
			if (scope.find(id) == scope.end())
				errors.push_back({this, "undefined " + id});

			this->get_type(p, scope);
		}

		virtual std::string get_type(Program* p, std::unordered_map<std::string, std::string> scope) {
			if (scope.find(id) != scope.end())
				type_ = scope[id];
			else
				type_ = "error";

			return type_;
		}
};

class Integer: public Expr {
	public:
		/* Constructors */
		Integer(int value): value(value) {}

		/* Fields */
		int value;

		/* Methods */
		virtual std::string to_string_aux() const {
			return std::to_string(value);
		}

		virtual void check(Program* p, std::unordered_map<std::string, std::string> scope, std::vector<Error>& errors) { this->get_type(p, scope); }

		virtual std::string get_type(Program* p, std::unordered_map<std::string, std::string> scope) {
			type_ = "int32";
			return type_;
		}
};

class String: public Expr {
	public:
		/* Constructors */
		String(const std::string& str): str(str) {}

		/* Fields */
		std::string str;

		/* Methods */
		virtual std::string to_string_aux() const {
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

		virtual void check(Program* p, std::unordered_map<std::string, std::string> scope, std::vector<Error>& errors) { this->get_type(p, scope); }

		virtual std::string get_type(Program* p, std::unordered_map<std::string, std::string> scope) {
			type_ = "string";
			return type_;
		}
};

class Boolean: public Expr {
	public:
		/* Constructors */
		Boolean(bool b): b(b) {}

		/* Fields */
		bool b;

		/* Methods */
		virtual std::string to_string_aux() const {
			return b ? "true" : "false";
		}

		virtual void check(Program* p, std::unordered_map<std::string, std::string> scope, std::vector<Error>& errors) { this->get_type(p, scope); }

		virtual std::string get_type(Program* p, std::unordered_map<std::string, std::string> scope) {
			type_ = "bool";
			return type_;
		}
};

class Unit: public Expr {
	public:
		/* Methods */
		virtual std::string to_string_aux() const {
			return "()";
		}

		virtual void check(Program* p, std::unordered_map<std::string, std::string> scope, std::vector<Error>& errors) { this->get_type(p, scope); }

		virtual std::string get_type(Program* p, std::unordered_map<std::string, std::string> scope) {
			type_ = "unit";
			return type_;
		}
};

static bool is_class(Program* p, const std::string& type) {
	return p->classes_table.find(type) != p->classes_table.end();
}

static bool is_type(Program* p, const std::string& type) {
	return type == "int32" or type == "string" or type == "bool" or type == "unit";
}

static bool conforms_to(Program* p, std::string a, std::string b) {
	if (is_class(p, a))
		for (; a != "Object"; a = p->classes_table[a]->parent)
			if (a == b)
				return true;

	return a == b;
}

static std::string common_ancestor(Program* p, std::string a, std::string b) {
	std::unordered_set<std::string> A;
	std::unordered_set<std::string> B;

	while (a != "Object" and b != "Object") {
		if (a == b)
			return a;
		else if (A.find(b) == A.end())
			return b;
		else if (B.find(a) == B.end())
			return a;
		else {
			A.insert(a);
			B.insert(b);
			a = p->classes_table[a]->parent;
			b = p->classes_table[b]->parent;
		}
	}
}

#endif
