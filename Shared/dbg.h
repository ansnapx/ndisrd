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

#define DL_EXTRA_LOUD       0x80
#define DL_VERY_LOUD        0x40
#define DL_LOUD             0x20
#define DL_INFO             0x10
#define DL_TRACE            0x8
#define DL_WARN             0x4
#define DL_ERROR            0x2
#define DL_FATAL            0x0

#ifdef DBG

#ifndef DRIVER_NAME 
#define DRIVER_NAME ""
#endif

#define DEBUGP( lev, ... ) \
	if( lev & ( DL_FATAL | DL_ERROR | DL_WARN | DL_TRACE | DL_INFO | DL_LOUD ) ) \
	{ \
		KdPrint(( "[" DRIVER_NAME "] " __VA_ARGS__ )); \
	}

#define dprintf( ... ) DEBUGP( DL_INFO, __VA_ARGS__ )

#define DBGPRINT(Fmt)                                        \
    {                                                        \
        DbgPrint("[" DRIVER_NAME "] ");                      \
        DbgPrint Fmt;                                        \
    }

#else

#define DEBUGP( lev, ... )

#define dprintf( ... )

#define DBGPRINT(Fmt)

#endif // DBG

#define PRINT_IP_ADDR(addr) \
	((UCHAR *)&(addr))[0], ((UCHAR *)&(addr))[1], ((UCHAR *)&(addr))[2], ((UCHAR *)&(addr))[3]