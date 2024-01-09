#ifndef STRING_MAP_H
#define STRING_MAP_H

#include "common.h"
#include "StringView.h"

namespace APLogViewer
{
	class StringMap
	{
	public:
		char *str;
		size_t len;

		StringMap(char *base, StringView view);
	};
}

#endif // STRING_MAP_H