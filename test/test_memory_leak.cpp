#include "TestBase.hpp"
#include "common/test_common.h"

namespace {

void test_memory_leak() {
  TEST_BEGIN("test_memory_leak");
  new float[1024L*1024*200];
  TEST_END();
}

} // namespace

int main() {
  // App::Platform::initPlatformEnvironment("daw");
  test_memory_leak();
  return 0;
}
