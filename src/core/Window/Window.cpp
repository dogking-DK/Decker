/* Copyright (c) 2018-2023, Arm Limited and Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 the "License";
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "Window.h"


namespace dk::core
{
	Window::Window(const Properties& properties) :
		_properties{ properties }
	{
	}

	Window::Extent Window::resize(const Extent& new_extent)
	{
		if (_properties.resizable)
		{
			_properties.extent.width = new_extent.width;
			_properties.extent.height = new_extent.height;
		}

		return _properties.extent;
	}

	const Window::Extent& Window::get_extent() const
	{
		return _properties.extent;
	}

	float Window::get_content_scale_factor() const
	{
		return 1.0f;
	}

	Window::mode Window::get_window_mode() const
	{
		return _properties.mode;
	}

	bool Window::get_display_present_info(VkDisplayPresentInfoKHR* info,
		uint32_t src_width, uint32_t src_height) const
	{
		// Default is to not use the extra present info
		return false;
	}
}
