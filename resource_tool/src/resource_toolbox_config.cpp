#include "resource_toolbox_config.h"

#include "resource_package.h"

namespace imapp
{
	using namespace tiki;

	ResourceToolboxConfig::ResourceToolboxConfig()
	{
		m_fields =
		{
			{ "Button Height",		ResourceToolboxConfigFieldType::Float,		{ &m_config.button.height } },
			{ "Button Padding",		ResourceToolboxConfigFieldType::Border,		{ &m_config.button.padding } },
			{ "Button Height",		ResourceToolboxConfigFieldType::Float,		{ &m_config.button.height } },
			{ "Button Height",		ResourceToolboxConfigFieldType::Float,		{ &m_config.button.height } },
			{ "Button Height",		ResourceToolboxConfigFieldType::Texture,	{ &m_config.button.height } },
		};
	}
}