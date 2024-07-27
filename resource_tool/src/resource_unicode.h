#pragma once

namespace imapp
{
	struct ResourceUnicodeBlock
	{
		uint32		first;
		uint32		last;
		const char*	name;
	};

	ConstArrayView< ResourceUnicodeBlock > getUnicodeBlocks();
}
