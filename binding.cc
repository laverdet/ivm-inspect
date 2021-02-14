#include <nan.h>
#include <isolated_vm.h>

#include <chrono>
#include <thread>

using namespace Nan;
using v8::Array;
using v8::Context;
using v8::Local;
using v8::Object;
using v8::String;
using v8::Value;

template <class Type>
void Set(v8::Local<v8::Object> target, const char* name, const Type& value) {
	Set(target, New(name).ToLocalChecked(), New(value));
}

template <class Type>
void Set(v8::Local<v8::Object> target, const char* name, v8::Local<Type> value) {
	Nan::Set(target, New(name).ToLocalChecked(), value);
}

//
// `types`
#define IS_TYPE_FUNCTIONS(V) \
	V(External) \
	V(Date) \
	V(ArgumentsObject) \
	V(BigIntObject) \
	V(BooleanObject) \
	V(NumberObject) \
	V(StringObject) \
	V(SymbolObject) \
	V(NativeError) \
	V(RegExp) \
	V(AsyncFunction) \
	V(GeneratorFunction) \
	V(GeneratorObject) \
	V(Promise) \
	V(Map) \
	V(Set) \
	V(MapIterator) \
	V(SetIterator) \
	V(WeakMap) \
	V(WeakSet) \
	V(ArrayBuffer) \
	V(DataView) \
	V(SharedArrayBuffer) \
	V(Proxy) \
	V(ModuleNamespaceObject)

#define V(Type) \
	NAN_METHOD(Is##Type) { \
		info.GetReturnValue().Set(info[0]->Is##Type()); \
	}
	IS_TYPE_FUNCTIONS(V)
#undef V

NAN_METHOD(IsAnyArrayBuffer) {
	info.GetReturnValue().Set(
		info[0]->IsArrayBuffer() || info[0]->IsSharedArrayBuffer());
}

NAN_METHOD(IsBoxedPrimitive) {
	info.GetReturnValue().Set(
		info[0]->IsNumberObject() ||
		info[0]->IsStringObject() ||
		info[0]->IsBooleanObject() ||
		info[0]->IsBigIntObject() ||
		info[0]->IsSymbolObject());
}

//
// `util`
NAN_METHOD(GetConstructorName) {
	v8::Local<Object> object = info[0].As<Object>();
	v8::Local<String> name = object->GetConstructorName();
	info.GetReturnValue().Set(name);
}

NAN_METHOD(GetOwnNonIndexProperties) {
	Local<Context> context = Nan::GetCurrentContext();
	Local<Object> object = info[0].As<Object>();
	Local<Array> properties;

	v8::PropertyFilter filter =
		static_cast<v8::PropertyFilter>(info[1].As<v8::Uint32>()->Value());

	if (!object->GetPropertyNames(
				context, v8::KeyCollectionMode::kOwnOnly,
				filter,
				v8::IndexFilter::kSkipIndices)
					.ToLocal(&properties)) {
		return;
	}
	info.GetReturnValue().Set(properties);
}

NAN_METHOD(GetProxyDetails) {
	// Return undefined if it's not a proxy.
	if (!info[0]->IsProxy())
		return;

	Local<v8::Proxy> proxy = info[0].As<v8::Proxy>();

	// TODO(BridgeAR): Remove the length check as soon as we prohibit access to
	// the util binding layer. It's accessed in the wild and `esm` would break in
	// case the check is removed.
	if (info.Length() == 1 || info[1]->IsTrue()) {
		v8::Local<v8::Value> ret[] = {
			proxy->GetTarget(),
			proxy->GetHandler()
		};

		info.GetReturnValue().Set(v8::Array::New(info.GetIsolate(), ret, 2));
	} else {
		info.GetReturnValue().Set(proxy->GetTarget());
	}
}

NAN_METHOD(PreviewEntries) {
	if (!info[0]->IsObject())
		return;

	auto isolate = v8::Isolate::GetCurrent();
	bool is_key_value;
	Local<Array> entries;
	if (!info[0].As<Object>()->PreviewEntries(&is_key_value).ToLocal(&entries))
		return;
	// Fast path for WeakMap and WeakSet.
	if (info.Length() == 1)
		return info.GetReturnValue().Set(entries);

	Local<Value> ret[] = {
		entries,
		v8::Boolean::New(isolate, is_key_value)
	};
	return info.GetReturnValue().Set(
			Array::New(isolate, ret, 2));
}


NAN_METHOD(binding) {
	auto binding = New<v8::Object>();

	// `buffer`
	auto buffer = New<v8::Object>();
	Set(binding, "buffer", buffer);
	Set(buffer, "kMaxLength", 0);

	// `config`
	auto config = New<v8::Object>();
	Set(binding, "config", config);
	Set(config, "hasIntl", false);

	// `constants`
	auto constants = New<v8::Object>();
	Set(binding, "constants", constants);
	Set(constants, "os", New<v8::Object>());

	// `types`
	auto types = New<v8::Object>();
	Set(binding, "types", types);
#define V(Type) \
	SetMethod(types, "is"#Type, Is##Type);
	IS_TYPE_FUNCTIONS(V)
#undef V
	SetMethod(types, "isAnyArrayBuffer", IsAnyArrayBuffer);
	SetMethod(types, "isBoxedPrimitive", IsBoxedPrimitive);

	// `util`
	auto util = New<v8::Object>();
	Set(binding, "util", util);
	SetMethod(util, "getProxyDetails", GetProxyDetails);
	SetMethod(util, "getConstructorName", GetConstructorName);
	SetMethod(util, "getOwnNonIndexProperties", GetOwnNonIndexProperties);
	SetMethod(util, "previewEntries", PreviewEntries);
	Set(util, "kPending", v8::Promise::PromiseState::kPending);
	Set(util, "kRejected", v8::Promise::PromiseState::kRejected);
	auto propertyFilters = New<v8::Object>();
	Set(util, "propertyFilter", propertyFilters);
	Set(propertyFilters, "ALL_PROPERTIES", v8::ALL_PROPERTIES);
	Set(propertyFilters, "ONLY_ENUMERABLE", v8::ONLY_ENUMERABLE);

	info.GetReturnValue().Set(binding);
}

ISOLATED_VM_MODULE void InitForContext(v8::Isolate* isolate, v8::Local<v8::Context> context, v8::Local<v8::Object> target) {
	SetMethod(target, "makeBinding", binding);
}

NAN_MODULE_INIT(init) {
	v8::Isolate* isolate = v8::Isolate::GetCurrent();
	InitForContext(isolate, isolate->GetCurrentContext(), target);
}
NODE_MODULE(native, init);
