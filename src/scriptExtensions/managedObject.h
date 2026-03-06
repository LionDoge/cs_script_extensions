#pragma once
#include "v8.h"
#include "scriptExtensions/userMessageInfo.h"

template<typename T>
class ManagedObject 
{
public:
    ManagedObject(v8::Isolate* isolate, v8::Local<v8::Object> obj, T* d)
        : data(d), persistent(isolate, obj)
    {
        persistent.SetWeak(
            this,
            &ManagedObject::WeakCallback,
            v8::WeakCallbackType::kParameter
        );
    }

    T* GetData() const {
        return data;
    }

protected:
    static void WeakCallback(const v8::WeakCallbackInfo<ManagedObject>& info) {
        ManagedObject* self = info.GetParameter();
        self->persistent.Reset();
        delete self->data;
        delete self;
    }

    T* data;
    v8::Persistent<v8::Object> persistent;
};