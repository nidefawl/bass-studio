#include "TestBase.hpp"
#include <stdio.h>
#include "segmented-vector.hpp"
namespace TestVector {
    void test_pushBack() {
        TEST_BEGIN("pushBack");
        DAW::SegmentedVector<int> vec;
        TEST_ASSERT_EQUAL(vec.size(), size_t(0));
        vec.push_back(1);
        TEST_ASSERT_EQUAL(vec.size(), size_t(1));
        TEST_ASSERT_EQUAL(vec[0], int(1));
        vec.push_back(2);
        TEST_ASSERT_EQUAL(vec.size(), size_t(2));
        TEST_ASSERT_EQUAL(vec[0], int(1));
        TEST_ASSERT_EQUAL(vec[1], int(2));
        for (int i = 0; i < 1024; ++i) {
            vec.push_back(i);
        }
        TEST_ASSERT_EQUAL(vec.size(), size_t(1026));
        TEST_END();
    }
    template<size_t SEGMENT_SIZE>
    void test_pointer_stability() {
        struct LargeStruct {
            int data[128];
        };
        TEST_BEGIN("test_pointer_stability " + std::to_string(SEGMENT_SIZE));
        DAW::SegmentedVector<LargeStruct, SEGMENT_SIZE> vec;
        vec.push_back(LargeStruct());
        LargeStruct* ptr = &vec[0];
        for (int i = 0; i < SEGMENT_SIZE; ++i) {
            TEST_ASSERT_EQUAL(ptr, &vec[0]);
            vec.push_back(LargeStruct());
        }
        TEST_ASSERT_EQUAL(ptr, &vec[0]);
        TEST_END();
    }
}

int main() {
    TestVector::test_pushBack();
    TestVector::test_pointer_stability<1>();
    TestVector::test_pointer_stability<16>();
    TestVector::test_pointer_stability<128>();
    TestVector::test_pointer_stability<1024>();
    return 0;
}