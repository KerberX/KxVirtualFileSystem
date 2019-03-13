/*
Copyright � 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/Utility.h"

namespace KxVFS::Utility
{
	class KxVFS_API KxFileFinder;

	class KxVFS_API KxFileItem
	{
		friend class KxFileFinder;

		public:
			using TShortName = KxBasicDynamicString<wchar_t, ARRAYSIZE(WIN32_FIND_DATAW::cAlternateFileName)>;

		private:
			KxDynamicStringW m_Name;
			KxDynamicStringW m_Source;
			TShortName m_ShortName;
			uint32_t m_Attributes = INVALID_FILE_ATTRIBUTES;
			uint32_t m_ReparsePointAttributes = 0;
			FILETIME m_CreationTime;
			FILETIME m_LastAccessTime;
			FILETIME m_ModificationTime;
			int64_t m_FileSize = -1;

		private:
			void MakeNull(bool attribuesOnly = false);
			FILETIME FileTimeFromLARGE_INTEGER(const LARGE_INTEGER& value) const
			{
				return *reinterpret_cast<const FILETIME*>(&value);
			}
			bool DoUpdateInfo(KxDynamicStringRefW fullPath);

		public:
			KxFileItem() = default;
			KxFileItem(KxDynamicStringRefW fullPath);
			KxFileItem(KxDynamicStringRefW source, KxDynamicStringRefW fileName);

		private:
			KxFileItem(KxFileFinder* finder, const WIN32_FIND_DATAW& fileInfo);

		public:
			bool IsOK() const
			{
				return m_Attributes != INVALID_FILE_ATTRIBUTES;
			}
			bool UpdateInfo()
			{
				return DoUpdateInfo(GetFullPath());
			}

			bool IsNormalItem() const
			{
				return IsOK() && !IsReparsePoint() && !IsCurrentOrParent();
			}
			bool IsCurrentOrParent() const;
			bool IsReparsePoint() const
			{
				return m_Attributes & FILE_ATTRIBUTE_REPARSE_POINT;
			}

			bool IsDirectory() const
			{
				return m_Attributes & FILE_ATTRIBUTE_DIRECTORY;
			}
			bool IsDirectoryEmpty() const;
			KxFileItem& SetDirectory()
			{
				Utility::ModFlagRef(m_Attributes, FILE_ATTRIBUTE_DIRECTORY, true);
				return *this;
			}

			bool IsFile() const
			{
				return !IsDirectory();
			}
			KxFileItem& SetFile()
			{
				Utility::ModFlagRef(m_Attributes, FILE_ATTRIBUTE_DIRECTORY, false);
				return *this;
			}

			bool IsReadOnly() const
			{
				return m_Attributes & FILE_ATTRIBUTE_READONLY;
			}
			KxFileItem& SetReadOnly(bool value = true)
			{
				Utility::ModFlagRef(m_Attributes, FILE_ATTRIBUTE_READONLY, value);
				return *this;
			}

			uint32_t GetAttributes() const
			{
				return m_Attributes;
			}
			uint32_t GetReparsePointAttributes() const
			{
				return m_ReparsePointAttributes;
			}
			void SetNormalAttributes()
			{
				m_Attributes = FILE_ATTRIBUTE_NORMAL;
			}
			void SetAttributes(uint32_t value)
			{
				m_Attributes = value;
			}

			FILETIME GetCreationTime() const
			{
				return m_CreationTime;
			}
			void SetCreationTime(const FILETIME& value)
			{
				m_CreationTime = value;
			}
			void SetCreationTime(const LARGE_INTEGER& value)
			{
				m_CreationTime = FileTimeFromLARGE_INTEGER(value);
			}
			
			FILETIME GetLastAccessTime() const
			{
				return m_LastAccessTime;
			}
			void SetLastAccessTime(const FILETIME& value)
			{
				m_LastAccessTime = value;
			}
			void SetLastAccessTime(const LARGE_INTEGER& value)
			{
				m_LastAccessTime = FileTimeFromLARGE_INTEGER(value);
			}

			FILETIME GetModificationTime() const
			{
				return m_ModificationTime;
			}
			void SetModificationTime(const FILETIME& value)
			{
				m_ModificationTime = value;
			}
			void SetModificationTime(const LARGE_INTEGER& value)
			{
				m_ModificationTime = FileTimeFromLARGE_INTEGER(value);
			}

			int64_t GetFileSize() const
			{
				return m_FileSize;
			}
			void SetFileSize(int64_t size)
			{
				m_FileSize = size;
			}

			KxDynamicStringRefW GetSource() const
			{
				return m_Source;
			}
			void SetSource(KxDynamicStringRefW source)
			{
				m_Source = source;
			}
			
			KxDynamicStringRefW GetName() const
			{
				return m_Name;
			}
			void SetName(KxDynamicStringRefW name)
			{
				m_Name = name;
			}
			
			KxDynamicStringRefW GetShortName() const
			{
				return m_ShortName;
			}
			void SetShortName(KxDynamicStringRefW name)
			{
				m_ShortName = name;
			}

			KxDynamicStringW GetFileExtension() const;
			void SetFileExtension(KxDynamicStringRefW ext);

			KxDynamicStringW GetFullPath() const
			{
				KxDynamicStringW fullPath = m_Source;
				fullPath += L'\\';
				fullPath += m_Name;
				return fullPath;
			}
			KxDynamicStringW GetFullPathWithNS() const
			{
				KxDynamicStringW fullPath = Utility::LongPathPrefix;
				fullPath += m_Source;
				fullPath += L'\\';
				fullPath += m_Name;
				return fullPath;
			}
			void SetFullPath(KxDynamicStringRefW fullPath)
			{
				m_Source = KxDynamicStringW(fullPath).before_last(L'\\', &m_Name);
			}
			
			void FromWIN32_FIND_DATA(const WIN32_FIND_DATAW& findInfo);
			void ToWIN32_FIND_DATA(WIN32_FIND_DATAW& findData) const;
			WIN32_FIND_DATAW ToWIN32_FIND_DATA() const
			{
				WIN32_FIND_DATAW findData = {0};
				ToWIN32_FIND_DATA(findData);
				return findData;
			}
	};
}
