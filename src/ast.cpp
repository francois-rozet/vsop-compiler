#include "ast.hpp"
#include "tools.hpp"

#include <iterator>

using namespace std;

/***** Debugging

#include <iostream>

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

/***** Static functions *****/

/*
 * Cast a value into a target type, if possible.
 *
 * @remark If the value is already of the target type, nothing is done.
 */
static llvm::Value* castToTargetTy(Program& p, LLVMHelper& h, llvm::Value* value, llvm::Type* target_t) {
	if (not value)
		return nullptr;

	llvm::Type* value_t = value->getType();

	if (isSameAs(value_t, target_t))
		return value;
	else if (isNumeric(value_t) and isNumeric(target_t))
		return h.numericCast(value, target_t);
	else if (p.isSubclassOf(asString(value_t), asString(target_t)))
		return h.builder->CreatePointerCast(value, target_t);

	return nullptr;
}

/***** Block *****/

string Block::_toString(bool with_t) const {
	if (exprs.size() == 1)
		return exprs.front()->_toString(with_t);
	return exprs.toString(with_t);
}

llvm::Value* Block::_codegen(Program& p, LLVMHelper& h) {
	exprs.codegen(p, h);
	return exprs.empty() ? nullptr : exprs.back()->getValue();
}

/***** Field *****/

string Field::toString(bool with_t) const {
	string str = "Field(" + name + "," + type;
	if (init)
		str += "," + init->toString(with_t);
	return str + ")";
}

llvm::Value* Field::_codegen(Program& p, LLVMHelper& h) {
	llvm::Type* field_t = h.asType(type);

	if (init) {
		init->codegen(p, h);

		if (isUnit(init->getType()) and isUnit(field_t))
			return nullptr;

		llvm::Value* casted = castToTargetTy(p, h, init->getValue(), field_t);

		if (casted)
			return casted;
		else
			h.errors.push_back({init->pos, "expected type '" + type + "', but got initializer of type '" + asString(init->getType()) + "'"});
	}

	return h.defaultValue(field_t);
}

/***** Formal *****/

string Formal::toString(bool with_t) const {
	return name + ":" + type;
}

/***** Method *****/

string Method::toString(bool with_t) const {
	string str = "Method(" + name + "," + formals.toString(with_t);
	if (variadic)
		str += "...";
	str += "," + type;
	if (block)
		str += "," + block->toString(with_t);
	return str + ")";
}

void Method::codegen(Program& p, LLVMHelper& h) {
	if (not block) // extern method
		return;

	llvm::Function* f = this->getFunction(h);

	llvm::BasicBlock* entry_block = llvm::BasicBlock::Create(*h.context, "", f);
	h.builder->SetInsertPoint(entry_block);

	// Add arguments to scope
	for (auto& it: f->args()) {
		h.alloc(it.getName(), it.getType());
		h.store(it.getName(), &it);
	}

	// Method block
	block->codegen(p, h);

	// Remove arguments from scope
	for (auto& it: f->args())
		h.pop(it.getName());

	// Result casting
	llvm::Type* return_t = f->getReturnType();

	llvm::Value* casted = nullptr;

	if (isUnit(block->getType()) and isUnit(return_t));
	else {
		casted = castToTargetTy(p, h, block->getValue(), return_t);

		if (not casted) {
			h.errors.push_back({block->pos, "expected type '" + type + "', but got return value of type '" + asString(block->getType()) + "'"});
			casted = h.defaultValue(return_t);
		}
	}

	h.builder->CreateRet(casted);
}

void Method::declaration(LLVMHelper& h) {
	// Formals
	for (auto it = formals.begin(); it != formals.end(); ++it) {
		llvm::Type* t = (*it)->getType(h);

		if (not t) { // invalid type
			h.errors.push_back({(*it)->pos, "unknown type '" + (*it)->type  + "' for formal " + (*it)->name});
			it = prev(formals.erase(it));
		} else if (formals_table.find((*it)->name) != formals_table.end()) { // formal already exists
			h.errors.push_back({(*it)->pos, "redefinition of formal " + (*it)->name + " of method " + this->getName(true)});
			it = prev(formals.erase(it));
		} else
			formals_table[(*it)->name] = *it;
	}

	// Return type
	llvm::Type* return_t = h.asType(type);

	if (return_t) {
		// Parameters
		vector<llvm::Type*> params_t;

		if (parent)
			params_t.push_back((llvm::Type*) parent->getType(h)->getPointerTo());

		for (shared_ptr<Formal>& formal: formals)
			params_t.push_back(formal->getType(h));

		// Prototype
		llvm::FunctionType* ft = llvm::FunctionType::get(return_t, params_t, variadic);

		// Forward declaration
		llvm::Function* f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, this->getName(), *h.module);

		// First argument as 'self'
		if (parent)
			f->arg_begin()->setName("self");

		// Set arguments names
		int i = 0;
		for (auto it = f->arg_begin() + (parent ? 1 : 0); it != f->arg_end(); ++it)
			it->setName(formals[i++]->name);
	} else
		h.errors.push_back({this->pos, "unknown return type '" + type + "' of method " + this->getName(true)});
}

/***** Class *****/

string Class::toString(bool with_t) const {
	return "Class(" + name + "," + parent_name + "," + fields.toString(with_t) + "," + methods.toString(with_t) + ")";
}

void Class::codegen(Program& p, LLVMHelper& h) {
	// Init
	llvm::Function* f = h.module->getFunction(name + "__init");

	llvm::BasicBlock* entry_block = llvm::BasicBlock::Create(*h.context, "", f);
	h.builder->SetInsertPoint(entry_block);

	// Call parent's initializer
	if (parent)
		h.builder->CreateCall(
			h.module->getFunction(parent->name + "__init"),
			{h.builder->CreatePointerCast(
				f->arg_begin(),
				parent->getType(h)->getPointerTo()
			)}
		);

	// Initialize the fields
	for (shared_ptr<Field>& field: fields) {
		field->codegen(p, h);

		if (not isUnit(field->getType()))
			h.builder->CreateStore(
				field->getValue(),
				h.builder->CreateStructGEP(
					f->arg_begin(),
					fields_table[field->name]->idx
				)
			);
	}

	h.builder->CreateRetVoid();

	// New
	f = h.module->getFunction(name + "__new");

	entry_block = llvm::BasicBlock::Create(*h.context, "", f);
	llvm::BasicBlock* init_block = llvm::BasicBlock::Create(*h.context, "init", f);
	llvm::BasicBlock* null_block = llvm::BasicBlock::Create(*h.context, "null", f);

	h.builder->SetInsertPoint(entry_block);

	// Allocation of heap memory
	size_t alloc_size = h.module->getDataLayout().getTypeAllocSize(this->getType(h));
	llvm::Value* memory = h.builder->CreateCall(
		h.module->getOrInsertFunction(
			"malloc",
			llvm::FunctionType::get(
				llvm::Type::getInt8PtrTy(*h.context),
				{llvm::Type::getInt64Ty(*h.context)},
				false
			)
		),
		{llvm::ConstantInt::get(llvm::Type::getInt64Ty(*h.context), alloc_size)}
	);

	// Conditional branching
	h.builder->CreateCondBr(
		h.builder->CreateIsNull(memory),
		null_block,
		init_block
	);

	// Initialization block
	h.builder->SetInsertPoint(init_block);

	llvm::Value* instance = h.builder->CreateBitCast(
		memory,
		this->getType(h)->getPointerTo()
	);

	h.builder->CreateCall(
		h.module->getFunction(name + "__init"),
		{instance}
	);

	h.builder->CreateStore(
		h.module->getNamedValue("vtable." + name), // vtable
		h.builder->CreateStructGEP(instance, 0) // vtable slot
	);

	h.builder->CreateRet(instance);

	// Null block
	h.builder->SetInsertPoint(null_block);
	h.builder->CreateRet(
		llvm::ConstantPointerNull::get(this->getType(h)->getPointerTo())
	);

	// Methods code generation
	methods.codegen(p, h);
}

void Class::declaration(LLVMHelper& h) {
	// Ensure parent is declared
	if (parent and not parent->isDeclared(h))
		parent->declaration(h);

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
		llvm::Type* t = h.asType((*it)->type);

		if (not t) { // invalid type
			h.errors.push_back({this->pos, "unknown type '" + (*it)->type + "' for field " + (*it)->name});
			it = prev(fields.erase(it));
		} else if (fields_table.find((*it)->name) != fields_table.end()) { // field already exists
			h.errors.push_back({(*it)->pos, "redefinition of field " + (*it)->name + " of class " + name});
			it = prev(fields.erase(it));
		} else if (parent and parent->fields_table.find((*it)->name) != parent->fields_table.end()) { // field already exists in parent
			h.errors.push_back({(*it)->pos, "overriding field " + (*it)->name + " of class " + name});
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
			h.errors.push_back({(*it)->pos, "redefinition of method " + (*it)->getName(true)});
			it = prev(methods.erase(it));
		} else {
			(*it)->declaration(h);
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
					h.errors.push_back({(*it)->pos, "overriding method " + m->getName(true) + " with different signature"});
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
	llvm::StructType* vtable_t = llvm::StructType::create(*h.context, this->getStructName() + "VTable");

	// Class structure definition
	vector<llvm::Type*> elements_t;

	elements_t.push_back(vtable_t->getPointerTo()); // vtable slot

	for (auto it: fields_table) {
		llvm::Type* t = h.asType(it.second->type);

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

	for (auto it: methods_table) {
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
		elements[it.second->idx] = (llvm::Constant*) h.builder->CreatePointerCast(f, ft->getPointerTo());
		elements_t[it.second->idx] = ft->getPointerTo();
	}

	vtable_t->setBody(elements_t);

	llvm::GlobalVariable* vtable = new llvm::GlobalVariable(
		*h.module,
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
	llvm::Function* f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, name + "__new", *h.module);

	// Init
	ft = llvm::FunctionType::get(
		llvm::Type::getVoidTy(*h.context),
		{self_t->getPointerTo()},
		false
	);
	f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, name + "__init", *h.module);
	f->arg_begin()->setName("self");
}

/***** Program *****/

string Program::toString(bool with_t) const {
	string str = classes.toString(with_t);
	if (not functions.empty())
		str += "," + functions.toString(with_t);
	return str;
}

void Program::codegen(Program& p, LLVMHelper& h) {
	// Classes code generation
	classes.codegen(p, h);

	// Functions code generation
	functions.codegen(p, h);

	// Main
	if (functions_table.find("main") != functions_table.end()) {
		shared_ptr<Method> m = functions_table["main"];

		if (m->formals.size() != 0 or m->type != "int32")
			h.errors.push_back({m->pos, "function " + m->getName(true) + " declared with wrong signature"});
	} else if (classes_table.find("Main") != classes_table.end()) {
		shared_ptr<Class> c = classes_table["Main"];

		if (c->methods_table.find("main") != c->methods_table.end()) {
			shared_ptr<Method> m = c->methods_table["main"];

			if (m->formals.size() == 0 and m->type == "int32") { // main() : int32
				llvm::FunctionType* ft = llvm::FunctionType::get(
					h.asType("int32"), {}, false
				);
				llvm::Function* f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "main", *h.module);

				// { (new Main).main() }
				llvm::BasicBlock* entry_block = llvm::BasicBlock::Create(*h.context, "", f);
				h.builder->SetInsertPoint(entry_block);

				h.builder->CreateRet(
					Call(
						new New("Main"), "main", {}
					)._codegen(p, h)
				);
			} else
				h.errors.push_back({m->pos, "method " + m->getName(true) + " declared with wrong signature"});
		} else
			h.errors.push_back({c->pos, "undeclared method main in class Main"});
	} else
		h.errors.push_back({this->pos, "undeclared class Main"});
}

void Program::declaration(LLVMHelper& h) {
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

	classes_table["Object"]->getType(h); // class forward declaration

	// Classes redefinition and overriding
	int size;
	do {
		size = classes_table.size();

		for (auto it = classes.begin(); it != classes.end(); ++it)
			if ((*it)->parent) // class has already been processed
				continue;
			else if (classes_table.find((*it)->name) != classes_table.end()) { // class already exists
				h.errors.push_back({(*it)->pos, "redefinition of class " + (*it)->name});
				it = classes.erase(it);
			} else if (classes_table.find((*it)->parent_name) != classes_table.end()) { // class parent exists
				classes_table[(*it)->name] = *it;
				(*it)->parent = classes_table[(*it)->parent_name].get();
				(*it)->getType(h); // class forward declaration
			}

	} while (size < classes_table.size());

	for (auto it = classes.begin(); it != classes.end(); ++it)
		if ((*it)->parent) // class has been processed
			(*it)->declaration(h);
		else {
			h.errors.push_back({(*it)->pos, "class " + (*it)->name + " cannot extend class " + (*it)->parent_name});
			it = prev(classes.erase(it));
		}

	// Functions redefinition
	for (auto it = functions.begin(); it != functions.end(); ++it) {
		(*it)->declaration(h);

		llvm::Function* f = (*it)->getFunction(h);

		if (not f) // invalid function
			it = prev(functions.erase(it));
		else if (functions_table.find((*it)->name) != functions_table.end()) { // function already exists
			h.errors.push_back({(*it)->pos, "redefinition of function " + (*it)->getName(true)});
			f->eraseFromParent();
			it = prev(functions.erase(it));
		} else
			functions_table[(*it)->name] = *it;
	}
}

/***** If *****/

string If::_toString(bool with_t) const {
	string str = "If(" + cond->toString(with_t) + "," + then->toString(with_t);
	if (els)
		str += "," + els->toString(with_t);
	return str + ")";
}

llvm::Value* If::_codegen(Program& p, LLVMHelper& h) {
	cond->codegen(p, h);
	llvm::Type* cond_t = cond->getType();

	if (not isBoolean(cond_t))
		h.errors.push_back({cond->pos, "expected type 'bool', but got condition of type '" + asString(cond_t) + "'"});

	llvm::Function* f = h.builder->GetInsertBlock()->getParent();

	// If blocks
	llvm::BasicBlock* then_block = llvm::BasicBlock::Create(*h.context, "then", f);
	llvm::BasicBlock* else_block = llvm::BasicBlock::Create(*h.context, "else", f);
	llvm::BasicBlock* end_block = llvm::BasicBlock::Create(*h.context, "end", f);

	// Conditional branching
	h.builder->CreateCondBr(
		isBoolean(cond_t) ? cond->getValue() : llvm::ConstantInt::getFalse(*h.context),
		then_block,
		else_block
	);

	// Then block
	h.builder->SetInsertPoint(then_block);
	then->codegen(p, h);
	llvm::BasicBlock* then_bis = h.builder->GetInsertBlock();

	// Else block
	h.builder->SetInsertPoint(else_block);
	if (els)
		els->codegen(p, h);
	llvm::BasicBlock* else_bis = h.builder->GetInsertBlock();

	// Return type
	llvm::Value* then_val = then->getValue();
	llvm::Value* else_val = els ? els->getValue() : nullptr;

	llvm::Type* then_t = then_val ? then_val->getType() : nullptr;
	llvm::Type* else_t = else_val ? else_val->getType() : nullptr;
	llvm::Type* end_t = nullptr;

	if (isSameAs(then_t, else_t))
		end_t = then_t;
	else if (isNumeric(then_t) and isNumeric(else_t))
		end_t = h.asType("double");
	else if (isClass(then_t) and isClass(else_t))
		end_t = p.commonAncestor(asString(then_t), asString(else_t))->getType(h)->getPointerTo();
	else if (not isUnit(then_t) and not isUnit(else_t))
		h.errors.push_back({this->pos, "expected agreeing branch types, but got types '" + asString(then_t) + "' and '" + asString(else_t) + "'"});

	// Then block
	h.builder->SetInsertPoint(then_bis);
	if (not isUnit(end_t) and not isSameAs(then_t, end_t))
		then_val = castToTargetTy(p, h, then_val, end_t);
	h.builder->CreateBr(end_block);

	// Else block
	h.builder->SetInsertPoint(else_bis);
	if (not isUnit(end_t) and not isSameAs(else_t, end_t))
		else_val = castToTargetTy(p, h, else_val, end_t);
	h.builder->CreateBr(end_block);

	// End block
	h.builder->SetInsertPoint(end_block);

	if (not isUnit(end_t)) {
		auto* phi = h.builder->CreatePHI(end_t, 2);
		phi->addIncoming(then_val, then_bis);
		phi->addIncoming(else_val, else_bis);

		return phi;
	}

	return nullptr;
}

/***** While *****/

string While::_toString(bool with_t) const {
	return "While(" + cond->toString(with_t) + "," + body->toString(with_t) + ")";
}

llvm::Value* While::_codegen(Program& p, LLVMHelper& h) {
	llvm::Function* f = h.builder->GetInsertBlock()->getParent();

	// While blocks
	llvm::BasicBlock* cond_block = llvm::BasicBlock::Create(*h.context, "cond", f);
	llvm::BasicBlock* body_block = llvm::BasicBlock::Create(*h.context, "body", f);
	llvm::BasicBlock* exit_block = llvm::BasicBlock::Create(*h.context, "exit", f);

	// (-ext) Push break point
	h.exits.push_back(exit_block);

	h.builder->CreateBr(cond_block);

	// Cond block
	h.builder->SetInsertPoint(cond_block);

	cond->codegen(p, h);
	llvm::Type* cond_t = cond->getType();

	if (not isBoolean(cond_t))
		h.errors.push_back({cond->pos, "expected type 'bool', but got condition of type '" + asString(cond_t) + "'"});

	// Conditional branching
	h.builder->CreateCondBr(
		isBoolean(cond_t) ? cond->getValue() : llvm::ConstantInt::getFalse(*h.context),
		body_block,
		exit_block
	);

	// Body block
	h.builder->SetInsertPoint(body_block);

	body->codegen(p, h); // don't care about the type

	h.builder->CreateBr(cond_block);

	// Exit block
	h.builder->SetInsertPoint(exit_block);

	// (-ext) Pop break point
	h.exits.pop_back();

	return nullptr;
}

/***** Break *****/

llvm::Value* Break::_codegen(Program& p, LLVMHelper& h) {
	if (not h.exits.empty()) {
		// Jump to exit branch
		h.builder->CreateBr(h.exits.back());

		// Dump all following instructions in unreachable block
		h.builder->SetInsertPoint(
			llvm::BasicBlock::Create(*h.context, "unreachable", h.exits.back()->getParent())
		);
	} else
		h.errors.push_back({this->pos, "'break' instruction not in loop"});

	return nullptr;
}

/***** For *****/

string For::_toString(bool with_t) const {
	return "For(" + name + "," + first->toString(with_t) + "," + last->toString(with_t) + "," + body->toString(with_t) + ")";
}

llvm::Value* For::_codegen(Program& p, LLVMHelper& h) {
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
	)._codegen(p, h);
}

/***** Let *****/

string Let::_toString(bool with_t) const {
	string str = "Let(" + name + "," + type + ",";
	if (init)
		str += init->toString(with_t) + ",";
	return str + scope->toString(with_t) + ")";
}

llvm::Value* Let::_codegen(Program& p, LLVMHelper& h) {
	llvm::Type* let_t = h.asType(type);

	if (let_t) {
		llvm::Value* casted = nullptr;

		if (init) {
			init->codegen(p, h);

			if (isUnit(init->getType()) and isUnit(let_t));
			else {
				casted = castToTargetTy(p, h, init->getValue(), let_t);

				if (not casted)
					h.errors.push_back({init->pos, "expected type '" + type + "', but got initializer of type '" + asString(init->getType()) + "'"});
			}
		}

		// Allocate and store variable
		h.alloc(name, let_t);
		h.store(name, casted ? casted : h.defaultValue(let_t));
	} else
		h.errors.push_back({this->pos, "unknown type '" + type + "'"});

	scope->codegen(p, h);

	// Remove variable from scope
	h.pop(name);

	return scope->getValue();
}

/***** Lets *****/

string Lets::_toString(bool with_t) const {
	return "Lets(" + fields.toString(with_t) + "," + scope->toString(with_t) + ")";
}

llvm::Value* Lets::_codegen(Program& p, LLVMHelper& h) {
	shared_ptr<Expr> x = scope;

	for (auto it = fields.rbegin(); it != fields.rend(); ++it)
		x = make_shared<Let>((*it)->name, (*it)->type, (*it)->init, x); // recursive

	return x->_codegen(p, h);
}

/***** Assign *****/

string Assign::_toString(bool with_t) const {
	return "Assign(" + name + "," + value->toString(with_t) + ")";
}

llvm::Value* Assign::_codegen(Program& p, LLVMHelper& h) {
	value->codegen(p, h);

	// Search self's fields
	llvm::Type* self_t = h.getType("self");
	shared_ptr<Class> c = self_t ? p.classes_table[asString(self_t)] : nullptr;

	// Get target type
	llvm::Type* target_t = nullptr;

	if (h.contains(name))
		target_t = h.getType(name);
	else if (c and c->fields_table.find(name) != c->fields_table.end())
		target_t = c->fields_table[name]->getType();
	else {
		h.errors.push_back({this->pos, "assignation to undeclared identifier " + name});
		return nullptr;
	}

	// Cast value to target type
	llvm::Value* casted = nullptr;

	if (isUnit(value->getType()) and isUnit(target_t));
	else {
		casted = castToTargetTy(p, h, value->getValue(), target_t);

		if (not casted) {
			h.errors.push_back({value->pos, "expected type '" + asString(target_t) + "', but got r-value of type '" + asString(value->getType()) + "'"});
			return nullptr;
		}
	}

	// Store casted value
	if (h.contains(name))
		h.store(name, casted);
	else if (not isUnit(target_t))
		h.builder->CreateStore(
			casted,
			h.builder->CreateStructGEP(
				h.load("self"),
				c->fields_table[name]->idx
			)
		);

	return casted;
}

/***** Unary *****/

string Unary::_toString(bool with_t) const {
	string str = "UnOp(";
	switch (type) {
		case NOT: str += "not"; break;
		case MINUS: str += "-"; break;
		case ISNULL: str += "isnull"; break;
	}
	return str + "," + value->toString(with_t) + ")";
}

llvm::Value* Unary::_codegen(Program& p, LLVMHelper& h) {
	value->codegen(p, h);
	llvm::Type* value_t = value->getType();

	llvm::Value* out = nullptr;
	string expected;

	switch (type) {
		case NOT:
			if (isBoolean(value_t))
				return h.builder->CreateNot(value->getValue());
			else {
				out = h.defaultValue("bool");
				expected = "bool";
			}
			break;
		case MINUS:
			if (isNumeric(value_t))
				return h.builder->CreateNeg(value->getValue());
			else {
				out = h.defaultValue("int32");
				expected = "int32 or double";
			}
			break;
		case ISNULL:
			if (isClass(value_t)) return h.builder->CreateIsNull(value->getValue());
			else {
				out = h.defaultValue("bool");
				expected = "Object";
			}
	}

	h.errors.push_back({value->pos, "expected type '" + expected + "', but got operand of type '" + asString(value_t) + "'"});

	return out;
}

/***** Binary *****/

string Binary::_toString(bool with_t) const {
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

llvm::Value* Binary::_codegen(Program& p, LLVMHelper& h) {
	llvm::Value* out = nullptr;
	string expected;

	switch (type) {
		case AND:
			return If(left, right, make_shared<Boolean>(false))._codegen(p, h);
		case OR:
			return If(left, make_shared<Boolean>(true), right)._codegen(p, h);
		case NEQUAL:
			return Unary(Unary::NOT, new Binary(EQUAL, left, right))._codegen(p, h);
		default:
			left->codegen(p, h);
			right->codegen(p, h);

			llvm::Type* left_t = left->getType();
			llvm::Type* right_t = right->getType();

			switch (type) {
				case EQUAL:
					if (isSameAs(left_t, right_t)) {
						if (isString(left_t)) {
							llvm::Value* comp = h.builder->CreateCall(
								h.module->getOrInsertFunction(
									"strcmp",
									llvm::FunctionType::get(
										h.asType("int32"),
										{
											llvm::Type::getInt8PtrTy(*h.context),
											llvm::Type::getInt8PtrTy(*h.context),
										},
										false
									)
								),
								{left->getValue(), right->getValue()}
							);

							return h.builder->CreateICmpEQ(comp, h.defaultValue("int32"));
						} else if (isUnit(left_t))
							return llvm::ConstantInt::getTrue(*h.context);
						else if (isReal(left_t))
							return h.builder->CreateFCmpOEQ(left->getValue(), right->getValue());
						else
							return h.builder->CreateICmpEQ(left->getValue(), right->getValue());
					} else if (isNumeric(left_t) and isNumeric(right_t))
						return h.builder->CreateFCmpOEQ(
							h.numericCast(left->getValue()),
							h.numericCast(right->getValue())
						);
					else if (isClass(left_t) and isClass(right_t)) {
						llvm::Type* comm_t = p.commonAncestor(asString(left_t), asString(right_t))->getType(h)->getPointerTo();
						return h.builder->CreateICmpEQ( // cast pointers to same type for address comparison
							castToTargetTy(p, h, left->getValue(), comm_t),
							castToTargetTy(p, h, right->getValue(), comm_t)
						);
					} else
						h.errors.push_back({this->pos, "expected agreeing operand types, but got types '" + asString(left_t) + "' and '" + asString(right_t) + "'"});

					return h.defaultValue("bool");
				default:
					if (isInteger(left_t) and isInteger(right_t)) {
						switch (type) {
							case LOWER: return h.builder->CreateICmpSLT(left->getValue(), right->getValue());
							case LOWER_EQUAL: return h.builder->CreateICmpSLE(left->getValue(), right->getValue());
							case GREATER: return h.builder->CreateICmpSGT(left->getValue(), right->getValue());
							case GREATER_EQUAL: return h.builder->CreateICmpSGE(left->getValue(), right->getValue());
							case PLUS: return h.builder->CreateAdd(left->getValue(), right->getValue());
							case MINUS: return h.builder->CreateSub(left->getValue(), right->getValue());
							case TIMES: return h.builder->CreateMul(left->getValue(), right->getValue());
							case DIV: return h.builder->CreateSDiv(left->getValue(), right->getValue());
							case POW:
								return h.builder->CreateFPToSI(
									h.builder->CreateCall(
										h.module->getOrInsertFunction(
											"llvm.powi.f64",
											llvm::FunctionType::get(
												h.asType("double"),
												{
													h.asType("double"),
													h.asType("int32"),
												},
												false
											)
										),
										{
											h.builder->CreateSIToFP(
												left->getValue(),
												h.asType("double")
											),
											right->getValue()
										}
									),
									h.asType("int32")
								);
							case MOD: return h.builder->CreateSRem(left->getValue(), right->getValue());
							default: break;
						}
					} else if (isNumeric(left_t) and isNumeric(right_t)) {
						llvm::Value* left_bis = h.numericCast(left->getValue());
						llvm::Value* right_bis = h.numericCast(right->getValue());

						switch (type) {
							case LOWER: return h.builder->CreateFCmpOLT(left_bis, right_bis);
							case LOWER_EQUAL: return h.builder->CreateFCmpOLE(left_bis, right_bis);
							case GREATER: return h.builder->CreateFCmpOGT(left_bis, right_bis);
							case GREATER_EQUAL: return h.builder->CreateFCmpOGE(left_bis, right_bis);
							case PLUS: return h.builder->CreateFAdd(left_bis, right_bis);
							case MINUS: return h.builder->CreateFSub(left_bis, right_bis);
							case TIMES: return h.builder->CreateFMul(left_bis, right_bis);
							case DIV: return h.builder->CreateFDiv(left_bis, right_bis);
							case POW:
								return h.builder->CreateCall(
									h.module->getOrInsertFunction(
										"llvm.pow.f64",
										llvm::FunctionType::get(
											h.asType("double"),
											{
												h.asType("double"),
												h.asType("double"),
											},
											false
										)
									),
									{left_bis, right_bis}
								);
							case MOD: return h.builder->CreateFRem(left_bis, right_bis);
							default: break;
						}
					} else {
						switch (type) {
							case LOWER:
							case LOWER_EQUAL:
							case GREATER:
							case GREATER_EQUAL:
								out = h.defaultValue("bool");
								break;
							default:
								out = h.defaultValue("int32");
						}
						expected = "int32 or double";
					}
			}

			h.errors.push_back({this->pos, "expected type '" + expected + "', but got operand of types '" + asString(left_t) + "' and '" + asString(right_t) + "'"});
	}

	return out;
}

/***** Call *****/

string Call::_toString(bool with_t) const { // improvement -> replace 'self' by
	return "Call(" + scope->toString(with_t) + "," + name + "," + args.toString(with_t) + ")";
}

llvm::Value* Call::_codegen(Program& p, LLVMHelper& h) {
	scope->codegen(p, h);
	llvm::Type* scope_t = scope->getType();

	args.codegen(p, h);

	if (isUnit(scope_t) or isClass(scope_t)) {
		llvm::Function* f = nullptr;
		llvm::FunctionType* ft = nullptr;
		vector<llvm::Value*> params;

		if (isUnit(scope_t) and p.functions_table.find(name) != p.functions_table.end()) { // top-level function
			f = p.functions_table[name]->getFunction(h);
			ft = f->getFunctionType();
		} else if (not isUnit(scope_t) or h.contains("self")) {
			llvm::Value* obj = isUnit(scope_t) ? h.load("self") : scope->getValue();

			shared_ptr<Class> c = p.classes_table[asString(obj->getType())];

			if (c->methods_table.find(name) != c->methods_table.end()) { // class method
				f = (llvm::Function*) h.builder->CreateLoad(
					h.builder->CreateStructGEP(
						h.builder->CreateLoad(
							h.builder->CreateStructGEP(obj, 0)
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
			int nump = ft->getNumParams();

			// Compare call with signature
			if (args.size() + align >= nump) {
				bool valid = true;

				for (int i = 0; i < args.size(); ++i) {
					llvm::Type* arg_t = args[i]->getType();
					llvm::Type* param_t = i + align < nump ? ft->getParamType(i + align) : arg_t;

					if (isSameAs(arg_t, param_t))
						params.push_back(args[i]->getValue());
					else if (isNumeric(arg_t) and isNumeric(param_t))
						params.push_back(h.numericCast(args[i]->getValue(), param_t));
					else if (p.isSubclassOf(asString(arg_t), asString(param_t)))
						params.push_back(
							h.builder->CreatePointerCast(
								args[i]->getValue(),
								param_t
							)
						);
					else {
						h.errors.push_back({args[i]->pos, "expected type '" + asString(param_t) + "', but got argument of type '" + asString(arg_t) + "'"});
						valid = false;
					}
				}

				if (valid) { // all arguments have the expected type
					if (params.size() == nump or (ft->isVarArg() and params.size() > nump))
						return h.builder->CreateCall(f, params);
					else
						h.errors.push_back({this->pos, "call to method " + name + " with too many arguments"});
				}
			} else
				h.errors.push_back({this->pos, "call to method " + name + " with too few arguments"});
		} else
			h.errors.push_back({this->pos, "call to undeclared method " + name});
	} else
		h.errors.push_back({scope->pos, "expected object type, but got scope of type '" + asString(scope_t) + "'"});

	return nullptr;
}


/***** New *****/

string New::_toString(bool with_t) const {
	return "New(" + type + ")";
}

llvm::Value* New::_codegen(Program& p, LLVMHelper& h) {
	llvm::Function* f = h.module->getFunction(type + "__new");

	if (not f) {
		h.errors.push_back({this->pos, "new instance of unknown object type '" + type + "'"});
		return nullptr;
	}

	return h.builder->CreateCall(f, {});
}

/***** Identifier *****/

string Identifier::_toString(bool with_t) const {
	return id;
}

llvm::Value* Identifier::_codegen(Program& p, LLVMHelper& h) {
	// Load from scope
	if (h.contains(id))
		return h.load(id);

	// Search self's fields
	llvm::Type* self_t = h.getType("self");
	shared_ptr<Class> c = self_t ? p.classes_table[asString(self_t)] : nullptr;

	if (c and c->fields_table.find(id) != c->fields_table.end())
		return h.builder->CreateLoad(
			h.builder->CreateStructGEP(
				h.load("self"),
				c->fields_table[id]->idx
			)
		); // self->id

	h.errors.push_back({this->pos, "undeclared identifier " + id});

	return nullptr;
}

/***** Integer *****/

string Integer::_toString(bool with_t) const {
	return std::to_string(value);
}

llvm::Value* Integer::_codegen(Program& p, LLVMHelper& h) {
	return llvm::ConstantInt::get(h.asType("int32"), value);
}

/***** Real *****/

string Real::_toString(bool with_t) const {
	return std::to_string(value);
}

llvm::Value* Real::_codegen(Program& p, LLVMHelper& h) {
	return llvm::ConstantFP::get(h.asType("double"), value);
}

/***** Boolean ****/

string Boolean::_toString(bool with_t) const {
	return b ? "true" : "false";
}

llvm::Value* Boolean::_codegen(Program& p, LLVMHelper& h) {
	return llvm::ConstantInt::get(h.asType("bool"), b);
}

/***** String *****/

string String::_toString(bool with_t) const {
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

llvm::Value* String::_codegen(Program& p, LLVMHelper& h) {
	return h.builder->CreateGlobalStringPtr(str, "str");
}

/***** Unit *****/

string Unit::_toString(bool with_t) const {
	return "()";
}

llvm::Value* Unit::_codegen(Program& p, LLVMHelper& h) {
	return nullptr;
}
