/* X-Chat
 * Copyright (C) 1998 Peter Zelezny.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <unistd.h>

#include "fe-gtk.h"
#include "palette.h"

#include "../common/hexchat.h"
#include "../common/util.h"
#include "../common/cfgfiles.h"
#include "../common/typedef.h"
#include "../common/hexchatc.h"


/* Color scheme definitions - each scheme has MAX_COL+1 colors */
/* Scheme 0 is "Custom" - uses user-defined colors, not stored here */

/* Default scheme (light theme) - index 1 */
static const GdkColor scheme_default[] = {
	/* mIRC colors 0-15 */
	{0, 0xd3d3, 0xd7d7, 0xcfcf}, /* 0 white */
	{0, 0x2e2e, 0x3434, 0x3636}, /* 1 black */
	{0, 0x3434, 0x6565, 0xa4a4}, /* 2 blue */
	{0, 0x4e4e, 0x9a9a, 0x0606}, /* 3 green */
	{0, 0xcccc, 0x0000, 0x0000}, /* 4 red */
	{0, 0x8f8f, 0x3939, 0x0202}, /* 5 light red */
	{0, 0x5c5c, 0x3535, 0x6666}, /* 6 purple */
	{0, 0xcece, 0x5c5c, 0x0000}, /* 7 orange */
	{0, 0xc4c4, 0xa0a0, 0x0000}, /* 8 yellow */
	{0, 0x7373, 0xd2d2, 0x1616}, /* 9 green */
	{0, 0x1111, 0xa8a8, 0x7979}, /* 10 aqua */
	{0, 0x5858, 0xa1a1, 0x9d9d}, /* 11 light aqua */
	{0, 0x5757, 0x7979, 0x9e9e}, /* 12 blue */
	{0, 0xa0d0, 0x42d4, 0x6562}, /* 13 light purple */
	{0, 0x5555, 0x5757, 0x5353}, /* 14 grey */
	{0, 0x8888, 0x8a8a, 0x8585}, /* 15 light grey */
	/* Local colors 16-31 (copy of 0-15) */
	{0, 0xd3d3, 0xd7d7, 0xcfcf}, /* 16 white */
	{0, 0x2e2e, 0x3434, 0x3636}, /* 17 black */
	{0, 0x3434, 0x6565, 0xa4a4}, /* 18 blue */
	{0, 0x4e4e, 0x9a9a, 0x0606}, /* 19 green */
	{0, 0xcccc, 0x0000, 0x0000}, /* 20 red */
	{0, 0x8f8f, 0x3939, 0x0202}, /* 21 light red */
	{0, 0x5c5c, 0x3535, 0x6666}, /* 22 purple */
	{0, 0xcece, 0x5c5c, 0x0000}, /* 23 orange */
	{0, 0xc4c4, 0xa0a0, 0x0000}, /* 24 yellow */
	{0, 0x7373, 0xd2d2, 0x1616}, /* 25 green */
	{0, 0x1111, 0xa8a8, 0x7979}, /* 26 aqua */
	{0, 0x5858, 0xa1a1, 0x9d9d}, /* 27 light aqua */
	{0, 0x5757, 0x7979, 0x9e9e}, /* 28 blue */
	{0, 0xa0d0, 0x42d4, 0x6562}, /* 29 light purple */
	{0, 0x5555, 0x5757, 0x5353}, /* 30 grey */
	{0, 0x8888, 0x8a8a, 0x8585}, /* 31 light grey */
	/* Special colors 32-41 */
	{0, 0xd3d3, 0xd7d7, 0xcfcf}, /* 32 marktext Fore (white) */
	{0, 0x2020, 0x4a4a, 0x8787}, /* 33 marktext Back (blue) */
	{0, 0x2512, 0x29e8, 0x2b85}, /* 34 foreground (black) */
	{0, 0xfae0, 0xfae0, 0xf8c4}, /* 35 background (white) */
	{0, 0x8f8f, 0x3939, 0x0202}, /* 36 marker line (red) */
	{0, 0x3434, 0x6565, 0xa4a4}, /* 37 tab New Data (blue) */
	{0, 0x4e4e, 0x9a9a, 0x0606}, /* 38 tab Nick Mentioned (green) */
	{0, 0xcece, 0x5c5c, 0x0000}, /* 39 tab New Message (orange) */
	{0, 0x8888, 0x8a8a, 0x8585}, /* 40 away user (grey) */
	{0, 0xa4a4, 0x0000, 0x0000}, /* 41 spell checker color (red) */
};

/* Dark scheme - index 2 */
static const GdkColor scheme_dark[] = {
	/* mIRC colors 0-15 */
	{0, 0xd3d3, 0xd7d7, 0xcfcf}, /* 0 white */
	{0, 0x2e2e, 0x3434, 0x3636}, /* 1 black */
	{0, 0x5757, 0x9999, 0xffff}, /* 2 blue */
	{0, 0x7a7a, 0xc9c9, 0x3636}, /* 3 green */
	{0, 0xffff, 0x5555, 0x5555}, /* 4 red */
	{0, 0xcfcf, 0x6a6a, 0x4c4c}, /* 5 light red */
	{0, 0xadad, 0x7f7f, 0xa8a8}, /* 6 purple */
	{0, 0xffff, 0xaaaa, 0x0000}, /* 7 orange */
	{0, 0xffff, 0xffff, 0x5555}, /* 8 yellow */
	{0, 0x5555, 0xffff, 0x5555}, /* 9 light green */
	{0, 0x0000, 0xd3d3, 0xd3d3}, /* 10 aqua */
	{0, 0x8c8c, 0xe8e8, 0xe8e8}, /* 11 light aqua */
	{0, 0x5555, 0x5555, 0xffff}, /* 12 light blue */
	{0, 0xffff, 0x5555, 0xffff}, /* 13 light purple */
	{0, 0x7f7f, 0x7f7f, 0x7f7f}, /* 14 grey */
	{0, 0xd0d0, 0xd0d0, 0xd0d0}, /* 15 light grey */
	/* Local colors 16-31 */
	{0, 0xd3d3, 0xd7d7, 0xcfcf}, /* 16 white */
	{0, 0x2e2e, 0x3434, 0x3636}, /* 17 black */
	{0, 0x5757, 0x9999, 0xffff}, /* 18 blue */
	{0, 0x7a7a, 0xc9c9, 0x3636}, /* 19 green */
	{0, 0xffff, 0x5555, 0x5555}, /* 20 red */
	{0, 0xcfcf, 0x6a6a, 0x4c4c}, /* 21 light red */
	{0, 0xadad, 0x7f7f, 0xa8a8}, /* 22 purple */
	{0, 0xffff, 0xaaaa, 0x0000}, /* 23 orange */
	{0, 0xffff, 0xffff, 0x5555}, /* 24 yellow */
	{0, 0x5555, 0xffff, 0x5555}, /* 25 light green */
	{0, 0x0000, 0xd3d3, 0xd3d3}, /* 26 aqua */
	{0, 0x8c8c, 0xe8e8, 0xe8e8}, /* 27 light aqua */
	{0, 0x5555, 0x5555, 0xffff}, /* 28 light blue */
	{0, 0xffff, 0x5555, 0xffff}, /* 29 light purple */
	{0, 0x7f7f, 0x7f7f, 0x7f7f}, /* 30 grey */
	{0, 0xd0d0, 0xd0d0, 0xd0d0}, /* 31 light grey */
	/* Special colors 32-41 */
	{0, 0xd3d3, 0xd7d7, 0xcfcf}, /* 32 marktext Fore */
	{0, 0x4040, 0x6060, 0x9090}, /* 33 marktext Back */
	{0, 0xd0d0, 0xd0d0, 0xd0d0}, /* 34 foreground (light grey) */
	{0, 0x1e1e, 0x1e1e, 0x2e2e}, /* 35 background (dark) */
	{0, 0xffff, 0x5555, 0x5555}, /* 36 marker line (red) */
	{0, 0x5757, 0x9999, 0xffff}, /* 37 tab New Data (blue) */
	{0, 0x7a7a, 0xc9c9, 0x3636}, /* 38 tab Nick Mentioned (green) */
	{0, 0xffff, 0xaaaa, 0x0000}, /* 39 tab New Message (orange) */
	{0, 0x7f7f, 0x7f7f, 0x7f7f}, /* 40 away user (grey) */
	{0, 0xffff, 0x5555, 0x5555}, /* 41 spell checker color (red) */
};

/* Monokai scheme - index 3 */
static const GdkColor scheme_monokai[] = {
	/* mIRC colors 0-15 */
	{0, 0xf8f8, 0xf8f8, 0xf2f2}, /* 0 white */
	{0, 0x2727, 0x2828, 0x2222}, /* 1 black */
	{0, 0x6666, 0xd9d9, 0xefef}, /* 2 blue */
	{0, 0xa6a6, 0xe2e2, 0x2e2e}, /* 3 green */
	{0, 0xf9f9, 0x2626, 0x7272}, /* 4 red */
	{0, 0xfdfd, 0x9797, 0x1f1f}, /* 5 light red (orange) */
	{0, 0xaeae, 0x8181, 0xffff}, /* 6 purple */
	{0, 0xfdfd, 0x9797, 0x1f1f}, /* 7 orange */
	{0, 0xe6e6, 0xdbdb, 0x7474}, /* 8 yellow */
	{0, 0xa6a6, 0xe2e2, 0x2e2e}, /* 9 light green */
	{0, 0xa1a1, 0xefef, 0xe4e4}, /* 10 aqua */
	{0, 0x6666, 0xd9d9, 0xefef}, /* 11 light aqua */
	{0, 0x6666, 0xd9d9, 0xefef}, /* 12 light blue */
	{0, 0xaeae, 0x8181, 0xffff}, /* 13 light purple */
	{0, 0x7575, 0x7171, 0x5e5e}, /* 14 grey */
	{0, 0xa5a5, 0x9f9f, 0x8585}, /* 15 light grey */
	/* Local colors 16-31 */
	{0, 0xf8f8, 0xf8f8, 0xf2f2}, /* 16 white */
	{0, 0x2727, 0x2828, 0x2222}, /* 17 black */
	{0, 0x6666, 0xd9d9, 0xefef}, /* 18 blue */
	{0, 0xa6a6, 0xe2e2, 0x2e2e}, /* 19 green */
	{0, 0xf9f9, 0x2626, 0x7272}, /* 20 red */
	{0, 0xfdfd, 0x9797, 0x1f1f}, /* 21 light red (orange) */
	{0, 0xaeae, 0x8181, 0xffff}, /* 22 purple */
	{0, 0xfdfd, 0x9797, 0x1f1f}, /* 23 orange */
	{0, 0xe6e6, 0xdbdb, 0x7474}, /* 24 yellow */
	{0, 0xa6a6, 0xe2e2, 0x2e2e}, /* 25 light green */
	{0, 0xa1a1, 0xefef, 0xe4e4}, /* 26 aqua */
	{0, 0x6666, 0xd9d9, 0xefef}, /* 27 light aqua */
	{0, 0x6666, 0xd9d9, 0xefef}, /* 28 light blue */
	{0, 0xaeae, 0x8181, 0xffff}, /* 29 light purple */
	{0, 0x7575, 0x7171, 0x5e5e}, /* 30 grey */
	{0, 0xa5a5, 0x9f9f, 0x8585}, /* 31 light grey */
	/* Special colors 32-41 */
	{0, 0xf8f8, 0xf8f8, 0xf2f2}, /* 32 marktext Fore */
	{0, 0x4949, 0x4848, 0x3e3e}, /* 33 marktext Back */
	{0, 0xf8f8, 0xf8f8, 0xf2f2}, /* 34 foreground */
	{0, 0x2727, 0x2828, 0x2222}, /* 35 background */
	{0, 0xf9f9, 0x2626, 0x7272}, /* 36 marker line (pink) */
	{0, 0x6666, 0xd9d9, 0xefef}, /* 37 tab New Data (blue) */
	{0, 0xa6a6, 0xe2e2, 0x2e2e}, /* 38 tab Nick Mentioned (green) */
	{0, 0xfdfd, 0x9797, 0x1f1f}, /* 39 tab New Message (orange) */
	{0, 0x7575, 0x7171, 0x5e5e}, /* 40 away user (grey) */
	{0, 0xf9f9, 0x2626, 0x7272}, /* 41 spell checker color (pink) */
};

/* Solarized Dark scheme - index 4 */
static const GdkColor scheme_solarized_dark[] = {
	/* mIRC colors 0-15 */
	{0, 0xfdfd, 0xf6f6, 0xe3e3}, /* 0 base3 (brightest) */
	{0, 0x0000, 0x2b2b, 0x3636}, /* 1 base03 (darkest) */
	{0, 0x2626, 0x8b8b, 0xd2d2}, /* 2 blue */
	{0, 0x8585, 0x9999, 0x0000}, /* 3 green */
	{0, 0xdcdc, 0x3232, 0x2f2f}, /* 4 red */
	{0, 0xcbcb, 0x4b4b, 0x1616}, /* 5 orange */
	{0, 0xd3d3, 0x3636, 0x8282}, /* 6 magenta */
	{0, 0xcbcb, 0x4b4b, 0x1616}, /* 7 orange */
	{0, 0xb5b5, 0x8989, 0x0000}, /* 8 yellow */
	{0, 0x8585, 0x9999, 0x0000}, /* 9 green */
	{0, 0x2a2a, 0xa1a1, 0x9898}, /* 10 cyan */
	{0, 0x2a2a, 0xa1a1, 0x9898}, /* 11 cyan */
	{0, 0x2626, 0x8b8b, 0xd2d2}, /* 12 blue */
	{0, 0x6c6c, 0x7171, 0xc4c4}, /* 13 violet */
	{0, 0x5858, 0x6e6e, 0x7575}, /* 14 base01 */
	{0, 0x8383, 0x9494, 0x9696}, /* 15 base0 */
	/* Local colors 16-31 */
	{0, 0xfdfd, 0xf6f6, 0xe3e3}, /* 16 */
	{0, 0x0000, 0x2b2b, 0x3636}, /* 17 */
	{0, 0x2626, 0x8b8b, 0xd2d2}, /* 18 */
	{0, 0x8585, 0x9999, 0x0000}, /* 19 */
	{0, 0xdcdc, 0x3232, 0x2f2f}, /* 20 */
	{0, 0xcbcb, 0x4b4b, 0x1616}, /* 21 */
	{0, 0xd3d3, 0x3636, 0x8282}, /* 22 */
	{0, 0xcbcb, 0x4b4b, 0x1616}, /* 23 */
	{0, 0xb5b5, 0x8989, 0x0000}, /* 24 */
	{0, 0x8585, 0x9999, 0x0000}, /* 25 */
	{0, 0x2a2a, 0xa1a1, 0x9898}, /* 26 */
	{0, 0x2a2a, 0xa1a1, 0x9898}, /* 27 */
	{0, 0x2626, 0x8b8b, 0xd2d2}, /* 28 */
	{0, 0x6c6c, 0x7171, 0xc4c4}, /* 29 */
	{0, 0x5858, 0x6e6e, 0x7575}, /* 30 */
	{0, 0x8383, 0x9494, 0x9696}, /* 31 */
	/* Special colors 32-41 */
	{0, 0x9393, 0xa1a1, 0xa1a1}, /* 32 marktext Fore (base1) */
	{0, 0x0707, 0x3636, 0x4242}, /* 33 marktext Back (base02) */
	{0, 0x8383, 0x9494, 0x9696}, /* 34 foreground (base0) */
	{0, 0x0000, 0x2b2b, 0x3636}, /* 35 background (base03) */
	{0, 0xdcdc, 0x3232, 0x2f2f}, /* 36 marker line (red) */
	{0, 0x2626, 0x8b8b, 0xd2d2}, /* 37 tab New Data (blue) */
	{0, 0x8585, 0x9999, 0x0000}, /* 38 tab Nick Mentioned (green) */
	{0, 0xcbcb, 0x4b4b, 0x1616}, /* 39 tab New Message (orange) */
	{0, 0x5858, 0x6e6e, 0x7575}, /* 40 away user (base01) */
	{0, 0xdcdc, 0x3232, 0x2f2f}, /* 41 spell checker color (red) */
};

/* Solarized Light scheme - index 5 */
static const GdkColor scheme_solarized_light[] = {
	/* mIRC colors 0-15 */
	{0, 0xfdfd, 0xf6f6, 0xe3e3}, /* 0 base3 (brightest) */
	{0, 0x0000, 0x2b2b, 0x3636}, /* 1 base03 (darkest) */
	{0, 0x2626, 0x8b8b, 0xd2d2}, /* 2 blue */
	{0, 0x8585, 0x9999, 0x0000}, /* 3 green */
	{0, 0xdcdc, 0x3232, 0x2f2f}, /* 4 red */
	{0, 0xcbcb, 0x4b4b, 0x1616}, /* 5 orange */
	{0, 0xd3d3, 0x3636, 0x8282}, /* 6 magenta */
	{0, 0xcbcb, 0x4b4b, 0x1616}, /* 7 orange */
	{0, 0xb5b5, 0x8989, 0x0000}, /* 8 yellow */
	{0, 0x8585, 0x9999, 0x0000}, /* 9 green */
	{0, 0x2a2a, 0xa1a1, 0x9898}, /* 10 cyan */
	{0, 0x2a2a, 0xa1a1, 0x9898}, /* 11 cyan */
	{0, 0x2626, 0x8b8b, 0xd2d2}, /* 12 blue */
	{0, 0x6c6c, 0x7171, 0xc4c4}, /* 13 violet */
	{0, 0x5858, 0x6e6e, 0x7575}, /* 14 base01 */
	{0, 0x8383, 0x9494, 0x9696}, /* 15 base0 */
	/* Local colors 16-31 */
	{0, 0xfdfd, 0xf6f6, 0xe3e3}, /* 16 */
	{0, 0x0000, 0x2b2b, 0x3636}, /* 17 */
	{0, 0x2626, 0x8b8b, 0xd2d2}, /* 18 */
	{0, 0x8585, 0x9999, 0x0000}, /* 19 */
	{0, 0xdcdc, 0x3232, 0x2f2f}, /* 20 */
	{0, 0xcbcb, 0x4b4b, 0x1616}, /* 21 */
	{0, 0xd3d3, 0x3636, 0x8282}, /* 22 */
	{0, 0xcbcb, 0x4b4b, 0x1616}, /* 23 */
	{0, 0xb5b5, 0x8989, 0x0000}, /* 24 */
	{0, 0x8585, 0x9999, 0x0000}, /* 25 */
	{0, 0x2a2a, 0xa1a1, 0x9898}, /* 26 */
	{0, 0x2a2a, 0xa1a1, 0x9898}, /* 27 */
	{0, 0x2626, 0x8b8b, 0xd2d2}, /* 28 */
	{0, 0x6c6c, 0x7171, 0xc4c4}, /* 29 */
	{0, 0x5858, 0x6e6e, 0x7575}, /* 30 */
	{0, 0x8383, 0x9494, 0x9696}, /* 31 */
	/* Special colors 32-41 */
	{0, 0x5858, 0x6e6e, 0x7575}, /* 32 marktext Fore (base01) */
	{0, 0xeeee, 0xe8e8, 0xd5d5}, /* 33 marktext Back (base2) */
	{0, 0x6565, 0x7b7b, 0x8383}, /* 34 foreground (base00) */
	{0, 0xfdfd, 0xf6f6, 0xe3e3}, /* 35 background (base3) */
	{0, 0xdcdc, 0x3232, 0x2f2f}, /* 36 marker line (red) */
	{0, 0x2626, 0x8b8b, 0xd2d2}, /* 37 tab New Data (blue) */
	{0, 0x8585, 0x9999, 0x0000}, /* 38 tab Nick Mentioned (green) */
	{0, 0xcbcb, 0x4b4b, 0x1616}, /* 39 tab New Message (orange) */
	{0, 0x9393, 0xa1a1, 0xa1a1}, /* 40 away user (base1) */
	{0, 0xdcdc, 0x3232, 0x2f2f}, /* 41 spell checker color (red) */
};

/* Array of pointers to color schemes */
static const GdkColor *color_schemes[] = {
	NULL,                    /* 0 = Custom (no predefined colors) */
	scheme_default,          /* 1 = Default */
	scheme_dark,             /* 2 = Dark */
	scheme_monokai,          /* 3 = Monokai */
	scheme_solarized_dark,   /* 4 = Solarized Dark */
	scheme_solarized_light,  /* 5 = Solarized Light */
};

/* Number of color schemes available */
const int palette_scheme_count = sizeof(color_schemes) / sizeof(color_schemes[0]);

/* Color scheme names for GUI */
const char * const palette_scheme_names[] = {
	N_("Custom"),
	N_("Default"),
	N_("Dark"),
	N_("Monokai"),
	N_("Solarized Dark"),
	N_("Solarized Light"),
	NULL
};


GdkColor colors[] = {
	/* colors for xtext */
	{0, 0xd3d3, 0xd7d7, 0xcfcf}, /* 0 white */
	{0, 0x2e2e, 0x3434, 0x3636}, /* 1 black */
	{0, 0x3434, 0x6565, 0xa4a4}, /* 2 blue */
	{0, 0x4e4e, 0x9a9a, 0x0606}, /* 3 green */
	{0, 0xcccc, 0x0000, 0x0000}, /* 4 red */
	{0, 0x8f8f, 0x3939, 0x0202}, /* 5 light red */
	{0, 0x5c5c, 0x3535, 0x6666}, /* 6 purple */
	{0, 0xcece, 0x5c5c, 0x0000}, /* 7 orange */
	{0, 0xc4c4, 0xa0a0, 0x0000}, /* 8 yellow */
	{0, 0x7373, 0xd2d2, 0x1616}, /* 9 green */
	{0, 0x1111, 0xa8a8, 0x7979}, /* 10 aqua */
	{0, 0x5858, 0xa1a1, 0x9d9d}, /* 11 light aqua */
	{0, 0x5757, 0x7979, 0x9e9e}, /* 12 blue */
	{0, 0xa0d0, 0x42d4, 0x6562}, /* 13 light purple */
	{0, 0x5555, 0x5757, 0x5353}, /* 14 grey */
	{0, 0x8888, 0x8a8a, 0x8585}, /* 15 light grey */

	{0, 0xd3d3, 0xd7d7, 0xcfcf}, /* 16 white */
	{0, 0x2e2e, 0x3434, 0x3636}, /* 17 black */
	{0, 0x3434, 0x6565, 0xa4a4}, /* 18 blue */
	{0, 0x4e4e, 0x9a9a, 0x0606}, /* 19 green */
	{0, 0xcccc, 0x0000, 0x0000}, /* 20 red */
	{0, 0x8f8f, 0x3939, 0x0202}, /* 21 light red */
	{0, 0x5c5c, 0x3535, 0x6666}, /* 22 purple */
	{0, 0xcece, 0x5c5c, 0x0000}, /* 23 orange */
	{0, 0xc4c4, 0xa0a0, 0x0000}, /* 24 yellow */
	{0, 0x7373, 0xd2d2, 0x1616}, /* 25 green */
	{0, 0x1111, 0xa8a8, 0x7979}, /* 26 aqua */
	{0, 0x5858, 0xa1a1, 0x9d9d}, /* 27 light aqua */
	{0, 0x5757, 0x7979, 0x9e9e}, /* 28 blue */
	{0, 0xa0d0, 0x42d4, 0x6562}, /* 29 light purple */
	{0, 0x5555, 0x5757, 0x5353}, /* 30 grey */
	{0, 0x8888, 0x8a8a, 0x8585}, /* 31 light grey */

	{0, 0xd3d3, 0xd7d7, 0xcfcf}, /* 32 marktext Fore (white) */
	{0, 0x2020, 0x4a4a, 0x8787}, /* 33 marktext Back (blue) */
	{0, 0x2512, 0x29e8, 0x2b85}, /* 34 foreground (black) */
	{0, 0xfae0, 0xfae0, 0xf8c4}, /* 35 background (white) */
	{0, 0x8f8f, 0x3939, 0x0202}, /* 36 marker line (red) */

	/* colors for GUI */
	{0, 0x3434, 0x6565, 0xa4a4}, /* 37 tab New Data (dark red) */
	{0, 0x4e4e, 0x9a9a, 0x0606}, /* 38 tab Nick Mentioned (blue) */
	{0, 0xcece, 0x5c5c, 0x0000}, /* 39 tab New Message (red) */
	{0, 0x8888, 0x8a8a, 0x8585}, /* 40 away user (grey) */
	{0, 0xa4a4, 0x0000, 0x0000}, /* 41 spell checker color (red) */
};

void
palette_alloc (GtkWidget * widget)
{
	int i;
	static int done_alloc = FALSE;
	GdkColormap *cmap;

	if (!done_alloc)		  /* don't do it again */
	{
		done_alloc = TRUE;
		cmap = gtk_widget_get_colormap (widget);
		for (i = MAX_COL; i >= 0; i--)
			gdk_colormap_alloc_color (cmap, &colors[i], FALSE, TRUE);
	}
}

void
palette_load (void)
{
	int i, j, fh;
	char prefname[256];
	struct stat st;
	char *cfg;
	guint16 red, green, blue;

	fh = hexchat_open_file ("colors.conf", O_RDONLY, 0, 0);
	if (fh != -1)
	{
		fstat (fh, &st);
		cfg = g_malloc0 (st.st_size + 1);
		read (fh, cfg, st.st_size);

		/* mIRC colors 0-31 are here */
		for (i = 0; i < 32; i++)
		{
			g_snprintf (prefname, sizeof prefname, "color_%d", i);
			cfg_get_color (cfg, prefname, &red, &green, &blue);
			colors[i].red = red;
			colors[i].green = green;
			colors[i].blue = blue;
		}

		/* our special colors are mapped at 256+ */
		for (i = 256, j = 32; j < MAX_COL+1; i++, j++)
		{
			g_snprintf (prefname, sizeof prefname, "color_%d", i);
			cfg_get_color (cfg, prefname, &red, &green, &blue);
			colors[j].red = red;
			colors[j].green = green;
			colors[j].blue = blue;
		}
		g_free (cfg);
		close (fh);
	}
}

void
palette_save (void)
{
	int i, j, fh;
	char prefname[256];

	fh = hexchat_open_file ("colors.conf", O_TRUNC | O_WRONLY | O_CREAT, 0600, XOF_DOMODE);
	if (fh != -1)
	{
		/* mIRC colors 0-31 are here */
		for (i = 0; i < 32; i++)
		{
			g_snprintf (prefname, sizeof prefname, "color_%d", i);
			cfg_put_color (fh, colors[i].red, colors[i].green, colors[i].blue, prefname);
		}

		/* our special colors are mapped at 256+ */
		for (i = 256, j = 32; j < MAX_COL+1; i++, j++)
		{
			g_snprintf (prefname, sizeof prefname, "color_%d", i);
			cfg_put_color (fh, colors[j].red, colors[j].green, colors[j].blue, prefname);
		}

		close (fh);
	}
}

void
palette_apply_scheme (int scheme)
{
	int i;
	const GdkColor *scheme_colors;

	/* scheme 0 = Custom, do nothing */
	if (scheme <= 0 || scheme >= palette_scheme_count)
		return;

	scheme_colors = color_schemes[scheme];
	if (scheme_colors == NULL)
		return;

	/* Copy all colors from the scheme to the active palette */
	for (i = 0; i <= MAX_COL; i++)
	{
		colors[i].red = scheme_colors[i].red;
		colors[i].green = scheme_colors[i].green;
		colors[i].blue = scheme_colors[i].blue;
	}
}
