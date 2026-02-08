#ifndef LUA_GENERICS_HPP
#define LUA_GENERICS_HPP
#ifdef __cplusplus
extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}
#endif
#include <format>
namespace boot::lua {

    namespace {
        template <typename T>
        void genericpush(lua_State* L, const T& val)
        {
            if constexpr (std::is_same_v<T, bool>) {
                lua_pushboolean(L, val);
            } else if constexpr (std::is_integral_v<T>) {
                lua_pushinteger(L, val);
            } else if constexpr (std::is_floating_point_v<T>) {
                lua_pushnumber(L, val);
            } else if constexpr (std::is_same_v<T, const char*>) {
                lua_pushstring(L, val);
            } else {
                static_assert(false, "Unsupported type T");
            }
        }
        template <typename T>
        inline std::optional<T> genericget(lua_State* L)
        {
            if (lua_isnil(L, -1))
                return std::nullopt;
            bool check = false;
            if constexpr (std::is_same_v<T, bool>) {
                check = lua_isboolean(L, -1);
            } else if constexpr (std::is_integral_v<T>) {
                check = lua_isinteger(L, -1);
            } else if constexpr (std::is_floating_point_v<T>) {
                check = lua_isnumber(L, -1);
            } else if constexpr (std::is_same_v<T, const char*> || std::is_same_v<T, std::string>) {
                check = lua_isstring(L, -1);
            } else {
                static_assert(false, "Unsupported type T");
            }
            if (!check) {
                lua_settop(L, 0);
                return std::nullopt;
            }
            std::optional<T> ret;
            if constexpr (std::is_same_v<T, bool>) {
                ret = lua_toboolean(L, -1);
            } else if constexpr (std::is_integral_v<T>) {
                ret = lua_tointeger(L, -1);
            } else if constexpr (std::is_floating_point_v<T>) {
                ret = lua_tonumber(L, -1);
            } else if constexpr (std::is_same_v<T, const char*>) {
                ret = lua_tostring(L, -1);
            } else if constexpr (std::is_same_v<T, std::string>) {
                auto tmp = lua_tostring(L, -1);
                ret = std::string(tmp);
            }
            lua_settop(L, 0);
            return ret;
        }
    }
    template <int RetCount, typename... Args>
    inline void genericpcall(lua_State* L, const char* function, Args const&... args)
    {
        if (function) {
            lua_getglobal(L, function);
            if (!lua_isfunction(L, -1)) {
                lua_settop(L, 0);
                throw std::runtime_error(std::format("Lua function '{}' does not exist", function));
            }
        }
        constexpr const size_t argcount = sizeof...(args);
        if constexpr (argcount > 0) {
            (genericpush(L, args), ...);
        }
        auto status = lua_pcall(L, argcount, RetCount, 0);
        if (status != LUA_OK) {
            auto error = lua_tostring(L, -1);
            lua_settop(L, 0);
            throw std::runtime_error(error);
        }
    }
    template <typename T>
    inline std::optional<T> getglobalv(lua_State* L, const char* name)
    {
        lua_getglobal(L, name);
        return genericget<T>(L);
    }
    template <typename T>
    inline void setglobalv(lua_State* L, const char* name, const T& val)
    {
        genericpush(L, val);
        lua_setglobal(L, name);
    }

    template <typename... Args>
    inline void voidpcall(lua_State* L, const char* function, Args const&... args)
    {
        genericpcall<0>(L, function, args...);
        lua_settop(L, 0);
    }
    template <typename Ret, typename... Args>
    inline std::optional<Ret> pcall1(lua_State* L, const char* function, Args const&... args)
    {
        genericpcall<1>(L, function, args...);
        return genericget<Ret>(L);
    }

    namespace {
        template <typename Optional>
        struct StripOptional {
            using value_type = Optional;
        };

        template <typename T>
        struct StripOptional<std::optional<T>> {
            using value_type = T;
        };

        template <
            /// type of the incoming tuple that will hold the return values of the previous call
            typename Out,
            typename std::size_t N,
            typename Index = std::make_index_sequence<N>>
        inline void set_return_values(Out& ret, lua_State* L)
        {
            [&]<std::size_t... I>(std::index_sequence<I...>) {
                ((std::get<I>(ret) = genericget<typename StripOptional<std::tuple_element_t<I, Out>>::value_type>(L)), ...);
            }(Index {});
        }
        template <typename Tuple>
        struct ApplyOption;

        /// this type transforms a std::tuple<Ts...> into tuple<std::optional<Ts>...>
        template <typename... Ts>
        struct ApplyOption<std::tuple<Ts...>> {
            using value_type = std::tuple<std::optional<Ts>...>;
        };
    }

    template <typename... Elements>
    using return_t = std::tuple<Elements...>;

    template <
        /// Function return signature e.g: std::tuple<int, int>.
        typename RetSig,
        /// Used internally, wraps RetSig into std::optional so that the std::tuple<int, int> from the example
        /// becomes an std::tuple<std::optional<int>, std::optional<int>>. This is the actual return
        /// type of this function.
        typename Ret = ApplyOption<RetSig>::value_type,
        typename... Args>
    inline Ret pcall(lua_State* L, const char* function, Args const&... args)
    {
        constexpr const std::size_t retnum = std::tuple_size_v<RetSig>;
        genericpcall<retnum>(L, function, args...);
        Ret ret {};
        set_return_values<Ret, retnum>(ret, L);
        return ret;
    }
}
#endif
