#include "common.h"
#include "StringMap.h"

namespace APLogViewer
{
	StringMap::StringMap(char *base, StringView view)
	{
		this->str = base + view.offset;
		this->len = view.length;
	}
}
