#pragma once
#include "v8.h"
#include "scriptExtensions/userMessageInfo.h"

struct ManagedObjectTest {
    ScriptUserMessageInfo* data;
    v8::Persistent<v8::Object> persistent;

    ManagedObjectTest(v8::Isolate* isolate, v8::Local<v8::Object> obj, ScriptUserMessageInfo* d)
        : data(d), persistent(isolate, obj)
    {
        persistent.SetWeak(
            this,
            &ManagedObjectTest::WeakCallback,
            v8::WeakCallbackType::kParameter
        );
    }

    static void WeakCallback(const v8::WeakCallbackInfo<ManagedObjectTest>& info) {
        ManagedObjectTest* self = info.GetParameter();
        self->persistent.Reset();
        delete self->data;
        delete self;
    }

    ScriptUserMessageInfo* GetData() const {
        return data;
	}
};