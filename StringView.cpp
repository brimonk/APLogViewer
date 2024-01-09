#include "common.h"
#include "StringView.h"

namespace APLogViewer
{
	StringView::StringView(u64 offset, u64 length)
	{
		this->offset = offset;
		this->length = length;
	}
}
