#ifndef AST_H
#define AST_H

#include "tools.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

/* Forward Declarations */

class Node;
class Program;

/************************/

class Scope: std::unordered_map<std::string, std::vector<std::string>> {
	public:
		/* Methods */
		Scope& push(const std::string& name, const std::string& type) {
			if (this->contains(name))
				this->at(name).push_back(type);
			else
				this->insert({name, {type}});

			return *this;
		}

		Scope& pop(const std::string& name) {
			if (this->contains(name)) {
				if (this->at(name).size() > 1)
					this->at(name).pop_back();
				else
					this->erase(name);
			}

			return *this;
		}

		bool contains(const std::string& name) const {
			return this->find(name) != this->end();
		}

		const std::string& get(const std::string& name) const {
			if (this->contains(name))
				return this->at(name).back();
			else
				throw std::out_of_range("Unknown key " + name);
		}
};

struct Error {
	Node* node;
	std::string msg;
};

class Node {
	public:
		/* Constructors */
		Node() {}

		/* Fields */
		int line = 1, column = 1;

		/* Methods */
		virtual std::string to_string() const = 0;

		virtual Scope& increase(Scope& s) const { return s; }
		virtual Scope& decrease(Scope& s) const { return s; }

		virtual void check(Program*, Scope&, std::vector<Error>&) = 0;
		virtual std::string get_type(Program*, Scope&) = 0;
};

static bool is_class(Program*, const std::string&);
static bool is_prim(Program*, const std::string&);
static bool conforms_to(Program*, const std::string&, const std::string&);
static std::string common_ancestor(Program*, const std::string&, const std::string&);

template <typename T>
class List: public std::vector<T*> {
	public:
		/* Constructors */
		List() {}
		List(std::initializer_list<T*> init): std::vector<T*>(init) {}

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

		Scope& increase(Scope& s) const {
			for (T* t: *this)
				t->increase(s);

			return s;
		}

		Scope& decrease(Scope& s) const {
			for (T* t: *this)
				t->decrease(s);

			return s;
		}

		void check(Program* p, Scope& s, std::vector<Error>& errors) {
			for (T* t: *this)
				t->check(p, s, errors);
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

		virtual void check(Program* p, Scope& s, std::vector<Error>& errors) { this->get_type(p, s); }

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

		virtual void check(Program* p, Scope& s, std::vector<Error>& errors) {
			exprs.check(p, s, errors);
			this->get_type(p, s);
		}

		virtual std::string get_type(Program* p, Scope& s) {
			type_ = exprs.empty() ? "error" : exprs.back()->get_type(p, s);
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

		virtual Scope& increase(Scope& s) const {
			return s.push(name, type);
		}

		virtual Scope& decrease(Scope& s) const {
			return s.pop(name);
		}

		virtual void check(Program* p, Scope& s, std::vector<Error>& errors) {
			if (init)
				init->check(p, s, errors);

			if (not is_prim(p, type) and not is_class(p, type))
				errors.push_back({this, "unknown type " + type});
			else if (init and not conforms_to(p, init->get_type(p, s), type))
				errors.push_back({this, "expected type " + type + ", but got type " + init->get_type(p, s)});
		}

		virtual std::string get_type(Program* p, Scope& s) {
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

		virtual Scope& increase(Scope& s) const {
			return s.push(name, type);
		}

		virtual Scope& decrease(Scope& s) const {
			return s.pop(name);
		}

		virtual void check(Program* p, Scope& s, std::vector<Error>& errors) {
			if (not is_prim(p, type) and not is_class(p, type))
				errors.push_back({this, "unknown type " + type});
		}

		virtual std::string get_type(Program* p, Scope& s) {
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

		virtual Scope& increase(Scope& s) const {
			return formals.increase(s);
		}

		virtual Scope& decrease(Scope& s) const {
			return formals.decrease(s);
		}

		void redefinition(std::vector<Error>& errors) {
			for (auto it = formals.begin(); it != formals.end();)
				if (formals_table.find((*it)->name) == formals_table.end()) {
					formals_table[(*it)->name] = *it;
					++it;
				} else {
					errors.push_back({*it, "redefinition of formal " + (*it)->name + " of method " + name});
					it = formals.erase(it);
				}
		}

		virtual void check(Program* p, Scope& s, std::vector<Error>& errors) {
			formals.check(p, s, errors);

			this->increase(s);

			block.check(p, s, errors);

			if (not is_prim(p, type) and not is_class(p, type))
				errors.push_back({this, "unknown type " + type});
			else if (not conforms_to(p, block.get_type(p, s), type))
				errors.push_back({this, "expected type " + type + ", but got type " + block.get_type(p, s)});

			this->decrease(s);
		}

		virtual std::string get_type(Program* p, Scope& s) {
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
			name(name), parent_name(parent), fields(fields), methods(methods) {
				std::reverse(this->fields.begin(), this->fields.end());
				std::reverse(this->methods.begin(), this->methods.end());
			}

		/* Fields */
		std::string name, parent_name;

		List<Field> fields;
		std::unordered_map<std::string, Field*> fields_table;
		List<Method> methods;
		std::unordered_map<std::string, Method*> methods_table;

		Class* parent;

		/* Methods */
		virtual std::string to_string() const {
			return "Class(" + name + "," + parent_name + "," + fields.to_string() + "," + methods.to_string() + ")";
		}

		virtual Scope& increase(Scope& s) const {
			fields.increase(s);
			if (parent)
				parent->increase(s);
			return s.push("self", name);
		}

		virtual Scope& decrease(Scope& s) const {
			s.pop("self");
			if (parent)
				parent->decrease(s);
			return fields.decrease(s);
		}

		void redefinition(std::vector<Error>& errors) {
			for (auto it = fields.begin(); it != fields.end();)
				if (fields_table.find((*it)->name) == fields_table.end()) {
					fields_table[(*it)->name] = *it;
					++it;
				} else {
					errors.push_back({*it, "redefinition of field " + (*it)->name + " of class " + name});
					it = fields.erase(it);
				}

			for (auto it = methods.begin(); it != methods.end();)
				if (methods_table.find((*it)->name) == methods_table.end()) {
					methods_table[(*it)->name] = *it;
					(*it)->redefinition(errors);
					++it;
				} else {
					errors.push_back({*it, "redefinition of method " + (*it)->name + " of class " + name});
					it = methods.erase(it);
				}
		}

		void override(std::vector<Error>& errors) {
			for (auto it = fields.begin(); it != fields.end();) {
				Class* ancestor = parent;

				for (; ancestor != NULL; ancestor = ancestor->parent)
					if (ancestor->fields_table.find((*it)->name) != ancestor->fields_table.end())
						break;

				if (ancestor == NULL)
					++it;
				else {
					errors.push_back({*it, "overriding field " + (*it)->name});
					fields_table.erase((*it)->name);
					it = fields.erase(it);
				}
			}

			for (auto it = methods.begin(); it != methods.end();) {
				Class* ancestor = parent;

				for (; ancestor != NULL; ancestor = ancestor->parent) {
					if (ancestor->methods_table.find((*it)->name) == ancestor->methods_table.end())
						continue;

					Method* m = ancestor->methods_table[(*it)->name];

					if ((*it)->type != m->type) // different return type
						break;
					else if ((*it)->formals.size() != m->formals.size()) // different number of formals
						break;
					else { // different formal types
						int i = 0;
						for (; i < (*it)->formals.size(); ++i)
							if ((*it)->formals[i]->type != m->formals[i]->type)
								break;

						if (i < (*it)->formals.size())
							break;
					}
				}

				if (ancestor == NULL)
					++it;
				else {
					errors.push_back({*it, "overriding method " + (*it)->name + " with different signature"});
					methods_table.erase((*it)->name);
					it = methods.erase(it);
				}
			}
		}

		virtual void check(Program* p, Scope& s, std::vector<Error>& errors) {
			fields.check(p, s, errors);

			this->increase(s);
			methods.check(p, s, errors);
			this->decrease(s);
		}

		virtual std::string get_type(Program* p, Scope& s) {
			return name;
		}
};

class Program: public Node {
	public:
		/* Constructors */
		Program(const List<Class>& classes): classes(classes) {
			std::reverse(this->classes.begin(), this->classes.end());

			Class* object = new Class("Object", "Object", {},
				List<Method>({
					new Method("print", {new Formal("s", "string")}, "Object", Block()),
					new Method("printBool", {new Formal("b", "bool")}, "Object", Block()),
					new Method("printInt32", {new Formal("i", "int32")}, "Object", Block()),
					new Method("inputLine", {}, "string", Block()),
					new Method("inputBool", {}, "bool", Block()),
					new Method("inputInt32", {}, "int32", Block())
				})
			);

			for (Method* m: object->methods)
				object->methods_table[m->name] = m;

			classes_table["Object"] = object;
		}

		/* Fields */
		List<Class> classes;
		std::unordered_map<std::string, Class*> classes_table;

		/* Methods */
		virtual std::string to_string() const {
			return classes.to_string();
		}

		void redefinition(std::vector<Error>& errors) {
			for (auto it = classes.begin(); it != classes.end();)
				if (classes_table.find((*it)->name) == classes_table.end()) {
					classes_table[(*it)->name] = *it;
					(*it)->redefinition(errors);
					++it;
				} else {
					errors.push_back({*it, "redefinition of class " + (*it)->name});
					it = classes.erase(it);
				}
		}

		void inheritance(std::vector<Error>& errors) {
			std::unordered_set<std::string> genealogy;

			for (auto it = classes.begin(); it != classes.end(); genealogy.clear()) {
				Class* c = *it;

				for (; c->name != "Object"; c = classes_table[c->parent_name])
					if (genealogy.find(c->name) != genealogy.end())
						break;
					else if (classes_table.find(c->parent_name) == classes_table.end())
						break;
					else
						genealogy.insert(c->name);

				if (c->name == "Object") {
					(*it)->parent = classes_table[(*it)->parent_name];
					++it;
				} else {
					errors.push_back({*it, "class " + (*it)->name + " cannot extend class " + c->parent_name});
					classes_table.erase((*it)->name);
					it = classes.erase(it);
				}
			}
		}

		void override(std::vector<Error>& errors) {
			for (Class* c: classes)
				c->override(errors);
		}

		void main(std::vector<Error>& errors) {
			if (classes_table.find("Main") == classes_table.end())
				errors.push_back({this, "undefined class Main"});
			else {
				Class* main = classes_table["Main"];

				if (main->methods_table.find("main") == main->methods_table.end())
					errors.push_back({main, "undefined method main"});
				else {
					bool cond = main->methods_table["main"]->formals.size() > 0;
					cond = cond or main->methods_table["main"]->type != "int32";

					if (cond)
						errors.push_back({main, "main method of class Main defined with wrong signature"});
				}
			}
		}

		virtual void check(Program* p, Scope& s, std::vector<Error>& errors) {
			classes.check(p, s, errors);
		}

		virtual std::string get_type(Program* p, Scope& s) {
			return "main";
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

		virtual void check(Program* p, Scope& s, std::vector<Error>& errors) {
			cond->check(p, s, errors);
			then->check(p, s, errors);
			if (els)
				els->check(p, s, errors);

			if (cond->get_type(p, s) != "bool")
				errors.push_back({this, "expected type bool, but got type " + cond->get_type(p, s)});

			this->get_type(p, s);

			if (type_ == "error")
				errors.push_back({this, "types " + then->get_type(p, s) + " and " + els->get_type(p, s) + " don't agree"});
		}

		virtual std::string get_type(Program* p, Scope& s) {
			std::string then_t = then->get_type(p, s);
			std::string els_t = els ? els->get_type(p, s) : "unit";

			if (then_t == "unit" or els_t == "unit")
				type_ = "unit";
			else if (is_prim(p, then_t) and then_t == els_t)
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

		virtual void check(Program* p, Scope& s, std::vector<Error>& errors) {
			cond->check(p, s, errors);
			body->check(p, s, errors);

			if (cond->get_type(p, s) != "bool")
				errors.push_back({this, "expected type bool, but got type " + cond->get_type(p, s)});

			this->get_type(p, s);
		}

		virtual std::string get_type(Program* p, Scope& s) {
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

		virtual Scope& increase(Scope& s) const {
			return s.push(name, type);
		}

		virtual Scope& decrease(Scope& s) const {
			return s.pop(name);
		}

		virtual void check(Program* p, Scope& s, std::vector<Error>& errors) {
			if (not is_prim(p, type) and not is_class(p, type))
				errors.push_back({this, "unknown type " + type});

			if (init)
				init->check(p, s, errors);

			if (init and not conforms_to(p, init->get_type(p, s), type))
				errors.push_back({this, "expected type " + type + ", but got type " + init->get_type(p, s)});

			this->increase(s);
			scope->check(p, s, errors);
			this->decrease(s);

			this->get_type(p, s);
		}

		virtual std::string get_type(Program* p, Scope& s) {
			this->increase(s);
			type_ = scope->get_type(p, s);
			this->decrease(s);

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

		virtual void check(Program* p, Scope& s, std::vector<Error>& errors) {
			if (not s.contains(name))
				errors.push_back({this, "trying to assign to undefined " + name});
			else if (not conforms_to(p, value->get_type(p, s), s.get(name)))
				errors.push_back({this, "expected type " + s.get(name) + ", but got type " + value->get_type(p, s)});

			this->get_type(p, s);
		}

		virtual std::string get_type(Program* p, Scope& s) {
			type_ = value->get_type(p, s);
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

		virtual void check(Program* p, Scope& s, std::vector<Error>& errors) {
			value->check(p, s, errors);

			std::string expected;

			switch (type) {
				case NOT: expected = "bool"; break;
				case MINUS: expected = "int32"; break;
				case ISNULL: expected = "Object"; break;
			}

			if (not conforms_to(p, value->get_type(p, s), expected))
				errors.push_back({this, "expected type " + expected + ", but got type " + value->get_type(p, s)});

			this->get_type(p, s);
		}

		virtual std::string get_type(Program* p, Scope& scope) {
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

		virtual void check(Program* p, Scope& s, std::vector<Error>& errors) {
			left->check(p, s, errors);
			right->check(p, s, errors);

			std::string left_t = left->get_type(p, s);
			std::string right_t = right->get_type(p, s);

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

			if (expected.empty()) {
				bool cond = is_prim(p, left_t) and is_prim(p, right_t) and left_t != right_t;
				cond = cond or (is_prim(p, left_t) xor is_prim(p, right_t));

				if (cond)
					errors.push_back({this, "types " + left_t + " and " + right_t + " don't agree"});
			} else {
				if (left_t != right_t)
					errors.push_back({this, "types " + left_t + " and " + right_t + " don't agree"});

				if (left_t != expected)
					errors.push_back({this, "expected type " + expected + ", but got type " + left_t});
			}

			this->get_type(p, s);
		}

		virtual std::string get_type(Program* p, Scope& s) {
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

		virtual void check(Program* p, Scope& s, std::vector<Error>& errors) {
			scope->check(p, s, errors);
			args.check(p, s, errors);

			std::string type = scope->get_type(p, s);

			if (is_class(p, type)) {
				Class* c = p->classes_table[type];

				for (; c != NULL; c = c->parent)
					if (c->methods_table.find(name) != c->methods_table.end())
						break;

				if (c == NULL)
					errors.push_back({this, "unknown method " + name});
				else {
					Method* m = c->methods_table[name];

					if (args.size() != m->formals.size())
						errors.push_back({this, "call of method " + m->name + " with wrong number of arguments"});
					else
						for (int i = 0; i < args.size(); ++i)
							if (not conforms_to(p, args[i]->get_type(p, s), m->formals[i]->type))
								errors.push_back({args[i], "expected type " + m->formals[i]->type + ", but got type " + args[i]->get_type(p, s)});
				}
			} else
				errors.push_back({this, "unknown type " + type});

			this->get_type(p, s);
		}

		virtual std::string get_type(Program* p, Scope& s) {
			type_ = "error";

			std::string type = scope->get_type(p, s);

			if (is_class(p, type)) {
				Class* c = p->classes_table[type];

				for (; c != NULL; c = c->parent)
					if (c->methods_table.find(name) != c->methods_table.end())
						break;

				if (c != NULL)
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

		virtual void check(Program* p, Scope& s, std::vector<Error>& errors) {
			if (not is_prim(p, type) and not is_class(p, type))
				errors.push_back({this, "unknown type " + type});

			this->get_type(p, s);
		}

		virtual std::string get_type(Program* p, Scope& s) {
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

		virtual void check(Program* p, Scope& s, std::vector<Error>& errors) {
			if (not s.contains(id))
				errors.push_back({this, "undefined " + id});

			this->get_type(p, s);
		}

		virtual std::string get_type(Program* p, Scope& s) {
			type_ = s.contains(id) ? s.get(id) : "error";
			return type_;
		}
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
		virtual std::string to_string_aux() const {
			return std::to_string(value);
		}

		virtual std::string get_type(Program* p, Scope& s) {
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

		virtual std::string get_type(Program* p, Scope& s) {
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

		virtual std::string get_type(Program* p, Scope& s) {
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

		virtual std::string get_type(Program* p, Scope& s) {
			type_ = "unit";
			return type_;
		}
};

static bool is_class(Program* p, const std::string& type) {
	return p->classes_table.find(type) != p->classes_table.end();
}

static bool is_prim(Program* p, const std::string& type) {
	return type == "int32" or type == "string" or type == "bool" or type == "unit";
}

static bool conforms_to(Program* p, const std::string& a, const std::string& b) {
	if (is_class(p, a))
		for (Class* c = p->classes_table[a]; c != NULL; c = c->parent)
			if (c->name == b)
				return true;

	return a == b;
}

static std::string common_ancestor(Program* p, const std::string& a, const std::string& b) {
	Class* c = p->classes_table[a];
	Class* d = p->classes_table[b];

	std::unordered_set<std::string> C;
	std::unordered_set<std::string> D;

	while (c != NULL and d != NULL)
		if (c->name == d->name)
			return c->name;
		else if (C.find(d->name) == C.end())
			return d->name;
		else if (D.find(c->name) == D.end())
			return c->name;
		else {
			C.insert(c->name);
			D.insert(d->name);
			c = c->parent;
			d = d->parent;
		}

	return "Object";
}

#endif
