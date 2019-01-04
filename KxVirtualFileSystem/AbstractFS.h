/*
Copyright © 2018 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem.h"
#include "IncludeDokan.h"
#include "IRequestDispatcher.h"
#include "FSError.h"
#include "Utility.h"

namespace KxVFS
{
	class Service;

	class KxVFS_API AbstractFS: public IRequestDispatcher
	{
		public:
			static bool UnMountDirectory(KxDynamicStringRefW mountPoint);
			static KxDynamicStringRefW& NormalizeFilePath(KxDynamicStringRefW& path);

			// Writes a string 'source' into specified buffer but no more than 'maxDstLength' CHARS.
			// Returns number of BYTES written.
			static size_t WriteString(KxDynamicStringRefW source, wchar_t* destination, const size_t maxDstLength);

		private:
			Service& m_Service;
			Dokany2::DOKAN_OPTIONS m_Options = {0};
			Dokany2::DOKAN_OPERATIONS m_Operations = {0};
			Dokany2::DOKAN_HANDLE m_Handle = nullptr;

			KxDynamicStringW m_MountPoint;
			CriticalSection m_UnmountCS;
			uint32_t m_Flags = 0;
			bool m_IsMounted = false;
			bool m_IsDestructing = false;

		protected:
			// Some utility functions
			template<class TOption, class TValue> bool SetOptionIfNotMounted(TOption& option, TValue&& value)
			{
				if (!m_IsMounted)
				{
					option = value;
					return true;
				}
				return false;
			}

			bool IsRequestToRoot(KxDynamicStringRefW fileName) const
			{
				return fileName.empty() || (fileName.length() == 1 && fileName.front() == L'\\');
			}
			bool IsWriteRequest(KxDynamicStringRefW filePath, ACCESS_MASK desiredAccess, DWORD createDisposition) const;
			bool IsReadRequest(KxDynamicStringRefW filePath, ACCESS_MASK desiredAccess, DWORD createDisposition) const
			{
				return !IsWriteRequest(filePath, desiredAccess, createDisposition);
			}
			bool IsDirectory(ULONG kernelCreateOptions) const;
			
			bool IsRequestingSACLInfo(const PSECURITY_INFORMATION securityInformation) const;
			void ProcessSESecurityPrivilege(PSECURITY_INFORMATION securityInformation) const;

		private:
			void SetMounted(bool value);
			FSError DoMount();
			bool DoUnMount();

		public:
			static const uint32_t DefFlags = 0;

			AbstractFS(Service& service, KxDynamicStringRefW mountPoint, uint32_t flags = DefFlags);
			virtual ~AbstractFS();

		public:
			virtual FSError Mount();
			virtual bool UnMount();

			virtual KxDynamicStringW GetVolumeLabel() const;
			virtual KxDynamicStringW GetVolumeFileSystemName() const;
			virtual uint32_t GetVolumeSerialNumber() const;

			Service& GetService();
			const Service& GetService() const;
			bool IsMounted() const;

			KxDynamicStringRefW GetMountPoint() const;
			bool SetMountPoint(KxDynamicStringRefW mountPoint);
		
			uint32_t GetFlags() const;
			bool SetFlags(uint32_t flags);

			NTSTATUS GetNtStatusByWin32ErrorCode(DWORD nWin32ErrorCode) const;
			NTSTATUS GetNtStatusByWin32LastErrorCode() const;

		private:
			NTSTATUS OnMountInternal(EvtMounted& eventInfo);
			NTSTATUS OnUnMountInternal(EvtUnMounted& eventInfo);

		protected:
			// KxVFS callbacks
			// For OnMount, OnUnMount, OnCloseFile, OnCleanUp return 'STATUS_NOT_SUPPORTED'
			// as underlaying function has void as its return type.
			virtual NTSTATUS OnMount(EvtMounted& eventInfo) = 0;
			virtual NTSTATUS OnUnMount(EvtUnMounted& eventInfo) = 0;

			virtual NTSTATUS OnGetVolumeFreeSpace(EvtGetVolumeFreeSpace& eventInfo) = 0;
			virtual NTSTATUS OnGetVolumeInfo(EvtGetVolumeInfo& eventInfo) = 0;
			virtual NTSTATUS OnGetVolumeAttributes(EvtGetVolumeAttributes& eventInfo) = 0;

			virtual NTSTATUS OnCreateFile(EvtCreateFile& eventInfo) = 0;
			virtual NTSTATUS OnCloseFile(EvtCloseFile& eventInfo) = 0;
			virtual NTSTATUS OnCleanUp(EvtCleanUp& eventInfo) = 0;
			virtual NTSTATUS OnMoveFile(EvtMoveFile& eventInfo) = 0;
			virtual NTSTATUS OnCanDeleteFile(EvtCanDeleteFile& eventInfo) = 0;

			virtual NTSTATUS OnLockFile(EvtLockFile& eventInfo) = 0;
			virtual NTSTATUS OnUnlockFile(EvtUnlockFile& eventInfo) = 0;
			virtual NTSTATUS OnGetFileSecurity(EvtGetFileSecurity& eventInfo) = 0;
			virtual NTSTATUS OnSetFileSecurity(EvtSetFileSecurity& eventInfo) = 0;

			virtual NTSTATUS OnReadFile(EvtReadFile& eventInfo) = 0;
			virtual NTSTATUS OnWriteFile(EvtWriteFile& eventInfo) = 0;
			virtual NTSTATUS OnFlushFileBuffers(EvtFlushFileBuffers& eventInfo) = 0;
			virtual NTSTATUS OnSetEndOfFile(EvtSetEndOfFile& eventInfo) = 0;
			virtual NTSTATUS OnSetAllocationSize(EvtSetAllocationSize& eventInfo) = 0;
			virtual NTSTATUS OnGetFileInfo(EvtGetFileInfo& eventInfo) = 0;
			virtual NTSTATUS OnSetBasicFileInfo(EvtSetBasicFileInfo& eventInfo) = 0;

			virtual NTSTATUS OnFindFiles(EvtFindFiles& eventInfo) = 0;
			virtual NTSTATUS OnFindStreams(EvtFindStreams& eventInfo) = 0;

		private:
			// Dokan callbacks
			static AbstractFS* GetFromContext(Dokany2::DOKAN_OPTIONS* dokanOptions)
			{
				return reinterpret_cast<AbstractFS*>(dokanOptions->GlobalContext);
			}
			template<class T> static AbstractFS* GetFromContext(const T* eventInfo)
			{
				return GetFromContext(eventInfo->DokanFileInfo->DokanOptions);
			}

			static void DOKAN_CALLBACK Dokan_Mount(EvtMounted* eventInfo);
			static void DOKAN_CALLBACK Dokan_Unmount(EvtUnMounted* eventInfo);

			static NTSTATUS DOKAN_CALLBACK Dokan_GetVolumeFreeSpace(EvtGetVolumeFreeSpace* eventInfo);
			static NTSTATUS DOKAN_CALLBACK Dokan_GetVolumeInfo(EvtGetVolumeInfo* eventInfo);
			static NTSTATUS DOKAN_CALLBACK Dokan_GetVolumeAttributes(EvtGetVolumeAttributes* eventInfo);

			static NTSTATUS DOKAN_CALLBACK Dokan_CreateFile(EvtCreateFile* eventInfo);
			static void DOKAN_CALLBACK Dokan_CloseFile(EvtCloseFile* eventInfo);
			static void DOKAN_CALLBACK Dokan_CleanUp(EvtCleanUp* eventInfo);
			static NTSTATUS DOKAN_CALLBACK Dokan_MoveFile(EvtMoveFile* eventInfo);
			static NTSTATUS DOKAN_CALLBACK Dokan_CanDeleteFile(EvtCanDeleteFile* eventInfo);

			static NTSTATUS DOKAN_CALLBACK Dokan_LockFile(EvtLockFile* eventInfo);
			static NTSTATUS DOKAN_CALLBACK Dokan_UnlockFile(EvtUnlockFile* eventInfo);
			static NTSTATUS DOKAN_CALLBACK Dokan_GetFileSecurity(EvtGetFileSecurity* eventInfo);
			static NTSTATUS DOKAN_CALLBACK Dokan_SetFileSecurity(EvtSetFileSecurity* eventInfo);

			static NTSTATUS DOKAN_CALLBACK Dokan_ReadFile(EvtReadFile* eventInfo);
			static NTSTATUS DOKAN_CALLBACK Dokan_WriteFile(EvtWriteFile* eventInfo);
			static NTSTATUS DOKAN_CALLBACK Dokan_FlushFileBuffers(EvtFlushFileBuffers* eventInfo);
			static NTSTATUS DOKAN_CALLBACK Dokan_SetEndOfFile(EvtSetEndOfFile* eventInfo);
			static NTSTATUS DOKAN_CALLBACK Dokan_SetAllocationSize(EvtSetAllocationSize* eventInfo);
			static NTSTATUS DOKAN_CALLBACK Dokan_GetFileInfo(EvtGetFileInfo* eventInfo);
			static NTSTATUS DOKAN_CALLBACK Dokan_SetBasicFileInfo(EvtSetBasicFileInfo* eventInfo);

			static NTSTATUS DOKAN_CALLBACK Dokan_FindFiles(EvtFindFiles* eventInfo);
			static NTSTATUS DOKAN_CALLBACK Dokan_FindStreams(EvtFindStreams* eventInfo);
	};
}
