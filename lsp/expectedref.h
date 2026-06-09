#pragma once

#include <cassert>
#include <optional>
#include <utility>

namespace lsp{

template<typename T, typename E>
class ExpectedRef{
public:
	ExpectedRef(T& value) noexcept
		: m_value(&value){}

	ExpectedRef(std::unexpected<E> error)
		: m_error(std::move(error.error())){}

	[[nodiscard]] bool has_value() const noexcept{ return m_value != nullptr; }
	[[nodiscard]] explicit operator bool() const noexcept{ return has_value(); }

	[[nodiscard]] T& value() const
	{
		assert(m_value);
		return *m_value;
	}

	[[nodiscard]] E& error() &
	{
		assert(m_error.has_value());
		return *m_error;
	}

	[[nodiscard]] const E& error() const&
	{
		assert(m_error.has_value());
		return *m_error;
	}

private:
	T*               m_value = nullptr;
	std::optional<E> m_error;
};

} // namespace lsp
