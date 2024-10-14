/*
 * Copyright (C) 2024 mutmux
 *
 * This file is part of sssink.
 *
 * sssink is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * sssink is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with sssink. If not, see <https://www.gnu.org/licenses/>. 
 */

#ifndef MAIN_H
#define MAIN_H

void is_mounted(char *);

void help(int argc, char *argv[]);

void list(int argc, char *argv[]);
void push(int argc, char *argv[]);
void pull(int argc, char *argv[]);
void del(int argc, char *argv[]);

struct Cmd {
    const char *name;
    const char *usage;
    const char *desc;
    void (*func)(int argc, char *argv[]);
};

struct Cmd cmds[] = {
    {"help", "$ sssink help", "print helpful information", help},
    {"list", "$ sssink list <mountpoint>", "list music stored on device", list},
    {"push", "$ sssink push <mountpoint> <track>", "send track to device", push},
    {"pull", "$ sssink pull <mountpoint> <track> (destination)", "get track from device", pull},
    {"del", "$ sssink del <mountpoint> <track>", "delete track from device", del}
};

#endif