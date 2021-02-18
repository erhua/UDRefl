#pragma once

#include <UTemplate/Func.h>

#include <cstdint>
#include <functional>
#include <iterator>

#define UBPA_UDREFL_ENUM_BOOL_OPERATOR_DEFINE(Name)                      \
constexpr Name operator & (const Name& lhs, const Name& rhs) noexcept {  \
    static_assert(std::is_enum_v<Name>);                                 \
    using T = std::underlying_type_t<Name>;                              \
    return static_cast<Name>(static_cast<T>(lhs) & static_cast<T>(rhs)); \
}                                                                        \
constexpr Name operator | (const Name& lhs, const Name& rhs) noexcept {  \
    static_assert(std::is_enum_v<Name>);                                 \
    using T = std::underlying_type_t<Name>;                              \
    return static_cast<Name>(static_cast<T>(lhs) | static_cast<T>(rhs)); \
}                                                                        \
constexpr Name operator ~ (const Name& e) noexcept {                     \
    static_assert(std::is_enum_v<Name>);                                 \
    using T = std::underlying_type_t<Name>;                              \
    return static_cast<Name>(~static_cast<T>(e));                        \
}                                                                        \
constexpr Name& operator &= (Name& lhs, const Name& rhs) noexcept {      \
    static_assert(std::is_enum_v<Name>);                                 \
    using T = std::underlying_type_t<Name>;                              \
    lhs = static_cast<Name>(static_cast<T>(lhs) & static_cast<T>(rhs));  \
    return lhs;                                                          \
}                                                                        \
constexpr Name& operator |= (Name& lhs, const Name& rhs) noexcept {      \
    static_assert(std::is_enum_v<Name>);                                 \
    using T = std::underlying_type_t<Name>;                              \
    lhs = static_cast<Name>(static_cast<T>(lhs) | static_cast<T>(rhs));  \
    return lhs;                                                          \
}

namespace Ubpa::UDRefl {
	using Offsetor = std::function<void*(void*)>;
	using Destructor = std::function<void(const void*)>;
	using DeleteFunc = std::function<void(void*)>; // destruct + free

	template<typename Obj, typename T>
	std::size_t field_forward_offset_value(T Obj::* field_ptr) noexcept {
		static_assert(!std::is_function_v<T>);
		static_assert(!has_virtual_base_v<Obj>);
		return reinterpret_cast<std::size_t>(&(reinterpret_cast<const Obj*>(0)->*field_ptr));
	}

	template<auto fieldptr>
	struct field_offsetor_impl;

	template<typename Obj, typename T, T Obj::* fieldptr>
	struct field_offsetor_impl<fieldptr> {
		static_assert(!std::is_function_v<T>);
		static constexpr auto get() noexcept {
			return [](void* ptr) noexcept -> void* {
				return &(reinterpret_cast<Obj*>(ptr)->*fieldptr);
			};
		}
	};

	template<auto fieldptr>
	constexpr auto field_offsetor() noexcept {
		return field_offsetor_impl<fieldptr>::get();
	}

	// result size of field_offsetor(fieldptr) > result size of field_offsetor<fieldptr> 
	template<typename T, typename Obj>
	constexpr auto field_offsetor(T Obj::* fieldptr) noexcept {
		static_assert(!std::is_function_v<T>);
		return [fieldptr](void* ptr) noexcept -> void* {
			return &(reinterpret_cast<Obj*>(ptr)->*fieldptr);
		};
	}

	struct InheritCastFunctions {
		Offsetor static_derived_to_base;
		Offsetor static_base_to_derived;
		Offsetor dynamic_base_to_derived;
	};

	template<typename From, typename To>
	constexpr auto static_cast_functor() noexcept {
		static_assert(!is_virtual_base_of_v<From, To>);
		return [](void* obj) noexcept -> void* {
			return static_cast<To*>(reinterpret_cast<From*>(obj));
		};
	}

	template<typename Base, typename Derived>
	constexpr auto dynamic_cast_function() noexcept {
		static_assert(std::is_base_of_v<Base, Derived>);
		if constexpr (std::is_polymorphic_v<Base>) {
			return [](void* obj) noexcept -> void* {
				return dynamic_cast<Derived*>(reinterpret_cast<Base*>(obj));
			};
		}
		else
			return static_cast_functor<Base, Derived>();
	}

	// polymorphic: dynamic_cast
	// virtual    : no static_cast (Base -> Derived)
	template<typename Derived, typename Base>
	InheritCastFunctions inherit_cast_functions() {
		static_assert(std::is_base_of_v<Base, Derived>);
		if constexpr (std::is_polymorphic_v<Derived>) {
			if constexpr (is_virtual_base_of_v<Base, Derived>) {
				return {
					static_cast_functor<Derived, Base>(),
					nullptr,
					dynamic_cast_function<Base, Derived>()
				};
			}
			else {
				return {
					static_cast_functor<Derived, Base>(),
					static_cast_functor<Base, Derived>(),
					dynamic_cast_function<Base, Derived>()
				};
			}
		}
		else{
			if constexpr (is_virtual_base_of_v<Base, Derived>) {
				return {
					static_cast_functor<Derived, Base>(),
					nullptr,
					nullptr
				};
			}
			else {
				return {
					static_cast_functor<Derived, Base>(),
					static_cast_functor<Base, Derived>(),
					nullptr
				};
			}
		}
	}

	template<typename T>
	Destructor destructor() {
		if constexpr (std::is_fundamental_v<T>)
			return {};
		else {
			static_assert(std::is_destructible_v<T>);
			if constexpr (!std::is_trivially_destructible_v<T>) {
				return [](const void* ptr) {
					reinterpret_cast<const T*>(ptr)->~T();
				};
			}
			else
				return {};
		}
	}

	constexpr void* forward_offset(void* ptr, std::size_t offset) noexcept {
		return (std::uint8_t*)ptr + offset;
	}

	constexpr const void* forward_offset(const void* ptr, std::size_t offset) noexcept {
		return forward_offset(const_cast<void*>(ptr), offset);
	}

	constexpr void* backward_offset(void* ptr, std::size_t offset) noexcept {
		return (std::uint8_t*)ptr - offset;
	}

	constexpr const void* backward_offset(const void* ptr, std::size_t offset) noexcept {
		return backward_offset(const_cast<void*>(ptr), offset);
	}

	template<typename T>
	constexpr T& buffer_get(void* buffer, std::size_t offset) noexcept {
		auto ptr = forward_offset(buffer, offset);
		return *reinterpret_cast<T*>(ptr);
	}

	template<typename T>
	constexpr const T& buffer_get(const void* buffer, std::size_t offset) noexcept {
		return buffer_get<T>(const_cast<void*>(buffer), offset);
	}

	template<typename T>
	constexpr T& buffer_as(void* buffer) noexcept {
		return buffer_get<T>(buffer, 0);
	}

	template<typename T>
	constexpr const T& buffer_as(const void* buffer) noexcept {
		return buffer_get<T>(buffer, 0);
	}

	template<typename T>
	constexpr T* ptr_const_cast(const T* ptr) noexcept {
		return const_cast<T*>(ptr);
	}

	//
	// Wrap
	/////////
	//
	// - if result is reference, function will store a pointer in the result buffer
	//

	// pointer const array type (pointer is const, and pointer to non - const / referenced object)
	using ArgPtrBuffer = void* const*;

	// [func_ptr]
	// - Func Obj::* : Func isn't && (const && is ok)
	// - Func*
	// [result]
	// - type : void(void* obj, void* result_buffer, ArgPtrBuffer argptr_buffer)
	// - size : 1
	template<auto func_ptr>
	constexpr auto wrap_member_function() noexcept;

	// [func_ptr]
	// - Func*
	// [result]
	// - type : void(void*, void* result_buffer, ArgPtrBuffer argptr_buffer)
	// - size : 1
	template<auto func_ptr>
	constexpr auto wrap_static_function() noexcept;

	// static dispatch to
	// - wrap_member_function
	// - wrap_static_function
	template<auto func_ptr>
	constexpr auto wrap_function() noexcept;

	// Func: Ret(const? volatile? Object&, Args...)
	// [result]
	// - type : void(void* obj, void* result_buffer, ArgPtrBuffer argptr_buffer)
	// - size : sizeof(Func)
	template<typename Func>
	constexpr auto wrap_member_function(Func&& func) noexcept;

	// Func: Ret(Args...)
	// [result]
	// - type : void(void*, void* result_buffer, ArgPtrBuffer argptr_buffer)
	// - size : sizeof(Func)
	template<typename Func>
	constexpr auto wrap_static_function(Func&& func) noexcept;
	
	template<typename Enum> requires std::is_enum_v<Enum>
	constexpr decltype(auto) enum_cast(Enum&& e) noexcept;
	template<typename Enum> requires std::is_enum_v<Enum>
	constexpr bool enum_empty(const Enum& e) noexcept;
	template<typename Enum> requires std::is_enum_v<Enum>
	constexpr bool enum_single(const Enum& e) noexcept;
	template<typename Enum> requires std::is_enum_v<Enum>
	constexpr bool enum_contain_any(const Enum& e, const Enum& flag) noexcept;
	template<typename Enum> requires std::is_enum_v<Enum>
	constexpr bool enum_contain(const Enum& e, const Enum& flag) noexcept;
	template<typename Enum> requires std::is_enum_v<Enum>
	constexpr Enum enum_combine(std::initializer_list<Enum> flags) noexcept;
	template<typename Enum> requires std::is_enum_v<Enum>
	constexpr Enum enum_remove(const Enum& e, const Enum& flag) noexcept;
	template<typename Enum> requires std::is_enum_v<Enum>
	constexpr Enum enum_within(const Enum& e, const Enum& flag) noexcept;

	////////////
	// Traits //
	////////////

	//
	// operation
	//////////////

	template<typename T>
	concept operator_bool = std::convertible_to<T, bool>;

	template<typename T>
	concept operator_plus = requires(T t) { +t; };
	template<typename T>
	concept operator_minus = !std::is_unsigned_v<T> && requires(T t) { -t; };

	template<typename T>
	concept operator_add = requires(T lhs, T rhs) { lhs + rhs; };
	template<typename T>
	concept operator_sub = requires(T lhs, T rhs) { lhs - rhs; };
	template<typename T>
	concept operator_mul = requires(T lhs, T rhs) { lhs * rhs; };
	template<typename T>
	concept operator_div = !std::is_same_v<std::remove_cvref_t<T>, bool> && requires(T lhs, T rhs) { lhs / rhs; };
	template<typename T>
	concept operator_mod = !std::is_same_v<std::remove_cvref_t<T>, bool> && requires(T lhs, T rhs) { lhs % rhs; };

	template<typename T>
	concept operator_bnot = !std::is_same_v<std::remove_cvref_t<T>, bool> && requires(T t) { ~t; };
	template<typename T>
	concept operator_band = requires(T lhs, T rhs) { lhs & rhs; };
	template<typename T>
	concept operator_bor = requires(T lhs, T rhs) { lhs | rhs; };
	template<typename T>
	concept operator_bxor = requires(T lhs, T rhs) { lhs ^ rhs; };
	template<typename T, typename U>
	concept operator_lshift = requires(T lhs, U rhs) { lhs << rhs; };
	template<typename T, typename U>
	concept operator_rshift = requires(T lhs, U rhs) { lhs >> rhs; };

	template<typename T>
	concept operator_pre_inc = !std::is_same_v<T, bool> && requires(T t) { ++t; };
	template<typename T>
	concept operator_post_inc = !std::is_same_v<T, bool> && requires(T t) { t++; };
	template<typename T>
	concept operator_pre_dec = !std::is_same_v<T, bool> && requires(T t) { --t; };
	template<typename T>
	concept operator_post_dec = !std::is_same_v<T, bool> && requires(T t) { t--; };

	template<typename T>
	concept operator_assign_copy = std::is_copy_assignable_v<T> && requires(T lhs, const T & rhs) { {lhs = rhs}->std::same_as<T&>; };
	template<typename T>
	concept operator_assign_move = std::is_move_assignable_v<T> && requires(T lhs, T rhs) { {lhs = std::move(rhs) }->std::same_as<T&>; };
	template<typename T>
	concept operator_assign_add = !std::is_same_v<T, bool> && requires(T lhs, const T & rhs) { {lhs += rhs }->std::same_as<T&>; };
	template<typename T>
	concept operator_assign_sub = !std::is_same_v<T, bool> && requires(T lhs, const T & rhs) { {lhs -= rhs }->std::same_as<T&>; };
	template<typename T>
	concept operator_assign_mul = !std::is_same_v<T, bool> && requires(T lhs, const T & rhs) { {lhs *= rhs }->std::same_as<T&>; };
	template<typename T>
	concept operator_assign_div = !std::is_same_v<T, bool> && requires(T lhs, const T & rhs) { {lhs /= rhs }->std::same_as<T&>; };
	template<typename T>
	concept operator_assign_mod = !std::is_same_v<T, bool> && requires(T lhs, const T & rhs) { {lhs %= rhs }->std::same_as<T&>; };
	template<typename T>
	concept operator_assign_band = !std::is_same_v<T, bool> && requires(T lhs, const T & rhs) { {lhs &= rhs }->std::same_as<T&>; };
	template<typename T>
	concept operator_assign_bor = !std::is_same_v<T, bool> && requires(T lhs, const T & rhs) { {lhs |= rhs }->std::same_as<T&>; };
	template<typename T>
	concept operator_assign_bxor = !std::is_same_v<T, bool> && requires(T lhs, const T & rhs) { {lhs ^= rhs }->std::same_as<T&>; };
	template<typename T>
	concept operator_assign_lshift = !std::is_same_v<T, bool> && requires(T lhs, const T & rhs) { {lhs <<= rhs }->std::same_as<T&>; };
	template<typename T>
	concept operator_assign_rshift = !std::is_same_v<T, bool> && requires(T lhs, const T & rhs) { {lhs >>= rhs }->std::same_as<T&>; };

	template<typename T>
	concept operator_eq = !std::is_array_v<T> && requires(const T & lhs, const T & rhs) { {lhs == rhs }->std::convertible_to<bool>; };
	template<typename T>
	concept operator_ne = !std::is_array_v<T> && requires(const T & lhs, const T & rhs) { {lhs != rhs }->std::convertible_to<bool>; };
	template<typename T>
	concept operator_lt = !std::is_array_v<T> && requires(const T & lhs, const T & rhs) { {lhs <  rhs }->std::convertible_to<bool>; };
	template<typename T>
	concept operator_le = !std::is_array_v<T> && requires(const T & lhs, const T & rhs) { {lhs <= rhs }->std::convertible_to<bool>; };
	template<typename T>
	concept operator_gt = !std::is_array_v<T> && requires(const T & lhs, const T & rhs) { {lhs >  rhs }->std::convertible_to<bool>; };
	template<typename T>
	concept operator_ge = !std::is_array_v<T> && requires(const T & lhs, const T & rhs) { {lhs >= rhs }->std::convertible_to<bool>; };

	template<typename T, typename U>
	concept operator_subscript = !std::is_void_v<std::remove_pointer_t<T>> && requires(T lhs, const U & rhs) { lhs[rhs]; };
	template<typename T>
	concept operator_deref = requires(T t) { *t; };
	template<typename T>
	concept operator_ref = requires(T t) { &t; };

	//
	// pair
	///////////

	template<typename T>
	concept pair_first_type = requires{ typename T::first_type; };
	template<typename T>
	concept pair_second_type = requires{ typename T::second_type; };
	template<typename T>
	concept pair_first = requires(T t) { t.first; };
	template<typename T>
	concept pair_second = requires(T t) { t.second; };
	
	//
	// tuple
	//////////
	template<typename T>
	concept tuple_size = requires() { std::tuple_size<T>::value; };
	template<typename T, std::size_t Idx>
	concept tuple_element = requires() { typename std::tuple_element<Idx, T>::type; };

	//
	// container
	//////////////

	// - member type
	// - > key_type
	// - > mapped_type
	// - > value_type
	// - > allocator_type
	// - > size_type
	// - > difference_type
	// - > pointer
	// - > const_pointer
	// - > key_compare
	// - > value_coompare
	// - > iterator
	// - > const_iterator
	// - > local_iterator
	// - > const_local_iterator
	// - > reverse_iterator
	// - > const_reverse_iterator
	// - > node_type
	// - > insert_return_type
	// - > > position
	// - > > inserted
	// - > > node

	template<typename T>
	concept container_key_type = requires {typename T::key_type; };
	template<typename T>
	concept container_mapped_type = requires { typename T::mapped_type; };
	template<typename T>
	concept container_value_type = requires { typename T::value_type; };
	template<typename T>
	concept container_allocator_type = requires { typename T::allocator_type; };
	template<typename T>
	concept container_size_type = requires { typename T::size_type; };
	template<typename T>
	concept container_difference_type = requires { typename T::difference_type; };
	template<typename T>
	concept container_pointer_type = requires { typename T::pointer; };
	template<typename T>
	concept container_const_pointer_type = requires { typename T::const_pointer; };
	template<typename T>
	concept container_key_compare = requires { typename T::key_compare; };
	template<typename T>
	concept container_value_compare = requires { typename T::value_coompare; };
	template<typename T>
	concept container_iterator = requires { typename T::iterator; };
	template<typename T>
	concept container_const_iterator = requires { typename T::const_iterator; };
	template<typename T>
	concept container_reverse_iterator = requires { typename T::reverse_iterator; };
	template<typename T>
	concept container_const_reverse_iterator = requires { typename T::const_reverse_iterator; };
	template<typename T>
	concept container_local_iterator = requires { typename T::local_iterator; };
	template<typename T>
	concept container_const_local_iterator = requires { typename T::const_local_iterator; };
	template<typename T>
	concept container_node_type = requires { typename T::node_type; };
	template<typename T>
	concept container_insert_return_type = requires { typename T::insert_return_type; };

	template<typename T>
	concept container_assign = container_size_type<T> && container_value_type<T> && requires(T t, const typename T::size_type& s, const typename T::value_type& v) { t.assgin(s, v); };

	// - iterator

	template<typename T>
	concept container_begin = requires(T t) { std::begin(t); };
	template<typename T>
	concept container_cbegin = requires(const T& t) { std::cbegin(t); };
	template<typename T>
	concept container_rbegin = requires(T t) { std::rbegin(t); };
	template<typename T>
	concept container_crbegin = requires(const T & t) { std::crbegin(t); };
	template<typename T>
	concept container_end = requires(T t) { std::end(t); };
	template<typename T>
	concept container_cend = requires(const T & t) { std::cend(t); };
	template<typename T>
	concept container_rend = requires(T t) { std::rend(t); };
	template<typename T>
	concept container_crend = requires(const T & t) { std::crend(t); };

	// - element access

	template<typename T, typename U>
	concept container_at = requires(T t, const U& key) { t.at(key); };
	template<typename T>
	concept container_at_size = container_size_type<T> && container_at<T, typename T::size_type>;
	template<typename T>
	concept container_at_key = container_key_type<T> && container_at<T, typename T::key_type>;
	template<typename T, typename U>
	concept container_subscript = requires(T t, U key) { t[std::forward<U>(key)]; };
	template<typename T>
	concept container_subscript_size = container_size_type<T> && container_subscript<T, const typename T::size_type&>;
	template<typename T>
	concept container_subscript_key_cl = container_key_type<T> && container_subscript<T, const typename T::key_type&>;
	template<typename T>
	concept container_subscript_key_r = container_key_type<T> && container_subscript<T, typename T::key_type>;
	template<typename T>
	concept container_data = requires(T t) { std::data(t); };
	template<typename T>
	concept container_front = requires(T t) { t.front(); };
	template<typename T>
	concept container_back = requires(T t) { t.back(); };

	// - capacity

	template<typename T>
	concept container_empty = requires(const T & t) { {std::empty(t)}->std::convertible_to<bool>; };
	template<typename T>
	concept container_size = requires(const T & t) { {std::size(t)}->std::convertible_to<std::size_t>; };
	template<typename T>
	concept container_resize_cnt = container_size_type<T> && requires(T t, const typename T::size_type& cnt) { t.resize(cnt); };
	template<typename T>
	concept container_resize_cnt_value = container_size_type<T> && container_value_type<T>
		&& requires(T t, const typename T::size_type & cnt, const typename T::value_type& value) { t.resize(cnt, value); };
	template<typename T>
	concept container_capacity = requires(const T & t) { {t.capacity()}->std::convertible_to<std::size_t>; };
	template<typename T>
	concept container_bucket_count = requires(const T & t) { {t.bucket_count()}->std::convertible_to<std::size_t>; };
	template<typename T>
	concept container_reserve = container_size_type<T> && requires(T t, const typename T::size_type & cnt) { t.reserve(cnt); };
	template<typename T>
	concept container_shrink_to_fit = container_size_type<T> && requires(T t) { t.shrink_to_fit(); };

	// - modifiers

	template<typename T>
	concept container_clear = container_size_type<T> && requires(T t) { t.clear(); };

	template<typename T, typename V>
	concept container_insert = requires(T t, V value) { t.insert(std::forward<V>(value)); };
	template<typename T>
	concept container_insert_clvalue = container_value_type<T> && container_insert<T, const typename T::value_type&>;
	template<typename T>
	concept container_insert_rvalue = container_value_type<T> && container_insert<T, typename T::value_type&&>;
	template<typename T>
	concept container_insert_rnode = container_node_type<T> && container_insert<T, typename T::node_type&&>;

	template<typename T, typename V>
	concept container_insert_citer = container_const_iterator<T>
		&& requires(T t, const typename T::const_iterator& iter, V value) { t.insert(iter, std::forward<V>(value)); };
	template<typename T>
	concept container_insert_citer_clvalue = container_value_type<T> && container_insert_citer<T, const typename T::value_type&>;
	template<typename T>
	concept container_insert_citer_rvalue = container_value_type<T> && container_insert_citer<T, typename T::value_type&&>;
	template<typename T>
	concept container_insert_citer_rnode = container_node_type<T> && container_insert_citer<T, typename T::node_type&&>;

	template<typename T, typename S, typename V>
	concept container_insert_citer_size = container_const_iterator<T>
		&& requires(T t, const typename T::const_iterator & iter, const S& size, const V& value) { t.insert(iter, size, value); };
	template<typename T>
	concept container_insert_citer_size_value = container_value_type<T> && container_size_type<T>
		&& container_insert_citer_size<T, typename T::size_type, typename T::value_type>;

	template<typename T, typename U, typename V>
	concept container_insert_or_assign = requires(T t, U u, V v) { t.insert_or_assign(std::forward<U>(u), std::forward<V>(v)); };
	template<typename T>
	concept container_insert_or_assign_clkey_rmap = container_key_type<T> && container_mapped_type<T>
		&& container_insert_or_assign<T, const typename T::key_type&, typename T::mapped_type&&>;
	template<typename T>
	concept container_insert_or_assign_rkey_rmap = container_key_type<T> && container_mapped_type<T>
		&& container_insert_or_assign<T, typename T::key_type&&, typename T::mapped_type&&>;

	template<typename T, typename U, typename V>
	concept container_insert_or_assign_citer = container_const_iterator<T>
		&& requires(T t, const typename T::const_iterator& citer, U u, V v) { t.insert_or_assign(citer, std::forward<U>(u), std::forward<V>(v)); };
	template<typename T>
	concept container_insert_or_assign_citer_clkey_rmap = container_key_type<T> && container_mapped_type<T>
		&& container_insert_or_assign_citer<T, const typename T::key_type&, typename T::mapped_type&&>;
	template<typename T>
	concept container_insert_or_assign_citer_rkey_rmap = container_key_type<T> && container_mapped_type<T>
		&& container_insert_or_assign_citer<T, typename T::key_type&&, typename T::mapped_type&&>;

	template<typename T, typename U>
	concept container_erase = requires(T t, const U& u) { t.erase(u); };
	template<typename T>
	concept container_erase_citer = container_const_iterator<T>&& container_erase<T, typename T::const_iterator>;
	template<typename T>
	concept container_erase_key = container_key_type<T> && container_erase<T, typename T::key_type>;

	template<typename T, typename U>
	concept container_erase_range = requires(T t, const U & b, const U & e) { t.erase(b, e); };
	template<typename T>
	concept container_erase_range_citer = container_const_iterator<T> && container_erase_range<T, typename T::const_iterator>;

	template<typename T, typename U>
	concept container_push_front = requires(T t, U u) { t.push_front(std::forward<U>(u)); };
	template<typename T>
	concept container_push_front_clvalue = container_value_type<T>&& container_push_front<T, const typename T::value_type&>;
	template<typename T>
	concept container_push_front_rvalue = container_value_type<T>&& container_push_front<T, typename T::value_type&&>;

	template<typename T>
	concept container_pop_front = requires(T t) { t.pop_front(); };

	template<typename T, typename U>
	concept container_push_back = requires(T t, U u) { t.push_back(std::forward<U>(u)); };
	template<typename T>
	concept container_push_back_clvalue = container_value_type<T> && container_push_back<T, const typename T::value_type&>;
	template<typename T>
	concept container_push_back_rvalue = container_value_type<T> && container_push_back<T, typename T::value_type&&>;

	template<typename T>
	concept container_pop_back = requires(T t) { t.pop_back(); };

	template<typename T>
	concept container_swap = requires(T lhs, T rhs) { std::swap(lhs, rhs); };

	template<typename T, typename U>
	concept container_merge = requires(T t, U u) { t.merge(std::forward<U>(u)); };
	template<typename T>
	concept container_merge_l = container_merge<T, T&>;
	template<typename T>
	concept container_merge_r = container_merge<T, T&&>;

	template<typename T, typename U>
	concept container_extract = requires(T t, const U& u) { t.extract(u); };
	template<typename T>
	concept container_extract_citer = container_const_iterator<T> && container_extract<T, typename T::const_iterator>;
	template<typename T>
	concept container_extract_key = container_key_type<T> && container_extract<T, typename T::key_type>;

	// - lookup

	template<typename T>
	concept container_count = container_key_type<T> && requires(const T& t, const typename T::key_type & u) { {t.count(u)}->std::convertible_to<std::size_t>; };

	template<typename T>
	concept container_find = container_key_type<T> && requires(T t, const typename T::key_type & u) { t.find(u); };

	template<typename T>
	concept container_contains = container_key_type<T> && requires(const T & t, const typename T::key_type & u) { {t.count(u)}->std::convertible_to<bool>; };

	template<typename T>
	concept container_lower_bound = container_key_type<T> && requires(T t, const typename T::key_type & u) { t.lower_bound(u); };

	template<typename T>
	concept container_upper_bound = container_key_type<T> && requires(T t, const typename T::key_type & u) { t.upper_bound(u); };

	template<typename T>
	concept container_equal_range = container_key_type<T> && requires(T t, const typename T::key_type& u) { t.equal_range(u); };

	// - list operations (TODO)
}

#include "details/Util.inl"
