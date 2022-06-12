#include "TestBase.hpp"
#include "common/test_common.h"
#include <muParser.h>

namespace {

double MySqr(double a_fVal) {  return a_fVal*a_fVal; }

void test_muparser() {
	try
	{
		double var_a = 1;
		mu::Parser p;
		p.DefineVar("a", &var_a); 
		p.DefineFun("MySqr", MySqr); 
		p.SetExpr("MySqr(a)*_pi+min(10,a)");

		for (std::size_t a=0; a<100; ++a)
		{
			var_a = a;  // Change value of variable a
			std::cout << p.Eval() << std::endl;
		}
	}
	catch (mu::Parser::exception_type &e)
	{
		std::cout << e.GetMsg() << std::endl;
	}
}

} // namespace

int main() {
  test_muparser();
  return 0;
}
