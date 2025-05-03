// CuFormat by chenzyadb@github.com
// Based on C++17 STL (GNUC)

#if !defined(_CU_FORMAT_)
#define _CU_FORMAT_ 1

#if defined(_MSC_VER)
#pragma warning(disable : 4996)
#endif // defined(_MSC_VER)

#include <exception>
#include <string>
#include <string_view>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>

namespace CU 
{
    constexpr size_t _npos = static_cast<size_t>(-1);

    class FormatExcept : public std::exception
    {
        public:
            FormatExcept(const std::string &message) : message_(message) { }

            const char* what() const noexcept override
            {
                return message_.data();
            }

        private:
            const std::string message_;
    };

    struct _Format_String
    {
        char stack_buffer[32];
        char* heap_block;
        size_t length;
        size_t capacity;

        _Format_String() noexcept : stack_buffer(), heap_block(nullptr), length(0), capacity(sizeof(stack_buffer)) { }

        _Format_String(const _Format_String &other) noexcept :
            stack_buffer(), 
            heap_block(nullptr), 
            length(0), 
            capacity(sizeof(stack_buffer))
        {
            append(other);
        }
        
        _Format_String(const char* src) noexcept : 
            stack_buffer(), 
            heap_block(nullptr), 
            length(0), 
            capacity(sizeof(stack_buffer))
        {
            append(src);
        }

        _Format_String(_Format_String &&other) noexcept :
            stack_buffer(), 
            heap_block(other.heap_block), 
            length(other.length), 
            capacity(other.capacity)
        { 
            std::memcpy(stack_buffer, other.stack_buffer, sizeof(stack_buffer));
            other.heap_block = nullptr;
        }

        ~_Format_String() noexcept
        {
            if (heap_block != nullptr) {
                std::free(heap_block);
            }
        }

        void append(const _Format_String &other) noexcept
        {
            auto new_len = length + other.length;
            if (new_len >= capacity) {
                resize(capacity + new_len);
            }
            std::memcpy((data() + length), other.data(), other.length);
            *(data() + new_len) = '\0';
            length = new_len;
        }

        void append(const char* src) noexcept
        {
            auto src_len = std::strlen(src);
            auto new_len = length + src_len;
            if (new_len >= capacity) {
                resize(capacity + new_len);
            }
            std::memcpy((data() + length), src, src_len);
            *(data() + new_len) = '\0';
            length = new_len;
        }

        void append(char ch) noexcept
        {
            if ((length + 1) >= capacity) {
                resize(capacity * 2);
            }
            *(data() + length) = ch;
            *(data() + length + 1) = '\0';
            length++;
        }

        void resize(size_t req_capacity) noexcept
        {
            if (req_capacity < capacity && length >= req_capacity) {
                shrink(req_capacity - 1);
            }
            if (req_capacity > sizeof(stack_buffer)) {
                auto new_block = reinterpret_cast<char*>(std::realloc(heap_block, req_capacity));
                if (new_block != nullptr) {
                    if (heap_block == nullptr) {
                        std::memcpy(new_block, stack_buffer, length);
                    }
                    *(new_block + length) = '\0';
                    heap_block = new_block;
                    capacity = req_capacity;
                }
            } else {
                capacity = sizeof(stack_buffer);
                if (heap_block != nullptr && length > 0) {
                    std::memcpy(stack_buffer, heap_block, length);
                    std::free(heap_block);
                }
                stack_buffer[length] = '\0';
            }
        }

        void shrink(size_t req_length) noexcept
        {
            if (req_length < length) {
                *(data() + req_length) = '\0';
                length = req_length;
            }
        }

        char* data() noexcept
        {
            if (heap_block != nullptr) {
                return heap_block;
            }
            return stack_buffer;
        }

        const char* data() const noexcept
        {
            if (heap_block != nullptr) {
                return heap_block;
            }
            return stack_buffer;
        }

        void clear() noexcept
        {
            if (heap_block != nullptr) {
                std::free(heap_block);
                heap_block = nullptr;
            }
            stack_buffer[0] = '\0';
            length = 0;
            capacity = sizeof(stack_buffer);
        }
    };

    inline size_t _Find_Char(const char* str, char ch, size_t start_pos = 0) noexcept
    {
        for (auto pos = start_pos; *(str + pos) != '\0'; pos++) {
            if (*(str + pos) == ch) {
                return pos;
            }
        }
        return _npos;
    }

    inline int _String_To_Int(const char* str) noexcept
    {
        int value = 0;
        for (size_t offset = 0; *(str + offset) != '\0'; offset++) {
            char ch = *(str + offset);
            if (ch >= '0' && ch <= '9') {
                value = value * 10 + (ch - '0');
            } else {
                break;
            }
        }
        return value;
    }

    template <typename _Ty>
    inline _Format_String _Decimal_To_String(_Ty value) noexcept
    {
        char buffer[16] = ".";
        auto dec = (value - static_cast<size_t>(value)) * 10;
        for (size_t pos = 1; pos < (sizeof(buffer) - 1); pos++) {
            buffer[pos] = '0' + static_cast<char>(static_cast<size_t>(dec) % 10);
            dec = (dec - static_cast<size_t>(dec)) * 10;
            if (dec == 0) {
                break;
            }
        }
        return buffer;
    }

    template <typename _Ty>
    inline _Format_String _Int_To_String(_Ty value) noexcept
    {
        char buffer[32]{};
        auto pos = sizeof(buffer) - 2;
        if (value > 0) {
            for (auto integer = value; integer > 0; integer /= 10) {
                buffer[pos] = '0' + static_cast<char>(integer % 10);
                pos--;
            }
            pos++;
        } else if (value < 0) {
            for (auto integer = -value; integer > 0; integer /= 10) {
                buffer[pos] = '0' + static_cast<char>(integer % 10);
                pos--;
            }
            buffer[pos] = '-';
        } else {
            buffer[pos] = '0';
        }
        return &buffer[pos];
    }

    template <typename _Ty>
    inline _Format_String _Float_To_String(_Ty value) noexcept
    {
        if (value < 0) {
            _Format_String str("-");
            str.append(_Int_To_String(static_cast<size_t>(-value)));
            str.append(_Decimal_To_String(-value));
            return str;
        } else if (value > 0) {
            _Format_String str(_Int_To_String(static_cast<size_t>(value)));
            str.append(_Decimal_To_String(value));
            return str;
        }
        return "0";
    }

    template <typename _Ptr_Ty>
    inline _Format_String _To_Format_String(const _Ptr_Ty* value) noexcept
    {
        auto addr_val = reinterpret_cast<size_t>(value);
        if (addr_val > 0) {
            return _Int_To_String(addr_val);
        }
        return "NULL";
    }

    inline _Format_String _To_Format_String(std::nullptr_t) noexcept
    {
        return "NULL";
    }

    inline _Format_String _To_Format_String(long double value) noexcept
    {
        return _Float_To_String(value);
    }

    inline _Format_String _To_Format_String(double value) noexcept
    {
        return _Float_To_String(value);
    }

    inline _Format_String _To_Format_String(float value) noexcept
    {
        return _Float_To_String(value); 
    }

    inline _Format_String _To_Format_String(unsigned long long value) noexcept
    {
        return _Int_To_String(value);
    }

    inline _Format_String _To_Format_String(unsigned long value) noexcept
    {
        return _Int_To_String(value);
    }

    inline _Format_String _To_Format_String(unsigned int value) noexcept
    {
        return _Int_To_String(value);
    }

    inline _Format_String _To_Format_String(unsigned short value) noexcept
    {
        return _Int_To_String(value);
    }

    inline _Format_String _To_Format_String(unsigned char value) noexcept
    {
        return _Int_To_String(value);
    }

    inline _Format_String _To_Format_String(long long value) noexcept
    {
        return _Int_To_String(value);
    }

    inline _Format_String _To_Format_String(long value) noexcept
    {
        return _Int_To_String(value);
    }

    inline _Format_String _To_Format_String(int value) noexcept
    {
        return _Int_To_String(value);
    }

    inline _Format_String _To_Format_String(short value) noexcept
    {
        return _Int_To_String(value);
    }

    inline _Format_String _To_Format_String(signed char value) noexcept
    {
        return _Int_To_String(value);
    }

    inline _Format_String _To_Format_String(bool value) noexcept
    {
        if (value) {
            return "true";
        }
        return "false";
    }

    inline _Format_String _To_Format_String(char value) noexcept
    {
        char buffer[2]{};
        buffer[0] = value;
        return buffer;
    }

    inline _Format_String _To_Format_String(const char* value) noexcept
    {
        return value;
    }

    inline _Format_String _To_Format_String(std::string_view value) noexcept
    {
        return value.data();
    }

    template <typename _Arg_Ty>
    inline void _Args_Impl(std::vector<_Format_String> &args_list, const _Arg_Ty &arg) 
    { 
        args_list.emplace_back(_To_Format_String(arg));
    }

    template <typename _Arg_Ty, typename... _Args>
    inline void _Args_Impl(std::vector<_Format_String> &args_list, const _Arg_Ty &arg, const _Args &...args)
    {
        args_list.emplace_back(_To_Format_String(arg));
        _Args_Impl(args_list, args...);
    }

    template <typename... _Args>
    inline std::vector<_Format_String> _Args_Impl(size_t reserve_size, const _Args &...args)
    {
        std::vector<_Format_String> args_list{};
        args_list.reserve(reserve_size);
        _Args_Impl(args_list, args...);
        return args_list;
    }

    struct _Format_Item
    {
        _Format_String content;
        int arg_idx;
        int max_length;

        _Format_Item() noexcept : content(), arg_idx(-1), max_length(INT_MAX) { }

        _Format_Item(const _Format_Item &other) noexcept : 
            content(other.content), 
            arg_idx(other.arg_idx), 
            max_length(other.max_length) 
        { }

        _Format_Item(_Format_Item &&other) noexcept : 
            content(std::move(other.content)), 
            arg_idx(other.arg_idx), 
            max_length(other.max_length) 
        { }
    };

    inline std::vector<_Format_Item> _Format_Impl(const char* format)
    {
        std::vector<_Format_Item> format_items{};
        format_items.emplace_back();
        size_t pos = 0;
        while (pos != _npos && *(format + pos) != '\0') {
            if (*(format + pos) == '{' && *(format + pos + 1) != '\0') {
                pos++;
                switch (*(format + pos)) {
                    case '{':
                        {
                            format_items.back().content.append('{');
                            pos++;
                        }
                        break;
                    case '}':
                        {
                            format_items.back().arg_idx = format_items.size() - 1;
                            format_items.emplace_back();
                            pos++;
                        }
                        break;
                    case ':':
                        {
                            format_items.back().arg_idx = format_items.size() - 1;
                            format_items.back().max_length = _String_To_Int(format + pos + 1);
                            format_items.emplace_back();
                            pos = _Find_Char(format, '}', (pos + 1)) + 1;
                        }
                        break;
                    case '0':
                    case '1':
                    case '2':
                    case '3':
                    case '4':
                    case '5':
                    case '6':
                    case '7':
                    case '8':
                    case '9':
                        {
                            format_items.back().arg_idx = _String_To_Int(format + pos);
                            auto next_pos = _Find_Char(format, '}', (pos + 1)) + 1;
                            auto size_ch_pos = _Find_Char(format, ':', (pos + 1));
                            if (size_ch_pos != _npos && size_ch_pos < next_pos) {
                                format_items.back().max_length = _String_To_Int(format + size_ch_pos + 1);
                            }
                            format_items.emplace_back();
                            pos = next_pos;
                        }
                        break;
                    default:
                        throw FormatExcept("Invalid format rule");
                }
            } else if (*(format + pos) == '}') {
                if (*(format + pos + 1) == '}') {
                    format_items.back().content.append('}');
                    pos += 2;
                } else {
                    throw FormatExcept("Invalid format rule");
                }
            } else {
                format_items.back().content.append(*(format + pos));
                pos++;
            }
        }
        if (pos == _npos) {
            throw FormatExcept("Invalid format rule");
        }
        return format_items;
    }

    template <typename... _Args>
    inline std::string Format(std::string_view format, const _Args &...args) 
    {
        _Format_String content{};
        std::vector<_Format_Item> format_items(_Format_Impl(format.data()));
        std::vector<_Format_String> args_list(_Args_Impl(format_items.size(), args...));
        for (auto iter = format_items.begin(); iter < format_items.end(); ++iter) {
            content.append(iter->content);
            if (iter->arg_idx != -1) {
                if (iter->arg_idx >= static_cast<int>(args_list.size())) {
                    throw FormatExcept("Argument index out of bound");
                }
                if (iter->max_length < INT_MAX) {
                    args_list[iter->arg_idx].shrink(iter->max_length);
                    content.append(args_list[iter->arg_idx]);
                } else {
                    content.append(args_list[iter->arg_idx]);
                }
            }
        }
        return content.data();
    }

    inline std::string Format(std::string_view format)
    {
        return format.data();
    }

    template <typename... _Args>
    inline int Println(std::string_view format, const _Args &...args) 
    {
        return std::puts(Format(format, args...).data());
    }

    inline int Println(std::string_view format) noexcept
    {
        return std::puts(format.data());
    }

    template <typename _Ty>
    inline std::string To_String(const _Ty &value)
    {
        return _To_Format_String(value).data();
    }

    inline std::string CFormat(const char* format, ...)
    {
        std::string content{};
        int len = 0;
        {
            va_list args{};
            va_start(args, format);
            len = std::vsnprintf(nullptr, 0, format, args) + 1;
            va_end(args);
        }
        if (len > 1) {
            auto buffer = new char[len];
            std::memset(buffer, 0, len);
            va_list args{};
            va_start(args, format);
            std::vsnprintf(buffer, len, format, args);
            va_end(args);
            content = buffer;
            delete[] buffer;
        }
        return content;
    }
}

#endif // !defined(_CU_FORMAT_)
