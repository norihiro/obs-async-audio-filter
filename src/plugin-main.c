/*
OBS Asynchronous Audio Source Plugin
Copyright (C) 2022 Norihiro Kamae <norihiro@nagater.net>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include <obs-module.h>

#include "plugin-macros.generated.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

extern const struct obs_source_info asynchronous_audio_info;

bool obs_module_load(void)
{
	obs_register_source(&asynchronous_audio_info);
	blog(LOG_INFO, "plugin loaded (version %s)", PLUGIN_VERSION);
	return true;
}

void obs_module_unload()
{
	blog(LOG_INFO, "plugin unloaded");
}
