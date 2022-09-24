#include "expression_parser.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <vector>

namespace zylann {
namespace ExpressionParser {

struct StringView {
	const char *ptr;
	size_t size;
};

struct Token {
	enum Type { //
		NUMBER,
		NAME,
		PLUS,
		MINUS,
		DIVIDE,
		MULTIPLY,
		PARENTHESIS_OPEN,
		PARENTHESIS_CLOSE,
		POWER,
		COMMA,
		COUNT,
		INVALID
	};

	Type type = INVALID;
	union Data {
		// Can't put a std::string_view inside a union, not sure why
		StringView str;
		float number;
	} data;
};

StringView pack(std::string_view text) {
	return StringView{ text.data(), text.size() };
}

std::string_view unpack(StringView sv) {
	return std::string_view(sv.ptr, sv.size);
}

bool is_name_starter(char c) {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

bool is_digit(char c) {
	return (c >= '0' && c <= '9');
}

bool is_name_char(char c) {
	return is_name_starter(c) || is_digit(c);
}

std::string_view get_name(const std::string_view text, unsigned int &pos) {
	unsigned int begin_pos = pos;
	while (pos < text.size()) {
		const char c = text[pos];
		if (!is_name_char(c)) {
			return text.substr(begin_pos, pos - begin_pos);
		}
		++pos;
	}
	return text.substr(begin_pos);
}

bool get_number_token(const std::string_view text, unsigned int &pos, Token &out_token, bool negative) {
	// Integer part
	int64_t n = 0;
	char c = 0;
	while (pos < text.size()) {
		c = text[pos];
		if (!is_digit(c)) {
			break;
		}
		n = n * 10 + (c - '0');
		++pos;
	}
	if (negative) {
		n = -n;
	}

	// Decimal part
	double f;
	bool is_float = false;
	if (c == '.') {
		++pos;
		int64_t d = 0;
		int64_t p = 1;
		while (pos < text.size()) {
			c = text[pos];
			if (!is_digit(c)) {
				break;
			}
			d = d * 10 + (c - '0');
			p *= 10;
			++pos;
		}
		f = n + double(d) / double(p);
		if (negative) {
			f = -f;
		}
		is_float = true;
	}

	if (!is_name_starter(c)) {
		if (is_float) {
			out_token.type = Token::NUMBER;
			out_token.data.number = f;
		} else {
			out_token.type = Token::NUMBER;
			out_token.data.number = n;
		}
		return true;
	}

	return false;
}

class Tokenizer {
public:
	Tokenizer(std::string_view text) : _text(text), _error(ERROR_NONE), _position(0) {}

	bool get_next(Token &out_token) {
		struct CharToken {
			const char character;
			Token::Type type;
		};
		static const CharToken s_char_tokens[] = {
			{ '(', Token::PARENTHESIS_OPEN }, //
			{ ')', Token::PARENTHESIS_CLOSE }, //
			{ ',', Token::COMMA }, //
			{ '+', Token::PLUS }, //
			{ '-', Token::MINUS }, //
			{ '*', Token::MULTIPLY }, //
			{ '/', Token::DIVIDE }, //
			{ '^', Token::POWER }, //
			{ 0, Token::INVALID } //
		};

		while (_position < _text.size()) {
			const char c = _text[_position];

			if (c == ' ' || c == '\t') {
				++_position;
				continue;
			}

			{
				const CharToken *ct = s_char_tokens;
				while (ct->character != 0) {
					if (ct->character == c) {
						Token token;
						token.type = ct->type;
						out_token = token;
						++_position;
						return true;
					}
					++ct;
				}
			}

			if (is_name_starter(c)) {
				Token token;
				token.type = Token::NAME;
				token.data.str = pack(get_name(_text, _position));
				ZN_ASSERT(token.data.str.size != 0);
				out_token = token;
				return true;
			}

			// TODO Unary operator `-`
			/*if (c == '-') {
				++_position;
				if (_position >= _text.size()) {
					_error = ERROR_UNEXPECTED_END;
					return false;
				}
				if (!get_number_token(_text, _position, out_token, true)) {
					_error = ERROR_INVALID_NUMBER;
					return false;
				}
				return true;
			}*/

			if (is_digit(c)) {
				if (!get_number_token(_text, _position, out_token, false)) {
					_error = ERROR_INVALID_NUMBER;
					return false;
				}
				return true;
			}

			_error = ERROR_INVALID_TOKEN;
			return false;
		}

		return false;
	}

	ErrorID get_error() const {
		return _error;
	}

	unsigned int get_position() const {
		return _position;
	}

private:
	std::string_view _text;
	ErrorID _error;
	unsigned int _position;
};

bool as_operator(Token::Type token_type, OperatorNode::Operation &op) {
	switch (token_type) {
		case Token::PLUS:
			op = OperatorNode::ADD;
			return true;
		case Token::MULTIPLY:
			op = OperatorNode::MULTIPLY;
			return true;
		case Token::MINUS:
			op = OperatorNode::SUBTRACT;
			return true;
		case Token::DIVIDE:
			op = OperatorNode::DIVIDE;
			return true;
		case Token::POWER:
			op = OperatorNode::POWER;
			return true;
		default:
			return false;
	}
}

const int MAX_PRECEDENCE = 100;

int get_operator_precedence(OperatorNode::Operation op) {
	switch (op) {
		case OperatorNode::ADD:
		case OperatorNode::SUBTRACT:
			return 1;
		case OperatorNode::MULTIPLY:
		case OperatorNode::DIVIDE:
			return 2;
		case OperatorNode::POWER:
			return 3;
		default:
			ZN_CRASH();
			return 0;
	}
}

struct OpEntry {
	int precedence;
	UniquePtr<OperatorNode> node;
};

template <typename T>
inline T pop(std::vector<T> &stack) {
	ZN_ASSERT(stack.size() != 0);
	T t = std::move(stack.back());
	stack.pop_back();
	return t;
}

unsigned int get_operator_argument_count(OperatorNode::Operation op_type) {
	switch (op_type) {
		case OperatorNode::ADD:
		case OperatorNode::SUBTRACT:
		case OperatorNode::MULTIPLY:
		case OperatorNode::DIVIDE:
		case OperatorNode::POWER:
			return 2;
		default:
			ZN_CRASH();
			return 0;
	}
}

ErrorID pop_expression_operator(std::vector<OpEntry> &operations_stack, std::vector<UniquePtr<Node>> &operand_stack) {
	OpEntry last_op = pop(operations_stack);
	ZN_ASSERT(last_op.node != nullptr);
	ZN_ASSERT(last_op.node->type == Node::OPERATOR);
	UniquePtr<OperatorNode> last_node = std::move(last_op.node);

	const unsigned int argc = get_operator_argument_count(last_node->op);
	ZN_ASSERT(argc >= 1 && argc <= 2);

	if (operand_stack.size() < argc) {
		return ERROR_MISSING_OPERAND_ARGUMENTS;
	}

	if (argc == 1) {
		last_node->n0 = pop(operand_stack);
	} else {
		UniquePtr<Node> right = pop(operand_stack);
		UniquePtr<Node> left = pop(operand_stack);
		last_node->n0 = std::move(left);
		last_node->n1 = std::move(right);
	}

	// Push result back to stack
	operand_stack.push_back(std::move(last_node));

	return ERROR_NONE;
}

bool is_operand(const Token &token) {
	return token.type == Token::NAME || token.type == Token::NUMBER;
}

UniquePtr<Node> operand_to_node(const Token token) {
	switch (token.type) {
		case Token::NUMBER:
			return make_unique_instance<NumberNode>(token.data.number);

		case Token::NAME:
			return make_unique_instance<VariableNode>(unpack(token.data.str));

		default:
			ZN_CRASH_MSG("Token not handled");
			return nullptr;
	}
}

const Function *find_function_by_name(std::string_view name, Span<const Function> functions) {
	for (unsigned int i = 0; i < functions.size(); ++i) {
		const Function &f = functions[i];
		if (f.name == name) {
			return &f;
		}
	}
	return nullptr;
}

Result parse_expression(
		Tokenizer &tokenizer, bool in_argument_list, Span<const Function> functions, Token *out_last_token);

Error parse_function(
		Tokenizer &tokenizer, std::vector<UniquePtr<Node>> &operand_stack, Span<const Function> functions) {
	std::string_view fname;
	{
		// We'll replace the variable with a function call node
		UniquePtr<Node> top = pop(operand_stack);
		ZN_ASSERT(top->type == Node::VARIABLE);
		const VariableNode *node = static_cast<VariableNode *>(top.get());
		fname = node->name;
	}

	const Function *fn = find_function_by_name(fname, functions);
	if (fn == nullptr) {
		Error error;
		error.id = ERROR_UNKNOWN_FUNCTION;
		error.position = tokenizer.get_position();
		error.symbol = fname;
		return error;
	}

	UniquePtr<FunctionNode> fnode = make_unique_instance<FunctionNode>();
	fnode->function_id = fn->id;
	ZN_ASSERT(fn->argument_count < fnode->args.size());

	Token last_token;

	for (unsigned int arg_index = 0; arg_index < fn->argument_count; ++arg_index) {
		Result arg_result = parse_expression(tokenizer, true, functions, &last_token);

		if (arg_result.error.id != ERROR_NONE) {
			return arg_result.error;
		}

		if (arg_result.root == nullptr) {
			Error error;
			error.id = ERROR_EXPECTED_ARGUMENT;
			error.position = tokenizer.get_position();
			error.symbol = fname;
			return error;

		} else if (last_token.type == Token::PARENTHESIS_CLOSE && arg_index + 1 < fn->argument_count) {
			Error error;
			error.id = ERROR_TOO_FEW_ARGUMENTS;
			error.position = tokenizer.get_position();
			error.symbol = fname;
			return error;
		}

		fnode->args[arg_index] = std::move(arg_result.root);
	}

	if (last_token.type != Token::PARENTHESIS_CLOSE) {
		Error error;
		error.id = ERROR_TOO_MANY_ARGUMENTS;
		error.position = tokenizer.get_position();
		error.symbol = fname;
		return error;
	}

	operand_stack.push_back(std::move(fnode));
	return Error();
}

// void free_nodes(std::vector<OpEntry> &operations_stack, std::vector<UniquePtr<Node>> operand_stack) {
// 	operand_stack.clear();
// 	operations_stack.clear();
// }

Result parse_expression(
		Tokenizer &tokenizer, bool in_argument_list, Span<const Function> functions, Token *out_last_token) {
	Token token;

	std::vector<OpEntry> operations_stack;
	std::vector<UniquePtr<Node>> operand_stack;
	int precedence_base = 0;
	bool previous_was_operand = false;

	while (tokenizer.get_next(token)) {
		if (in_argument_list && token.type == Token::COMMA) {
			break;
		}

		bool current_is_operand = false;

		OperatorNode::Operation op_type;
		if (as_operator(token.type, op_type)) {
			OpEntry op;
			op.precedence = precedence_base + get_operator_precedence(op_type);
			// Operands will be assigned when we pop operations from the stack
			op.node = make_unique_instance<OperatorNode>(op_type, nullptr, nullptr);

			while (operations_stack.size() > 0) {
				const OpEntry &last_op = operations_stack.back();
				// While the current operator has lower precedence, pop last operand
				if (op.precedence <= last_op.precedence) {
					const ErrorID err = pop_expression_operator(operations_stack, operand_stack);
					if (err != ERROR_NONE) {
						Result result;
						result.error.id = err;
						result.error.position = tokenizer.get_position();
						return result;
					}
				} else {
					break;
				}
			}

			operations_stack.push_back(std::move(op));

		} else if (is_operand(token)) {
			if (previous_was_operand) {
				Result result;
				result.error.id = ERROR_MULTIPLE_OPERANDS;
				result.error.position = tokenizer.get_position();
				return result;
			}
			operand_stack.push_back(operand_to_node(token));
			current_is_operand = true;

		} else if (token.type == Token::PARENTHESIS_OPEN) {
			if (operand_stack.size() > 0 && operand_stack.back()->type == Node::VARIABLE) {
				Error fn_error = parse_function(tokenizer, operand_stack, functions);
				if (fn_error.id != ERROR_NONE) {
					Result result;
					result.error = fn_error;
					return result;
				}

			} else {
				// Increase precedence for what will go inside parenthesis
				precedence_base += MAX_PRECEDENCE;
			}

		} else if (token.type == Token::PARENTHESIS_CLOSE) {
			if (in_argument_list && precedence_base < MAX_PRECEDENCE) {
				break;
			}
			if (precedence_base < MAX_PRECEDENCE) {
				Result result;
				result.error.id = ERROR_UNEXPECTED_TOKEN;
				result.error.position = tokenizer.get_position();
				return result;
			}
			precedence_base -= MAX_PRECEDENCE;
			ZN_ASSERT(precedence_base >= 0);

		} else {
			Result result;
			result.error.id = ERROR_UNEXPECTED_TOKEN;
			result.error.position = tokenizer.get_position();
			return result;
		}

		previous_was_operand = current_is_operand;
	}

	if (tokenizer.get_error() != ERROR_NONE) {
		Result result;
		result.error.id = tokenizer.get_error();
		result.error.position = tokenizer.get_position();
		return result;
	}

	if (precedence_base != 0) {
		Result result;
		result.error.id = ERROR_UNCLOSED_PARENTHESIS;
		result.error.position = tokenizer.get_position();
		return result;
	}

	if (out_last_token != nullptr) {
		*out_last_token = token;
	}

	// All remaining operations should end up with ascending precedence,
	// so popping them should be correct
	// Note: will not work correctly if precedences are equal
	while (operations_stack.size() > 0) {
		const ErrorID err = pop_expression_operator(operations_stack, operand_stack);
		if (err != ERROR_NONE) {
			Result result;
			result.error.id = err;
			result.error.position = tokenizer.get_position();
			return result;
		}
	}

	Result result;

	ZN_ASSERT(operand_stack.size() <= 1);
	// The stack can be empty if the expression was empty
	if (operand_stack.size() > 0) {
		result.root = std::move(operand_stack.back());
	}

	return result;
}

void find_variables(const Node &node, std::vector<std::string_view> &variables) {
	switch (node.type) {
		case Node::NUMBER:
			break;

		case Node::VARIABLE: {
			const VariableNode &vnode = static_cast<const VariableNode &>(node);
			// A variable can appear multiple times, only get it once
			if (std::find(variables.begin(), variables.end(), vnode.name) == variables.end()) {
				variables.push_back(vnode.name);
			}
		} break;

		case Node::OPERATOR: {
			const OperatorNode &onode = static_cast<const OperatorNode &>(node);
			if (onode.n0 != nullptr) {
				find_variables(*onode.n0, variables);
			}
			if (onode.n1 != nullptr) {
				find_variables(*onode.n1, variables);
			}
		} break;

		case Node::FUNCTION: {
			const FunctionNode &fnode = static_cast<const FunctionNode &>(node);
			for (unsigned int i = 0; i < fnode.args.size(); ++i) {
				if (fnode.args[i] != nullptr) {
					find_variables(*fnode.args[i], variables);
				}
			}
		} break;

		default:
			ZN_CRASH();
	}
}

// Returns true if the passed node is constant (or gets changed into a constant).
// `out_number` is the value of the node if it is constant.
bool precompute_constants(UniquePtr<Node> &node, float &out_number, Span<const Function> functions) {
	ZN_ASSERT(node != nullptr);
	switch (node->type) {
		case Node::NUMBER: {
			const NumberNode *nn = static_cast<NumberNode *>(node.get());
			out_number = nn->value;
			return true;
		}

		case Node::VARIABLE:
			return false;

		case Node::OPERATOR: {
			OperatorNode &onode = static_cast<OperatorNode &>(*node);
			if (onode.n0 != nullptr && onode.n1 != nullptr) {
				float n0;
				float n1;
				const bool constant0 = precompute_constants(onode.n0, n0, functions);
				const bool constant1 = precompute_constants(onode.n1, n1, functions);
				if (constant0 && constant1) {
					switch (onode.op) {
						case OperatorNode::ADD:
							out_number = n0 + n1;
							break;
						case OperatorNode::SUBTRACT:
							out_number = n0 - n1;
							break;
						case OperatorNode::MULTIPLY:
							out_number = n0 * n1;
							break;
						case OperatorNode::DIVIDE:
							out_number = n0 / n1;
							break;
						case OperatorNode::POWER:
							out_number = powf(n0, n1);
							break;
						default:
							ZN_CRASH();
					}

					node = make_unique_instance<NumberNode>(out_number);
					return true;
				}
				// TODO Unary operators
			}
			return false;
		} break;

		case Node::FUNCTION: {
			FunctionNode &fnode = static_cast<FunctionNode &>(*node);
			bool all_constant = true;
			FixedArray<float, 4> constant_args;
			const Function *f = find_function_by_id(fnode.function_id, functions);
			for (unsigned int i = 0; i < f->argument_count; ++i) {
				if (!precompute_constants(fnode.args[i], constant_args[i], functions)) {
					all_constant = false;
				}
			}
			if (all_constant) {
				ZN_ASSERT(f != nullptr);
				ZN_ASSERT(f->func != nullptr);
				out_number = f->func(to_span_const(constant_args, f->argument_count));

				node = make_unique_instance<NumberNode>(out_number);
				return true;
			}
			return false;
		} break;

		default:
			ZN_CRASH();
			return false;
	}
}

Result parse(std::string_view text, Span<const Function> functions) {
	for (unsigned int i = 0; i < functions.size(); ++i) {
		const Function &f = functions[i];
		ZN_ASSERT(f.name != "");
		ZN_ASSERT(f.func != nullptr);
	}
	Tokenizer tokenizer(text);
	Result result = parse_expression(tokenizer, false, functions, nullptr);
	if (result.error.id != ERROR_NONE) {
		return result;
	}
	if (result.root != nullptr) {
		float _;
		precompute_constants(result.root, _, functions);
	}
	return result;
}

bool is_tree_equal(const Node &a, const Node &b, Span<const Function> functions) {
	if (a.type != b.type) {
		return false;
	}
	switch (a.type) {
		case Node::NUMBER: {
			const NumberNode &nn_a = static_cast<const NumberNode &>(a);
			const NumberNode &nn_b = static_cast<const NumberNode &>(b);
			return nn_a.value == nn_b.value;
		}
		case Node::VARIABLE: {
			const VariableNode &va = static_cast<const VariableNode &>(a);
			const VariableNode &vb = static_cast<const VariableNode &>(b);
			return va.name == vb.name;
		}
		case Node::OPERATOR: {
			const OperatorNode &oa = static_cast<const OperatorNode &>(a);
			const OperatorNode &ob = static_cast<const OperatorNode &>(b);
			if (oa.op != ob.op) {
				return false;
			}
			ZN_ASSERT(oa.n0 != nullptr);
			ZN_ASSERT(ob.n0 != nullptr);
			if (oa.n1 == nullptr && ob.n1 == nullptr) {
				return is_tree_equal(*oa.n0, *ob.n0, functions);
			}
			ZN_ASSERT(oa.n1 != nullptr);
			ZN_ASSERT(ob.n1 != nullptr);
			return is_tree_equal(*oa.n0, *ob.n0, functions) && is_tree_equal(*oa.n1, *ob.n1, functions);
		}
		case Node::FUNCTION: {
			const FunctionNode &fa = static_cast<const FunctionNode &>(a);
			const FunctionNode &fb = static_cast<const FunctionNode &>(b);
			if (fa.function_id != fb.function_id) {
				return false;
			}
			const Function *f = find_function_by_id(fa.function_id, functions);
			ZN_ASSERT(f != nullptr);
			for (unsigned int i = 0; i < f->argument_count; ++i) {
				ZN_ASSERT(fa.args[i] != nullptr);
				ZN_ASSERT(fb.args[i] != nullptr);
				if (!is_tree_equal(*fa.args[i], *fb.args[i], functions)) {
					return false;
				}
			}
			return true;
		}
		default:
			ZN_CRASH();
			return false;
	}
}

const char *to_string(OperatorNode::Operation op) {
	switch (op) {
		case OperatorNode::ADD:
			return "+";
		case OperatorNode::SUBTRACT:
			return "-";
		case OperatorNode::MULTIPLY:
			return "*";
		case OperatorNode::DIVIDE:
			return "/";
		case OperatorNode::POWER:
			return "^";
		default:
			ZN_CRASH();
			return "?";
	}
}

void tree_to_string(const Node &node, int depth, std::stringstream &output, Span<const Function> functions) {
	for (int i = 0; i < depth; ++i) {
		output << "  ";
	}
	switch (node.type) {
		case Node::NUMBER: {
			const NumberNode &nn = static_cast<const NumberNode &>(node);
			output << nn.value;
		} break;

		case Node::VARIABLE: {
			const VariableNode &vn = static_cast<const VariableNode &>(node);
			output << vn.name;
		} break;

		case Node::OPERATOR: {
			const OperatorNode &on = static_cast<const OperatorNode &>(node);
			output << to_string(on.op);
			output << '\n';
			ZN_ASSERT(on.n0 != nullptr);
			if (on.n1 == nullptr) {
				tree_to_string(*on.n0, depth + 1, output, functions);
			} else {
				ZN_ASSERT(on.n1 != nullptr);
				tree_to_string(*on.n0, depth + 1, output, functions);
				output << '\n';
				tree_to_string(*on.n1, depth + 1, output, functions);
			}
		} break;

		case Node::FUNCTION: {
			const FunctionNode &fn = static_cast<const FunctionNode &>(node);
			const Function *f = find_function_by_id(fn.function_id, functions);
			ZN_ASSERT(f != nullptr);
			output << f->name << "()";
			for (unsigned int i = 0; i < f->argument_count; ++i) {
				ZN_ASSERT(fn.args[i] != nullptr);
				output << '\n';
				tree_to_string(*fn.args[i], depth + 1, output, functions);
			}
		} break;

		default:
			ZN_CRASH();
	}
}

std::string tree_to_string(const Node &node, Span<const Function> functions) {
	std::stringstream ss;
	tree_to_string(node, 0, ss, functions);
	return ss.str();
}

std::string to_string(const Error error) {
	switch (error.id) {
		case ERROR_NONE:
			return "";
		case ERROR_INVALID:
			return "Invalid expression";
		case ERROR_UNEXPECTED_END:
			return "Unexpected end of expression";
		case ERROR_INVALID_NUMBER:
			return "Invalid number";
		case ERROR_INVALID_TOKEN:
			return "Invalid token";
		case ERROR_UNEXPECTED_TOKEN:
			return "Unexpected token";
		case ERROR_UNKNOWN_FUNCTION: {
			std::string s = "Unknown function: '";
			s += error.symbol;
			s += '\'';
			return s;
		}
		case ERROR_TOO_MANY_ARGUMENTS:
			return "Too many arguments";
		case ERROR_UNCLOSED_PARENTHESIS:
			return "Non-closed parenthesis";
		case ERROR_MISSING_OPERAND_ARGUMENTS:
			return "Missing operand arguments";
		case ERROR_MULTIPLE_OPERANDS:
			return "Multiple operands";
		default:
			return "Unknown error";
	}
}

} //namespace ExpressionParser
} //namespace zylann
