#include "test_expression_parser.h"
#include "../util/expression_parser.h"
#include "../util/math/funcs.h"
#include "testing.h"

namespace zylann::tests {

void test_expression_parser() {
	using namespace ExpressionParser;

	{
		Result result = parse("", Span<const Function>());
		ZN_TEST_ASSERT(result.error.id == ERROR_NONE);
		ZN_TEST_ASSERT(result.root == nullptr);
	}
	{
		Result result = parse("   ", Span<const Function>());
		ZN_TEST_ASSERT(result.error.id == ERROR_NONE);
		ZN_TEST_ASSERT(result.root == nullptr);
	}
	{
		Result result = parse("42", Span<const Function>());
		ZN_TEST_ASSERT(result.error.id == ERROR_NONE);
		ZN_TEST_ASSERT(result.root != nullptr);
		ZN_TEST_ASSERT(result.root->type == Node::NUMBER);
		const NumberNode &nn = static_cast<NumberNode &>(*result.root);
		ZN_TEST_ASSERT(Math::is_equal_approx(nn.value, 42.f));
	}
	{
		Result result = parse("()", Span<const Function>());
		ZN_TEST_ASSERT(result.error.id == ERROR_NONE);
		ZN_TEST_ASSERT(result.root == nullptr);
	}
	{
		Result result = parse("((()))", Span<const Function>());
		ZN_TEST_ASSERT(result.error.id == ERROR_NONE);
		ZN_TEST_ASSERT(result.root == nullptr);
	}
	{
		Result result = parse("42)", Span<const Function>());
		ZN_TEST_ASSERT(result.error.id == ERROR_UNEXPECTED_TOKEN);
		ZN_TEST_ASSERT(result.root == nullptr);
	}
	{
		Result result = parse("(42)", Span<const Function>());
		ZN_TEST_ASSERT(result.error.id == ERROR_NONE);
		ZN_TEST_ASSERT(result.root != nullptr);
		ZN_TEST_ASSERT(result.root->type == Node::NUMBER);
		const NumberNode &nn = static_cast<NumberNode &>(*result.root);
		ZN_TEST_ASSERT(Math::is_equal_approx(nn.value, 42.f));
	}
	{
		Result result = parse("(", Span<const Function>());
		ZN_TEST_ASSERT(result.error.id == ERROR_UNCLOSED_PARENTHESIS);
		ZN_TEST_ASSERT(result.root == nullptr);
	}
	{
		Result result = parse("(666", Span<const Function>());
		ZN_TEST_ASSERT(result.error.id == ERROR_UNCLOSED_PARENTHESIS);
		ZN_TEST_ASSERT(result.root == nullptr);
	}
	{
		Result result = parse("1+", Span<const Function>());
		ZN_TEST_ASSERT(result.error.id == ERROR_MISSING_OPERAND_ARGUMENTS);
		ZN_TEST_ASSERT(result.root == nullptr);
	}
	{
		Result result = parse("++", Span<const Function>());
		ZN_TEST_ASSERT(result.error.id == ERROR_MISSING_OPERAND_ARGUMENTS);
		ZN_TEST_ASSERT(result.root == nullptr);
	}
	{
		Result result = parse("1 2 3", Span<const Function>());
		ZN_TEST_ASSERT(result.error.id == ERROR_MULTIPLE_OPERANDS);
		ZN_TEST_ASSERT(result.root == nullptr);
	}
	{
		Result result = parse("???", Span<const Function>());
		ZN_TEST_ASSERT(result.error.id == ERROR_INVALID_TOKEN);
		ZN_TEST_ASSERT(result.root == nullptr);
	}
	{
		Result result = parse("1+2-3*4/5", Span<const Function>());
		ZN_TEST_ASSERT(result.error.id == ERROR_NONE);
		ZN_TEST_ASSERT(result.root != nullptr);
		ZN_TEST_ASSERT(result.root->type == Node::NUMBER);
		const NumberNode &nn = static_cast<NumberNode &>(*result.root);
		ZN_TEST_ASSERT(Math::is_equal_approx(nn.value, 0.6f));
	}
	{
		Result result = parse("1*2-3/4+5", Span<const Function>());
		ZN_TEST_ASSERT(result.error.id == ERROR_NONE);
		ZN_TEST_ASSERT(result.root != nullptr);
		ZN_TEST_ASSERT(result.root->type == Node::NUMBER);
		const NumberNode &nn = static_cast<NumberNode &>(*result.root);
		ZN_TEST_ASSERT(Math::is_equal_approx(nn.value, 6.25f));
	}
	{
		Result result = parse("(5 - 3)^2 + 2.5/(4 + 6)", Span<const Function>());
		ZN_TEST_ASSERT(result.error.id == ERROR_NONE);
		ZN_TEST_ASSERT(result.root != nullptr);
		ZN_TEST_ASSERT(result.root->type == Node::NUMBER);
		const NumberNode &nn = static_cast<NumberNode &>(*result.root);
		ZN_TEST_ASSERT(Math::is_equal_approx(nn.value, 4.25f));
	}
	{
		/*
					-
				   / \
				  /   \
				 /     \
				*       -
			   / \     / \
			  4   ^   c   d
				 / \
				+   2
			   / \
			  a   b
		*/
		UniquePtr<VariableNode> node_a = make_unique_instance<VariableNode>("a");
		UniquePtr<VariableNode> node_b = make_unique_instance<VariableNode>("b");
		UniquePtr<OperatorNode> node_add =
				make_unique_instance<OperatorNode>(OperatorNode::ADD, std::move(node_a), std::move(node_b));
		UniquePtr<NumberNode> node_two = make_unique_instance<NumberNode>(2);
		UniquePtr<OperatorNode> node_power =
				make_unique_instance<OperatorNode>(OperatorNode::POWER, std::move(node_add), std::move(node_two));
		UniquePtr<NumberNode> node_four = make_unique_instance<NumberNode>(4);
		UniquePtr<OperatorNode> node_mul =
				make_unique_instance<OperatorNode>(OperatorNode::MULTIPLY, std::move(node_four), std::move(node_power));
		UniquePtr<VariableNode> node_c = make_unique_instance<VariableNode>("c");
		UniquePtr<VariableNode> node_d = make_unique_instance<VariableNode>("d");
		UniquePtr<OperatorNode> node_sub =
				make_unique_instance<OperatorNode>(OperatorNode::SUBTRACT, std::move(node_c), std::move(node_d));
		UniquePtr<OperatorNode> expected_root =
				make_unique_instance<OperatorNode>(OperatorNode::SUBTRACT, std::move(node_mul), std::move(node_sub));

		Result result = parse("4*(a+b)^2-(c-d)", Span<const Function>());
		ZN_TEST_ASSERT(result.error.id == ERROR_NONE);
		ZN_TEST_ASSERT(result.root != nullptr);
		// {
		// 	const std::string s1 = tree_to_string(*expected_root, Span<const Function>());
		// 	print_line(String(s1.c_str()));
		// 	print_line("---");
		// 	const std::string s2 = tree_to_string(*result.root, Span<const Function>());
		// 	print_line(String(s2.c_str()));
		// }
		ZN_TEST_ASSERT(is_tree_equal(*result.root, *expected_root, Span<const Function>()));
	}
	{
		FixedArray<Function, 2> functions;

		{
			Function f;
			f.name = "sqrt";
			f.id = 0;
			f.argument_count = 1;
			f.func = [](Span<const float> args) { //
				return Math::sqrt(args[0]);
			};
			functions[0] = f;
		}
		{
			Function f;
			f.name = "clamp";
			f.id = 1;
			f.argument_count = 3;
			f.func = [](Span<const float> args) { //
				return math::clamp(args[0], args[1], args[2]);
			};
			functions[1] = f;
		}

		Result result = parse("clamp(sqrt(20 + sqrt(25)), 1, 2.0 * 2.0)", to_span_const(functions));
		ZN_TEST_ASSERT(result.error.id == ERROR_NONE);
		ZN_TEST_ASSERT(result.root != nullptr);
		ZN_TEST_ASSERT(result.root->type == Node::NUMBER);
		const NumberNode &nn = static_cast<NumberNode &>(*result.root);
		ZN_TEST_ASSERT(Math::is_equal_approx(nn.value, 4.f));
	}
	{
		FixedArray<Function, 2> functions;

		const unsigned int F_SIN = 0;
		const unsigned int F_CLAMP = 1;

		{
			Function f;
			f.name = "sin";
			f.id = F_SIN;
			f.argument_count = 1;
			f.func = [](Span<const float> args) { //
				return Math::sin(args[0]);
			};
			functions[0] = f;
		}
		{
			Function f;
			f.name = "clamp";
			f.id = F_CLAMP;
			f.argument_count = 3;
			f.func = [](Span<const float> args) { //
				return math::clamp(args[0], args[1], args[2]);
			};
			functions[1] = f;
		}

		Result result = parse("x+sin(y, clamp(z, 0, 1))", to_span_const(functions));

		ZN_TEST_ASSERT(result.error.id == ERROR_TOO_MANY_ARGUMENTS);
		ZN_TEST_ASSERT(result.root == nullptr);
	}
	{
		FixedArray<Function, 1> functions;

		const unsigned int F_CLAMP = 1;

		{
			Function f;
			f.name = "clamp";
			f.id = F_CLAMP;
			f.argument_count = 3;
			f.func = [](Span<const float> args) { //
				return math::clamp(args[0], args[1], args[2]);
			};
			functions[0] = f;
		}

		Result result = parse("clamp(z,", to_span_const(functions));

		ZN_TEST_ASSERT(result.error.id == ERROR_EXPECTED_ARGUMENT);
		ZN_TEST_ASSERT(result.root == nullptr);
	}
	{
		FixedArray<Function, 1> functions;

		const unsigned int F_CLAMP = 1;

		{
			Function f;
			f.name = "clamp";
			f.id = F_CLAMP;
			f.argument_count = 3;
			f.func = [](Span<const float> args) { //
				return math::clamp(args[0], args[1], args[2]);
			};
			functions[0] = f;
		}

		Result result = parse("clamp(z)", to_span_const(functions));

		ZN_TEST_ASSERT(result.error.id == ERROR_TOO_FEW_ARGUMENTS);
		ZN_TEST_ASSERT(result.root == nullptr);
	}
	{
		FixedArray<Function, 1> functions;

		const unsigned int F_CLAMP = 1;

		{
			Function f;
			f.name = "clamp";
			f.id = F_CLAMP;
			f.argument_count = 3;
			f.func = [](Span<const float> args) { //
				return math::clamp(args[0], args[1], args[2]);
			};
			functions[0] = f;
		}

		Result result = parse("clamp(z,)", to_span_const(functions));

		ZN_TEST_ASSERT(result.error.id == ERROR_EXPECTED_ARGUMENT);
		ZN_TEST_ASSERT(result.root == nullptr);
	}
}

} // namespace zylann::tests
