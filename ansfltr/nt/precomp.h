/*
 * Copyright 2001-2021 ansnap@sina.com 
 *
 * This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#pragma once

// WDK headers
#include <ndis.h>
#include <windef.h>
#include <ntstrsafe.h>

#define _WINNT
#define _NDISRD_IM

#define DRIVER_NAME "ansfltr"
#define DRIVER_TAG 'tlfc'

// Common headers
#include "..\..\Shared\ansfltr.h"
#include "..\..\Shared\dbg.h" 
#include "..\..\Shared\ether.h"
#include "..\..\Shared\iphlp.h"

// Project headers
#include "..\callbacks.h"
#include "..\ioctl.h"
#include "packet.h"
#include "extens.h"
#include "ndisrd.h"
#include "request.h"
#include "util.h"
#include "proc.h"
#include "send.h"
