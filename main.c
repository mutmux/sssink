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

#include "main.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>

#include <gpod/itdb.h>
#include <tag_c.h>

const char *version = "1.0.0";
const int cmd_count = sizeof(cmds) / sizeof(struct Cmd);

int main(int argc, char *argv[]) {
    if (argc < 2) {
        help(argc, argv);
        return 1;
    }

    int is_cmd = 0;

    for (int i = 0; i < cmd_count; i++) {
        if (strcmp(argv[1], cmds[i].name) == 0) {
            cmds[i].func(argc, argv);
            is_cmd = 1;
            break;
        }
    }
    
    if (!is_cmd) {
        printf("unknown command. try `$ sssink help`.\n");
        return 1;
    }

    return 0;
}

void is_mounted(char *mountpoint) {
    struct stat st;

    /* remove trailing slash (if any) */
    size_t mountpoint_len = strlen(mountpoint);
    while (mountpoint_len > 1 && mountpoint[mountpoint_len - 1] == '/') {
        mountpoint[mountpoint_len - 1] = '\0';
        mountpoint_len = strlen(mountpoint);
    }
    
    /* construct full path, then check for itdb dir... */
    char itdb_path[1024];
    snprintf(itdb_path, sizeof(itdb_path), "%s/iTunes_Control", mountpoint);

    if (stat(itdb_path, &st) == -1) {
        printf("ERROR: invalid device mount\n\n");
        printf("is the device plugged in? is the mount point correct?\n");
        printf("done a first-time sync with iTunes? is the itdb corrupt?\n");

        exit(1);
    }
}

void help(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    printf("sssink %s <https://github.com/mutmux/sssink>\n\n", version);
    /* print commands, descriptions and usage examples */
    for (int i = 0; i < cmd_count; i++) {
        printf("%s\t%s\n\t%s\n\n", cmds[i].name, cmds[i].usage, cmds[i].desc);
    }
}

void list(int argc, char *argv[]) {
    /* check if all required args supplied before continuing... */
    if (argc < 3) {
        printf("ERROR: missing arguments\n");
        printf("usage: %s\n", cmds[1].usage);
        exit(1);
    }
    /* confirm device is mounted, and mount has itdb */
    char *mountpoint = argv[2];
    is_mounted(mountpoint);

    Itdb_iTunesDB *db = NULL;
    GError *err = NULL;

    /* load and parse the db, free and exit if error in doing so */
    db = itdb_parse(mountpoint, &err);
    if (err != NULL) {
        perror("ERROR: loading iTunes database failed\n\n");
        printf("done a first-time sync with iTunes? is the itdb corrupt?\n");
        
        g_error_free(err);
        itdb_free(db);

        exit(1);
    }

    /* iterate through each track, print title and artist. */
    int track_count = 0;

    GList *track_list = db->tracks;
    while (track_list != NULL) {
        Itdb_Track *track = (Itdb_Track *)track_list->data;

        printf("%s - %s\n", track->title, track->artist);

        track_list = track_list->next;
        track_count++;
    }

    itdb_free(db);
    printf("\n%d tracks total on %s\n", track_count, argv[2]);
}

void push(int argc, char *argv[]) {
    /* check that all required args supplied */
    if (argc < 4) {
        printf("ERROR: missing arguments\n");
        printf("usage: %s\n", cmds[2].usage);
        exit(1);
    }
    /* confirm device is mounted, and mount has itdb */
    char *mountpoint = argv[2];
    is_mounted(mountpoint);

    Itdb_iTunesDB *db;
    Itdb_Track *track;
    GError *err = NULL;

    /* TODO: get path of track. account for directory (same, leading slash, etc.) */
    const char *trackfilepath = argv[3];
    const char *trackfilename = strrchr(trackfilepath, '/');
    if (trackfilename == NULL) {
        trackfilename = trackfilepath;  /* assume whole path is fiiename */
    } else {
        trackfilename++;
    }

    /* check for tag information, exit if not found */
    TagLib_File *trackfile = taglib_file_new(trackfilename);
    if (trackfile == NULL) {
        printf("ERROR: couldn't open file %s", trackfilename);
        exit(1);
    }

    /* attempt to extract tag... */
    TagLib_Tag *tag = taglib_file_tag(trackfile);
    if (tag == NULL) {
        printf("ERROR: file is invalid/incompatible, or lacks known tag\n");
        taglib_file_free(trackfile);
        exit(1);
    }

    /* load and parse the db, free and exit if error in doing so */
    db = itdb_parse(mountpoint, &err);
    if (err != NULL) {
        perror("ERROR: loading iTunes database failed\n\n");
        printf("done a first-time sync with iTunes? is the itdb corrupt?\n");
        
        g_error_free(err);
        itdb_free(db);

        exit(1);
    }

    /* create track object */
    track = itdb_track_new();

    /* fill in metadata from taglib, use unknown for missing vals */
    /* audio_filename, Unknown Artist, Unknown Album, no genre, no comment */
    if (strcmp(taglib_tag_title(tag), "") == 0) {
        track->title = g_strdup(trackfilename);
    }
    else {
        track->title = g_strdup(taglib_tag_title(tag));
    }

    if (strcmp(taglib_tag_artist(tag), "") == 0) {
        track->artist = g_strdup("Unknown Artist");
    }
    else {
        track->artist = g_strdup(taglib_tag_artist(tag));
    }

    if (strcmp(taglib_tag_album(tag), "") == 0) {
        track->album = g_strdup("Unknown Album");
    }
    else {
        track->album = g_strdup(taglib_tag_album(tag));
    }

    track->genre = g_strdup(taglib_tag_genre(tag));
    track->comment = g_strdup("");

    /* free taglib file object when done */
    taglib_file_free(trackfile);

    /* set itdb track filename, add track to itdb */
    itdb_filename_fs2ipod(trackfilename);

    itdb_track_add(db, track, -1);

    /* use libgpod to copy file to device */
    itdb_cp_track_to_ipod(track, trackfilename, &err);
    if (err != NULL) {
        printf("ERROR: failed to copy file to device\n");
        printf("done a first-time sync with iTunes? is the itdb corrupt?\n");
        printf("is the file supported for your device? is it valid?\n");
        printf("has the file moved? is your device still mounted?\n");

        g_error_free(err);
        itdb_free(db);

        exit(1);
    }

    /* write itdb changes */
    itdb_write(db, &err);
    if (err != NULL) {
        printf("ERROR: failed to write to itdb\n");
        printf("done a first-time sync with iTunes? is the itdb corrupt?\n");

        g_error_free(err);
        itdb_free(db);

        exit(1);
    }

    /* free itdb, print that we done, exit */
    itdb_free(db);
    printf("synced `%s` to %s :)\n", trackfilename, mountpoint);
}

void pull(int argc, char *argv[]) {
    /* check that all required args supplied */
    if (argc < 4) {
        printf("ERROR: missing arguments\n");
        printf("usage: %s\n", cmds[3].usage);
        exit(1);
    }
    /* confirm device is mounted, and mount has itdb */
    char *mountpoint = argv[2];
    is_mounted(mountpoint);

    /* if no destination supplied, presume pwd */
    /* wildcard * to pull all tracks */
}

void del(int argc, char *argv[]) {
    /* check that all required args supplied */
    if (argc < 4) {
        printf("ERROR: missing arguments\n");
        printf("usage: %s\n", cmds[4].usage);
        exit(1);
    }
    /* confirm device is mounted, and mount has itdb */
    char *mountpoint = argv[2];
    is_mounted(mountpoint);

    /* wildcard * to nuke all tracks */
}
