/*
    This file is part of Genesys.

    Genesys is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Genesys is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Genesys.  If not, see <http://www.gnu.org/licenses/>.

    Copyright (C) 2016 Clemens Kirchgatterer <clemens@1541.org>.
*/

#ifndef _FILESYSTEM_H_
#define _FILESYSTEM_H_

#include <Arduino.h>
#include <FS.h>

extern FS *rootfs;

int fs_state(void);
bool fs_init(void);
bool fs_fini(void);
void fs_poll(void);

bool fs_full(void);
void fs_usage(int &total, int &used, int &unused);
void fs_format(void);
void fs_ls(String &str);
void fs_mv(const String &from, const String &to);
void fs_rm(const String &path);
void fs_df(String &str);

String fs_format_bytes(size_t bytes);

#endif // _FILESYSTEM_H_
