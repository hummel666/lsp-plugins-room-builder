/*
 * Copyright (C) 2021 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2021 Vladimir Sadovnikov <sadko4u@gmail.com>
 *
 * This file is part of lsp-plugins-room-builder
 * Created on: 3 авг. 2021 г.
 *
 * lsp-plugins-room-builder is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * lsp-plugins-room-builder is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with lsp-plugins-room-builder. If not, see <https://www.gnu.org/licenses/>.
 */

#include <lsp-plug.in/plug-fw/meta/ports.h>
#include <lsp-plug.in/shared/meta/developers.h>
#include <private/meta/room_builder.h>

#define LSP_PLUGINS_ROOM_BUILDER_VERSION_MAJOR       1
#define LSP_PLUGINS_ROOM_BUILDER_VERSION_MINOR       0
#define LSP_PLUGINS_ROOM_BUILDER_VERSION_MICRO       0

#define LSP_PLUGINS_ROOM_BUILDER_VERSION  \
    LSP_MODULE_VERSION( \
        LSP_PLUGINS_ROOM_BUILDER_VERSION_MAJOR, \
        LSP_PLUGINS_ROOM_BUILDER_VERSION_MINOR, \
        LSP_PLUGINS_ROOM_BUILDER_VERSION_MICRO  \
    )

namespace lsp
{
    namespace meta
    {

    } // namespace meta
} // namespace lsp
