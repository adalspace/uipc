#pragma once

#include <algorithm>
#include <cassert>
#include <cstring>
#include <functional>
#include <limits>
#include <ostream>
#include <string>
#include <string_view>
#include <type_traits>

#if __cplusplus >= 202002L
#include <compare>
#endif

#include "def.hpp"
#include "array.hpp"

template<typename SizeT = Uint32>
class BasicString
{
    static_assert(
        std::is_integral_v<SizeT> && std::is_unsigned_v<SizeT>,
        "BasicString SizeT must be an unsigned integral type"
    );

public:
    static constexpr SizeT npos =
        std::numeric_limits<SizeT>::max();

public:
    BasicString()
    {
        // Always maintain a trailing '\0'.
        m_data.ResizeUninitialized(1);
        m_data.Data()[0] = '\0';
    }

    BasicString(const char* str)
    {
        assert(str != nullptr);

        const std::size_t len = std::strlen(str);

        assert(len <= std::numeric_limits<SizeT>::max());

        m_data.ResizeUninitialized(
            static_cast<SizeT>(len) + 1
        );

        std::memcpy(m_data.Data(), str, len);
        m_data.Data()[len] = '\0';
    }

    BasicString(const std::string& str)
    {
        assert(str.size() <= std::numeric_limits<SizeT>::max());

        m_data.ResizeUninitialized(
            static_cast<SizeT>(str.size()) + 1
        );

        std::memcpy(
            m_data.Data(),
            str.data(),
            str.size()
        );

        m_data.Data()[str.size()] = '\0';
    }

    BasicString(const BasicString&) = default;
    BasicString& operator=(const BasicString&) = default;

    BasicString(BasicString&&) noexcept = default;
    BasicString& operator=(BasicString&&) noexcept = default;

public:
    SizeT Size() const noexcept
    {
        // m_data always includes one trailing '\0'.
        return m_data.Size() - 1;
    }

    bool Empty() const noexcept
    {
        return Size() == 0;
    }

    char* Data() noexcept
    {
        return m_data.Data();
    }

    const char* Data() const noexcept
    {
        return m_data.Data();
    }

    const char* c_str() const noexcept
    {
        return m_data.Data();
    }

public:
    char* begin() noexcept
    {
        return m_data.Data();
    }

    const char* begin() const noexcept
    {
        return m_data.Data();
    }

    const char* cbegin() const noexcept
    {
        return m_data.Data();
    }

    char* end() noexcept
    {
        return m_data.Data() + Size();
    }

    const char* end() const noexcept
    {
        return m_data.Data() + Size();
    }

    const char* cend() const noexcept
    {
        return m_data.Data() + Size();
    }

public:
    void Extend(const BasicString& other)
    {
        const SizeT oldSize = Size();
        const SizeT addedSize = other.Size();

        assert(
            addedSize <=
            std::numeric_limits<SizeT>::max() - oldSize
        );

        const SizeT newSize = oldSize + addedSize;

        m_data.ResizeUninitialized(newSize + 1);

        std::memcpy(
            m_data.Data() + oldSize,
            other.Data(),
            addedSize
        );

        m_data.Data()[newSize] = '\0';
    }

    void Extend(const char* other)
    {
        assert(other != nullptr);

        const std::size_t rawLen = std::strlen(other);

        assert(rawLen <= std::numeric_limits<SizeT>::max());

        const SizeT len = static_cast<SizeT>(rawLen);
        const SizeT oldSize = Size();

        assert(
            len <=
            std::numeric_limits<SizeT>::max() - oldSize
        );

        const SizeT newSize = oldSize + len;

        m_data.ResizeUninitialized(newSize + 1);

        std::memcpy(
            m_data.Data() + oldSize,
            other,
            len
        );

        m_data.Data()[newSize] = '\0';
    }

    void Extend(char other)
    {
        const SizeT oldSize = Size();

        assert(oldSize < std::numeric_limits<SizeT>::max());

        m_data.ResizeUninitialized(oldSize + 2);

        m_data.Data()[oldSize] = other;
        m_data.Data()[oldSize + 1] = '\0';
    }

public:
    BasicString Substr(
        SizeT pos,
        SizeT count = npos
    ) const
    {
        assert(pos <= Size());

        const SizeT available = Size() - pos;
        const SizeT length =
            count == npos
                ? available
                : std::min(available, count);

        BasicString result;

        result.m_data.ResizeUninitialized(length + 1);

        if (length != 0)
        {
            std::memcpy(
                result.m_data.Data(),
                m_data.Data() + pos,
                length
            );
        }

        result.m_data.Data()[length] = '\0';

        return result;
    }

    SizeT Find(char what) const noexcept
    {
        for (SizeT i = 0; i < Size(); ++i)
        {
            if (m_data.Data()[i] == what)
                return i;
        }

        return npos;
    }

    SizeT FindLastOf(char what) const noexcept
    {
        for (SizeT i = Size(); i > 0; --i)
        {
            if (m_data.Data()[i - 1] == what)
                return i - 1;
        }

        return npos;
    }

public:
    std::string_view View() const noexcept
    {
        return std::string_view(
            Data(),
            static_cast<std::size_t>(Size())
        );
    }

    bool operator==(
        const BasicString& other
    ) const noexcept
    {
        return View() == other.View();
    }

#if __cplusplus >= 202002L
    std::strong_ordering operator<=>(
        const BasicString& other
    ) const noexcept
    {
        return View() <=> other.View();
    }
#else
    bool operator!=(
        const BasicString& other
    ) const noexcept
    {
        return !(*this == other);
    }
#endif

public:
    char& operator[](SizeT index) noexcept
    {
        assert(index < Size());
        return m_data.Data()[index];
    }

    const char& operator[](SizeT index) const noexcept
    {
        assert(index < Size());
        return m_data.Data()[index];
    }

    BasicString operator+(
        const BasicString& other
    ) const
    {
        BasicString result(*this);
        result.Extend(other);
        return result;
    }

    BasicString operator+(
        const char* other
    ) const
    {
        BasicString result(*this);
        result.Extend(other);
        return result;
    }

    friend BasicString operator+(
        const char* lhs,
        const BasicString& rhs
    )
    {
        BasicString result(lhs);
        result.Extend(rhs);
        return result;
    }

    friend std::ostream& operator<<(
        std::ostream& stream,
        const BasicString& str
    )
    {
        stream.write(
            str.Data(),
            static_cast<std::streamsize>(str.Size())
        );

        return stream;
    }

private:
    // Invariant:
    //
    // m_data.Size() >= 1
    // m_data.Data()[m_data.Size() - 1] == '\0'
    //
    // The terminating '\0' is NOT included in Size().
    Array<char, SizeT> m_data;
};

namespace std
{
    template<typename SizeT>
    struct hash<::BasicString<SizeT>>
    {
        std::size_t operator()(
            const ::BasicString<SizeT>& value
        ) const noexcept
        {
            return std::hash<std::string_view>{}(
                value.View()
            );
        }
    };
}

using String = BasicString<>;
