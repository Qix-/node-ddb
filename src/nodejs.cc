#include "./av.hh"

#include <node_api.h>

#include <iostream> // XXX DEBUG

namespace ddb {

class callback_stream : public av::stream {
	napi_env env;
	napi_value cb_read;
	napi_value cb_seek;
	napi_value cb_tell;

	virtual int read(unsigned char *buf, long bufsize) override {
		napi_value global;
		napi_status status = napi_get_global(env, &global);
		if (status != napi_ok) return -1;
		// Yes, this means you ABSOLUTELY CANNOT SAVE THE REFERENCE
		// to the buffer in ANY code.
		napi_value args[2];
		status = napi_create_external_buffer(env, bufsize, buf, nullptr, nullptr, &args[0]);
		if (status != napi_ok) return -1;
		status = napi_create_int64(env, bufsize, &args[1]);
		if (status != napi_ok) return -1;
		napi_value result;
		status = napi_call_function(
			env,
			global,
			cb_read,
			2,
			&args[0],
			&result
		);
		if (status != napi_ok) return -1;
		napi_valuetype type;
		status = napi_typeof(env, result, &type);
		if (status != napi_ok) return -1;
		if (type != napi_number) {
			napi_throw_type_error(env, nullptr, "return value must be number");
			return -1;
		}
		int32_t result_i;
		status = napi_get_value_int32(env, result, &result_i);
		if (status != napi_ok) return -1;
		return result_i;
	}

	virtual bool seek(long offset, whence w) override {
		napi_value global;
		napi_status status = napi_get_global(env, &global);
		if (status != napi_ok) return -1;
		napi_value args[2];
		status = napi_create_int64(env, offset, &args[0]);
		if (status != napi_ok) return false;
		status = napi_create_int32(env, (int)w, &args[1]);
		if (status != napi_ok) return false;
		napi_value result;
		status = napi_call_function(
			env,
			global,
			cb_seek,
			2,
			&args[0],
			&result
		);
		if (status != napi_ok) return false;
		napi_valuetype type;
		status = napi_typeof(env, result, &type);
		if (status != napi_ok) return -1;
		if (type != napi_boolean) {
			napi_throw_type_error(env, nullptr, "return value must be boolean");
			return -1;
		}
		bool result_b;
		status = napi_get_value_bool(env, result, &result_b);
		if (status != napi_ok) return -1;
		return result_b;
	}

	virtual long tell() override {
		napi_value global;
		napi_status status = napi_get_global(env, &global);
		if (status != napi_ok) return -1;
		napi_value result;
		status = napi_call_function(
			env,
			global,
			cb_tell,
			0,
			nullptr,
			&result
		);
		if (status != napi_ok) return false;
		napi_valuetype type;
		status = napi_typeof(env, result, &type);
		if (status != napi_ok) return -1;
		if (type != napi_number) {
			napi_throw_type_error(env, nullptr, "return value must be number");
			return -1;
		}
		long result_l;
		status = napi_get_value_int64(env, result, &result_l);
		if (status != napi_ok) return -1;
		return result_l;
	}

public:
	callback_stream(napi_env env, napi_value cbs[3])
	: av::stream{}
	, env(env)
	, cb_read(cbs[0])
	, cb_seek(cbs[1])
	, cb_tell(cbs[2])
	{}
};

napi_value extract_frames(napi_env env, napi_callback_info args) {
	napi_status status;

	size_t argc = 4;
	napi_value argv[4];
	status = napi_get_cb_info(
		env,
		args,
		&argc,
		&argv[0],
		nullptr,
		nullptr
	);

	if (argc < 4) {
		napi_throw_type_error(env, nullptr, "four callback functions are required");
		return nullptr;
	}

	for (size_t i = 0; i < 4; i++) {
		napi_valuetype type;
		status = napi_typeof(env, argv[i], &type);
		if (status != napi_ok) return nullptr;
		if (type != napi_function) {
			napi_throw_type_error(env, nullptr, "one of the arguments is not a function");
			return nullptr;
		}
	}

	callback_stream stream { env, &argv[0] };

	std::error_code err;
	stream.init(err);
	if (err) {
		const auto msg = err.message();
		napi_throw_error(env, nullptr, msg.c_str());
		return nullptr;
	}

	const auto frames = stream.decode(err);
	if (err) {
		const auto msg = err.message();
		napi_throw_error(env, nullptr, msg.c_str());
		return nullptr;
	}

	napi_value result_arr;
	status = napi_create_array_with_length(env, frames.size(), &result_arr);
	if (status != napi_ok) return nullptr;

	std::size_t i = 0;
	for (const auto &frame : frames) {
		napi_value frame_value;
		status = napi_create_buffer_copy(
			env,
			frame.pixels.size(),
			frame.pixels.data(),
			nullptr,
			&frame_value
		);
		if (status != napi_ok) return nullptr;
		status = napi_set_element(
			env,
			result_arr,
			i++,
			frame_value
		);
		if (status != napi_ok) return nullptr;
	}

	napi_value global;
	status = napi_get_global(env, &global);
	if (status != napi_ok) return nullptr;

	napi_call_function(
		env,
		global,
		argv[3],
		1,
		&result_arr,
		nullptr
	);

	napi_value ret;
	status = napi_get_boolean(env, true, &ret);
	if (status != napi_ok) return nullptr;

	return ret;
}

napi_value init(napi_env env, napi_value exports) {
	ddb::av::init();

	napi_status status;
	napi_value fn;

	status = napi_create_function(env, nullptr, 0, extract_frames, nullptr, &fn);
	if (status != napi_ok) return nullptr;

	status = napi_set_named_property(env, exports, "extractFrames", fn);
	if (status != napi_ok) return nullptr;

	napi_value whence_values[3];
	status = napi_create_int32(env, ddb::av::stream::BEGINNING, &whence_values[0]);
	if (status != napi_ok) return nullptr;
	status = napi_create_int32(env, ddb::av::stream::RELATIVE, &whence_values[1]);
	if (status != napi_ok) return nullptr;
	status = napi_create_int32(env, ddb::av::stream::END, &whence_values[2]);
	if (status != napi_ok) return nullptr;

	napi_value frame_size;
	status = napi_create_uint32(env, ddb::av::frame::frame_size, &frame_size);
	if (status != napi_ok) return nullptr;

	status = napi_set_named_property(env, exports, "extractFrames", fn);
	if (status != napi_ok) return nullptr;
	status = napi_set_named_property(env, exports, "BEGINNING", whence_values[0]);
	if (status != napi_ok) return nullptr;
	status = napi_set_named_property(env, exports, "RELATIVE", whence_values[1]);
	if (status != napi_ok) return nullptr;
	status = napi_set_named_property(env, exports, "END", whence_values[2]);
	if (status != napi_ok) return nullptr;
	status = napi_set_named_property(env, exports, "FRAME_SIZE", frame_size);
	if (status != napi_ok) return nullptr;

	return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, init)

}
