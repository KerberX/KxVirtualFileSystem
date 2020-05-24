#pragma once
#include "KxVirtualFileSystem/Common.hpp"
#include "KxVirtualFileSystem/Misc/IncludeWindows.h"
#include "ServiceConstants.h"
#include "ServiceHandle.h"

namespace KxVFS
{
	class KxVFS_API ServiceManager final
	{
		private:
			ServiceHandle m_Handle;

		public:
			ServiceManager() = default;
			ServiceManager(FlagSet<ServiceManagerAccess> accessMode) noexcept
			{
				Open(accessMode);
			}

		public:
			bool IsOK() const noexcept
			{
				return m_Handle.IsValid();
			}
			bool Open(FlagSet<ServiceManagerAccess> accessMode) noexcept;

		public:
			operator SC_HANDLE() const noexcept
			{
				return m_Handle;
			}

			explicit operator bool() const noexcept
			{
				return IsOK();
			}
			bool operator!() const noexcept
			{
				return !IsOK();
			}
	};
}
