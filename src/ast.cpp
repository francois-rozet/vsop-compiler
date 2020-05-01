#include "ast.hpp"
#include "tools.hpp"

#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/GlobalVariable.h"

#include <iterator>

using namespace std;

/***** Debugging

#include <iostream>
#include "llvm/Support/raw_ostream.h"

static string rso_string;
static llvm::raw_string_ostream rso(rso_string);

static void dump(llvm::Type* t) {
	if (t) t->print(rso);
	cout << rso.str() << endl;
	rso.str().clear();
}

static void dump(llvm::Value* v) {
	if (v) v->print(rso);
	cout << rso.str() << endl;
	rso.str().clear();
}

static void imhere(string str) {
	cout << str << endl;
}

static void imhere(int i) {
	cout << i << endl;
}

*****/

/***** Functions *****/

static bool isUnit(llvm::Type* t) {
	return (not t) or t->isVoidTy();
}

static bool isInteger(llvm::Type* t) {
	return t and t->isIntegerTy(32);
}

static bool isBoolean(llvm::Type* t) {
	return t and t->isIntegerTy(1);
}

static bool isString(llvm::Type* t) {
	return t and t->isPointerTy() and t->getPointerElementType()->isIntegerTy(8);
}

static bool isClass(llvm::Type* t) {
	return t and t->isPointerTy() and t->getPointerElementType()->isStructTy();
}

static bool isPrimitive(llvm::Type* t) {
	return isInteger(t) or isBoolean(t) or isString(t);
}

static bool isEqualTo(llvm::Type* a, llvm::Type* b) {
	bool cond = isUnit(a) and isUnit(b);
	cond = cond or (isInteger(a) and isInteger(b));
	cond = cond or (isBoolean(a) and isBoolean(b));
	cond = cond or (isString(a) and isString(b));
	cond = cond or (isClass(a) and isClass(b) and a->getPointerElementType() == b->getPointerElementType());

	return cond;
}

static llvm::Type* asType(LLVMHelper& h, const string& t) {
	if (t == "unit")
		return llvm::Type::getVoidTy(h.context);
	else if (t == "int32")
		return llvm::Type::getInt32Ty(h.context);
	else if (t == "bool")
		return llvm::Type::getInt1Ty(h.context);
	else if (t == "string")
		return llvm::Type::getInt8PtrTy(h.context);

	llvm::StructType* st = h.module.getTypeByName("struct." + t);
	return st ? st->getPointerTo() : nullptr;
}

static string asString(llvm::Type* t) {
	if (isUnit(t))
		return "unit";
	if (isInteger(t))
		return "int32";
	else if (isBoolean(t))
		return "bool";
	else if (isString(t))
		return "string";

	return t->getPointerElementType()->getStructName().str().substr(7, string::npos);
}

static bool isSubclassOf_aux(Class* a, Class* b) {
	for (; a; a = a->parent)
		if (a == b)
			return true;

	return false;
}

static bool isSubclassOf(Program* p, llvm::Type* a, llvm::Type* b) {
	if (not isClass(a) or not isClass(b))
		return false;

	return isSubclassOf_aux(
		p->classes_table[asString(a)].get(),
		p->classes_table[asString(b)].get()
	);
}

static Class* commonAncestor_aux(Class* a, Class* b) {
	for (; a->parent; a = a->parent)
		if (isSubclassOf_aux(b, a))
			return a;

	return a;
}

static Class* commonAncestor(Program* p, llvm::Type* a, llvm::Type* b) {
	return commonAncestor_aux(
		p->classes_table[asString(a)].get(),
		p->classes_table[asString(b)].get()
	);
}

static llvm::Value* defaultValue(LLVMHelper& h, llvm::Type* t) {
	if (isClass(t))
		return llvm::ConstantPointerNull::get((llvm::PointerType*) t);
	else if (isString(t))
		return h.builder.CreateGlobalStringPtr("", "str");
	else if (isPrimitive(t))
		return llvm::Constant::getNullValue(t);
	else
		return nullptr;
}

static llvm::Value* defaultValue(LLVMHelper& h, const string& t) {
	return defaultValue(h, asType(h, t));
}

/***** Scope *****/

/* Methods */
llvm::Value* Scope::push(const string& name, llvm::Value* ptr) {
	if (this->contains(name))
		this->at(name).push_back(ptr);
	else
		this->insert({name, {ptr}});

	return this->get(name);
}

llvm::Value* Scope::pop(const string& name) {
	llvm::Value* ptr = nullptr;

	if (this->contains(name)) {
		ptr = this->at(name).back();

		if (this->at(name).size() > 1)
			this->at(name).pop_back();
		else
			this->erase(name);
	}

	return ptr;
}

llvm::Value* Scope::alloc(LLVMHelper& h,const string& name, llvm::Type* type) {
	return this->push(
		name,
		isUnit(type) ? nullptr : h.builder.CreateAlloca(type)
	);
}

llvm::Value* Scope::store(LLVMHelper& h, const string& name, llvm::Value* value) {
	if (llvm::Value* ptr = this->get(name))
		return h.builder.CreateStore(value, ptr)->getOperand(0);
	return nullptr;
}

bool Scope::contains(const string& name) const {
	return this->find(name) != this->end();
}

llvm::Value* Scope::get(const string& name) const {
	if (this->contains(name))
		return this->at(name).back();
	return nullptr;
}

llvm::Type* Scope::getType(const string& name) const {
	if (llvm::Value* ptr = this->get(name))
		return ptr->getType()->getPointerElementType();
	return nullptr;
}

llvm::Value* Scope::load(LLVMHelper& h, const string& name) const {
	if (llvm::Value* ptr = this->get(name))
		return h.builder.CreateLoad(ptr);
	return nullptr;
}

/***** Expr *****/

string Expr::toString(bool with_t) const {
	string str = this->toString_aux(with_t);
	if (with_t)
		str += ":" + asString(val ? val->getType() : nullptr);
	return str;
}

/***** Block *****/

/* Methods */
string Block::toString_aux(bool with_t) const {
	if (exprs.size() == 1)
		return exprs.front()->toString_aux(with_t);
	return exprs.toString(with_t);
}

llvm::Value* Block::codegen_aux(Program* p, LLVMHelper& h, Scope& s, vector<Error>& errors) {
	exprs.codegen(p, h, s, errors);
	return exprs.empty() ? nullptr : exprs.back()->val;
}

/***** Field *****/

/* Methods */
string Field::toString(bool with_t) const {
	string str = "Field(" + name + "," + type;
	if (init)
		str += "," + init->toString(with_t);
	return str + ")";
}

llvm::Value* Field::codegen_aux(Program* p, LLVMHelper& h, Scope& s, vector<Error>& errors) {
	llvm::Type* field_t = this->getType(h);

	if (init) {
		init->codegen(p, h, s, errors);
		llvm::Type* init_t = init->val ? init->val->getType() : nullptr;

		if (isEqualTo(init_t, field_t))
			return init->val;
		else if (isSubclassOf(p, init_t, field_t))
			return h.builder.CreatePointerCast(init->val, field_t);
		else
			errors.push_back({init->line, init->column, "expected type '" + type + "', but got initializer of type '" + asString(init_t) + "'"});
	}

	return defaultValue(h, field_t);
}

void Field::declaration(LLVMHelper& h, vector<Error>& errors) {
	if (not this->getType(h))
		errors.push_back({this->line, this->column, "unknown type '" + type + "' for field " + name});
}

llvm::Type* Field::getType(LLVMHelper& h) const {
	return asType(h, type);
}

/***** Formal *****/

/* Methods */
string Formal::toString(bool with_t) const {
	return name + ":" + type;
}

void Formal::declaration(LLVMHelper& h, vector<Error>& errors) {
	if (not this->getType(h))
		errors.push_back({this->line, this->column, "unknown type '" + type  + "' for formal " + name});
}

llvm::Type* Formal::getType(LLVMHelper& h) const {
	return asType(h, type);
}

/***** Method *****/

/* Methods */
string Method::toString(bool with_t) const {
	string str = "Method(" + name + "," + formals.toString(with_t) + "," + type;
	if (block)
		str += "," + block->toString(with_t);
	return str + ")";
}

void Method::codegen(Program* p, LLVMHelper& h, Scope& s, vector<Error>& errors) {
	if (not block) // extern method
		return;

	llvm::Function* f = this->getFunction(h);

	llvm::BasicBlock* entry_block = llvm::BasicBlock::Create(h.context, "", f);
	h.builder.SetInsertPoint(entry_block);

	// Add arguments to scope
	for (auto& it: f->args()) {
		s.alloc(h, it.getName(), it.getType());
		s.store(h, it.getName(), &it);
	}

	// Method block
	block->codegen(p, h, s, errors);

	// Remove arguments from scope
	for (auto& it: f->args())
		s.pop(it.getName());

	// Result casting
	llvm::Type* block_t = block->val ? block->val->getType() : nullptr;
	llvm::Type* return_t = f->getFunctionType()->getReturnType();

	if (isEqualTo(block_t, return_t))
		h.builder.CreateRet(block->val);
	else if (isSubclassOf(p, block_t, return_t))
		h.builder.CreateRet(h.builder.CreatePointerCast(block->val, return_t));
	else {
		errors.push_back({block->line, block->column, "expected type '" + type + "', but got return value of type '" + asString(block_t) + "'"});

		h.builder.CreateRet(defaultValue(h, return_t));
	}
}

void Method::declaration(LLVMHelper& h, vector<Error>& errors) {
	// Formals
	for (auto it = formals.begin(); it != formals.end(); ++it) {
		(*it)->declaration(h, errors);
		llvm::Type* t = (*it)->getType(h);

		if (not t) // invalid type
			it = prev(formals.erase(it));
		else if (formals_table.find((*it)->name) != formals_table.end()) { // formal already exists
			errors.push_back({(*it)->line, (*it)->column, "redefinition of formal " + (*it)->name + " of method " + this->getName(true)});
			it = prev(formals.erase(it));
		} else
			formals_table[(*it)->name] = *it;
	}

	// Return type
	llvm::Type* return_t = asType(h, type);

	if (return_t) {
		// Parameters
		vector<llvm::Type*> params_t;

		if (parent)
			params_t.push_back((llvm::Type*) parent->getType(h)->getPointerTo());

		for (shared_ptr<Formal>& formal: formals)
			params_t.push_back(formal->getType(h));

		// Prototype
		llvm::FunctionType* ft = llvm::FunctionType::get(return_t, params_t, false);

		// Forward declaration
		llvm::Function* f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, this->getName(), h.module);

		// First argument as 'self'
		if (parent)
			f->arg_begin()->setName("self");

		// Set arguments names
		int i = 0;
		for (auto it = f->arg_begin() + (parent ? 1 : 0); it != f->arg_end(); ++it)
			it->setName(formals[i++]->name);
	} else
		errors.push_back({this->line, this->column, "unknown return type '" + type + "' of method " + this->getName(true)});
}

string Method::getName(bool cpp) const {
	return parent ? (parent->name + (cpp ? "::" : "_") + name) : name;
}

llvm::Function* Method::getFunction(LLVMHelper& h) const {
	return h.module.getFunction(this->getName());
}

llvm::FunctionType* Method::getType(LLVMHelper& h) const {
	llvm::Function* f = this->getFunction(h);
	return f ? f->getFunctionType() : nullptr;
}

/***** Class *****/

/* Methods */
string Class::toString(bool with_t) const {
	return "Class(" + name + "," + parent_name + "," + fields.toString(with_t) + "," + methods.toString(with_t) + ")";
}

void Class::codegen(Program* p, LLVMHelper& h, Scope& s, vector<Error>& errors) {
	// Init
	llvm::Function* f = h.module.getFunction(name + "__init");

	llvm::BasicBlock* entry_block = llvm::BasicBlock::Create(h.context, "", f);
	h.builder.SetInsertPoint(entry_block);

	// Call parent's initializer
	if (parent)
		h.builder.CreateCall(
			h.module.getFunction(parent->name + "__init"),
			{h.builder.CreatePointerCast(
				f->arg_begin(),
				parent->getType(h)->getPointerTo()
			)}
		);

	// Initialize the fields
	for (shared_ptr<Field>& field: fields) {
		field->codegen(p, h, s, errors);

		if (not isUnit(field->getType(h)))
			h.builder.CreateStore(
				field->val,
				h.builder.CreateStructGEP(
					f->arg_begin(),
					fields_table[field->name]->idx
				)
			);
	}

	h.builder.CreateRetVoid();

	// New
	f = h.module.getFunction(name + "__new");

	entry_block = llvm::BasicBlock::Create(h.context, "", f);
	llvm::BasicBlock* init_block = llvm::BasicBlock::Create(h.context, "init", f);
	llvm::BasicBlock* null_block = llvm::BasicBlock::Create(h.context, "null", f);

	h.builder.SetInsertPoint(entry_block);

	// Allocation of heap memory
	size_t alloc_size = h.module.getDataLayout().getTypeAllocSize(this->getType(h));
	llvm::Value* memory = h.builder.CreateCall(
		h.module.getOrInsertFunction(
			"malloc",
			llvm::FunctionType::get(
				llvm::Type::getInt8PtrTy(h.context),
				{llvm::Type::getInt64Ty(h.context)},
				false
			)
		),
		{llvm::ConstantInt::get(llvm::Type::getInt64Ty(h.context), alloc_size)}
	);

	// Conditional branching
	h.builder.CreateCondBr(
		h.builder.CreateIsNull(memory),
		null_block,
		init_block
	);

	// Initialization block
	h.builder.SetInsertPoint(init_block);

	llvm::Value* instance = h.builder.CreateBitCast(
		memory,
		this->getType(h)->getPointerTo()
	);

	h.builder.CreateCall(
		h.module.getFunction(name + "__init"),
		{instance}
	);

	h.builder.CreateStore(
		h.module.getNamedValue("vtable." + name), // vtable
		h.builder.CreateStructGEP(instance, 0) // vtable slot
	);

	h.builder.CreateRet(instance);

	// Null block
	h.builder.SetInsertPoint(null_block);
	h.builder.CreateRet(
		llvm::ConstantPointerNull::get(this->getType(h)->getPointerTo())
	);

	// Methods code generation
	methods.codegen(p, h, s, errors);
}

void Class::declaration(LLVMHelper& h, vector<Error>& errors) {
	// Ensure parent is declared
	if (parent and not parent->isDeclared(h))
		parent->declaration(h, errors);

	// Indices
	unsigned f_idx = 1, m_idx = 0;

	if (parent) {
		for (auto& it: parent->fields_table)
			f_idx = max(f_idx, it.second->idx + 1);
		for (auto& it: parent->methods_table)
			m_idx = max(m_idx, it.second->idx + 1);
	}

	// Fields
	for (auto it = fields.begin(); it != fields.end(); ++it) {
		(*it)->declaration(h, errors);
		llvm::Type* t = (*it)->getType(h);

		if (not t) // invalid type
			it = prev(fields.erase(it));
		else if (fields_table.find((*it)->name) != fields_table.end()) { // field already exists
			errors.push_back({(*it)->line, (*it)->column, "redefinition of field " + (*it)->name + " of class " + name});
			it = prev(fields.erase(it));
		} else if (parent and parent->fields_table.find((*it)->name) != parent->fields_table.end()) { // field already exists in parent
			errors.push_back({(*it)->line, (*it)->column, "overriding field " + (*it)->name + " of class " + name});
			it = prev(fields.erase(it));
		} else {
			fields_table[(*it)->name] = *it;
			(*it)->idx = isUnit(t) ? f_idx : f_idx++;
		}
	}

	if (parent)
		fields_table.insert(parent->fields_table.begin(), parent->fields_table.end());

	// Methods
	for (auto it = methods.begin(); it != methods.end(); ++it) {
		(*it)->parent = this;

		if (methods_table.find((*it)->name) != methods_table.end()) { // method already exists
			errors.push_back({(*it)->line, (*it)->column, "redefinition of method " + (*it)->getName(true)});
			it = prev(methods.erase(it));
		} else {
			(*it)->declaration(h, errors);
			llvm::Function* f = (*it)->getFunction(h);

			if (not f) // invalid function
				it = prev(methods.erase(it));
			else if (parent and parent->methods_table.find((*it)->name) != parent->methods_table.end()) { // method already exists in parent
				shared_ptr<Method> m = parent->methods_table[(*it)->name];

				int i = 0;
				if ((*it)->formals.size() == m->formals.size())
					for (; i < (*it)->formals.size(); ++i)
						if ((*it)->formals[i]->type != m->formals[i]->type)
							break;

				if ((*it)->type == m->type and i == (*it)->formals.size()) {
					methods_table[(*it)->name] = *it;
					(*it)->idx = m->idx;
				} else {
					errors.push_back({(*it)->line, (*it)->column, "overriding method " + m->getName(true) + " with different signature"});
					f->eraseFromParent();
					it = prev(methods.erase(it));
				}
			} else {
				methods_table[(*it)->name] = *it;
				(*it)->idx = m_idx++;
			}
		}
	}

	if (parent)
		for (auto it = parent->methods_table.begin(); it != parent->methods_table.end(); ++it)
			if (methods_table.find(it->first) == methods_table.end())
				methods_table[it->first] = it->second;

	// Initialize struct and vtable types
	llvm::StructType* self_t = this->getType(h);
	llvm::StructType* vtable_t = llvm::StructType::create(h.context, this->getName() + "VTable");

	// Class structure definition
	vector<llvm::Type*> elements_t;

	elements_t.push_back(vtable_t->getPointerTo()); // vtable slot

	for (auto it: this->fields_table) {
		llvm::Type* t = it.second->getType(h);

		if (isUnit(t))
			continue;

		if (it.second->idx >= elements_t.size())
			elements_t.resize(it.second->idx + 1);

		elements_t[it.second->idx] = t;
	}

	self_t->setBody(elements_t);

	// Vtable structure definition & instance
	vector<llvm::Constant*> elements;
	elements_t.clear();

	for (auto it: this->methods_table) {
		if (it.second->idx >= elements_t.size()) {
			elements_t.resize(it.second->idx + 1);
			elements.resize(it.second->idx + 1);
		}

		// Method reference
		llvm::Function* f = it.second->getFunction(h);

		// Edit 'self' reference type
		llvm::Type* return_t = f->getReturnType();
		vector<llvm::Type*> params_t = f->getFunctionType()->params();
		params_t[0] = self_t->getPointerTo();

		llvm::FunctionType* ft = llvm::FunctionType::get(return_t, params_t, false);

		// Cast method to new type
		elements[it.second->idx] = (llvm::Constant*) h.builder.CreatePointerCast(f, ft->getPointerTo());
		elements_t[it.second->idx] = ft->getPointerTo();
	}

	vtable_t->setBody(elements_t);

	llvm::GlobalVariable* vtable = new llvm::GlobalVariable(
		h.module,
		vtable_t, // StructType
		true, // isConstant
		llvm::GlobalVariable::InternalLinkage,
		llvm::ConstantStruct::get(
			vtable_t,
			elements
		), // Initializer
		"vtable." + name // Name
	);

	// New
	llvm::FunctionType* ft = llvm::FunctionType::get(self_t->getPointerTo(), false);
	llvm::Function* f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, name + "__new", h.module);

	// Init
	ft = llvm::FunctionType::get(
		llvm::Type::getVoidTy(h.context),
		{self_t->getPointerTo()},
		false
	);
	f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, name + "__init", h.module);
	f->arg_begin()->setName("self");
}

string Class::getName() const {
	return "struct." + name;
}

bool Class::isDeclared(LLVMHelper& h) const {
	return h.module.getFunction(name + "__new");
}

llvm::StructType* Class::getType(LLVMHelper& h) const {
	llvm::StructType* s = h.module.getTypeByName(this->getName());
	return s ? s : llvm::StructType::create(h.context, this->getName());
}

/***** Program *****/

/* Methods */
string Program::toString(bool with_t) const {
	string str = classes.toString(with_t);
	if (not functions.empty())
		str += "," + functions.toString(with_t);
	return str;
}

void Program::codegen(Program* p, LLVMHelper& h, Scope& s, vector<Error>& errors) {
	// Classes code generation
	classes.codegen(p, h, s, errors);

	// Functions code generation
	functions.codegen(p, h, s, errors);

	// Main
	if (functions_table.find("main") != functions_table.end()) {
		shared_ptr<Method> m = functions_table["main"];

		if (m->formals.size() != 0 or m->type != "int32")
			errors.push_back({m->line, m->column, "function " + m->getName(true) + " declared with wrong signature"});
	} else if (classes_table.find("Main") != classes_table.end()) {
		shared_ptr<Class> c = classes_table["Main"];

		if (c->methods_table.find("main") != c->methods_table.end()) {
			shared_ptr<Method> m = c->methods_table["main"];

			if (m->formals.size() == 0 and m->type == "int32") { // main() : int32
				llvm::FunctionType* ft = llvm::FunctionType::get(
					llvm::Type::getInt32Ty(h.context), {}, false
				);
				llvm::Function* f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "main", h.module);

				// { (new Main).main() }
				llvm::BasicBlock* entry_block = llvm::BasicBlock::Create(h.context, "", f);
				h.builder.SetInsertPoint(entry_block);

				h.builder.CreateRet(
					Call(
						new New("Main"), "main", {}
					).codegen_aux(p, h, s, errors)
				);
			} else
				errors.push_back({m->line, m->column, "method " + m->getName(true) + " declared with wrong signature"});
		} else
			errors.push_back({c->line, c->column, "undeclared method main in class Main"});
	} else
		errors.push_back({this->line, this->column, "undeclared class Main"});
}

void Program::declaration(LLVMHelper& h, vector<Error>& errors) {
	// Object
	classes_table["Object"] = shared_ptr<Class>(new Class("Object", "Object", {},
		List<Method>({
			new Method("print", {new Formal("s", "string")}, "Object", nullptr),
			new Method("printBool", {new Formal("b", "bool")}, "Object", nullptr),
			new Method("printInt32", {new Formal("i", "int32")}, "Object", nullptr),
			new Method("inputLine", {}, "string", nullptr),
			new Method("inputBool", {}, "bool", nullptr),
			new Method("inputInt32", {}, "int32", nullptr)
		})
	));

	classes_table["Object"]->parent = nullptr;
	classes_table["Object"]->getType(h); // class forward declaration

	// Classes redefinition and overriding
	int size;
	do {
		size = classes_table.size();

		for (auto it = classes.begin(); it != classes.end(); ++it)
			if ((*it)->parent) // class has already been processed
				continue;
			else if (classes_table.find((*it)->name) != classes_table.end()) { // class already exists
				errors.push_back({(*it)->line, (*it)->column, "redefinition of class " + (*it)->name});
				it = classes.erase(it);
			} else if (classes_table.find((*it)->parent_name) != classes_table.end()) { // class parent exists
				classes_table[(*it)->name] = *it;
				(*it)->parent = classes_table[(*it)->parent_name].get();
				(*it)->getType(h); // class forward declaration
			}

	} while (size < classes_table.size());

	for (auto it = classes.begin(); it != classes.end(); ++it)
		if ((*it)->parent) // class has been processed
			(*it)->declaration(h, errors);
		else {
			errors.push_back({(*it)->line, (*it)->column, "class " + (*it)->name + " cannot extend class " + (*it)->parent_name});
			it = prev(classes.erase(it));
		}

	// Functions redefinition
	for (auto it = functions.begin(); it != functions.end(); ++it) {
		(*it)->parent = nullptr;
		(*it)->declaration(h, errors);

		llvm::Function* f = (*it)->getFunction(h);

		if (not f) // invalid function
			it = prev(functions.erase(it));
		else if (functions_table.find((*it)->name) != functions_table.end()) { // function already exists
			errors.push_back({(*it)->line, (*it)->column, "redefinition of function " + (*it)->getName(true)});
			f->eraseFromParent();
			it = prev(functions.erase(it));
		} else
			functions_table[(*it)->name] = *it;
	}
}

/***** If *****/

/* Methods */
string If::toString_aux(bool with_t) const {
	string str = "If(" + cond->toString(with_t) + "," + then->toString(with_t);
	if (els)
		str += "," + els->toString(with_t);
	return str + ")";
}

llvm::Value* If::codegen_aux(Program* p, LLVMHelper& h, Scope& s, vector<Error>& errors) {
	cond->codegen(p, h, s, errors);
	llvm::Type* cond_t = cond->val ? cond->val->getType() : nullptr;

	if (not isBoolean(cond_t))
		errors.push_back({cond->line, cond->column, "expected type 'bool', but got condition of type '" + asString(cond_t) + "'"});

	llvm::Function* f = h.builder.GetInsertBlock()->getParent();

	// If blocks
	llvm::BasicBlock* then_block = llvm::BasicBlock::Create(h.context, "then", f);
	llvm::BasicBlock* else_block = llvm::BasicBlock::Create(h.context, "else", f);
	llvm::BasicBlock* end_block = llvm::BasicBlock::Create(h.context, "end", f);

	// Conditional branching
	h.builder.CreateCondBr(
		isBoolean(cond_t) ? cond->val : llvm::ConstantInt::getFalse(h.context),
		then_block,
		else_block
	);

	// Then block
	h.builder.SetInsertPoint(then_block);
	then->codegen(p, h, s, errors);
	llvm::BasicBlock* then_bis = h.builder.GetInsertBlock();

	// Else block
	h.builder.SetInsertPoint(else_block);
	if (els)
		els->codegen(p, h, s, errors);
	llvm::BasicBlock* else_bis = h.builder.GetInsertBlock();

	// Return type
	llvm::Type* then_t = then->val ? then->val->getType() : nullptr;
	llvm::Type* else_t = els and els->val ? els->val->getType() : nullptr;
	llvm::Type* end_t = nullptr;

	if (isEqualTo(then_t, else_t))
		end_t = then_t;
	else if (isClass(then_t) and isClass(else_t))
		end_t = commonAncestor(p, then_t, else_t)->getType(h)->getPointerTo();
	else if (not isUnit(then_t) and not isUnit(else_t))
		errors.push_back({this->line, this->column, "expected agreeing branch types, but got types '" + asString(then_t) + "' and '" + asString(else_t) + "'"});

	llvm::Value* then_val = nullptr;
	llvm::Value* else_val = nullptr;

	// Then block
	h.builder.SetInsertPoint(then_bis);
	if (not isUnit(end_t))
		then_val = isEqualTo(then_t, end_t) ? then->val : h.builder.CreatePointerCast(then->val, end_t);
	h.builder.CreateBr(end_block);

	// Else block
	h.builder.SetInsertPoint(else_bis);
	if (not isUnit(end_t))
		else_val = isEqualTo(else_t, end_t) ? els->val : h.builder.CreatePointerCast(els->val, end_t);
	h.builder.CreateBr(end_block);

	// End block
	h.builder.SetInsertPoint(end_block);

	if (not isUnit(end_t)) {
		auto* phi = h.builder.CreatePHI(end_t, 2);
		phi->addIncoming(then_val, then_bis);
		phi->addIncoming(else_val, else_bis);

		return phi;
	}

	return nullptr;
}

/***** While *****/

/* Methods */
string While::toString_aux(bool with_t) const {
	return "While(" + cond->toString(with_t) + "," + body->toString(with_t) + ")";
}

llvm::Value* While::codegen_aux(Program* p, LLVMHelper& h, Scope& s, vector<Error>& errors) {
	llvm::Function* f = h.builder.GetInsertBlock()->getParent();

	// While blocks
	llvm::BasicBlock* cond_block = llvm::BasicBlock::Create(h.context, "cond", f);
	llvm::BasicBlock* body_block = llvm::BasicBlock::Create(h.context, "body", f);
	llvm::BasicBlock* exit_block = llvm::BasicBlock::Create(h.context, "exit", f);

	// (-ext) Push break point
	h.exits.push_back(exit_block);

	h.builder.CreateBr(cond_block);

	// Cond block
	h.builder.SetInsertPoint(cond_block);

	cond->codegen(p, h, s, errors);
	llvm::Type* cond_t = cond->val ? cond->val->getType() : nullptr;

	if (not isBoolean(cond_t))
		errors.push_back({cond->line, cond->column, "expected type 'bool', but got condition of type '" + asString(cond_t) + "'"});

	// Conditional branching
	h.builder.CreateCondBr(
		isBoolean(cond_t) ? cond->val : llvm::ConstantInt::getFalse(h.context),
		body_block,
		exit_block
	);

	// Body block
	h.builder.SetInsertPoint(body_block);

	body->codegen(p, h, s, errors); // don't care about the type

	h.builder.CreateBr(cond_block);

	// Exit block
	h.builder.SetInsertPoint(exit_block);

	// (-ext) Pop break point
	h.exits.pop_back();

	return nullptr;
}

/***** Break *****/

/* Methods */
string Break::toString_aux(bool with_t) const {
	return "break";
}

llvm::Value* Break::codegen_aux(Program* p, LLVMHelper& h, Scope& s, vector<Error>& errors) {
	if (not h.exits.empty()) {
		// Jump to exit branch
		h.builder.CreateBr(h.exits.back());

		// Dump all following instructions in unreachable block
		h.builder.SetInsertPoint(
			llvm::BasicBlock::Create(h.context, "unreachable", h.exits.back()->getParent())
		);
	} else
		errors.push_back({this->line, this->column, "break outside loop"});

	return nullptr;
}

/***** For *****/

/* Methods */
string For::toString_aux(bool with_t) const {
	return "For(" + name + "," + first->toString(with_t) + "," + last->toString(with_t) + "," + body->toString(with_t) + ")";
}

llvm::Value* For::codegen_aux(Program* p, LLVMHelper& h, Scope& s, vector<Error>& errors) {
	return Lets(
		List<Field>({
			new Field(name, "int32", first),
			new Field("_last", "int32", last) // underscore starting identifier for privacy
		}),
		new While(
			new Binary(Binary::LOWER_EQUAL, new Identifier(name), new Identifier("_last")),
			new Block({
				body,
				make_shared<Assign>(name, new Binary(Binary::PLUS, new Identifier(name), new Integer(1))),
			})
		)
	).codegen_aux(p, h, s, errors);
}

/***** Let *****/

/* Methods */
string Let::toString_aux(bool with_t) const {
	string str = "Let(" + name + "," + type + ",";
	if (init)
		str += init->toString(with_t) + ",";
	return str + scope->toString(with_t) + ")";
}

llvm::Value* Let::codegen_aux(Program* p, LLVMHelper& h, Scope& s, vector<Error>& errors) {
	llvm::Type* let_t = asType(h, type);

	if (let_t) {
		llvm::Value* temp = nullptr;

		if (init) {
			init->codegen(p, h, s, errors);
			llvm::Type* init_t = init->val ? init->val->getType() : nullptr;

			if (isEqualTo(init_t, let_t))
				temp = init->val;
			else if (isSubclassOf(p, init_t, let_t))
				temp = h.builder.CreatePointerCast(init->val, let_t);
			else
				errors.push_back({init->line, init->column, "expected type '" + type + "', but got initializer of type '" + asString(init_t) + "'"});
		}

		// Allocate and store variable
		s.alloc(h, name, let_t);
		s.store(h, name, temp ? temp : defaultValue(h, let_t));
	} else
		errors.push_back({this->line, this->column, "unknown type '" + type + "'"});

	scope->codegen(p, h, s, errors);

	// Remove variable from scope
	s.pop(name);

	return scope->val;
}

/***** Lets *****/

/* Methods */
string Lets::toString_aux(bool with_t) const {
	return "Lets(" + fields.toString(with_t) + "," + scope->toString(with_t) + ")";
}

llvm::Value* Lets::codegen_aux(Program* p, LLVMHelper& h, Scope& s, vector<Error>& errors) {
	shared_ptr<Expr> x = scope;

	for (auto it = fields.rbegin(); it != fields.rend(); ++it)
		x = make_shared<Let>((*it)->name, (*it)->type, (*it)->init, x); // recursive

	return x->codegen_aux(p, h, s, errors);
}

/***** Assign *****/

/* Methods */
string Assign::toString_aux(bool with_t) const {
	return "Assign(" + name + "," + value->toString(with_t) + ")";
}

llvm::Value* Assign::codegen_aux(Program* p, LLVMHelper& h, Scope& s, vector<Error>& errors) {
	value->codegen(p, h, s, errors);
	llvm::Type* value_t = value->val ? value->val->getType() : nullptr;

	// Search self's fields
	llvm::Type* self_t = s.getType("self");
	shared_ptr<Class> c = self_t ? p->classes_table[asString(self_t)] : nullptr;

	// Get target type
	llvm::Type* target_t = nullptr;

	if (s.contains(name))
		target_t = s.getType(name);
	else if (c and c->fields_table.find(name) != c->fields_table.end())
		target_t = c->fields_table[name]->getType(h);
	else {
		errors.push_back({this->line, this->column, "assignation to undeclared identifier " + name});
		return nullptr;
	}

	// Cast value to target type
	llvm::Value* casted = nullptr;

	if (isEqualTo(value_t, target_t))
		casted = value->val;
	else if (isSubclassOf(p, value_t, target_t))
		casted = h.builder.CreatePointerCast(value->val, target_t);
	else {
		errors.push_back({value->line, value->column, "expected type '" + asString(target_t) + "', but got r-value of type '" + asString(value_t) + "'"});
		return nullptr;
	}

	// Store casted value
	if (s.contains(name))
		s.store(h, name, casted);
	else if (not isUnit(target_t))
		h.builder.CreateStore(
			casted,
			h.builder.CreateStructGEP(
				s.load(h, "self"),
				c->fields_table[name]->idx
			)
		);

	return casted;
}

/***** Unary *****/

/* Methods */
string Unary::toString_aux(bool with_t) const {
	string str = "UnOp(";
	switch (type) {
		case NOT: str += "not"; break;
		case MINUS: str += "-"; break;
		case ISNULL: str += "isnull"; break;
	}
	return str + "," + value->toString(with_t) + ")";
}

llvm::Value* Unary::codegen_aux(Program* p, LLVMHelper& h, Scope& s, vector<Error>& errors) {
	value->codegen(p, h, s, errors);
	llvm::Type* value_t = value->val ? value->val->getType() : nullptr;

	llvm::Value* out = nullptr;
	string expected;

	switch (type) {
		case NOT:
			if (isBoolean(value_t))
				return h.builder.CreateNot(value->val);
			else {
				out = defaultValue(h, "bool");
				expected = "bool";
			}
			break;
		case MINUS:
			if (isInteger(value_t))
				return h.builder.CreateNeg(value->val);
			else {
				out = defaultValue(h, "int32");
				expected = "int32";
			}
			break;
		case ISNULL:
			if (isClass(value_t)) return h.builder.CreateIsNull(value->val);
			else {
				out = defaultValue(h, "bool");
				expected = "Object";
			}
	}

	errors.push_back({value->line, value->column, "expected type '" + expected + "', but got operand of type '" + asString(value_t) + "'"});

	return out;
}

/***** Binary *****/

/* Methods */
string Binary::toString_aux(bool with_t) const {
	string str = "BinOp(";
	switch (type) {
		case AND: str += "and"; break;
		case OR: str += "or"; break; // -ext
		case EQUAL: str += "="; break;
		case NEQUAL: str += "!="; break; // -ext
		case LOWER: str += "<"; break;
		case GREATER: str += ">"; break; // -ext
		case LOWER_EQUAL: str += "<="; break; // -ext
		case GREATER_EQUAL: str += ">="; break; // -ext
		case PLUS: str += "+"; break;
		case MINUS: str += "-"; break;
		case TIMES: str += "*"; break;
		case DIV: str += "/"; break;
		case POW: str += "^"; break;
		case MOD: str += "mod"; break; // -ext
	}
	return str + "," + left->toString(with_t) + "," + right->toString(with_t) + ")";
}

llvm::Value* Binary::codegen_aux(Program* p, LLVMHelper& h, Scope& s, vector<Error>& errors) {
	llvm::Value* out = nullptr;
	string expected;

	switch (type) {
		case AND:
			return If(left, right, make_shared<Boolean>(false)).codegen_aux(p, h, s, errors);
		case OR:
			return If(left, make_shared<Boolean>(true), right).codegen_aux(p, h, s, errors);
		case NEQUAL:
			return Unary(Unary::NOT, new Binary(EQUAL, left, right)).codegen_aux(p, h, s, errors);
		default:
			left->codegen(p, h, s, errors);
			right->codegen(p, h, s, errors);

			llvm::Type* left_t = left->val ? left->val->getType() : nullptr;
			llvm::Type* right_t = right->val ? right->val->getType() : nullptr;

			switch (type) {
				case EQUAL:
					if (isEqualTo(left_t, right_t)) {
						if (isString(left_t)) {
							llvm::Value* comp = h.builder.CreateCall(
								h.module.getOrInsertFunction(
									"strcmp",
									llvm::FunctionType::get(
										llvm::Type::getInt32Ty(h.context),
										{
											llvm::Type::getInt8PtrTy(h.context),
											llvm::Type::getInt8PtrTy(h.context),
										},
										false
									)
								),
								{left->val, right->val}
							);

							return h.builder.CreateICmpEQ(comp, Integer(0).codegen_aux(p, h, s, errors));
						} else if (isUnit(left_t))
							return llvm::ConstantInt::getTrue(h.context);
						else
							return h.builder.CreateICmpEQ(left->val, right->val);
					} else if (isClass(left_t) and isClass(right_t)) {
						llvm::Type* comm_t = commonAncestor(p, left_t, left_t)->getType(h)->getPointerTo();
						llvm::Value* left_bis = isEqualTo(left_t, comm_t) ? left->val : h.builder.CreatePointerCast(left->val, comm_t);
						llvm::Value* right_bis = isEqualTo(right_t, comm_t) ? right->val : h.builder.CreatePointerCast(right->val, comm_t);

						return h.builder.CreateICmpEQ(left_bis, right_bis);
					} else
						errors.push_back({this->line, this->column, "expected agreeing operand types, but got types '" + asString(left_t) + "' and '" + asString(right_t) + "'"});

					return defaultValue(h, "bool");
				default:
					if (isInteger(left_t) and isInteger(right_t))
						switch (type) {
							case LOWER: return h.builder.CreateICmpSLT(left->val, right->val);
							case LOWER_EQUAL: return h.builder.CreateICmpSLE(left->val, right->val);
							case GREATER: return h.builder.CreateICmpSGT(left->val, right->val);
							case GREATER_EQUAL: return h.builder.CreateICmpSGE(left->val, right->val);
							case PLUS: return h.builder.CreateAdd(left->val, right->val);
							case MINUS: return h.builder.CreateSub(left->val, right->val);
							case TIMES: return h.builder.CreateMul(left->val, right->val);
							case DIV: return h.builder.CreateSDiv(left->val, right->val);
							case POW:
								return h.builder.CreateFPToSI(
									h.builder.CreateCall(
										h.module.getOrInsertFunction(
											"llvm.powi.f64",
											llvm::FunctionType::get(
												llvm::Type::getDoubleTy(h.context),
												{
													llvm::Type::getDoubleTy(h.context),
													llvm::Type::getInt32Ty(h.context),
												},
												false
											)
										),
										{
											h.builder.CreateSIToFP(
												left->val,
												llvm::Type::getDoubleTy(h.context)
											),
											right->val
										}
									),
									llvm::Type::getInt32Ty(h.context)
								);
							case MOD: return h.builder.CreateSRem(left->val, right->val);
							default: break;
						}
					else {
						switch (type) {
							case LOWER:
							case LOWER_EQUAL:
							case GREATER:
							case GREATER_EQUAL:
								out = defaultValue(h, "bool");
								break;
							default:
								out = defaultValue(h, "int32");
						}
						expected = "int32";
					}
			}

			errors.push_back({this->line, this->column, "expected type '" + expected + "', but got operand of types '" + asString(left_t) + "' and '" + asString(right_t) + "'"});
	}

	return out;
}

/***** Call *****/

/* Methods */
string Call::toString_aux(bool with_t) const { // improvement -> replace 'self' by
	return "Call(" + scope->toString(with_t) + "," + name + "," + args.toString(with_t) + ")";
}

llvm::Value* Call::codegen_aux(Program* p, LLVMHelper& h, Scope& s, vector<Error>& errors) {
	scope->codegen(p, h, s, errors);
	llvm::Type* scope_t = scope->val ? scope->val->getType() : nullptr;

	args.codegen(p, h, s, errors);

	if (isUnit(scope_t) or isClass(scope_t)) {
		llvm::Function* f = nullptr;
		llvm::FunctionType* ft = nullptr;
		vector<llvm::Value*> params;

		if (isUnit(scope_t) and p->functions_table.find(name) != p->functions_table.end()) { // top-level function
			f = p->functions_table[name]->getFunction(h);
			ft = f->getFunctionType();
		} else if (not isUnit(scope_t) or s.contains("self")) {
			llvm::Value* obj = isUnit(scope_t) ? s.load(h, "self") : scope->val;

			shared_ptr<Class> c = p->classes_table[asString(obj->getType())];

			if (c->methods_table.find(name) != c->methods_table.end()) { // class method
				f = (llvm::Function*) h.builder.CreateLoad(
					h.builder.CreateStructGEP(
						h.builder.CreateLoad(
							h.builder.CreateStructGEP(obj, 0)
						), // obj->vtable
						c->methods_table[name]->idx
					)
				); // vtable->method

				ft = (llvm::FunctionType*) f->getType()->getPointerElementType();

				// Add obj as self param
				params.push_back(obj);
			}
		}

		if (f) {
			int align = params.empty() ? 0 : 1;

			// Compare call with signature
			if (args.size() + align == ft->getNumParams()) {
				for (int i = 0; i < args.size(); ++i) {
					llvm::Type* param_t = ft->getParamType(i + align);
					llvm::Type* arg_t = args[i]->val ? args[i]->val->getType() : nullptr;

					if (isEqualTo(arg_t, param_t))
						params.push_back(args[i]->val);
					else if (isSubclassOf(p, arg_t, param_t))
						params.push_back(
							h.builder.CreatePointerCast(
								args[i]->val,
								param_t
							)
						);
					else
						errors.push_back({args[i]->line, args[i]->column, "expected type '" + asString(param_t) + "', but got argument of type '" + asString(arg_t) + "'"});
				}

				if (params.size() == ft->getNumParams()) // all arguments have the good type
					return h.builder.CreateCall(f, params);
			} else
				errors.push_back({this->line, this->column, "call to method " + name + " with wrong number of arguments"});
		} else
			errors.push_back({this->line, this->column, "call to undeclared method " + name});
	} else
		errors.push_back({scope->line, scope->column, "expected object type, but got scope of type '" + asString(scope_t) + "'"});

	return nullptr;
}


/***** New *****/

/* Methods */
string New::toString_aux(bool with_t) const {
	return "New(" + type + ")";
}

llvm::Value* New::codegen_aux(Program* p, LLVMHelper& h, Scope& s, vector<Error>& errors) {
	llvm::Function* f = h.module.getFunction(type + "__new");

	if (not f) {
		errors.push_back({this->line, this->column, "new instance of unknown object type '" + type + "'"});
		return nullptr;
	}

	return h.builder.CreateCall(f, {});
}

/***** Identifier *****/

/* Methods */
string Identifier::toString_aux(bool with_t) const {
	return id;
}

llvm::Value* Identifier::codegen_aux(Program* p, LLVMHelper& h, Scope& s, vector<Error>& errors) {
	// Load from scope
	if (s.contains(id))
		return s.load(h, id);

	// Search self's fields
	llvm::Type* self_t = s.getType("self");
	shared_ptr<Class> c = self_t ? p->classes_table[asString(self_t)] : nullptr;

	if (c and c->fields_table.find(id) != c->fields_table.end())
		return h.builder.CreateLoad(
			h.builder.CreateStructGEP(
				s.load(h, "self"),
				c->fields_table[id]->idx
			)
		); // self->id

	errors.push_back({this->line, this->column, "undeclared identifier " + id});

	return nullptr;
}

/***** Integer *****/

/* Methods */
string Integer::toString_aux(bool with_t) const {
	return std::to_string(value);
}

llvm::Value* Integer::codegen_aux(Program* p, LLVMHelper& h, Scope& s, vector<Error>& errors) {
	return llvm::ConstantInt::get(llvm::Type::getInt32Ty(h.context), value);
}

/***** String *****/

/* Methods */
string String::toString_aux(bool with_t) const {
	string temp;
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

llvm::Value* String::codegen_aux(Program* p, LLVMHelper& h, Scope& s, vector<Error>& errors) {
	return h.builder.CreateGlobalStringPtr(str, "str");
}

/***** Boolean ****/

/* Methods */
string Boolean::toString_aux(bool with_t) const {
	return b ? "true" : "false";
}

llvm::Value* Boolean::codegen_aux(Program* p, LLVMHelper& h, Scope& s, vector<Error>& errors) {
	return b ? llvm::ConstantInt::getTrue(h.context) : llvm::ConstantInt::getFalse(h.context);
}

/***** Unit *****/

/* Methods */
string Unit::toString_aux(bool with_t) const {
	return "()";
}

llvm::Value* Unit::codegen_aux(Program* p, LLVMHelper& h, Scope& s, vector<Error>& errors) {
	return nullptr;
}
