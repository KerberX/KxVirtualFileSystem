#include "stdafx.h"
#include "KxVFS/FileSystemService.h"
#include "KxVFS/IFileSystem.h"
#include "KxVFS/DokanyFileSystem.h"
#include "KxVFS/Utility/DisableWOW64FSRedirection.h"
#include "KxVFS/Utility.h"
#include "KxVFS/Misc/IncludeDokan.h"
#include <exception>
#pragma comment(lib, "Dokan2.lib")

namespace KxVFS
{
	FileSystemService* g_FileSystemServiceInstance = nullptr;

	bool InitDokany(Dokany2::DOKAN_LOG_CALLBACKS* logCallbacks = nullptr)
	{
		return DokanyFileSystem::SafelyCallDokanyFunction([logCallbacks]()
		{
			Dokany2::DokanInit(nullptr, logCallbacks);
		});
	}
	bool UninitDokany()
	{
		return DokanyFileSystem::SafelyCallDokanyFunction([]()
		{
			Dokany2::DokanShutdown();
		});
	}

	bool operator==(const LUID& left, const LUID& right) noexcept
	{
		return left.HighPart == right.HighPart && left.LowPart == right.LowPart;
	}
	DynamicStringW ProcessDokanyLogString(KxVFS::DynamicStringRefW logString)
	{
		DynamicStringW temp = logString;
		temp.trim_linebreak_chars();
		temp.trim_space_chars();

		DynamicStringW fullLogString = L"<Dokany> ";
		fullLogString += temp;
		fullLogString.trim_space_chars();

		return fullLogString;
	}

	ServiceHandle OpenNamedService(ServiceManager& manager, DynamicStringRefW serviceName, ServiceAccess serviceAccess)
	{
		if (!serviceName.empty())
		{
			if (!manager && !manager.Open(ServiceManagerAccess::Connect))
			{
				return {};
			}

			ServiceHandle service;
			if (service.Open(manager, serviceName, serviceAccess))
			{
				return service;
			}
		}
		return {};
	}
}

namespace KxVFS
{
	FileSystemService* FileSystemService::GetInstance()
	{
		return g_FileSystemServiceInstance;
	}

	DynamicStringW FileSystemService::GetLibraryVersion()
	{
		return L"2.1.1";
	}
	DynamicStringW FileSystemService::GetDokanyVersion()
	{
		// Return 2.0 for now because this is the version of Dokany used in KxVFS.
		// It seems that 'DOKAN_VERSION' constant hasn't updated (maybe because 2.x still beta).
		#undef DOKAN_VERSION
		#define DOKAN_VERSION 200

		const DynamicStringW temp = Utility::FormatString(L"%1", DOKAN_VERSION);
		return Utility::FormatString(L"%1.%2.%3", temp[0], temp[1], temp[2]);
	}

	DynamicStringW FileSystemService::GetDokanyDefaultServiceName()
	{
		return DOKAN_DRIVER_SERVICE;
	}
	DynamicStringW FileSystemService::GetDokanyDefaultDriverPath()
	{
		// There's a 'DOKAN_DRIVER_FULL_PATH' constant in Dokany control app, but it's defined in the .cpp file
		return Utility::ExpandEnvironmentStrings(L"%SystemRoot%\\System32\\Drivers\\Dokan" DOKAN_MAJOR_API_VERSION L".sys");
	}
	bool FileSystemService::IsDokanyDefaultInstallPresent()
	{
		ServiceManager manager;
		if (OpenNamedService(manager, GetDokanyDefaultServiceName(), ServiceAccess::QueryStatus))
		{
			DynamicStringW driverPath = GetDokanyDefaultDriverPath();

			DisableWOW64FSRedirection disable;
			return Utility::IsFileExist(driverPath);
		}
		return false;
	}

	ServiceHandle FileSystemService::OpenService(ServiceManager& manager, ServiceAccess serviceAccess) const
	{
		return OpenNamedService(manager, m_ServiceName, serviceAccess);
	}

	bool FileSystemService::AddSeSecurityNamePrivilege()
	{
		LUID securityLuid = {};
		if (!::LookupPrivilegeValueW(nullptr, SE_SECURITY_NAME, &securityLuid))
		{
			if (::GetLastError() != ERROR_SUCCESS)
			{
				return false;
			}
		}

		LUID restoreLuid = {};
		if (!::LookupPrivilegeValueW(nullptr, SE_RESTORE_NAME, &restoreLuid))
		{
			if (::GetLastError() != ERROR_SUCCESS)
			{
				return false;
			}
		}

		LUID backupLuid = {};
		if (!::LookupPrivilegeValueW(nullptr, SE_BACKUP_NAME, &backupLuid))
		{
			if (::GetLastError() != ERROR_SUCCESS)
			{
				return false;
			}
		}

		constexpr size_t privilegesSize = sizeof(TOKEN_PRIVILEGES) + (sizeof(LUID_AND_ATTRIBUTES) * 2);
		alignas(TOKEN_PRIVILEGES) uint8_t tokenPrivilegesBuffer[privilegesSize] = {};
		alignas(TOKEN_PRIVILEGES) uint8_t oldTokenPrivilegesBuffer[privilegesSize] = {};
		TOKEN_PRIVILEGES& tokenPrivileges = *reinterpret_cast<TOKEN_PRIVILEGES*>(tokenPrivilegesBuffer);
		TOKEN_PRIVILEGES& oldTokenPrivileges = *reinterpret_cast<TOKEN_PRIVILEGES*>(oldTokenPrivilegesBuffer);

		tokenPrivileges.PrivilegeCount = 3;
		tokenPrivileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
		tokenPrivileges.Privileges[0].Luid = securityLuid;
		tokenPrivileges.Privileges[1].Attributes = SE_PRIVILEGE_ENABLED;
		tokenPrivileges.Privileges[1].Luid = restoreLuid;
		tokenPrivileges.Privileges[2].Attributes = SE_PRIVILEGE_ENABLED;
		tokenPrivileges.Privileges[2].Luid = backupLuid;

		GenericHandle processToken;
		if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES|TOKEN_QUERY, &processToken))
		{
			if (::GetLastError() != ERROR_SUCCESS)
			{
				return false;
			}
		}

		DWORD returnLength = 0;
		::AdjustTokenPrivileges(processToken, FALSE, &tokenPrivileges, static_cast<DWORD>(privilegesSize), &oldTokenPrivileges, &returnLength);
		if (::GetLastError() != ERROR_SUCCESS)
		{
			return false;
		}

		bool securityPrivilegePresent = false;
		bool restorePrivilegePresent = false;
		for (size_t i = 0; i < oldTokenPrivileges.PrivilegeCount && (!securityPrivilegePresent || !restorePrivilegePresent); i++)
		{
			if (oldTokenPrivileges.Privileges[i].Luid == securityLuid)
			{
				securityPrivilegePresent = true;
			}
			else if (oldTokenPrivileges.Privileges[i].Luid == restoreLuid)
			{
				restorePrivilegePresent = true;
			}
		}
		return true;
	}
	bool FileSystemService::InitDriver()
	{
		if (IsInstalled())
		{
			if (m_IsFSInitialized)
			{
				return true;
			}

			if constexpr(Setup::EnableLog)
			{
				Dokany2::DOKAN_LOG_CALLBACKS logCallbacks = {};
				logCallbacks.DbgPrint = [](const char* logString)
				{
					DynamicStringW logStringW = DynamicStringA::to_utf16(logString, DynamicStringA::npos, CP_ACP);
					DynamicStringW fullLogString = ProcessDokanyLogString(logStringW);

					g_FileSystemServiceInstance->GetLogger().Log(LogLevel::Info, fullLogString);
				};
				logCallbacks.DbgPrintW = [](const wchar_t* logString)
				{
					DynamicStringW fullLogString = ProcessDokanyLogString(logString);
					g_FileSystemServiceInstance->GetLogger().Log(LogLevel::Info, logString);
				};

				m_IsFSInitialized = InitDokany(&logCallbacks);
			}
			else
			{
				m_IsFSInitialized = InitDokany();
			}
			return m_IsFSInitialized;
		}
		return false;
	}

	FileSystemService::FileSystemService(DynamicStringRefW serviceName)
		:m_ServiceName(serviceName), m_ServiceManager(ServiceManagerAccess::Connect), m_HasSeSecurityNamePrivilege(AddSeSecurityNamePrivilege())
	{
		// Init instance pointer
		if (g_FileSystemServiceInstance)
		{
			throw std::logic_error("KxVFS: an instance of 'FileSystemService' already created");
		}
		g_FileSystemServiceInstance = this;

		// Init default logger
		if (Setup::Configuration::Debug)
		{
			m_ChainLogger.AddLogger(m_StdOutLogger);
		}
		m_ChainLogger.AddLogger(m_DebugLogger);

		// Init driver
		if (!serviceName.empty())
		{
			InitDriver();
		}
	}
	FileSystemService::~FileSystemService()
	{
		if (m_IsFSInitialized)
		{
			UninitDokany();
			m_IsFSInitialized = false;
		}

		g_FileSystemServiceInstance = nullptr;
	}

	bool FileSystemService::IsOK() const
	{
		return OpenService(ServiceAccess::QueryStatus);
	}
	bool FileSystemService::InitService(DynamicStringRefW name)
	{
		if (m_ServiceName.empty())
		{
			m_ServiceName = name;
			return InitDriver();
		}
		return false;
	}
	
	DynamicStringRefW FileSystemService::GetServiceName() const
	{
		return m_ServiceName;
	}
	bool FileSystemService::IsInstalled() const
	{
		return IsOK();
	}
	bool FileSystemService::IsStarted() const
	{
		return OpenService(ServiceAccess::QueryStatus).GetStatus() == ServiceStatus::Running;
	}

	bool FileSystemService::Start()
	{
		return OpenService(ServiceAccess::Start).Start();
	}
	bool FileSystemService::Stop()
	{
		return OpenService(ServiceAccess::Stop).Stop();
	}
	bool FileSystemService::Install(DynamicStringRefW binaryPath, DynamicStringRefW displayName, DynamicStringRefW description)
	{
		if (Utility::IsFileExist(binaryPath))
		{
			// Create new service or reconfigure existing
			constexpr ServiceStartMode startMode = ServiceStartMode::Auto;

			if (OpenService(ServiceAccess::QueryStatus))
			{
				if (ServiceHandle service = OpenService(ServiceAccess::ChangeConfig))
				{
					if (service.SetConfig(m_ServiceManager, binaryPath, ServiceType::FileSystemDriver, startMode, ServiceErrorControl::Ignore))
					{
						service.SetDescription(description);
						return true;
					}
				}
				return false;
			}
			else
			{
				ServiceManager manager(ServiceManagerAccess::CreateService);
				if (manager)
				{
					ServiceHandle service;
					return service.Create(manager,
										  startMode,
										  binaryPath,
										  m_ServiceName,
										  !displayName.empty() ? displayName : m_ServiceName,
										  description
					);
				}
			}
		}
		return false;
	}
	bool FileSystemService::Uninstall()
	{
		return OpenService(ServiceAccess::Delete).Delete();
	}

	bool FileSystemService::UseDefaultDokanyInstallation()
	{
		DynamicStringW binaryPath = GetDokanyDefaultDriverPath();
		if (DisableWOW64FSRedirection disable; Utility::IsFileExist(binaryPath))
		{
			m_ServiceName = GetDokanyDefaultServiceName();
			return InitDriver();
		}
		return false;
	}
	bool FileSystemService::IsUsingDefaultDokanyInstallation() const
	{
		auto config = OpenService(ServiceAccess::QueryConfig).GetConfig();
		return config && config->DisplayName == GetDokanyDefaultServiceName();
	}

	void FileSystemService::AddActiveFS(IFileSystem& fileSystem)
	{
		m_ActiveFileSystems.remove(&fileSystem);
		m_ActiveFileSystems.push_back(&fileSystem);
	}
	void FileSystemService::RemoveActiveFS(IFileSystem& fileSystem)
	{
		m_ActiveFileSystems.remove(&fileSystem);
	}
}

BOOL WINAPI DllMain(HMODULE moduleHandle, DWORD event, LPVOID reserved)
{
	return Dokany2::DllMainRoutine(moduleHandle, event, reserved);
}
