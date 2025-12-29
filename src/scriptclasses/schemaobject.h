#pragma once
#include "../object_wrap.h"
#include "entityhandle.h"

class SchemaObject : public ObjectWrap {
public:
    static void Init();
    static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
    explicit SchemaObject(int _num)
        : num(_num)
    {}
private:
    
    ~SchemaObject();

    static void GetOffset(const v8::FunctionCallbackInfo<v8::Value>& args);
	static void GetValue(const v8::FunctionCallbackInfo<v8::Value>& args);

    int num;
    static inline v8::Global<v8::ObjectTemplate> _objectTemplate;
    static inline bool initialized = false;
};