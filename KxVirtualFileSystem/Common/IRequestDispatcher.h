/*
Copyright © 2018 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"

namespace KxVFS
{
	class KxVFS_API IRequestDispatcher
	{
		public:
			virtual KxDynamicStringW DispatchLocationRequest(KxDynamicStringRefW requestedPath) = 0;
	};
}