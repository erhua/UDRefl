#pragma once

#include "FieldInfo.h"
#include "MethodInfo.h"
#include "BaseInfo.h"

namespace Ubpa::UDRefl {
	struct TypeInfo {
		std::unordered_map<size_t, std::any> attrs;
		std::unordered_map<size_t, FieldInfo> fieldinfos;
		std::unordered_multimap<size_t, MethodInfo> methodinfos;
		std::unordered_multimap<size_t, BaseInfo> baseinfos;
		std::unordered_multimap<size_t, VirtualBaseInfo> vbaseinfos;

		// without bases, read/write field, static non-const
		ObjectPtr RWField(size_t fieldID) const noexcept;
		// without bases, read/write field, static non-const + static const
		ConstObjectPtr RField(size_t fieldID) const noexcept;
		// without bases, read/write field, non-const + static non-const
		ObjectPtr RWField(void* obj, size_t fieldID) const noexcept;
		// without bases, read field, non-const + static non-const + static non-const + static const
		ConstObjectPtr RField(const void* obj, size_t fieldID) const noexcept;

		// without bases, static
		bool IsStaticInvocable(size_t methodID, Span<size_t> argTypeIDs) const noexcept;
		// without bases, const + static
		bool IsConstInvocable(size_t methodID, Span<size_t> argTypeIDs) const noexcept;
		// without bases, non-const + const + static
		bool IsInvocable(size_t methodID, Span<size_t> argTypeIDs) const noexcept;

		// without bases, static
		InvokeResult Invoke(size_t methodID, Span<size_t> argTypeIDs, void* buffer) const;
		// without bases, const + static
		InvokeResult Invoke(const void* obj, size_t methodID, Span<size_t> argTypeIDs, void* buffer) const;
		// without bases, non-const + const + static, non-const && static > const
		InvokeResult Invoke(void* obj, size_t methodID, Span<size_t> argTypeIDs, void* buffer) const;
	};
}
