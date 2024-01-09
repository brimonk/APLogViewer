#ifndef STRING_VIEW_H
#define STRING_VIEW_H

#include "common.h"

namespace APLogViewer
{
	class StringView
	{
	public:
		u64 offset;
		u64 length;

		StringView(u64 offset, u64 length);
		StringView() = default;
	};
}

#endif // STRING_VIEW_H
