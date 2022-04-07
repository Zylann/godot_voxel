#ifndef ZYLANN_EXPRESSION_PARSER_H
#define ZYLANN_EXPRESSION_PARSER_H

#include "fixed_array.h"
#include "memory.h"
#include "span.h"
#include <string_view>

namespace zylann {
namespace ExpressionParser {

struct Node {
	enum Type { //
		NUMBER,
		VARIABLE,
		OPERATOR,
		FUNCTION,
		TYPE_COUNT,
		INVALID
	};

	Type type = INVALID;

	virtual ~Node() {}
};

struct NumberNode : Node {
	float value;

	NumberNode(float p_value) : value(p_value) {
		type = Node::NUMBER;
	}
};

struct VariableNode : Node {
	std::string_view name;

	VariableNode(std::string_view p_name) : name(p_name) {
		type = Node::VARIABLE;
	}
};

struct OperatorNode : Node {
	enum Operation { //
		ADD,
		SUBTRACT,
		MULTIPLY,
		DIVIDE,
		POWER,
		OP_COUNT
	};

	Operation op;
	UniquePtr<Node> n0;
	UniquePtr<Node> n1;

	OperatorNode(Operation p_op, UniquePtr<Node> a, UniquePtr<Node> b) : op(p_op), n0(std::move(a)), n1(std::move(b)) {
		type = OPERATOR;
	}
};

struct FunctionNode : Node {
	unsigned int function_id;
	FixedArray<UniquePtr<Node>, 4> args;

	FunctionNode() {
		type = Node::FUNCTION;
	}
};

enum ErrorID { //
	ERROR_NONE,
	ERROR_INVALID,
	ERROR_UNEXPECTED_END,
	ERROR_INVALID_NUMBER,
	ERROR_INVALID_TOKEN,
	ERROR_UNEXPECTED_TOKEN,
	ERROR_UNKNOWN_FUNCTION,
	ERROR_EXPECTED_ARGUMENT,
	ERROR_TOO_FEW_ARGUMENTS,
	ERROR_TOO_MANY_ARGUMENTS,
	ERROR_UNCLOSED_PARENTHESIS,
	ERROR_MISSING_OPERAND_ARGUMENTS,
	ERROR_MULTIPLE_OPERANDS,
	ERROR_COUNT
};

struct Error {
	ErrorID id = ERROR_NONE;
	std::string_view symbol;
	unsigned int position = 0;
};

struct Result {
	UniquePtr<Node> root;
	Error error;
};

typedef float (*FunctionCallback)(Span<const float>);

struct Function {
	std::string_view name;
	unsigned int argument_count = 0;
	unsigned int id = 0;
	FunctionCallback func = nullptr;
};

// TODO `text` should be `const`
Result parse(std::string_view text, Span<const Function> functions);
bool is_tree_equal(const Node &root_a, const Node &root_b, Span<const Function> functions);
std::string tree_to_string(const Node &node, Span<const Function> functions);
std::string to_string(const Error error);
void find_variables(const Node &node, std::vector<std::string_view> &variables);

// TODO Just use indices in the span? Or pointers?
inline const Function *find_function_by_id(unsigned int id, Span<const Function> functions) {
	for (unsigned int i = 0; i < functions.size(); ++i) {
		const Function &f = functions[i];
		if (f.id == id) {
			return &f;
		}
	}
	return nullptr;
}

} // namespace ExpressionParser
} // namespace zylann

#endif // ZYLANN_EXPRESSION_PARSER_H
