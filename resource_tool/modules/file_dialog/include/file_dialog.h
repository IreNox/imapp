#pragma once

#include <tiki/tiki_dynamic_string.h>

namespace imapp
{
	using namespace tiki;

	DynamicString	openFileDialog( const StringView& title, const StringView& initialPath, const ArrayView< StringView >& filters );
	DynamicString	saveFileDialog( const StringView& title, const StringView& initialPath, const ArrayView< StringView >& filters );
}
