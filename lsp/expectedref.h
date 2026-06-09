#pragma once

#include <cassert>
#include <exception>
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
		if(m_value)
			return *m_value;

		badExpectedRefAccess();
	}

	[[nodiscard]] E& error() &
	{
		if(m_error.has_value())
			return *m_error;

		badExpectedRefAccess();
	}

	[[nodiscard]] const E& error() const&
	{
		if(m_error.has_value())
			return *m_error;

		badExpectedRefAccess();
	}

private:
	[[noreturn]] static void badExpectedRefAccess()
	{
		assert(false && "bad ExpectedRef access");
		std::terminate();
	}

	T*               m_value = nullptr;
	std::optional<E> m_error;
};

} // namespace lsp
