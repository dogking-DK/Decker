#pragma once
#include "vk_types.h"


bool enable_extension(const char* required_ext_name,
	const std::vector<VkExtensionProperties>& available_exts,
	std::vector<const char*>& enabled_extensions)
{
	for (auto& avail_ext_it : available_exts)
	{
		if (strcmp(avail_ext_it.extensionName, required_ext_name) == 0)
		{
			auto it = std::find_if(enabled_extensions.begin(), enabled_extensions.end(),
				[required_ext_name](const char* enabled_ext_name) {
					return strcmp(enabled_ext_name, required_ext_name) == 0;
				});
			if (it != enabled_extensions.end())
			{
				// Extension is already enabled
			}
			else
			{
				fmt::print("Extension {} found, enabling it", required_ext_name);
				enabled_extensions.emplace_back(required_ext_name);
			}
			return true;
		}
	}

	fmt::print("Extension {} not found", required_ext_name);
	return false;
}