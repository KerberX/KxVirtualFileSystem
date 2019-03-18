/*
Copyright © 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/Misc/IncludeWindows.h"
#include "KxVirtualFileSystem/Utility/EnumClassOperations.h"

namespace KxVFS
{
	enum class ServiceStartType: uint32_t
	{
		Auto = SERVICE_AUTO_START,
		Boot = SERVICE_BOOT_START,
		System = SERVICE_SYSTEM_START,
		OnDemand = SERVICE_DEMAND_START,
		Disabled = SERVICE_DISABLED,
	};
	KxVFS_AllowEnumBitwiseOp(ServiceStartType);

	enum class ServiceStatus: uint32_t
	{
		Unknown = 0,

		Stopped = SERVICE_STOPPED,
		Paused = SERVICE_PAUSED,
		Running = SERVICE_RUNNING,

		PendingStart = SERVICE_START_PENDING,
		PendingStop = SERVICE_STOP_PENDING,
		PendingPause = SERVICE_PAUSE_PENDING,
		PendingContinue = SERVICE_CONTINUE_PENDING,
	};
	KxVFS_AllowEnumBitwiseOp(ServiceStatus);

	enum class ServiceAccess: uint32_t
	{
		None = 0,

		QueryConfig = SERVICE_QUERY_CONFIG,
		QueryStatus = SERVICE_QUERY_STATUS,
		ChangeConfig = SERVICE_CHANGE_CONFIG,
		EnumerateDependents = SERVICE_ENUMERATE_DEPENDENTS,

		Start = SERVICE_START,
		Stop = SERVICE_STOP,
		Interrogate = SERVICE_INTERROGATE,
		PauseContinue = SERVICE_PAUSE_CONTINUE,
		UserDefinedControl = SERVICE_USER_DEFINED_CONTROL,

		All = SERVICE_ALL_ACCESS,
	};
	KxVFS_AllowEnumBitwiseOp(ServiceAccess);
}