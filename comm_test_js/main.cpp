#include <v8/v8.h>
#include <v8/libplatform.h>
#include <comm/dir.h>

#include "factory.hpp"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>


namespace v8 {

    class ArrayBufferAllocator : public v8::ArrayBuffer::Allocator {
    public:
        virtual void* Allocate(size_t length) {
            void* data = AllocateUninitialized(length);
            return data == NULL ? data : memset(data, 0, length);
        }
        virtual void* AllocateUninitialized(size_t length) { return malloc(length); }
        virtual void Free(void* data, size_t) { free(data); }
    };

}

int main()
{
    coid::charstr cwd = coid::directory::get_cwd();
    coid::directory::append_path(cwd, "../../../../../../bin/c5e", false, '/');

    SetDllDirectory(cwd.c_str());

    // Initialize V8.
    //v8::V8::InitializeICU();
    //v8::V8::InitializeExternalStartupData(argv[0]);
    std::unique_ptr<v8::Platform> platform = v8::platform::NewDefaultPlatform();
    v8::V8::InitializePlatform(platform.get());
    v8::V8::Initialize();

    v8::ArrayBufferAllocator allocator;
    v8::Isolate::CreateParams create_params;
    create_params.array_buffer_allocator = &allocator;

    v8::Isolate* iso = v8::Isolate::New(create_params);
    v8::Isolate::Scope v8i(iso);
    v8::HandleScope scope(iso);

    iref<factory> A = factory::get();

    A->initialize();

    int result = A->run();

    return 0;
}