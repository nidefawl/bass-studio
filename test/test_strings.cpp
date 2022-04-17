#include "TestBase.hpp"
#include <limits>
#include "math/vec.h"
#include "str_util.h"
#include "logging.h"

namespace {
    void test_stringify() {
        TEST_BEGIN("test_stringify");
        auto log = [](auto str) {
            log_out("%s\n", StringAsCStr(str));
        };
        log(CppTest::stringify("const char*"));
        String str = "String";
        log(CppTest::stringify(str));
        int a = 13;
        int* p = p;
        log(CppTest::stringify(p));
        log(CppTest::stringify(a));
        float f = 1.0f / 3.0f;
        log(CppTest::stringify(f));
        log(CppTest::stringify(23.0f/7.0f));
        double d = 1.0 / 3.0;
        log(CppTest::stringify(d));
        log(CppTest::stringify(23.0/7.0));
        bool b = false;
        log(CppTest::stringify(b));
        log(CppTest::stringify(true));
        log(CppTest::stringify(false));
        vec2 v={f, f*4.0f};
        log(CppTest::stringify(v));
        ivec2 iv={f, f*4.0f};
        log(CppTest::stringify(iv));
        TEST_END();
    }
    void test_string_format() {
        TEST_BEGIN("test_string_format");
        auto log = [](auto str) {
            log_out("%s\n", StringAsCStr(str));
        };
        log(StringFormat("%.0f", std::numeric_limits<float>::lowest()));
        log(StringFormat("%.0f", std::numeric_limits<float>::max()));
        log(StringFormat("%.0f", std::numeric_limits<float>::min()));
        log(StringFormat("%.0f", std::numeric_limits<float>::quiet_NaN()));
        log(StringFormat("%.0f", std::numeric_limits<float>::infinity()));
        log(StringFormat("%.0f", -std::numeric_limits<float>::infinity()));
        TEST_END();
    }
    void test_append() {
        TEST_BEGIN("test_append");
        String str = "###\n";
        str.insert(0, StringFormat("#define B %zu.0\n", 123));
        str.insert(0, StringFormat("#define A %zu.0\n", 123));
        str.insert(0, "#version 1\n");
        TEST_ASSERT_EQUAL(str, "#version 1\n#define A 123.0\n#define B 123.0\n###\n");
        TEST_END();
    }
}// namespace

int main() {
    test_stringify();
    test_string_format();
    test_append();
    return 0;
}
