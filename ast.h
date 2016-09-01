#ifndef _WGTCC_AST_H_
#define _WGTCC_AST_H_

#include "error.h"
#include "string_pair.h"
#include "token.h"
#include "type.h"

#include <cassert>
#include <list>
#include <memory>
#include <string>


class Visitor;
template<typename T> class Evaluator;
class AddrEvaluator;
class Generator;

class Scope;
class Parser;
class ASTNode;
class Token;
class TokenSequence;

//Expression
class Expr;
class BinaryOp;
class UnaryOp;
class ConditionalOp;
class FuncCall;
class TempVar;
class Constant;

class Identifier;
class Object;
class Enumerator;

//statement
class Stmt;
class IfStmt;
class JumpStmt;
class LabelStmt;
class EmptyStmt;
class CompoundStmt;
class FuncDef;
class TranslationUnit;


/*
 * AST Node
 */

class ASTNode
{
public:
  virtual ~ASTNode() {}
  
  virtual void Accept(Visitor* v) = 0;

protected:
  ASTNode() {}

  MemPool* pool_ {nullptr};
};

typedef ASTNode ExtDecl;


/*********** Statement *************/

class Stmt : public ASTNode
{
public:
  virtual ~Stmt() {}

protected:
   Stmt() {}
};


struct Initializer
{
  int offset_;
  Type* type_;
  Expr* expr_;

  bool operator<(const Initializer& rhs) const {
    return offset_ < rhs.offset_;
  }
};

class Declaration: public Stmt
{
  template<typename T> friend class Evaluator;
  friend class AddrEvaluator;
  friend class Generator;
  
  typedef std::set<Initializer> InitList;

public:
  static Declaration* New(Object* obj);

  virtual ~Declaration() {}

  virtual void Accept(Visitor* v);

  InitList& Inits() {
    return inits_;
  }

  //StaticInitList StaticInits() {
  //    return _staticInits;
  //}

  Object* Obj() {
    return obj_;
  }

  void AddInit(Initializer init);

protected:
  Declaration(Object* obj): obj_(obj) {}

  Object* obj_;
  InitList inits_;
};


class EmptyStmt : public Stmt
{
  template<typename T> friend class Evaluator;
  friend class AddrEvaluator;
  friend class Generator;

public:
  static EmptyStmt* New();

  virtual ~EmptyStmt() {}
  
  virtual void Accept(Visitor* v);

protected:
  EmptyStmt() {}
};


// 构建此类的目的在于，在目标代码生成的时候，能够生成相应的label
class LabelStmt : public Stmt
{
  template<typename T> friend class Evaluator;
  friend class AddrEvaluator;
  friend class Generator;
public:
  static LabelStmt* New();

  ~LabelStmt() {}
  
  virtual void Accept(Visitor* v);
  
  std::string Label() const {
    return ".L" + std::to_string(tag_);
  }

protected:
  LabelStmt(): tag_(GenTag()) {}

private:
  static int GenTag() {
    static int tag = 0;
    return ++tag;
  }
  
  int tag_; // 使用整型的tag值，而不直接用字符串
};


class IfStmt : public Stmt
{
  template<typename T> friend class Evaluator;
  friend class AddrEvaluator;
  friend class Generator;
public:
  static IfStmt* New(Expr* cond, Stmt* then, Stmt* els=nullptr);

  virtual ~IfStmt() {}
  
  virtual void Accept(Visitor* v);

protected:
  IfStmt(Expr* cond, Stmt* then, Stmt* els = nullptr)
      : cond_(cond), then_(then), else_(els) {}

private:
  Expr* cond_;
  Stmt* then_;
  Stmt* else_;
};


class JumpStmt : public Stmt
{
  template<typename T> friend class Evaluator;
  friend class AddrEvaluator;
  friend class Generator;

public:
  static JumpStmt* New(LabelStmt* label);

  virtual ~JumpStmt() {}
  
  virtual void Accept(Visitor* v);
  
  void SetLabel(LabelStmt* label) {
    label_ = label;
  }

protected:
  JumpStmt(LabelStmt* label): label_(label) {}

private:
  LabelStmt* label_;
};


class ReturnStmt: public Stmt
{
  template<typename T> friend class Evaluator;
  friend class AddrEvaluator;
  friend class Generator;

public:
  static ReturnStmt* New(Expr* expr);

  virtual ~ReturnStmt() {}
  
  virtual void Accept(Visitor* v);

protected:
  ReturnStmt(::Expr* expr): expr_(expr) {}

private:
  ::Expr* expr_;
};


typedef std::list<Stmt*> StmtList;

class CompoundStmt : public Stmt
{
  template<typename T> friend class Evaluator;
  friend class AddrEvaluator;
  friend class Generator;

public:
  static CompoundStmt* New(StmtList& stmts, ::Scope* scope=nullptr);

  virtual ~CompoundStmt() {}
  
  virtual void Accept(Visitor* v);

  StmtList& Stmts() {
    return stmts_;
  }

  ::Scope* Scope() {
    return scope_;
  }

protected:
  CompoundStmt(const StmtList& stmts, ::Scope* scope=nullptr)
      : stmts_(stmts), scope_(scope) {}

private:
  StmtList stmts_;
  ::Scope* scope_;
};


/********** Expr ************/
/*
 * Expr
 *      BinaryOp
 *      UnaryOp
 *      ConditionalOp
 *      FuncCall
 *      Constant
 *      Identifier
 *          Object
 *      TempVar
 */

class Expr : public Stmt
{
  template<typename T> friend class Evaluator;
  friend class AddrEvaluator;
  friend class Generator;
  friend class LValGenerator;

public:
  virtual ~Expr() {}
  
  ::Type* Type() {
    return type_;
  }

  virtual bool IsLVal() = 0;

  virtual void TypeChecking() = 0;

  const Token* Tok() const {
    return tok_;
  }

  void SetTok(const Token* tok) {
    tok_ = tok;
  }

  static Expr* MayCast(Expr* expr);
  static Expr* MayCast(Expr* expr, ::Type* desType);

protected:
  /*
   * You can construct a expression without specifying a type,
   * then the type should be evaluated in TypeChecking()
   */
  Expr(const Token* tok, ::Type* type): tok_(tok), type_(type) {}

  const Token* tok_;
  ::Type* type_;
};


/***********************************************************
'+', '-', '*', '/', '%', '<', '>', '<<', '>>', '|', '&', '^'
'=',(复合赋值运算符被拆分为两个运算)
'==', '!=', '<=', '>=',
'&&', '||'
'['(下标运算符), '.'(成员运算符)
','(逗号运算符),
*************************************************************/
class BinaryOp : public Expr
{
  template<typename T> friend class Evaluator;
  friend class AddrEvaluator;
  friend class Generator;
  friend class LValGenerator;
  friend class Declaration;

public:
  static BinaryOp* New(const Token* tok, Expr* lhs, Expr* rhs);

  static BinaryOp* New(const Token* tok, int op, Expr* lhs, Expr* rhs);

  virtual ~BinaryOp() {}
  
  virtual void Accept(Visitor* v);
  
  //like member ref operator is a lvalue
  virtual bool IsLVal() {
    switch (op_) {
    case '.':
    case ']': return !Type()->ToArray();
    default: return false;
    }
  }

  ArithmType* Promote();

  virtual void TypeChecking();
  void SubScriptingOpTypeChecking();
  void MemberRefOpTypeChecking();
  void MultiOpTypeChecking();
  void AdditiveOpTypeChecking();
  void ShiftOpTypeChecking();
  void RelationalOpTypeChecking();
  void EqualityOpTypeChecking();
  void BitwiseOpTypeChecking();
  void LogicalOpTypeChecking();
  void AssignOpTypeChecking();
  void CommaOpTypeChecking();
  
protected:
  BinaryOp(const Token* tok, int op, Expr* lhs, Expr* rhs)
      : Expr(tok, nullptr), op_(op) {
        lhs_ = lhs, rhs_ = rhs;
        if (op != '.') {
          lhs_ = MayCast(lhs);
          rhs_ = MayCast(rhs);
        }
      }

  int op_;
  Expr* lhs_;
  Expr* rhs_;
};


/*
 * Unary Operator:
 * '++' (prefix/postfix)
 * '--' (prefix/postfix)
 * '&'  (ADDR)
 * '*'  (DEREF)
 * '+'  (PLUS)
 * '-'  (MINUS)
 * '~'
 * '!'
 * CAST // like (int)3
 */
class UnaryOp : public Expr
{
  template<typename T> friend class Evaluator;
  friend class AddrEvaluator;
  friend class Generator;
  friend class LValGenerator;

public:
  static UnaryOp* New(int op, Expr* operand, ::Type* type=nullptr);

  virtual ~UnaryOp() {}
  
  virtual void Accept(Visitor* v);

  //TODO: like '*p' is lvalue, but '~i' is not lvalue
  virtual bool IsLVal();

  ArithmType* Promote();

  void TypeChecking();
  void IncDecOpTypeChecking();
  void AddrOpTypeChecking();
  void DerefOpTypeChecking();
  void UnaryArithmOpTypeChecking();
  void CastOpTypeChecking();

protected:
  UnaryOp(int op, Expr* operand, ::Type* type = nullptr)
    : Expr(operand->Tok(), type), op_(op) {
      operand_ = operand;
      if (op_ != Token::CAST && op_ != Token::ADDR) {
        operand_ = MayCast(operand);
      }
    }

  int op_;
  Expr* operand_;
};


// cond ? true ： false
class ConditionalOp : public Expr
{
  template<typename T> friend class Evaluator;
  friend class AddrEvaluator;
  friend class Generator;

public:
  static ConditionalOp* New(const Token* tok,
      Expr* cond, Expr* exprTrue, Expr* exprFalse);
  
  virtual ~ConditionalOp() {}
  
  virtual void Accept(Visitor* v);

  virtual bool IsLVal() {
    return false;
  }

  ArithmType* Promote();
  
  virtual void TypeChecking();

protected:
  ConditionalOp(Expr* cond, Expr* exprTrue, Expr* exprFalse)
      : Expr(cond->Tok(), nullptr), cond_(MayCast(cond)),
        exprTrue_(MayCast(exprTrue)), exprFalse_(MayCast(exprFalse)) {}

private:
  Expr* cond_;
  Expr* exprTrue_;
  Expr* exprFalse_;
};


/************** Function Call ****************/
class FuncCall : public Expr
{
  template<typename T> friend class Evaluator;
  friend class AddrEvaluator;
  friend class Generator;

public:        
  typedef std::vector<Expr*> ArgList;

public:
  static FuncCall* New(Expr* designator, const ArgList& args);

  ~FuncCall() {}
  
  virtual void Accept(Visitor* v);

  //a function call is ofcourse not lvalue
  virtual bool IsLVal() {
    return false;
  }

  ArgList* Args() {
    return &args_;
  }

  Expr* Designator() {
    return designator_;
  }

  const std::string& Name() const {
    return tok_->str_;
  }

  ::FuncType* FuncType() {
    return designator_->Type()->ToFunc();
  }

  virtual void TypeChecking();

protected:
  FuncCall(Expr* designator, const ArgList& args)
    : Expr(designator->Tok(), nullptr),
      designator_(designator), args_(args) {}

  Expr* designator_;
  ArgList args_;
};


class Constant: public Expr
{
  template<typename T> friend class Evaluator;
  friend class AddrEvaluator;
  friend class Generator;

public:
  static Constant* New(const Token* tok, int tag, long val);
  static Constant* New(const Token* tok, int tag, double val);
  static Constant* New(const Token* tok, int tag, const std::string* val);

  ~Constant() {}
  
  virtual void Accept(Visitor* v);

  virtual bool IsLVal() {
    return false;
  }

  virtual void TypeChecking() {}

  long IVal() const {
    return ival_;
  }

  double FVal() const {
    return fval_;
  }

  const std::string* SVal() const {
    return sval_;
  }

  std::string SValRepr() const;

  std::string Label() const {
    return std::string(".LC") + std::to_string(id_);
  }

protected:
  Constant(const Token* tok, ::Type* type, long val)
      : Expr(tok, type), ival_(val) {}
  Constant(const Token* tok, ::Type* type, double val)
      : Expr(tok, type), fval_(val) {}
  Constant(const Token* tok, ::Type* type, const std::string* val)
      : Expr(tok, type), sval_(val) {}

  union {
    long ival_;
    double fval_;
    struct {
      long id_;
      const std::string* sval_;
    };
  };
};


//临时变量
class TempVar : public Expr
{
  template<typename T> friend class Evaluator;
  friend class AddrEvaluator;
  friend class Generator;

public:
  static TempVar* New(::Type* type);

  virtual ~TempVar() {}
  
  virtual void Accept(Visitor* v);
  
  virtual bool IsLVal() {
    return true;
  }

  virtual void TypeChecking() {}

protected:
  TempVar(::Type* type): Expr(nullptr, type), tag_(GenTag()) {}
  
private:
  static int GenTag() {
    static int tag = 0;
    return ++tag;
  }

  int tag_;
};


enum Linkage {
  L_NONE,
  L_EXTERNAL,
  L_INTERNAL,
};


class Identifier: public Expr
{
  template<typename T> friend class Evaluator;
  friend class AddrEvaluator;
  friend class Generator;
  friend class LValGenerator;

public:
  static Identifier* New(const Token* tok, ::Type* type, Linkage linkage);

  virtual ~Identifier() {}

  virtual void Accept(Visitor* v);

  virtual bool IsLVal() {
    return false;
  }

  virtual Object* ToObject() {
    return nullptr;
  }

  virtual Enumerator* ToEnumerator() {
    return nullptr;
  }

  /*
   * An identifer can be:
   *     object, sturct/union/enum tag, typedef name, function, label.
   */
   Identifier* ToTypeName() {
    // A typename has no linkage
    // And a function has external or internal linkage
    if (ToObject() || ToEnumerator() || _linkage != L_NONE)
      return nullptr;
    return this;
  }


  virtual const std::string& Name() const {
    return tok_->str_;
  }

  /*
  ::Scope* Scope() {
    return scope_;
  }
  */

  enum Linkage Linkage() const {
    return _linkage;
  }

  void SetLinkage(enum Linkage linkage) {
    _linkage = linkage;
  }

  /*
  virtual bool operator==(const Identifier& other) const {
    return Name() == other.Name()
      && *type_ == *other.type_
  }
  */

  virtual void TypeChecking() {}

protected:
  Identifier(const Token* tok, ::Type* type, enum Linkage linkage)
      : Expr(tok, type), _linkage(linkage) {}
  
  /*
  // An identifier has property scope
  ::Scope* scope_;
  */
  // An identifier has property linkage
  enum Linkage _linkage;
};


class Enumerator: public Identifier
{
  template<typename T> friend class Evaluator;
  friend class AddrEvaluator;
  friend class Generator;

public:
  static Enumerator* New(const Token* tok, int val);

  virtual ~Enumerator() {}

  virtual void Accept(Visitor* v);

  virtual Enumerator* ToEnumerator() {
    return this;
  }

  int Val() const {
    return _cons->IVal();
  }

protected:
  Enumerator(const Token* tok, int val)
      : Identifier(tok, ArithmType::New(T_INT), L_NONE),
        _cons(Constant::New(tok, T_INT, (long)val)) {}

  Constant* _cons;
};


class Object : public Identifier
{
  template<typename T> friend class Evaluator;
  friend class AddrEvaluator;
  friend class Generator;
  friend class LValGenerator;

public:
  static Object* New(const Token* tok, ::Type* type,
      int storage=0, enum Linkage linkage=L_NONE,
      unsigned char bitFieldBegin=0, unsigned char bitFieldWidth=0);

  ~Object() {}

  virtual void Accept(Visitor* v);
  
  virtual Object* ToObject() {
    return this;
  }

  virtual bool IsLVal() {
    // TODO(wgtdkp): not all object is lval?
    return true;
  }

  bool IsStatic() const {
    return (Storage() & S_STATIC) || (Linkage() != L_NONE);
  }

  int Storage() const {
    return storage_;
  }

  void SetStorage(int storage) {
    storage_ = storage;
  }

  int Offset() const {
    return offset_;
  }

  void SetOffset(int offset) {
    offset_ = offset;
  }

  Declaration* Decl() {
    return decl_;
  }

  void SetDecl(Declaration* decl) {
    decl_ = decl;
  }

  unsigned char BitFieldBegin() const {
    return bitFieldBegin_;
  }

  unsigned char BitFieldEnd() const {
    return bitFieldBegin_ + bitFieldWidth_;
  }

  unsigned char BitFieldWidth() const {
    return bitFieldWidth_;
  }

  static unsigned long BitFieldMask(Object* bitField) {
    return BitFieldMask(bitField->bitFieldBegin_, bitField->bitFieldWidth_);
  }

  static unsigned long BitFieldMask(unsigned char begin, unsigned char width) {
    auto end = begin + width;
    return ((0xFFFFFFFFFFFFFFFFUL << (64 - end)) >> (64 - width)) << begin;
  }


  bool HasInit() const {
    return decl_ && decl_->Inits().size();
  }

  bool Anonymous() const {
    return anonymous_;
  }

  void SetAnonymous(bool anonymous) {
    anonymous_ = anonymous;
  }

  virtual const std::string& Name() const {
    /*
    if (Anonymous()) {
      static auto anonyName = "anonymous<"
          + std::to_string(reinterpret_cast<unsigned long long>(this))
          + ">";
      return anonyName;
    }
    */
    return Identifier::Name();
  }

  /*
  bool operator==(const Object& other) const {
    // TODO(wgtdkp): Not implemented
    assert(false);
  }

  bool operator!=(const Object& other) const {
    return !(*this == other);
  }
  */
protected:
  Object(const Token* tok, ::Type* type,
         int storage=0, enum Linkage linkage=L_NONE,
         unsigned char bitFieldBegin=0, unsigned char bitFieldWidth=0)
      : Identifier(tok, type, linkage),
        storage_(storage), offset_(0), decl_(nullptr),
        bitFieldBegin_(bitFieldBegin), bitFieldWidth_(bitFieldWidth),
        anonymous_(false) {}

private:
  int storage_;
  
  // For code gen
  int offset_;

  Declaration* decl_;

  unsigned char bitFieldBegin_;
  // 0 means it's not a bitfield
  unsigned char bitFieldWidth_;

  bool anonymous_;
  //static size_t _labelId {0};
};



/*************** Declaration ******************/

class FuncDef : public ExtDecl
{
  template<typename T> friend class Evaluator;
  friend class AddrEvaluator;
  friend class Generator;

public:
  typedef std::vector<Object*> ParamList;
  
public:
  static FuncDef* New(Identifier* ident, LabelStmt* retLabel);

  virtual ~FuncDef() {}
  
  virtual FuncType* Type() {
    return ident_->Type()->ToFunc();
  }

  ParamList& Params() {
    return params_;
  }

  CompoundStmt* Body() {
    return body_;
  }

  void SetBody(CompoundStmt* body) {
    body_ = body;
  }

  std::string Name() const {
    return ident_->Name();
  }

  enum Linkage Linkage() {
    return ident_->Linkage();
  }
  
  virtual void Accept(Visitor* v);

protected:
  FuncDef(Identifier* ident, LabelStmt* retLabel)
      : ident_(ident), retLabel_(retLabel) {}

private:
  Identifier* ident_;
  LabelStmt* retLabel_;
  ParamList params_;
  CompoundStmt* body_;
};


class TranslationUnit : public ASTNode
{
  template<typename T> friend class Evaluator;
  friend class AddrEvaluator;
  friend class Generator;

public:
  static TranslationUnit* New() {
    return new TranslationUnit();
  }

  virtual ~TranslationUnit() {}

  virtual void Accept(Visitor* v);
  
  void Add(ExtDecl* extDecl) {
    extDecls_.push_back(extDecl);
  }

  std::list<ExtDecl*>& ExtDecls() {
    return extDecls_;
  }

private:
  TranslationUnit() {}

  std::list<ExtDecl*> extDecls_;
};


#endif
