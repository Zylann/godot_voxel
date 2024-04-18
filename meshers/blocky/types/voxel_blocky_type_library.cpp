#include "voxel_blocky_type_library.h"
#include "../../../constants/voxel_string_names.h"
#include "../../../util/containers/container_funcs.h"
#include "../../../util/godot/classes/json.h"
#include "../../../util/godot/classes/object.h"
#include "../../../util/godot/classes/time.h"
#include "../../../util/godot/core/array.h"
#include "../../../util/godot/core/string.h"
#include "../../../util/godot/core/typed_array.h"
#include "../../../util/profiling.h"
#include "../../../util/string/format.h"
#include "../voxel_blocky_model_cube.h"

namespace zylann::voxel {

void VoxelBlockyTypeLibrary::clear() {
	_types.clear();
}

void VoxelBlockyTypeLibrary::load_default() {
	clear();

	Ref<VoxelBlockyType> air_type;
	air_type.instantiate();
	air_type->set_unique_name(VoxelStringNames::get_singleton().air);

	Ref<VoxelBlockyModelCube> cube;
	cube.instantiate();

	Ref<VoxelBlockyType> solid_type;
	solid_type.instantiate();
	solid_type->set_base_model(cube);
	solid_type->set_unique_name(VoxelStringNames::get_singleton().cube);

	_types.push_back(air_type);
	_types.push_back(solid_type);

	_needs_baking = true;
}

void VoxelBlockyTypeLibrary::bake() {
	ZN_PROFILE_SCOPE();

	RWLockWrite lock(_baked_data_rw_lock);

	const uint64_t time_before = Time::get_singleton()->get_ticks_usec();

	// This is the only place we modify the data.

	_indexed_materials.clear();
	_baked_data.models.clear();

	StdVector<VoxelBlockyModel::BakedData> baked_models;
	StdVector<VoxelBlockyType::VariantKey> keys;
	VoxelBlockyModel::MaterialIndexer material_indexer{ _indexed_materials };

	_baked_data.models.resize(_id_map.size());

	for (size_t i = 0; i < _types.size(); ++i) {
		Ref<VoxelBlockyType> type = _types[i];
		ZN_ASSERT_CONTINUE_MSG(
				type.is_valid(), format("{} at index {} is null", godot::get_class_name_str<VoxelBlockyType>(), i));

		type->bake(baked_models, keys, material_indexer, nullptr, get_bake_tangents());

		VoxelID id;
		id.type_name = type->get_unique_name();

		unsigned int rel_key_index = 0;
		for (VoxelBlockyModel::BakedData &baked_model : baked_models) {
			// _baked_data.models.push_back(std::move(baked_model));
			id.variant_key = keys[rel_key_index];

			size_t model_index;
			// TODO Optimize this (when needed)
			// Find existing slot in the ID map. If found, use pre-allocated index.
			if (!find(to_span_const(_id_map), id, model_index)) {
				// If not found, pick an empty slot if any
				if (!find(to_span_const(_id_map), VoxelID(), model_index)) {
					// If not found, allocate a new index at the end
					model_index = _baked_data.models.size();
					_baked_data.models.push_back(VoxelBlockyModel::BakedData());
					_id_map.push_back(id);
				}
			}

			_baked_data.models[model_index] = std::move(baked_model);

			++rel_key_index;
		}

		baked_models.clear();
		keys.clear();
	}

	_baked_data.indexed_materials_count = _indexed_materials.size();

	generate_side_culling_matrix(_baked_data);

	const uint64_t time_spent = Time::get_singleton()->get_ticks_usec() - time_before;
	ZN_PRINT_VERBOSE(
			format("Took {} us to bake VoxelLibrary, indexed {} materials", time_spent, _indexed_materials.size()));
}

void VoxelBlockyTypeLibrary::update_id_map() {
	update_id_map(_id_map, nullptr);
}

void VoxelBlockyTypeLibrary::update_id_map(StdVector<VoxelID> &id_map, StdVector<uint16_t> *used_ids) const {
	StdVector<VoxelBlockyType::VariantKey> keys;

	for (size_t i = 0; i < _types.size(); ++i) {
		Ref<VoxelBlockyType> type = _types[i];

		if (type == nullptr) {
			continue;
		}

		type->generate_keys(keys, true);

		VoxelID id;
		id.type_name = type->get_unique_name();

		for (const VoxelBlockyType::VariantKey &key : keys) {
			id.variant_key = key;

			size_t model_index;
			// TODO Optimize this (when needed)
			// Find existing slot in the ID map. If found, use pre-allocated index.
			if (!find(to_span_const(id_map), id, model_index)) {
				// If not found, pick an empty slot if any
				if (!find(to_span_const(id_map), VoxelID(), model_index)) {
					// If not found, allocate a new index at the end
					model_index = id_map.size();
					id_map.push_back(id);
				}
			}

			if (used_ids != nullptr) {
				used_ids->push_back(model_index);
			}
		}

		keys.clear();
	}
}

#ifdef TOOLS_ENABLED

void VoxelBlockyTypeLibrary::get_configuration_warnings(PackedStringArray &out_warnings) const {
	ZN_PROFILE_SCOPE();

	// Check null indices
	StdVector<unsigned int> null_indices;
	for (unsigned int i = 0; i < _types.size(); ++i) {
		if (_types[i].is_null()) {
			null_indices.push_back(i);
		}
	}
	if (null_indices.size() > 0) {
		const String null_indices_str = godot::join_comma_separated<unsigned int>(to_span(null_indices));
		out_warnings.push_back(
				String("{0} contains null items at indices {1}").format(varray(get_class(), null_indices_str)));
	}

	// Check duplicate names
	struct DuplicateName {
		String name;
		unsigned int index1;
		unsigned int index2;
	};
	StdVector<DuplicateName> duplicate_names;
	for (unsigned int i = 0; i < _types.size(); ++i) {
		const Ref<VoxelBlockyType> &type1 = _types[i];
		if (type1.is_null()) {
			continue;
		}
		const StringName type1_name = type1->get_unique_name();
		for (unsigned int j = i + 1; j < _types.size(); ++j) {
			const Ref<VoxelBlockyType> &type2 = _types[j];
			if (type2.is_null()) {
				continue;
			}
			if (type2->get_unique_name() == type1_name) {
				duplicate_names.push_back(DuplicateName{ String(type1_name), i, j });
			}
		}
	}
	if (duplicate_names.size() > 0) {
		String message = String("{0} contains items with the same name: ").format(varray(get_class()));
		for (const DuplicateName &dn : duplicate_names) {
			message += String("{0} at indices {1} and {2}; ").format(varray(dn.name, dn.index1, dn.index2));
		}
		out_warnings.push_back(message);
	}

	// Check types
	unsigned int type_index = 0;
	for (const Ref<VoxelBlockyType> &type : _types) {
		if (type.is_null()) {
			continue;
		}

		String sname = String(type->get_unique_name()).strip_edges();
		if (sname.length() == 0) {
			out_warnings.push_back(String("{0} at index {1} has an empty name.")
										   .format(varray(godot::get_class_name_str<VoxelBlockyType>(), type_index)));
		}

		type->get_configuration_warnings(out_warnings);

		++type_index;
	}

	// TODO Check inconsistent attributes across types?
	// Currently, two attributes with the same name on two different types can have completely different values or
	// meaning. This is probably not a good idea.
}

#endif

int VoxelBlockyTypeLibrary::get_model_index_default(StringName type_name) const {
	Ref<VoxelBlockyType> type = get_type_from_name(type_name);
	if (type.is_null()) {
		return -1;
	}

	FixedArray<Ref<VoxelBlockyAttribute>, VoxelBlockyType::MAX_ATTRIBUTES> attribs;

	Span<const Ref<VoxelBlockyAttribute>> unordered_attribs = type->get_attributes();
	for (unsigned int i = 0; i < unordered_attribs.size(); ++i) {
		attribs[i] = unordered_attribs[i];
	}

	VoxelBlockyAttribute::sort_by_name(Span<Ref<VoxelBlockyAttribute>>(attribs.data(), unordered_attribs.size()));

	VoxelID id;
	id.type_name = type_name;

	for (unsigned int i = 0; i < unordered_attribs.size(); ++i) {
		id.variant_key.attribute_names[i] = attribs[i]->get_attribute_name();
		id.variant_key.attribute_values[i] = attribs[i]->get_default_value();
	}

	return get_model_index(id);
}

namespace {

bool parse_attribute_value(
		const Variant &vv, const VoxelBlockyType &type, const StringName &attrib_name, uint8_t &out_attrib_value) {
	if (vv.get_type() == Variant::STRING_NAME) {
		StringName value_name = vv;

		Ref<VoxelBlockyAttribute> attrib = type.get_attribute_by_name(attrib_name);
		ERR_FAIL_COND_V_MSG(attrib.is_null(), false,
				String("{0} type '{1}' has no attribute named '{2}'")
						.format(varray(VoxelBlockyType::get_class_static(), type.get_unique_name(), attrib_name)));

		int attrib_value = attrib->get_value_from_name(value_name);
		ERR_FAIL_COND_V_MSG(attrib_value == -1, false,
				String("{0} ('{1}') has no value with name '{2}'")
						.format(varray(attrib->get_class(), attrib->get_attribute_name(), value_name)));

		out_attrib_value = attrib_value;

	} else if (vv.get_type() == Variant::INT) {
		const int raw_value = vv;
		ERR_FAIL_COND_V_MSG(raw_value < 0, false, "Attribute integer value cannot be negative.");
		out_attrib_value = raw_value;

	} else if (vv.get_type() == Variant::BOOL) {
		const bool boolean_value = vv;
		out_attrib_value = boolean_value ? 1 : 0;

	} else {
		ZN_PRINT_ERROR(format("Failed to parse attribute value. Expected StringName, integer or boolean. Got {}",
				Variant::get_type_name(vv.get_type())));
		return false;
	}

	return true;
}

} // namespace

int VoxelBlockyTypeLibrary::get_model_index_single_attribute(StringName type_name, Variant p_attrib_value) const {
	ZN_PROFILE_SCOPE();

	Ref<VoxelBlockyType> type = get_type_from_name(type_name);
	if (type.is_null()) {
		return -1;
	}

	Span<const Ref<VoxelBlockyAttribute>> attribs = type->get_attributes();
	ZN_ASSERT_RETURN_V_MSG(attribs.size() >= 1, -1, "The requested type has no attribute.");

	Ref<VoxelBlockyAttribute> attrib = attribs[0];
	ZN_ASSERT_RETURN_V(attrib.is_valid(), -1);
	const StringName attrib_name = attrib->get_attribute_name();

	uint8_t attrib_value;
	if (!parse_attribute_value(p_attrib_value, **type, attrib_name, attrib_value)) {
		return -1;
	}

	VoxelID id;
	id.type_name = type_name;
	id.variant_key.attribute_names[0] = attrib_name;
	id.variant_key.attribute_values[0] = attrib_value;

	return get_model_index(id);
}

int VoxelBlockyTypeLibrary::get_model_index_with_attributes(StringName type_name, Dictionary attribs_dict) const {
	ZN_PROFILE_SCOPE();

	VoxelID id;
	id.type_name = type_name;

	Ref<VoxelBlockyType> type = get_type_from_name(type_name);
	if (type.is_null()) {
		return -1;
	}

	const unsigned int attribute_count = attribs_dict.size();
	const Array dict_keys = attribs_dict.keys();

	FixedArray<std::pair<StringName, uint8_t>, VoxelBlockyType::MAX_ATTRIBUTES> unordered_key;

	// Parse attributes
	for (unsigned int attrib_spec_index = 0; attrib_spec_index < attribute_count; ++attrib_spec_index) {
		Variant dict_key = dict_keys[attrib_spec_index];
		Variant vv = attribs_dict[dict_key];

		std::pair<StringName, uint8_t> pair;

		const Variant::Type dict_key_type = dict_key.get_type();
		if (dict_key_type == Variant::STRING || dict_key_type == Variant::STRING_NAME) {
			pair.first = dict_key;
		} else {
			ZN_PRINT_ERROR(
					format("Attribute name must be a StringName. Got {}", Variant::get_type_name(dict_key_type)));
			return -1;
		}

		if (!parse_attribute_value(vv, **type, pair.first, pair.second)) {
			return -1;
		}

		unordered_key[attrib_spec_index] = pair;
	}

	// Sort, because sets of attributes have a fixed order so they can be looked up more efficiently
	VoxelBlockyAttribute::sort_by_name(Span<std::pair<StringName, uint8_t>>(unordered_key.data(), attribute_count));

	for (unsigned int i = 0; i < attribute_count; ++i) {
		id.variant_key.attribute_names[i] = unordered_key[i].first;
		id.variant_key.attribute_values[i] = unordered_key[i].second;
	}

	return get_model_index(id);
}

int VoxelBlockyTypeLibrary::get_model_index(const VoxelID queried_id) const {
	// Not optimized, we'll see if it needs to be a HashMap or other
	for (unsigned int i = 0; i < _id_map.size(); ++i) {
		const VoxelID &id = _id_map[i];
		if (id == queried_id) {
			return i;
		}
	}
	return -1;
}

Ref<VoxelBlockyType> VoxelBlockyTypeLibrary::get_type_from_name(StringName p_name) const {
	// Not optimized, we'll see if it needs to be a HashMap or other
	for (const Ref<VoxelBlockyType> &type : _types) {
		if (type.is_valid() && type->get_unique_name() == p_name) {
			return type;
		}
	}
	return Ref<VoxelBlockyType>();
}

Array VoxelBlockyTypeLibrary::get_type_name_and_attributes_from_model_index(int model_index) const {
	ZN_ASSERT_RETURN_V(model_index >= 0 && model_index < int(_id_map.size()), Array());
	const VoxelID &id = _id_map[model_index];

	Array ret;
	ret.resize(2);

	ret[0] = id.type_name;

	Dictionary dict;
	for (unsigned int attribute_index = 0; attribute_index < id.variant_key.attribute_names.size(); ++attribute_index) {
		const StringName &attrib_name = id.variant_key.attribute_names[attribute_index];
		if (attrib_name == StringName()) {
			break;
		}
		dict[attrib_name] = id.variant_key.attribute_values[attribute_index];
	}
	ret[1] = dict;

	return ret;
}

namespace {

struct VoxelIDToken {
	enum Type { //
		INVALID,
		NAME,
		INTEGER,
		BOOLEAN,
		OPEN_BRACKET,
		CLOSE_BRACKET,
		COMMA,
		EQUALS
	};
	Type type = INVALID;
	union {
		uint32_t integer_value;
		bool boolean_value;
	};
	unsigned int position = 0;
	unsigned int size = 0;
};

class VoxelIDTokenizer {
public:
	VoxelIDTokenizer(Span<const char32_t> str) : _position(0), _str(str) {}

	enum Result { TOKEN, END, UNEXPECTED_TOKEN };

	Result get(VoxelIDToken &out_token) {
		struct CharToken {
			const char32_t character;
			VoxelIDToken::Type type;
		};
		static const CharToken s_char_tokens[] = {
			{ '[', VoxelIDToken::OPEN_BRACKET }, //
			{ ']', VoxelIDToken::CLOSE_BRACKET }, //
			{ ',', VoxelIDToken::COMMA }, //
			{ '=', VoxelIDToken::EQUALS }, //
			{ 0, VoxelIDToken::INVALID } //
		};

		while (_position < _str.size()) {
			const char32_t c = _str[_position];

			{
				const CharToken *ct = s_char_tokens;
				while (ct->character != 0) {
					if (ct->character == c) {
						VoxelIDToken token;
						token.type = ct->type;
						token.position = _position;
						token.size = 1;
						++_position;
						out_token = token;
						return TOKEN;
					}
					++ct;
				}
			}

			Span<const char32_t> substr = _str.sub(_position);
			if (is_keyword(substr, "true")) {
				out_token.type = VoxelIDToken::BOOLEAN;
				out_token.position = _position;
				out_token.size = 4;
				out_token.boolean_value = true;
				return TOKEN;
			}
			if (is_keyword(substr, "false")) {
				out_token.type = VoxelIDToken::BOOLEAN;
				out_token.position = _position;
				out_token.size = 5;
				out_token.boolean_value = false;
				return TOKEN;
			}

			if (is_name_starter(c)) {
				out_token = get_name();
				return TOKEN;
			}

			if (is_digit(c)) {
				out_token = get_integer();
				return TOKEN;
			}

			return UNEXPECTED_TOKEN;
		}

		return END;
	}

private:
	static inline bool is_name_starter(char c) {
		return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
	}

	static inline bool is_digit(char c) {
		return (c >= '0' && c <= '9');
	}

	static inline bool is_name_char(char c) {
		return is_name_starter(c) ||
				is_digit(c)
				// Special addition so names can contain a namespace
				|| c == ':';
	}

	static inline bool is_keyword(Span<const char32_t> str, const char *keyword) {
		unsigned int i = 0;
		while (keyword[i] != '\0') {
			// Keywords are all ASCII, but our input text might not be
			if (str[i] != char32_t(keyword[i])) {
				return false;
			}
			++i;
		}
		if (i == str.size()) {
			// All characters matched
			return true;
		}
		// We matched all characters but the tested string is longer.
		// Check if it ends with a separating character. If not, then it contains a longer name and therefore won't
		// match.
		++i;
		return !is_name_char(str[i]);
	}

	VoxelIDToken get_name() {
		const unsigned int begin_pos = _position;
		while (_position < _str.size()) {
			const char32_t c = _str[_position];
			if (!is_name_char(c)) {
				break;
			}
			++_position;
		}
		VoxelIDToken token;
		token.type = VoxelIDToken::NAME;
		token.position = begin_pos;
		token.size = _position - begin_pos;
		return token;
	}

	VoxelIDToken get_integer() {
		uint32_t n = 0;
		char32_t c = 0;
		const unsigned int begin_pos = _position;
		while (_position < _str.size()) {
			c = _str[_position];
			if (!is_digit(c)) {
				break;
			}
			n = n * 10 + (c - '0');
			++_position;
		}
		VoxelIDToken token;
		token.type = VoxelIDToken::INTEGER;
		token.position = begin_pos;
		token.size = _position - begin_pos;
		token.integer_value = n;
		return token;
	}

	unsigned int _position;
	Span<const char32_t> _str;
};

} // namespace

bool VoxelBlockyTypeLibrary::parse_voxel_id(const String &p_str, VoxelID &out_id) {
	Span<const char32_t> str(p_str.ptr(), p_str.length());

	VoxelIDTokenizer tokenizer(str);

	VoxelIDToken token;
	VoxelIDTokenizer::Result result = tokenizer.get(token);
	ZN_ASSERT_RETURN_V(result == VoxelIDTokenizer::TOKEN, false);
	ZN_ASSERT_RETURN_V(token.type == VoxelIDToken::NAME, false);
	out_id.type_name = p_str.substr(token.position, token.size);

	result = tokenizer.get(token);
	if (result == VoxelIDTokenizer::END) {
		return true;
	}
	ZN_ASSERT_RETURN_V(result == VoxelIDTokenizer::TOKEN, false);
	ZN_ASSERT_RETURN_V(token.type == VoxelIDToken::OPEN_BRACKET, false);

	for (unsigned int attribute_index = 0; attribute_index < VoxelBlockyType::MAX_ATTRIBUTES; ++attribute_index) {
		result = tokenizer.get(token);
		ZN_ASSERT_RETURN_V(result == VoxelIDTokenizer::TOKEN, false);
		if (token.type == VoxelIDToken::CLOSE_BRACKET) {
			break;
		}
		ZN_ASSERT_RETURN_V(token.type == VoxelIDToken::NAME, false);
		out_id.variant_key.attribute_names[attribute_index] = p_str.substr(token.position, token.size);

		result = tokenizer.get(token);
		ZN_ASSERT_RETURN_V(result == VoxelIDTokenizer::TOKEN, false);
		ZN_ASSERT_RETURN_V(token.type == VoxelIDToken::EQUALS, false);

		result = tokenizer.get(token);
		ZN_ASSERT_RETURN_V(result == VoxelIDTokenizer::TOKEN, false);

		if (token.type == VoxelIDToken::INTEGER) {
			ZN_ASSERT_RETURN_V(token.integer_value < VoxelBlockyAttribute::MAX_VALUES, false);
			out_id.variant_key.attribute_values[attribute_index] = token.integer_value;

		} else if (token.type == VoxelIDToken::BOOLEAN) {
			out_id.variant_key.attribute_values[attribute_index] = token.boolean_value ? 1 : 0;

		} else {
			// TODO Can't parse named attribute values without recognizing their type!
			// It may be possible to recognize built-in attributes, but not custom ones...
			ZN_PRINT_ERROR("Unsupported value");
			return false;
		}
	}

	return true;
}

bool VoxelBlockyTypeLibrary::load_id_map_from_string_array(PackedStringArray map_array) {
	ZN_PROFILE_SCOPE();

	ZN_ASSERT_RETURN_V_MSG(_id_map.size() == 0, false,
			"The current ID map isn't empty. Make sure you're not accidentally overwriting data, or clear the library "
			"first.");

	StdVector<VoxelID> id_map;
	id_map.resize(map_array.size());

	for (int model_index = 0; model_index < map_array.size(); ++model_index) {
		String model_str = map_array[model_index];
		if (model_str == "") {
			continue;
		}
		ZN_ASSERT_RETURN_V(parse_voxel_id(model_str, id_map[model_index]), false);
	}

	_id_map = std::move(id_map);
	return true;
}

bool VoxelBlockyTypeLibrary::load_id_map_from_json(String json_string) {
	ZN_PROFILE_SCOPE();

	ZN_ASSERT_RETURN_V_MSG(_id_map.size() == 0, false,
			"The current ID map isn't empty. Make sure you're not accidentally overwriting data, or clear the library "
			"first.");

	Ref<JSON> json;
	json.instantiate();
	const Error json_err = json->parse(json_string);
	if (json_err != OK) {
		const String json_err_msg = json->get_error_message();
		const int json_err_line = json->get_error_line();
		ZN_PRINT_ERROR(format("Error when parsing ID Map from JSON string: line {}: {}", json_err_line, json_err_msg));
		return false;
	}

	Variant res = json->get_data();
	ZN_ASSERT_RETURN_V(res.get_type() == Variant::DICTIONARY, false);
	Dictionary root_dict = res;

	ZN_ASSERT_RETURN_V(root_dict.has("map"), false);
	Variant map_v = root_dict.get("map", Variant());

	ZN_ASSERT_RETURN_V(map_v.get_type() == Variant::ARRAY, false);
	Array map_varray = map_v;

	PackedStringArray map_sarray;
	map_sarray.resize(map_varray.size());
	Span<String> map_sarray_w(map_sarray.ptrw(), map_sarray.size());
	for (int i = 0; i < map_varray.size(); ++i) {
		Variant v = map_varray[i];
		if (v.get_type() == Variant::NIL) {
			continue;
		} else if (v.get_type() == Variant::STRING) {
			map_sarray_w[i] = v;
		} else {
			ZN_PRINT_ERROR(format("Expected string or null in ID Map array at index {}", i));
			continue;
		}
	}

	return load_id_map_from_string_array(map_sarray);
}

String VoxelBlockyTypeLibrary::to_string(const VoxelID &id) {
	String s = id.type_name;
	if (id.variant_key.attribute_names[0] != StringName()) {
		s += "[";
		s += id.variant_key.to_string();
		s += "]";
	}
	return s;
}

PackedStringArray VoxelBlockyTypeLibrary::serialize_id_map_to_string_array(const StdVector<VoxelID> &id_map) {
	ZN_PROFILE_SCOPE();

	PackedStringArray array;
	array.resize(id_map.size());
	Span<String> array_w(array.ptrw(), array.size());
	for (unsigned int model_index = 0; model_index < id_map.size(); ++model_index) {
		const VoxelID &id = id_map[model_index];
		if (id.type_name == StringName()) {
			continue;
		}
		array_w[model_index] = to_string(id);
	}

	return array;
}

PackedStringArray VoxelBlockyTypeLibrary::serialize_id_map_to_string_array() const {
	return serialize_id_map_to_string_array(_id_map);
}

void VoxelBlockyTypeLibrary::get_id_map_preview(PackedStringArray &out_ids, StdVector<uint16_t> &used_ids) const {
	StdVector<VoxelID> id_map = _id_map;
	update_id_map(id_map, &used_ids);
	out_ids = serialize_id_map_to_string_array(id_map);
}

String VoxelBlockyTypeLibrary::serialize_id_map_to_json() const {
	ZN_PROFILE_SCOPE();

	PackedStringArray array = serialize_id_map_to_string_array();

	Dictionary root_dict;
	root_dict["map"] = array;

	return JSON::stringify(root_dict, "\t", true);
}

TypedArray<VoxelBlockyType> VoxelBlockyTypeLibrary::_b_get_types() const {
	return godot::to_typed_array(to_span(_types));
}

void VoxelBlockyTypeLibrary::_b_set_types(TypedArray<VoxelBlockyType> types) {
	godot::copy_to(_types, types);
	_needs_baking = true;
}

PackedStringArray VoxelBlockyTypeLibrary::_b_serialize_id_map_to_string_array() const {
	return serialize_id_map_to_string_array();
}

PackedStringArray VoxelBlockyTypeLibrary::_b_get_id_map() {
	// This is a hack so that when we save a library, its internal ID map is updated and saved.
	update_id_map();
	return serialize_id_map_to_string_array();
}

void VoxelBlockyTypeLibrary::_b_set_id_map(PackedStringArray sarray) {
	load_id_map_from_string_array(sarray);
}

void VoxelBlockyTypeLibrary::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_types"), &VoxelBlockyTypeLibrary::_b_get_types);
	ClassDB::bind_method(D_METHOD("set_types"), &VoxelBlockyTypeLibrary::_b_set_types);

	ClassDB::bind_method(
			D_METHOD("get_model_index_default", "type_name"), &VoxelBlockyTypeLibrary::get_model_index_default);
	ClassDB::bind_method(D_METHOD("get_model_index_single_attribute", "type_name", "attrib_value"),
			&VoxelBlockyTypeLibrary::get_model_index_single_attribute);
	ClassDB::bind_method(D_METHOD("get_model_index_with_attributes", "type_name", "attribs_dict"),
			&VoxelBlockyTypeLibrary::get_model_index_with_attributes);
	ClassDB::bind_method(D_METHOD("get_type_name_and_attributes_from_model_index", "model_index"),
			&VoxelBlockyTypeLibrary::get_type_name_and_attributes_from_model_index);
	ClassDB::bind_method(D_METHOD("load_id_map_from_json", "json"), &VoxelBlockyTypeLibrary::load_id_map_from_json);
	ClassDB::bind_method(D_METHOD("serialize_id_map_to_json"), &VoxelBlockyTypeLibrary::serialize_id_map_to_json);
	ClassDB::bind_method(D_METHOD("load_id_map_from_string_array", "str_array"),
			&VoxelBlockyTypeLibrary::load_id_map_from_string_array);
	ClassDB::bind_method(
			D_METHOD("serialize_id_map_to_string_array"), &VoxelBlockyTypeLibrary::_b_serialize_id_map_to_string_array);

	ClassDB::bind_method(D_METHOD("get_type_from_name", "type_name"), &VoxelBlockyTypeLibrary::get_type_from_name);

	ClassDB::bind_method(D_METHOD("_get_id_map_data"), &VoxelBlockyTypeLibrary::_b_get_id_map);
	ClassDB::bind_method(D_METHOD("_set_id_map_data", "sarray"), &VoxelBlockyTypeLibrary::_b_set_id_map);

	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "types", PROPERTY_HINT_ARRAY_TYPE,
						 MAKE_RESOURCE_TYPE_HINT(VoxelBlockyType::get_class_static())),
			"set_types", "get_types");

	// Internal property
	ADD_PROPERTY(
			PropertyInfo(Variant::PACKED_STRING_ARRAY, "_id_map_data", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE),
			"_set_id_map_data", "_get_id_map_data");
}

} // namespace zylann::voxel
