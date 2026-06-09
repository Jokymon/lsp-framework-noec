#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include <lsp/expectedref.h>
#include <lsp/exception.h>
#include <lsp/strmap.h>

namespace lsp::json{

/*
 * Types
 */

class Value;
class Object;

using Null    = std::nullptr_t;
using Boolean = bool;
using Integer = std::int32_t;
using Decimal = double;
using String  = std::string;
using Array   = std::vector<Value>;

/*
 * Errors
 */

class Error : public Exception{
protected:
	using Exception::Exception;
};

class TypeError : public Error{
public:
	TypeError(const std::string& message = "Unexpected json value") : Error{message}{}
};

class ParseError : public Error{
public:
	ParseError(const std::string& message, std::size_t textPos)
		: Error{message}
	  , m_textPos{textPos}{}

	std::size_t textPos() const noexcept{ return m_textPos; }

private:
	 std::size_t m_textPos = 0;
};

/*
 * Object
 */

class Object{
public:
	using MapType = StrMap<String, Value>;

	Object();
	Object(const Object& other);
	Object(Object&&) noexcept = default;
	~Object();

	Object& operator=(const Object& other);
	Object& operator=(Object&& other) noexcept = default;

	[[nodiscard]] bool operator==(const Object& other) const;
	[[nodiscard]] bool operator!=(const Object& other) const{ return !(*this == other); }

	[[nodiscard]] Value& operator[](std::string_view key);

	[[nodiscard]] std::size_t size() const;
	[[nodiscard]] bool empty() const;
	[[nodiscard]] bool contains(std::string_view key) const;
	[[nodiscard]] ExpectedRef<Value, TypeError> get(std::string_view key);
	[[nodiscard]] ExpectedRef<const Value, TypeError> get(std::string_view key) const;
	[[nodiscard]] Value* find(std::string_view key);
	[[nodiscard]] const Value* find(std::string_view key) const;

	[[nodiscard]] MapType& keyValueMap();
	[[nodiscard]] const MapType& keyValueMap() const;

private:
	std::unique_ptr<MapType> m_map;
};

/*
 * Value
 */

class Value{
public:
	using VariantType = std::variant<Null, Boolean, Integer, Decimal, String, Array, Object>;

	constexpr Value() = default;
	constexpr Value(Null){}
	constexpr Value(Boolean b) : m_variant{b}{}
	constexpr Value(Integer i) : m_variant{i}{}
	constexpr Value(Decimal d) : m_variant{d}{}
	Value(String&& s) : m_variant{std::move(s)}{}
	Value(Array&& a) : m_variant{std::move(a)}{}
	Value(Object&& o) : m_variant{std::move(o)}{}

	[[nodiscard]] constexpr bool isNull()    const{ return std::holds_alternative<Null>(m_variant); }
	[[nodiscard]] constexpr bool isBoolean() const{ return std::holds_alternative<Boolean>(m_variant); }
	[[nodiscard]] constexpr bool isInteger() const{ return std::holds_alternative<Integer>(m_variant); }
	[[nodiscard]] constexpr bool isDecimal() const{ return std::holds_alternative<Decimal>(m_variant); }
	[[nodiscard]] constexpr bool isNumber()  const{ return isInteger() || isDecimal(); }
	[[nodiscard]] constexpr bool isString()  const{ return std::holds_alternative<String>(m_variant); }
	[[nodiscard]] constexpr bool isObject()  const{ return std::holds_alternative<Object>(m_variant); }
	[[nodiscard]] constexpr bool isArray()   const{ return std::holds_alternative<Array>(m_variant); }

	[[nodiscard]] std::expected<Boolean, TypeError> boolean() const{ return getValue<Boolean>(); }
	[[nodiscard]] std::expected<Integer, TypeError> integer() const{ return getValue<Integer>(); }
	[[nodiscard]] std::expected<Decimal, TypeError> decimal() const{ return getValue<Decimal>(); }
	[[nodiscard]] ExpectedRef<const String, TypeError> string() const&{ return getRef<String>(); }
	[[nodiscard]] ExpectedRef<const Object, TypeError> object() const&{ return getRef<Object>(); }
	[[nodiscard]] ExpectedRef<const Array, TypeError>  array()  const&{ return getRef<Array>(); }
	[[nodiscard]] ExpectedRef<String, TypeError> string() &{ return getRef<String>(); }
	[[nodiscard]] ExpectedRef<Object, TypeError> object() &{ return getRef<Object>(); }
	[[nodiscard]] ExpectedRef<Array, TypeError>  array()  &{ return getRef<Array>(); }
	[[nodiscard]] std::expected<String, TypeError> string() &&{ return getValue<String>(); }
	[[nodiscard]] std::expected<Object, TypeError> object() &&{ return getValue<Object>(); }
	[[nodiscard]] std::expected<Array, TypeError>  array()  &&{ return getValue<Array>(); }

	[[nodiscard]] std::expected<Decimal, TypeError> number() const
	{
		if(isDecimal())
			return getValue<Decimal>().value();

		if(isInteger())
			return static_cast<Decimal>(getValue<Integer>().value());

		return std::unexpected(TypeError{});
	}

	[[nodiscard]] bool operator==(const Value& other) const = default;
	[[nodiscard]] bool operator!=(const Value& other) const = default;

	[[nodiscard]] const VariantType& variant() const{ return m_variant; }
	[[nodiscard]] VariantType& variant(){ return m_variant; }

private:
	VariantType m_variant;

	template<typename T>
	std::expected<T, TypeError> getValue() const&
	{
		if(auto* const v = std::get_if<T>(&m_variant))
			return *v;

		return std::unexpected(TypeError{});
	}

	template<typename T>
	std::expected<T, TypeError> getValue() &&
	{
		if(auto* const v = std::get_if<T>(&m_variant))
			return std::move(*v);

		return std::unexpected(TypeError{});
	}

	template<typename T>
	ExpectedRef<T, TypeError> getRef() &
	{
		if(auto* const v = std::get_if<T>(&m_variant))
			return *v;

		return std::unexpected(TypeError{});
	}

	template<typename T>
	ExpectedRef<const T, TypeError> getRef() const&
	{
		if(const auto* const v = std::get_if<T>(&m_variant))
			return *v;

		return std::unexpected(TypeError{});
	}
};

using Any [[deprecated("Use json::Value")]] = Value;

/*
 * parse/stringify
 */

std::expected<Value, ParseError> parse(std::string_view text);
std::string stringify(const Value& json, bool format = false);
std::string toStringLiteral(std::string_view str);
std::string fromStringLiteral(std::string_view str);

} // namespace lsp::json
