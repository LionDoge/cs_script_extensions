#include "schemaobject.h"

void SchemaObject::Init()
{
	auto isolate = v8::Isolate::GetCurrent();
	v8::HandleScope handleScope(isolate);
	auto context = isolate->GetCurrentContext();

	auto schemaObjTemplate = v8::ObjectTemplate::New(isolate);
	schemaObjTemplate->SetInternalFieldCount(1);
	schemaObjTemplate->Set(isolate, "GetValue", v8::FunctionTemplate::New(isolate, GetValue));

	SchemaObject::_objectTemplate.Reset(isolate, schemaObjTemplate);
}

SchemaObject::~SchemaObject()
{
}

void SchemaObject::New(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    if(!initialized)
    {
        Init();
        initialized = true;
	}
    v8::Isolate* isolate = args.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();

    if (args.Length() < 1 || !args[0]->IsNumber()) {
        isolate->ThrowException(v8::Exception::TypeError(
            v8::String::NewFromUtf8Literal(isolate,
                "Expected a number argument")));
        return;
    }

    // Create object from template
    v8::Local<v8::ObjectTemplate> tpl =
        v8::Local<v8::ObjectTemplate>::New(isolate, SchemaObject::_objectTemplate);
    v8::Local<v8::Object> instance =
        tpl->NewInstance(context).ToLocalChecked();

    // Create C++ object and wrap it
    int value = args[0]->Int32Value(context).FromMaybe(0);
	SchemaObject* obj = new SchemaObject(value);
    obj->Wrap(instance);

    args.GetReturnValue().Set(instance);
}

void SchemaObject::GetOffset(const v8::FunctionCallbackInfo<v8::Value>& args)
{
}

void SchemaObject::GetValue(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	v8::Isolate* isolate = args.GetIsolate();
	SchemaObject* obj = ObjectWrap::Unwrap<SchemaObject>(args.Holder());
	args.GetReturnValue().Set(v8::Number::New(isolate, obj->num));
}
