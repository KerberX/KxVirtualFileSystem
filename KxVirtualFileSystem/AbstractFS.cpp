/*
Copyright © 2018 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/AbstractFS.h"
#include "KxVirtualFileSystem/Service.h"
#include "KxVirtualFileSystem/Utility.h"

namespace KxVFS
{
	bool AbstractFS::UnMountDirectory(const WCHAR* mountPoint)
	{
		return Dokany2::DokanRemoveMountPoint(mountPoint);
	}
	bool AbstractFS::IsCodeSuccess(int errorCode)
	{
		return DOKAN_SUCCEEDED(errorCode);
	}
	KxDynamicStringW AbstractFS::GetErrorCodeMessage(int errorCode)
	{
		const WCHAR* message = nullptr;
		switch (errorCode)
		{
			case DOKAN_SUCCESS:
			{
				message = L"Success";
				break;
			}
			case DOKAN_ERROR:
			{
				message = L"Mount error";
				break;
			}
			case DOKAN_DRIVE_LETTER_ERROR:
			{
				message = L"Bad Drive letter";
				break;
			}
			case DOKAN_DRIVER_INSTALL_ERROR:
			{
				message = L"Can't install driver";
				break;
			}
			case DOKAN_START_ERROR:
			{
				message = L"Driver answer that something is wrong";
				break;
			}
			case DOKAN_MOUNT_ERROR:
			{
				message = L"Can't assign a drive letter or mount point, probably already used by another volume";
				break;
			}
			case DOKAN_MOUNT_POINT_ERROR:
			{
				message = L"Mount point is invalid";
				break;
			}
			case DOKAN_VERSION_ERROR:
			{
				message = L"Requested an incompatible version";
				break;
			}
			default:
			{
				return KxDynamicStringW::Format(L"Unknown error: %d", errorCode);
			}
		};
		return message;
	}
	size_t AbstractFS::WriteString(const WCHAR* source, WCHAR* destination, size_t maxDestLength)
	{
		size_t maxNameLength = maxDestLength * sizeof(WCHAR);
		size_t nameLength = std::min(maxNameLength, wcslen(source) * sizeof(WCHAR));
		memcpy_s(destination, maxNameLength, source, nameLength);

		return nameLength / sizeof(WCHAR);
	}
}

namespace KxVFS
{
	void AbstractFS::SetMounted(bool value)
	{
		m_IsMounted = value;
	}
	int AbstractFS::DoMount()
	{
		OutputDebugStringA(__FUNCTION__);
		OutputDebugStringA(": ");
		OutputDebugStringA(!IsMounted() ? "allowed" : "disallowed");
		OutputDebugStringA("\r\n");

		if (!IsMounted())
		{
			// Mount point is not a drive, remove folder if empty and create a new one
			if (m_MountPoint.length() > 2 && ::RemoveDirectoryW(m_MountPoint.data()))
			{
				Utility::CreateFolderTree(m_MountPoint);
			}

			// Allow mount to empty folders
			if (Utility::IsFolderEmpty(m_MountPoint))
			{
				// Update options
				m_Options.ThreadCount = 0;
				m_Options.MountPoint = m_MountPoint.data();
				m_Options.Options = m_Flags;
				m_Options.Timeout = 0;

				// Create file system
				return Dokany2::DokanCreateFileSystem(&m_Options, &m_Operations, &m_Handle);
			}
			return DOKAN_MOUNT_ERROR;
		}
		return DOKAN_ERROR;
	}
	bool AbstractFS::DoUnMount()
	{
		OutputDebugStringA(__FUNCTION__);
		OutputDebugStringA(": ");
		OutputDebugStringA(IsMounted() ? "allowed" : "disallowed");
		OutputDebugStringA("\r\n");

		if (IsMounted())
		{
			Dokany2::DokanCloseHandle(m_Handle);
			return true;
		}
		return false;
	}

	AbstractFS::AbstractFS(Service* vfsService, KxDynamicStringRefW mountPoint, uint32_t flags)
		:m_ServiceInstance(vfsService), m_MountPoint(mountPoint), m_Flags(flags)
	{
		// Options
		m_Options.GlobalContext = reinterpret_cast<ULONG64>(this);
		m_Options.Version = DOKAN_VERSION;

		// Operations
		m_Operations.Mounted = Dokan_Mount;
		m_Operations.Unmounted = Dokan_Unmount;

		m_Operations.GetVolumeFreeSpace = Dokan_GetVolumeFreeSpace;
		m_Operations.GetVolumeInformationW = Dokan_GetVolumeInfo;
		m_Operations.GetVolumeAttributes = Dokan_GetVolumeAttributes;

		m_Operations.ZwCreateFile = Dokan_CreateFile;
		m_Operations.CloseFile = Dokan_CloseFile;
		m_Operations.Cleanup = Dokan_CleanUp;
		m_Operations.MoveFileW = Dokan_MoveFile;
		m_Operations.CanDeleteFile = Dokan_CanDeleteFile;

		m_Operations.LockFile = Dokan_LockFile;
		m_Operations.UnlockFile = Dokan_UnlockFile;
		m_Operations.GetFileSecurityW = Dokan_GetFileSecurity;
		m_Operations.SetFileSecurityW = Dokan_SetFileSecurity;

		m_Operations.ReadFile = Dokan_ReadFile;
		m_Operations.WriteFile = Dokan_WriteFile;
		m_Operations.FlushFileBuffers = Dokan_FlushFileBuffers;
		m_Operations.SetEndOfFile = Dokan_SetEndOfFile;
		m_Operations.SetAllocationSize = Dokan_SetAllocationSize;
		m_Operations.GetFileInformation = Dokan_GetFileInfo;
		m_Operations.SetFileBasicInformation = Dokan_SetBasicFileInfo;

		m_Operations.FindFiles = Dokan_FindFiles;
		m_Operations.FindFilesWithPattern = nullptr; // Overriding 'FindFiles' is enough
		m_Operations.FindStreams = Dokan_FindStreams;
	}
	AbstractFS::~AbstractFS()
	{
		OutputDebugStringA(__FUNCTION__);
		OutputDebugStringA("\r\n");

		DoUnMount();
	}

	int AbstractFS::Mount()
	{
		return DoMount();
	}
	bool AbstractFS::UnMount()
	{
		return DoUnMount();
	}

	KxDynamicStringRefW AbstractFS::GetVolumeName() const
	{
		return GetService()->GetServiceName();
	}
	KxDynamicStringRefW AbstractFS::GetVolumeFileSystemName() const
	{
		// File system name could be anything up to 10 characters.
		// But Windows check few feature availability based on file system name.
		// For this, it is recommended to set NTFS or FAT here.
		return L"NTFS";
	}
	uint32_t AbstractFS::GetVolumeSerialNumber() const
	{
		// I assume the volume serial needs to be just a random number,
		// so I think this would suffice. Mirror sample uses '0x19831116' constant.

		#pragma warning (suppress: 4311)
		#pragma warning (suppress: 4302)
		return (uint32_t)(reinterpret_cast<size_t>(this) ^ reinterpret_cast<size_t>(GetService()));
	}

	Service* AbstractFS::GetService()
	{
		return m_ServiceInstance;
	}
	const Service* AbstractFS::GetService() const
	{
		return m_ServiceInstance;
	}

	bool AbstractFS::IsMounted() const
	{
		return m_IsMounted;
	}
	bool AbstractFS::SetMountPoint(KxDynamicStringRefW mountPoint)
	{
		if (!IsMounted())
		{
			m_MountPoint = mountPoint;
			return true;
		}
		return false;
	}

	uint32_t AbstractFS::GetFlags() const
	{
		return m_Flags;
	}
	bool AbstractFS::SetFlags(uint32_t flags)
	{
		if (!IsMounted())
		{
			m_Flags = flags;
			return true;
		}
		return false;
	}

	NTSTATUS AbstractFS::GetNtStatusByWin32ErrorCode(DWORD nWin32ErrorCode) const
	{
		return Dokany2::DokanNtStatusFromWin32(nWin32ErrorCode);
	}
	NTSTATUS AbstractFS::GetNtStatusByWin32LastErrorCode() const
	{
		return Dokany2::DokanNtStatusFromWin32(::GetLastError());
	}

	NTSTATUS AbstractFS::OnMountInternal(EvtMounted& eventInfo)
	{
		SetMounted(true);
		GetService()->AddFS(this);

		return OnMount(eventInfo);
	}
	NTSTATUS AbstractFS::OnUnMountInternal(EvtUnMounted& eventInfo)
	{
		OutputDebugStringA(__FUNCTION__);
		OutputDebugStringA("\r\n");

		NTSTATUS statusCode = STATUS_UNSUCCESSFUL;
		CriticalSectionLocker lock(m_UnmountCS);
		{
			OutputDebugStringA("In EnterCriticalSection: ");
			OutputDebugStringA(__FUNCTION__);
			OutputDebugStringA("\r\n");

			SetMounted(false);
			GetService()->RemoveFS(this);

			statusCode = OnUnMount(eventInfo);
		}
		return statusCode;
	}
}

namespace KxVFS
{
	void DOKAN_CALLBACK AbstractFS::Dokan_Mount(EvtMounted* eventInfo)
	{
		KxVFSDebugPrint(TEXT("%s\r\n"), TEXT(__FUNCTION__));
		return (void)GetFromContext(eventInfo->DokanOptions)->OnMountInternal(*eventInfo);
	}
	void DOKAN_CALLBACK AbstractFS::Dokan_Unmount(EvtUnMounted* eventInfo)
	{
		KxVFSDebugPrint(TEXT("%s\r\n"), TEXT(__FUNCTION__));
		return (void)GetFromContext(eventInfo->DokanOptions)->OnUnMountInternal(*eventInfo);
	}

	NTSTATUS DOKAN_CALLBACK AbstractFS::Dokan_GetVolumeFreeSpace(EvtGetVolumeFreeSpace* eventInfo)
	{
		KxVFSDebugPrint(TEXT("%s\r\n"), TEXT(__FUNCTION__));
		return GetFromContext(eventInfo)->OnGetVolumeFreeSpace(*eventInfo);
	}
	NTSTATUS DOKAN_CALLBACK AbstractFS::Dokan_GetVolumeInfo(EvtGetVolumeInfo* eventInfo)
	{
		KxVFSDebugPrint(TEXT("%s\r\n"), TEXT(__FUNCTION__));
		return GetFromContext(eventInfo)->OnGetVolumeInfo(*eventInfo);
	}
	NTSTATUS DOKAN_CALLBACK AbstractFS::Dokan_GetVolumeAttributes(EvtGetVolumeAttributes* eventInfo)
	{
		KxVFSDebugPrint(TEXT("%s\r\n"), TEXT(__FUNCTION__));
		return GetFromContext(eventInfo)->OnGetVolumeAttributes(*eventInfo);
	}

	NTSTATUS DOKAN_CALLBACK AbstractFS::Dokan_CreateFile(EvtCreateFile* eventInfo)
	{
		KxVFSDebugPrint(TEXT("%s: \"%s\"\r\n"), TEXT(__FUNCTION__), eventInfo->FileName);
		return GetFromContext(eventInfo)->OnCreateFile(*eventInfo);
	}
	void DOKAN_CALLBACK AbstractFS::Dokan_CloseFile(EvtCloseFile* eventInfo)
	{
		KxVFSDebugPrint(TEXT("%s: \"%s\", DeleteOnClose: %d\r\n"), TEXT(__FUNCTION__), eventInfo->FileName, (int)eventInfo->DokanFileInfo->DeleteOnClose);
		return (void)GetFromContext(eventInfo)->OnCloseFile(*eventInfo);
	}
	void DOKAN_CALLBACK AbstractFS::Dokan_CleanUp(EvtCleanUp* eventInfo)
	{
		KxVFSDebugPrint(TEXT("%s: \"%s\", DeleteOnClose: %d\r\n"), TEXT(__FUNCTION__), eventInfo->FileName, (int)eventInfo->DokanFileInfo->DeleteOnClose);
		return (void)GetFromContext(eventInfo)->OnCleanUp(*eventInfo);
	}
	NTSTATUS DOKAN_CALLBACK AbstractFS::Dokan_MoveFile(EvtMoveFile* eventInfo)
	{
		KxVFSDebugPrint(TEXT("%s: \"%s\" -> \"%s\"\r\n"), TEXT(__FUNCTION__), eventInfo->FileName, eventInfo->NewFileName);
		return GetFromContext(eventInfo)->OnMoveFile(*eventInfo);
	}
	NTSTATUS DOKAN_CALLBACK AbstractFS::Dokan_CanDeleteFile(EvtCanDeleteFile* eventInfo)
	{
		KxVFSDebugPrint(TEXT("%s: \"%s\", DeleteOnClose: %d\r\n"), TEXT(__FUNCTION__), eventInfo->FileName, (int)eventInfo->DokanFileInfo->DeleteOnClose);
		return GetFromContext(eventInfo)->OnCanDeleteFile(*eventInfo);
	}

	NTSTATUS DOKAN_CALLBACK AbstractFS::Dokan_LockFile(EvtLockFile* eventInfo)
	{
		KxVFSDebugPrint(TEXT("%s: \"%s\"\r\n"), TEXT(__FUNCTION__), eventInfo->FileName);
		return GetFromContext(eventInfo)->OnLockFile(*eventInfo);
	}
	NTSTATUS DOKAN_CALLBACK AbstractFS::Dokan_UnlockFile(EvtUnlockFile* eventInfo)
	{
		KxVFSDebugPrint(TEXT("%s: \"%s\"\r\n"), TEXT(__FUNCTION__), eventInfo->FileName);
		return GetFromContext(eventInfo)->OnUnlockFile(*eventInfo);
	}
	NTSTATUS DOKAN_CALLBACK AbstractFS::Dokan_GetFileSecurity(EvtGetFileSecurity* eventInfo)
	{
		KxVFSDebugPrint(TEXT("%s: \"%s\"\r\n"), TEXT(__FUNCTION__), eventInfo->FileName);
		return GetFromContext(eventInfo)->OnGetFileSecurity(*eventInfo);
	}
	NTSTATUS DOKAN_CALLBACK AbstractFS::Dokan_SetFileSecurity(EvtSetFileSecurity* eventInfo)
	{
		KxVFSDebugPrint(TEXT("%s: \"%s\"\r\n"), TEXT(__FUNCTION__), eventInfo->FileName);
		return GetFromContext(eventInfo)->OnSetFileSecurity(*eventInfo);
	}

	NTSTATUS DOKAN_CALLBACK AbstractFS::Dokan_ReadFile(EvtReadFile* eventInfo)
	{
		KxVFSDebugPrint(TEXT("%s: \"%s\"\r\n"), TEXT(__FUNCTION__), eventInfo->FileName);
		return GetFromContext(eventInfo)->OnReadFile(*eventInfo);
	}
	NTSTATUS DOKAN_CALLBACK AbstractFS::Dokan_WriteFile(EvtWriteFile* eventInfo)
	{
		KxVFSDebugPrint(TEXT("%s: \"%s\"\r\n"), TEXT(__FUNCTION__), eventInfo->FileName);
		return GetFromContext(eventInfo)->OnWriteFile(*eventInfo);
	}
	NTSTATUS DOKAN_CALLBACK AbstractFS::Dokan_FlushFileBuffers(EvtFlushFileBuffers* eventInfo)
	{
		KxVFSDebugPrint(TEXT("%s: \"%s\"\r\n"), TEXT(__FUNCTION__), eventInfo->FileName);
		return GetFromContext(eventInfo)->OnFlushFileBuffers(*eventInfo);
	}
	NTSTATUS DOKAN_CALLBACK AbstractFS::Dokan_SetEndOfFile(EvtSetEndOfFile* eventInfo)
	{
		KxVFSDebugPrint(TEXT("%s: \"%s\"\r\n"), TEXT(__FUNCTION__), eventInfo->FileName);
		return GetFromContext(eventInfo)->OnSetEndOfFile(*eventInfo);
	}
	NTSTATUS DOKAN_CALLBACK AbstractFS::Dokan_SetAllocationSize(EvtSetAllocationSize* eventInfo)
	{
		KxVFSDebugPrint(TEXT("%s: \"%s\"\r\n"), TEXT(__FUNCTION__), eventInfo->FileName);
		return GetFromContext(eventInfo)->OnSetAllocationSize(*eventInfo);
	}
	NTSTATUS DOKAN_CALLBACK AbstractFS::Dokan_GetFileInfo(EvtGetFileInfo* eventInfo)
	{
		KxVFSDebugPrint(TEXT("%s: \"%s\"\r\n"), TEXT(__FUNCTION__), eventInfo->FileName);
		return GetFromContext(eventInfo)->OnGetFileInfo(*eventInfo);
	}
	NTSTATUS DOKAN_CALLBACK AbstractFS::Dokan_SetBasicFileInfo(EvtSetBasicFileInfo* eventInfo)
	{
		KxVFSDebugPrint(TEXT("%s: \"%s\"\r\n"), TEXT(__FUNCTION__), eventInfo->FileName);
		return GetFromContext(eventInfo)->OnSetBasicFileInfo(*eventInfo);
	}

	NTSTATUS DOKAN_CALLBACK AbstractFS::Dokan_FindFiles(EvtFindFiles* eventInfo)
	{
		KxVFSDebugPrint(TEXT("%s: \"%s\\*\"\r\n"), TEXT(__FUNCTION__), eventInfo->PathName);
		return GetFromContext(eventInfo)->OnFindFiles(*eventInfo);
	}
	NTSTATUS DOKAN_CALLBACK AbstractFS::Dokan_FindStreams(EvtFindStreams* eventInfo)
	{
		KxVFSDebugPrint(TEXT("%s: \"%s\"\r\n"), TEXT(__FUNCTION__), eventInfo->FileName);
		return GetFromContext(eventInfo)->OnFindStreams(*eventInfo);
	}
}