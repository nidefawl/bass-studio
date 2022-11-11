#include "TestBase.hpp"
#include <vector>
#include "saferef.h"


namespace test_saferef {
class SomeObject;
static thread_local SafeRefStorage<SomeObject> safeRefs;
class SomeObject {
public:
    SafeRef<SomeObject> safeRef;
    SomeObject() {
        toRef();
    }
    ~SomeObject() {
        if (safeRef.handler) {
            safeRef.handler->safeRefDestroy(safeRef.refId);
        }
    }
    SafeRef<SomeObject> toRef() {
        // dbgassert(parentCtrl);
        if (!safeRef.handler) {
            auto& storage = safeRefs;
            safeRef = /* SafeRef<guibase> */{ storage.safeRefCreate(this), &storage };
        }
        return safeRef;
    }
};
    void testSafeRef() {
        TEST_BEGIN("testSafeRef");
        std::vector<SomeObject*> objects;
        std::vector<SafeRef<SomeObject>> objectsSafeRefs;
        for (size_t i = 0; i < 100; ++i) {
            auto obj = new SomeObject();
            objects.push_back(obj);
            objectsSafeRefs.push_back(obj->toRef());
        }
        for (size_t i = 0; i < 100; ++i) {
            auto obj = safeRefGet(objectsSafeRefs[i]);
            TEST_ASSERT_THROW(obj == objects[i]);
        }
        for (SomeObject* obj : objects) {
            delete obj;
        }
        objects.clear();

        for (size_t i = 0; i < 100; ++i) {
            auto obj = safeRefGet(objectsSafeRefs[i]);
            TEST_ASSERT_THROW(obj == nullptr);
        }
        TEST_END();
    }

}// namespace
int main() {
    test_saferef::testSafeRef();
    return 0;
}
