/* vi: set sw=4 ts=4: */
/*
 *  busybox patch applet to handle the unified diff format.
 *  Copyright (C) 2003 Glenn McGrath
 *
 *  Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 *
 *  This applet is written to work with patches generated by GNU diff,
 *  where there is equivalent functionality busybox patch shall behave
 *  as per GNU patch.
 *
 *  There is a SUSv3 specification for patch, however it looks to be
 *  incomplete, it doesnt even mention unified diff format.
 *  http://www.opengroup.org/onlinepubs/007904975/utilities/patch.html
 *
 *  Issues
 *   - Non-interactive
 *   - Patches must apply cleanly or patch (not just one hunk) will fail.
 *   - Reject file isnt saved
 */

#include <getopt.h>

#include "libbb.h"

static unsigned int copy_lines(FILE *src_stream, FILE *dest_stream, const unsigned int lines_count)
{
	unsigned int i = 0;

	while (src_stream && (i < lines_count)) {
		char *line;
		line = xmalloc_fgets(src_stream);
		if (line == NULL) {
			break;
		}
		if (fputs(line, dest_stream) == EOF) {
			bb_perror_msg_and_die("error writing to new file");
		}
		free(line);

		i++;
	}
	return i;
}

/* If patch_level is -1 it will remove all directory names
 * char *line must be greater than 4 chars
 * returns NULL if the file doesnt exist or error
 * returns malloc'ed filename
 */

static char *extract_filename(char *line, int patch_level)
{
	char *temp, *filename_start_ptr = line + 4;
	int i;

	/* Terminate string at end of source filename */
	temp = strchrnul(filename_start_ptr, '\t');
	*temp = '\0';

	/* Skip over (patch_level) number of leading directories */
	if (patch_level == -1)
		patch_level = INT_MAX;
	for (i = 0; i < patch_level; i++) {
		temp = strchr(filename_start_ptr, '/');
		if (!temp)
			break;
		filename_start_ptr = temp + 1;
	}

	return xstrdup(filename_start_ptr);
}

int patch_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int patch_main(int argc, char **argv)
{
	int patch_level = -1;
	char *patch_line;
	int ret;
	FILE *patch_file = NULL;
	struct stat saved_stat;
	
	{
		char *p, *i;
		ret = getopt32(argv, "p:i:", &p, &i);
		if (ret & 1)
			patch_level = xatol_range(p, -1, USHRT_MAX);
		if (ret & 2) {
			patch_file = xfopen(i, "r");
		} else {
			patch_file = stdin;
		}
		ret = 0;
	}

	patch_line = xmalloc_getline(patch_file);
	while (patch_line) {
		FILE *src_stream;
		FILE *dst_stream;
		char *original_filename;
		char *new_filename;
		char *backup_filename;
		unsigned int src_cur_line = 1;
		unsigned int dest_cur_line = 0;
		unsigned int dest_beg_line;
		unsigned int bad_hunk_count = 0;
		unsigned int hunk_count = 0;
		char copy_trailing_lines_flag = 0;

		/* Skip everything upto the "---" marker
		 * No need to parse the lines "Only in <dir>", and "diff <args>"
		 */
		while (patch_line && strncmp(patch_line, "--- ", 4) != 0) {
			free(patch_line);
			patch_line = xmalloc_getline(patch_file);
		}
		/* FIXME: patch_line NULL check?? */

		/* Extract the filename used before the patch was generated */
		original_filename = extract_filename(patch_line, patch_level);
		free(patch_line);

		patch_line = xmalloc_getline(patch_file);
		/* FIXME: NULL check?? */
		if (strncmp(patch_line, "+++ ", 4) != 0) {
			ret = 2;
			bb_error_msg("invalid patch");
			continue;
		}
		new_filename = extract_filename(patch_line, patch_level);
		free(patch_line);
		
		/* Get access rights from the file to be patched, -1 file does not exist */
		if (stat(new_filename, &saved_stat)) {
			char *line_ptr;
			/* Create leading directories */
			line_ptr = strrchr(new_filename, '/');
			if (line_ptr) {
				*line_ptr = '\0';
				bb_make_directory(new_filename, -1, FILEUTILS_RECUR);
				*line_ptr = '/';
			}
			dst_stream = xfopen(new_filename, "w+");
			backup_filename = NULL;
		} else {
			backup_filename = xmalloc(strlen(new_filename) + 6);
			strcpy(backup_filename, new_filename);
			strcat(backup_filename, ".orig");
			if (rename(new_filename, backup_filename) == -1) {
				bb_perror_msg_and_die("cannot create file %s",
						backup_filename);
			}
			dst_stream = xfopen(new_filename, "w");
			fchmod(fileno(dst_stream), saved_stat.st_mode);
		}

		if ((backup_filename == NULL) || stat(original_filename, &saved_stat)) {
			src_stream = NULL;
		} else {
			if (strcmp(original_filename, new_filename) == 0) {
				src_stream = xfopen(backup_filename, "r");
			} else {
				src_stream = xfopen(original_filename, "r");
			}
		}

		printf("patching file %s\n", new_filename);

		/* Handle each hunk */
		patch_line = xmalloc_fgets(patch_file);
		while (patch_line) {
			unsigned int count;
			unsigned int src_beg_line;
			unsigned int unused;
			unsigned int hunk_offset_start = 0;
			int hunk_error = 0;

			/* This bit should be improved */
			if ((sscanf(patch_line, "@@ -%d,%d +%d,%d @@", &src_beg_line, &unused, &dest_beg_line, &unused) != 4) &&
				(sscanf(patch_line, "@@ -%d,%d +%d @@", &src_beg_line, &unused, &dest_beg_line) != 3) &&
				(sscanf(patch_line, "@@ -%d +%d,%d @@", &src_beg_line, &dest_beg_line, &unused) != 3)) {
				/* No more hunks for this file */
				break;
			}
			free(patch_line);
			hunk_count++;

			if (src_beg_line && dest_beg_line) {
				/* Copy unmodified lines upto start of hunk */
				/* src_beg_line will be 0 if its a new file */
				count = src_beg_line - src_cur_line;
				if (copy_lines(src_stream, dst_stream, count) != count) {
					bb_error_msg_and_die("bad src file");
				}
				src_cur_line += count;
				dest_cur_line += count;
				copy_trailing_lines_flag = 1;
			}
			hunk_offset_start = src_cur_line;

			while ((patch_line = xmalloc_fgets(patch_file)) != NULL) {
				if ((*patch_line == '-') || (*patch_line == ' ')) {
					char *src_line = NULL;
					if (src_stream) {
						src_line = xmalloc_fgets(src_stream);
						if (!src_line) {
							hunk_error++;
							break;
						} else {
							src_cur_line++;
						}
						if (strcmp(src_line, patch_line + 1) != 0) {
							bb_error_msg("hunk #%d FAILED at %d", hunk_count, hunk_offset_start);
							hunk_error++;
							free(patch_line);
							/* Probably need to find next hunk, etc... */
							/* but for now we just bail out */
							patch_line = NULL;
							break;
						}
						free(src_line);
					}
					if (*patch_line == ' ') {
						fputs(patch_line + 1, dst_stream);
						dest_cur_line++;
					}
				} else if (*patch_line == '+') {
					fputs(patch_line + 1, dst_stream);
					dest_cur_line++;
				} else {
					break;
				}
				free(patch_line);
			}
			if (hunk_error) {
				bad_hunk_count++;
			}
		}

		/* Cleanup last patched file */
		if (copy_trailing_lines_flag) {
			copy_lines(src_stream, dst_stream, -1);
		}
		if (src_stream) {
			fclose(src_stream);
		}
		if (dst_stream) {
			fclose(dst_stream);
		}
		if (bad_hunk_count) {
			if (!ret) {
				ret = 1;
			}
			bb_error_msg("%d out of %d hunk FAILED", bad_hunk_count, hunk_count);
		} else {
			/* It worked, we can remove the backup */
			if (backup_filename) {
				unlink(backup_filename);
			}
			if ((dest_cur_line == 0) || (dest_beg_line == 0)) {
				/* The new patched file is empty, remove it */
				xunlink(new_filename);
				if (strcmp(new_filename, original_filename) != 0)
					xunlink(original_filename);
			}
		}
	}

	/* 0 = SUCCESS
	 * 1 = Some hunks failed
	 * 2 = More serious problems
	 */
	return ret;
}
