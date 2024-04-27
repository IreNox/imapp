#pragma once

namespace imapp
{
	struct ResourceUnicodeBlock
	{
		uint32		first;
		uint32		last;
		const char*	name;
	};

	ArrayView< ResourceUnicodeBlock > getUnicodeBlocks();
}
