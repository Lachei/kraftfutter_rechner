#pragma once

#include <optional>

#include "string_util.h"

#define JSON_ASSERT(x, message) if (!(x)) {LogError(message); return {}; }
constexpr std::optional<std::string_view> parse_remove_json_string(std::string_view &json) {
	skip_whitespace(json);
	JSON_ASSERT(json.size() && is_quote(json[0]), "Missing start quote string");
	auto end = std::min(json.find_first_of("\"'", 1), json.size());
	std::string_view s = json.substr(1, end - 1);
	json = json.substr(end);
	JSON_ASSERT(json.size() && is_quote(json[0]), "Missing end quote for string");
	json = json.substr(1);
	skip_whitespace(json);
	return s;
};
constexpr std::optional<double> parse_remove_json_double(std::string_view &json) {
	skip_whitespace(json);
	JSON_ASSERT(json.size(), "No number here");
	char* end = (char*)json.end();
	double d = std::strtod(json.data(), &end);
	json = json.substr(end - json.data());
	JSON_ASSERT(json.size(), "Missing cahracter after number");
	return d;
};
template<typename T>
constexpr bool parse_remove_json_double_array(std::string_view &json, T &a) {
	skip_whitespace(json);
	JSON_ASSERT(json.size() && json[0] == '[', "Missing array start character");
	json = json.substr(1);
	for (size_t i = 0; i < a.size(); ++i) {
		auto v = parse_remove_json_double(json);
		JSON_ASSERT(v, "Failed to parse array double");
		*(a.data() + i) = v.value();
		if (json.size() && json[0] == ']')
			break;
		JSON_ASSERT(json.size() && json[0] == ',', "Array missing comma");
	}
	JSON_ASSERT(json.size() && json[0] == ']', "Expected ']' at the end of array");
	json = json.substr(1);
	skip_whitespace(json);
	return true;
}

