#include "TestBase.hpp"
#include <vector>
#include "compiler.h"
#include "saferef.h"


namespace test_saferef {
class SomeObject;
DAW_CXX_CONSTINIT thread_local SafeRefStorage<SomeObject>* safeRefs;
class SomeObject {
    int32_t m_id;
public:
    SafeRef<SomeObject> safeRef;
    explicit SomeObject(int32_t id) : m_id(id) {
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
            safeRef = /* SafeRef<guibase> */{ storage->safeRefCreate(this), storage };
        }
        return safeRef;
    }
    String getClassName() const {
        return "SomeObject " + std::to_string(m_id);
    }
};
}// namespace
template<>
void SafeRefStorage<test_saferef::SomeObject>::onPreDestroy() {
    size_t numRefsLeaked = 0;
    std::vector<String> someNames;
    for (auto& ref : refs) {
        if (ref->ptr) {
            log_lf(Log::L_WARN, "%s leaked\n", ref->ptr->getClassName().c_str());
            ref->ptr->safeRef.handler = nullptr;
            ref->ptr = nullptr;
            numRefsLeaked++;
        }
    }
    if (numRefsLeaked > 0) {
        log_lf(Log::L_WARN, "%zu refs leaked\n", numRefsLeaked);
    }
}
namespace test_saferef {
    void testSafeRef() {
        TEST_BEGIN("testSafeRef");
        test_saferef::safeRefs = new SafeRefStorage<test_saferef::SomeObject>();
            std::vector<SomeObject*> objects;
            std::vector<SafeRef<SomeObject>> objectsSafeRefs;
            std::vector<SafeRef<SomeObject>> objectsSafeRefs2;
            for (size_t i = 0; i < 100; ++i) {
                auto obj = new SomeObject(i);
                objects.push_back(obj);
                objectsSafeRefs.push_back(obj->toRef());
            }
            for (size_t i = 0; i < 100; ++i) {
                auto obj = safeRefGet(objectsSafeRefs[i]);
                TEST_ASSERT_THROW(obj == objects[i]);
                objectsSafeRefs2.push_back(obj->toRef());
            }
            for (size_t i = 0; i < 100; ++i) {
                TEST_ASSERT_THROW(objectsSafeRefs[i] == objectsSafeRefs2[i]);
            }
            for (size_t i = 0; i < 99; ++i) {
                delete objects[i];
            }
            auto last = objects[99];
            objects.clear();

            for (size_t i = 0; i < 99; ++i) {
                auto obj = safeRefGet(objectsSafeRefs[i]);
                TEST_ASSERT_THROW(obj == nullptr);
            }
            TEST_ASSERT_THROW(safeRefGet(objectsSafeRefs[99]) != nullptr);
            delete last;
        test_saferef::safeRefs->onPreDestroy();
        delete test_saferef::safeRefs;
        TEST_END();
    }

}// namespace
int main() {
    test_saferef::testSafeRef();
    return 0;
}
