//#include "llvm/ADT/STLExtras.h"
#include <algorithm>
#include<map>
#include<cstdio>
#include<cctype>
#include<string>
#include <cstddef>
#include <vector>
#include <memory>

//Lexer 
enum Token
{
	tok_eof = -1,
	tok_def = -2,
	tok_extern = -3,

	tok_identifier = -4,
	tok_number = -5
};

static std::string IdentifierStr;
static double NumVal;

// returns the next token from standard input
static int gettok(){
	static int LastChar = ' ';

	// skipping whitespace
	while (isspace(LastChar))
		LastChar = getchar();

	while (isalpha(LastChar)){
		IdentifierStr = LastChar;
		while (isalnum(LastChar = getchar()))
			IdentifierStr += LastChar;

		if (IdentifierStr == "def")
			return tok_def;

		if (IdentifierStr == "extern")
			return tok_extern;
		return tok_identifier;
	}

	if (isdigit(LastChar) || LastChar == '.'){
		std::string NumStr;
		do {
			NumStr += LastChar;
			LastChar = getchar();
		} while (isdigit(LastChar) || LastChar == '.');

		NumVal = strtod(NumStr.c_str(), nullptr);
		return tok_number; 
	}
	if (LastChar == '#'){
		do {
			LastChar = getchar();
		} while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');
		
		if (LastChar != EOF)
			return gettok();
	}

	if (LastChar == EOF)
		return tok_eof;

	
	int ThisChar = LastChar;
	LastChar = getchar();
	return ThisChar;

}

// AST

//Base class for all expression nodes
class ExprAST{
public:
	//virtual ~ExprAST();	
};

class NumberExprAST : public ExprAST {
	double Val;
public:
	NumberExprAST(double val) : Val(val) {}
	//~NumberExprAST();	
};

class VariableExprAST : public ExprAST {
	std::string Name;
public:
	VariableExprAST(const std::string &Name) : Name(Name) {}
	//~VariableExprAST();
};

class BinaryExprAST : public ExprAST {
	char Op;
	std::unique_ptr<ExprAST> LHS, RHS;
public:
	BinaryExprAST(char Op, std::unique_ptr<ExprAST> LHS,
					std::unique_ptr<ExprAST> RHS ) 
	: Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}
	//~BinaryExprAST();	
};

class CallExprAST : public ExprAST {
	std::string Callee;
	std::vector<std::unique_ptr<ExprAST>> Args;
public:
	CallExprAST(const std::string &Callee, 
		std::vector<std::unique_ptr<ExprAST>> Args) 
	: Callee(Callee), Args(std::move(Args)) {}
	//~CallExprAST();	
};

class PrototypeAST {
	std::string Name;
	std::vector<std::string> Args;
public:
	PrototypeAST(const std::string &Name, std::vector<std::string> Args)
		: Name(Name), Args(std::move(Args)) {}
	const std::string &getName() const {return Name;}
	//~PrototypeAST();
};


class FunctionAST {
	std::unique_ptr<PrototypeAST> Proto;
	std::unique_ptr<ExprAST> Body;
public:
	FunctionAST(std::unique_ptr<PrototypeAST> Proto, 
				std::unique_ptr<ExprAST> Body) 
	: Proto(std::move(Proto)), Body(std::move(Body)) {}
	//~FunctionAST();
};


// Parser

static int CurTok;
static int getNextToken(){
	return CurTok = gettok();
}

std::unique_ptr<ExprAST> LogError(const char *str){
	fprintf(stderr, "LogError: %s\n", str);
	return nullptr;
}
std::unique_ptr<PrototypeAST> LogErrorP(const char *str){
	LogError(str);
	return nullptr;
}


static std::map<char, int> BinopPrecedence;

static int GetTokPrecedence(){
	if (!isascii(CurTok))
		return -1;

	int tokPrec = BinopPrecedence[CurTok];
	if (tokPrec < 0) return -1;
	return tokPrec;
}

static std::unique_ptr<ExprAST> ParseNumberExpr() {
	auto Result = std::make_unique<NumberExprAST>(NumVal);
	getNextToken();
	return std::move(Result);
}

static std::unique_ptr<ExprAST> ParseExpression();

static std::unique_ptr<ExprAST> ParseParenExpr(){
	getNextToken(); // consumes '('
	auto V = ParseExpression();
	if (!V)
		return nullptr;

	if (CurTok != ')')
		return LogError("expected ')'");
	getNextToken(); // consumes ')'
	return V;
}


static std::unique_ptr<ExprAST> ParseIdentifierExpr(){
	std::string IdName = IdentifierStr;
	getNextToken();

	if (CurTok != '(')
		return std::make_unique<VariableExprAST>(IdName);
	getNextToken();
	std::vector<std::unique_ptr<ExprAST>> Args;
	if (CurTok != ')'){
		while(true){
			if (auto Arg = ParseExpression())
				Args.push_back(std::move(Arg));
			else
				return nullptr;

			if (CurTok == ')')
				break;

			if (CurTok != ',')	{
				return LogError("Expected ')' or ',' in arg list");
			getNextToken();
				
			}
		}
	}
	getNextToken();
	return std::make_unique<CallExprAST>(IdName, std::move(Args));
}


static std::unique_ptr<ExprAST> ParsePrimary(){
	switch (CurTok){
		default :
			return LogError("unknown token expectin expr");
 		case tok_identifier:
 			return ParseIdentifierExpr();
 		case tok_number :
 			return ParseNumberExpr();
 		case '(':
 			return ParseParenExpr();
	}
}


static std::unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec,
	std::unique_ptr<ExprAST> LHS){

	while(true){
		int tokPrec = GetTokPrecedence();

		if (tokPrec < ExprPrec)
			return LHS;

		int binOp = CurTok;
		getNextToken();

		auto RHS = ParsePrimary();
		if (!RHS)
			return nullptr;

		int nextPrec = GetTokPrecedence();
		if (tokPrec < nextPrec){
			RHS = ParseBinOpRHS(tokPrec + 1, std::move(RHS));
			if (!RHS)
				return nullptr;
		}
		// Merging LHS and RHS
		LHS = std::make_unique<BinaryExprAST>(binOp, std::move(LHS),
			std::move(RHS));
	}
}

static std::unique_ptr<ExprAST> ParseExpression(){
	auto LHS = ParsePrimary();
	if (!LHS)
		return nullptr;
	return ParseBinOpRHS(0, std::move(LHS));
}
// definition  id ( expr )
static std::unique_ptr<PrototypeAST> ParsePrototype(){
	if (CurTok != tok_identifier)
		return LogErrorP("Expected function name in prototype");
	std::string name = IdentifierStr;
	getNextToken();

	if (CurTok != '(')
		return LogErrorP("Expected '(' in prottotype");
	std::vector<std::string> argNames;

	while(CurTok == tok_identifier){
		argNames.push_back(IdentifierStr);	
		getNextToken();
	}

	if (CurTok != '(')
		return LogErrorP("Expected ')' in prottotype ");
	getNextToken(); // eats ')'

	return std::make_unique<PrototypeAST>(std::move(name), std::move(argNames));
}



// definition ::= 'def' prottotype expression
static std::unique_ptr<FunctionAST> ParseDefinition(){
	getNextToken(); // eats 'def'
	auto proto = ParsePrototype();
	if (!proto)
		return nullptr;
	if (auto e = ParseExpression())
		return std::make_unique<FunctionAST>(std::move(proto), std::move(e));
	else
		return nullptr;
}

static std::unique_ptr<FunctionAST> ParseTopLevelExpr(){
	if (auto e = ParseExpression()){
		auto proto = std::make_unique<PrototypeAST>("__anon_expr",
			std::vector<std::string>());
		return std::make_unique<FunctionAST>(std::move(proto),
			std::move(e));
	}

	return nullptr;
}

static std::unique_ptr<PrototypeAST> ParseExtern(){
	getNextToken(); // eats extern
	return ParsePrototype();
}

// Top level parsing

static void HandleDefinition(){
	if (ParseDefinition())	{
		fprintf(stderr, "Parsed a function definition\n");
	} else{
		getNextToken();
	}

}

static void HandleExtern(){
	if (ParseExtern()){
		fprintf(stderr, "Parsed an extern\n");
	} else {
		getNextToken();
	}

}

static void HandleTopLevelExpression(){
	if (ParseTopLevelExpr()){
		fprintf(stderr, "Parsed a top-level expression\n");
	} else {
		getNextToken();
	}

}

static void Mainloop(){
	while(true){
		fprintf(stderr, "ready>" );
		switch(CurTok){
			case tok_eof:
				return;
			case ';':
				getNextToken();
				break;
			case tok_def:
				HandleDefinition();
				break;
			case tok_extern:
				HandleExtern();
				break;
			default :
				HandleTopLevelExpression();
				break;
		}
	}
}

// This is the main driver code 

int main(int argc, char** argv){
	
	BinopPrecedence['<'] = 10;
	BinopPrecedence['+'] = 20;
	BinopPrecedence['-'] = 20;
	BinopPrecedence['*'] = 40;

	// Get the first token
	fprintf(stderr, "ready>" );
	getNextToken();
	Mainloop();
	
	return 0;
}