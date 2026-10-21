/*
 * fake_riscos.c - just enough RISC OS for egl_riscos.c on a Linux host:
 * a 32bpp screen in memory, a few Wimp windows (one redraw rectangle each:
 * the visible area), OS_SpriteOp create/plot, mode and VDU variables.
 * Pointers go through 32-bit registers as on RISC OS, so everything that
 * reaches a SWI must live below 2 GB (the harness builds -no-pie, keeps
 * malloc on brk and runs its tests on a low stack). SWIs are serialised
 * by a lock, so threaded tests (dEQP's multithread groups) are safe.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <sys/socket.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <dirent.h>
#include <strings.h>
#include <pthread.h>
#include <cairo.h>
#include <kernel.h>
#include <swis.h>
#include "fake_riscos.h"

fake_screen_t fake_screen;
fake_window_t fake_windows[FAKE_MAX_WINDOWS];
int fake_update_rect[4];
int fake_vsyncs, fake_update_calls, fake_redraw_calls, fake_plots;
int fake_sprite_creates;
int fake_force_redraws, fake_force_rect[5], fake_scaled_plots;
int fake_wimp_nulls, fake_wimp_script[FAKE_SCRIPT_MAX][4], fake_wimp_script_len;
int fake_pointer[3], fake_wheel, fake_menus_opened, fake_pointer_shape = 1;
const int *fake_menu;
int fake_menu_x, fake_menu_y, fake_menus_closed;
/* Wimp messages: sent by the task (hook), queued for it (fake_queue_message) */
int (*fake_starttask_hook)(const char *command);
void (*fake_message_hook)(int reason, int *block, int dest, int icon);
int fake_dragbox_type = -1, fake_dragboxes;
const int *fake_ptr_sprite;
int fake_wimp_ptr_write = 1;
char fake_ptr_name[16] = "ptr_default";
int fake_ptr_shape = 1, fake_ptr_flags = 1, fake_ptr_hx, fake_ptr_hy, fake_ptr_sets;
int fake_ptr_palette[4] = { 0, 0xFFFF0010, 0x99000010, 0x0000FF10 };  /* OS_ReadPalette 25 */
static int msg_queue[16][65], msg_queue_len, next_my_ref = 1000;

void fake_queue_message(int reason, const int *block)
{
    int words = (block[0] + 3) / 4;
    if (msg_queue_len >= 16) return;
    if (words > 64) words = 64;
    memset(msg_queue[msg_queue_len], 0, sizeof msg_queue[0]);
    msg_queue[msg_queue_len][0] = reason;
    memcpy(&msg_queue[msg_queue_len][1], block, words * 4);
    msg_queue_len++;
}

int fake_new_ref(void)
{
    return next_my_ref++;
}
unsigned char fake_keys_down[128];
int fake_log_menus;
static int wimp_null_due, fake_wimp_title_window;
int fake_wimp_polls, fake_wimp_keys_passed, fake_wimp_tasks;
void (*fake_wimp_hook)(int reason);
int (*fake_swi_hook)(int no, _kernel_swi_regs *r, _kernel_oserror **e);
int fake_window_behind = -1;
const char *fake_wimp_title;
int fake_wimp_desktop;
static int wimp_next_handle = 0x7000, wimp_last_window, wimp_stage, wimp_step;
/* The window scripted events go to: fake_target_window if set, else the
   newest one (a program's tooltips and pop-ups are newer windows) */
int fake_target_window;
int fake_target_icon = -1;            /* Mouse_Click's icon (-1: none) */
int fake_iconbar_icons, fake_iconbar_flags, fake_processed_key = -1;
char fake_iconbar_sprite[16];
int fake_reports, fake_report_flags, fake_report_answer = 1;
char fake_report_text[256], fake_report_buttons[64];
#define TARGET_WINDOW (fake_target_window ? fake_target_window : wimp_last_window)
static int wimp_created[64], wimp_num_created;     /* windows in creation order */

static _kernel_oserror err = { 1, "fake error" };
/* 1 = OS_SpriteOp converts a 32bpp sprite to the screen's colour order
   (what riscos-mesa assumed); 0 = it copies the pixels as they are, as a
   Pi 4 with RISC OS 5 was seen to do (riscos-gtk, 2026-10-04). */
int fake_sprite_plot_converts = 1;
static int wimp_colour, plot_x, plot_y;
static const unsigned int wimp_palette_tbgr[16] = {
    0xFFFFFF, 0xDDDDDD, 0xBBBBBB, 0x999999, 0x777777, 0x555555, 0x333333, 0x000000,
    0x994400, 0x00EEEE, 0x00CC00, 0x0000DD, 0xBBEEEE, 0x008855, 0x00BBFF, 0xFFBB00 };
int fake_fills;
static int clip[4];                  /* graphics window, OS units x0,y0,x1,y1 (excl.) */

/* A 32bpp mode selector like OS_ScreenMode 1 would return. */
static int screen_selector[16];

static _kernel_oserror *error(const char *msg)
{
    snprintf(err.errmess, sizeof err.errmess, "%s", msg);
    return &err;
}

/* The script entry the last Wimp_Poll returned (-1 before the script) */
int fake_wimp_script_pos(void)
{
    return wimp_stage == 2 ? wimp_step - 1 : -1;
}

void fake_set_screen(int w, int h, int trgb, int log2bpp)
{
    free(fake_screen.mem);
    fake_screen.w = w;
    fake_screen.h = h;
    fake_screen.xeig = fake_screen.yeig = 1;
    /* FAKE_EIG=0: an EX0 EY0 (180 dpi) mode, 1 OS unit a pixel. The screen
       is made twice as many pixels each way, so it's the same size (in OS
       units, and in GDK pixels at GDK scale 2) as the test asked for, and
       its coordinates and fake_screen_pixel stay in GDK pixels. */
    if (getenv("FAKE_EIG"))
        fake_screen.xeig = fake_screen.yeig = atoi(getenv("FAKE_EIG"));
    if (fake_screen.xeig == 0 && fake_screen.yeig == 0) {
        w *= 2;
        h *= 2;
        fake_screen.w = w;
        fake_screen.h = h;
    }
    fake_screen.log2bpp = log2bpp;
    fake_screen.flags = trgb ? 0x4000 : 0;
    fake_screen.line_length = w * 4;
    fake_screen.mem = calloc((size_t) w * h * 3, 4);
    fake_screen.da_size = fake_screen.da_max = w * h * 4;     /* one bank, can't grow */
    fake_screen.vdu_bank = fake_screen.display_bank = 1;
}

static int stack_top;

fake_window_t *fake_open_window(int handle, int x0, int y0, int x1, int y1, int sx, int sy)
{
    int i;
    for (i = 0; i < FAKE_MAX_WINDOWS; i++) {
        if (fake_windows[i].handle == 0 || fake_windows[i].handle == handle) {
            fake_windows[i].handle = handle;
            fake_windows[i].x0 = x0; fake_windows[i].y0 = y0;
            fake_windows[i].x1 = x1; fake_windows[i].y1 = y1;
            fake_windows[i].sx = sx; fake_windows[i].sy = sy;
            fake_windows[i].stack = ++stack_top;     /* opened = brought to the front */
            return &fake_windows[i];
        }
    }
    return NULL;
}

static fake_window_t *find_window(int handle)
{
    int i;
    for (i = 0; i < FAKE_MAX_WINDOWS; i++)
        if (fake_windows[i].handle == handle && handle != 0)
            return &fake_windows[i];
    return NULL;
}

unsigned int fake_bank_pixel(int bank, int x, int y_from_top)
{
    return fake_screen.mem[(size_t) (bank - 1) * fake_screen.w * fake_screen.h +
                           (size_t) y_from_top * (fake_screen.line_length / 4) + x];
}

/* The pixel at GDK pixel x, y: the screen's pixel, or in an EX0 EY0 mode
   (GDK scale 2) the top left one of the four */
unsigned int fake_screen_pixel(int x, int y_from_top)
{
    if (fake_screen.xeig == 0 && fake_screen.yeig == 0) {
        x *= 2;
        y_from_top *= 2;
    }
    return fake_screen.mem[(size_t) y_from_top * (fake_screen.line_length / 4) + x];
}

/* Colour order of a sprite mode word or mode selector: 1 = TRGB. -1 if not 32bpp. */
static int mode_info(int mode, int *log2bpp, int *flags)
{
    if (mode == -1) {
        *log2bpp = fake_screen.log2bpp;
        *flags = fake_screen.flags;
        return 1;
    }
    if (mode & 1) {                                 /* sprite mode word */
        int type = (mode >> 27) & 15;
        if (type == 6) { *log2bpp = 5; *flags = 0; return 1; }
        if (type == 5) { *log2bpp = 4; *flags = 0; return 1; }
        return 0;
    }
    if (mode >= 256) {                              /* mode selector */
        const int *sel = (const int *) (long) mode;
        int i;
        if ((sel[0] & 0xFF) != 1) return 0;
        *log2bpp = sel[3];
        *flags = 0;
        for (i = 5; sel[i] != -1; i += 2)
            if (sel[i] == 0) *flags = sel[i + 1];
        return 1;
    }
    *log2bpp = 3;                                   /* old numbered mode: 8bpp */
    *flags = 0;
    return 1;
}


/* The Wimp's sprite pool (Wimp_SpriteOp 24 by name), a few sprites in
   different formats for the theme (gdkwimptheme-riscos.c). See
   fake_riscos.h for what each is. */
unsigned int fake_wimp_palette[20] = {
    0xFFFFFF10, 0xDDDDDD10, 0xBBBBBB10, 0x99999910, 0x77777710, 0x55555510, 0x33333310, 0x00000010,
    0x99440010, 0x00EEEE10, 0x00CC0010, 0x0000DD10, 0xBBEEEE10, 0x00885510, 0x00BBFF10, 0xFFBB0010,
    0, 0, 0, 0 };
int fake_wimp_sprites = 1;
static int pool[4096], pool_used;

static int *pool_sprite(const char *name, int wpx, int h, int bpp, unsigned mode, int pal_entries,
                        int mask_kind /* 0 none, 1 old, 2 new 1bpp, 3 alpha 8bpp */)
{
    int words = (wpx * bpp + 31) / 32, img_bytes = words * 4 * h, mask_bytes = 0;
    int *spr = &pool[pool_used];
    if (mask_kind == 1) mask_bytes = img_bytes;
    else if (mask_kind == 2) mask_bytes = (wpx + 31) / 32 * 4 * h;
    else if (mask_kind == 3) mask_bytes = (wpx * 8 + 31) / 32 * 4 * h;
    memset(spr, 0, 44 + pal_entries * 8 + img_bytes + mask_bytes);
    strncpy((char *) &spr[1], name, 12);
    spr[4] = words - 1; spr[5] = h - 1; spr[6] = 0; spr[7] = (wpx * bpp - 1) % 32;
    spr[8] = 44 + pal_entries * 8;
    spr[9] = mask_kind ? spr[8] + img_bytes : spr[8];
    spr[10] = (int) mode;
    spr[0] = spr[9] + mask_bytes - (mask_kind ? 0 : 0);
    if (!mask_kind) spr[0] = spr[8] + img_bytes;
    pool_used += spr[0] / 4;
    return spr;
}

static void put(int *spr, int x, int y, int bpp, unsigned v)
{
    int words = spr[4] + 1, bit = x * bpp;
    unsigned char *row = (unsigned char *) spr + spr[8] + y * words * 4;
    if (bpp == 32) { memcpy(row + x * 4, &v, 4); return; }
    row[bit >> 3] |= (unsigned char) (v << (bit & 7));
}

static void put_mask(int *spr, int kind, int bpp, int wpx, int x, int y, int a)
{
    unsigned char *m = (unsigned char *) spr + spr[9];
    if (kind == 1) {                   /* old: same depth as the image */
        int words = spr[4] + 1, bit = x * bpp;
        if (a) m[y * words * 4 + (bit >> 3)] |= (unsigned char) (((1u << bpp) - 1) << (bit & 7));
    } else if (kind == 2) {
        if (a) m[y * ((wpx + 31) / 32 * 4) + (x >> 3)] |= (unsigned char) (1 << (x & 7));
    } else if (kind == 3)
        m[y * ((wpx * 8 + 31) / 32 * 4) + x] = (unsigned char) a;
}

static void build_pool(void)
{
    int *s, x, y;
    unsigned mode8 = (4u << 27) | (90 << 14) | (90 << 1) | 1;
    unsigned mode4 = (3u << 27) | (90 << 14) | (90 << 1) | 1;
    unsigned mode32a = (6u << 27) | (90 << 14) | (90 << 1) | 1 | 0x80000000u;
    unsigned mode1h = (1u << 27) | (45 << 14) | (90 << 1) | 1;   /* 90 x 45 dpi */
    if (pool_used) return;
    pool_used = 4;                     /* a real sprite area: a header first */
    /* optoff/opton: 12x12 8bpp, no palette (standard 256 colours), 1bpp mask;
       border colour &33 (#bb7733), inside &FF (white) / &13 (red #ff0000? see .h) */
    s = pool_sprite("optoff", 12, 12, 8, mode8, 0, 2);
    for (y = 0; y < 12; y++) for (x = 0; x < 12; x++) {
        int edge = x == 0 || y == 0 || x == 11 || y == 11;
        put(s, x, y, 8, edge ? 0x33 : 0xFF); put_mask(s, 2, 8, 12, x, y, !(x == 0 && y == 0));
    }
    s = pool_sprite("opton", 12, 12, 8, mode8, 0, 2);
    for (y = 0; y < 12; y++) for (x = 0; x < 12; x++) {
        int edge = x == 0 || y == 0 || x == 11 || y == 11;
        put(s, x, y, 8, edge ? 0x33 : (x > 3 && x < 8 && y > 3 && y < 8) ? 0x17 : 0xFF);
        put_mask(s, 2, 8, 12, x, y, 1);
    }
    /* radiooff: 12x12 4bpp with its own palette: 0 white, 1 #12ab34 */
    s = pool_sprite("radiooff", 12, 12, 4, mode4, 16, 2);
    s[11] = s[12] = (int) 0xFFFFFF00u;
    s[13] = s[14] = 0x34AB1200;
    for (y = 0; y < 12; y++) for (x = 0; x < 12; x++) {
        int d = (2 * x - 11) * (2 * x - 11) + (2 * y - 11) * (2 * y - 11);
        put(s, x, y, 4, d > 81 ? 1 : 0); put_mask(s, 2, 4, 12, x, y, d <= 121);
    }
    /* radioon: 12x12 32bpp (#3456de) with a RISC OS 5 8bpp alpha mask */
    s = pool_sprite("radioon", 12, 12, 32, mode32a, 0, 3);
    for (y = 0; y < 12; y++) for (x = 0; x < 12; x++) {
        put(s, x, y, 32, 0x00DE5634); put_mask(s, 3, 32, 12, x, y, x < 6 ? 255 : 128);
    }
    /* gright: 10x10 old numbered mode 28 (8bpp), old-style mask; colour &C0 */
    s = pool_sprite("gright", 10, 10, 8, 28, 0, 1);
    for (y = 0; y < 10; y++) for (x = 0; x < 10; x++) {
        put(s, x, y, 8, 0xC0); put_mask(s, 1, 8, 10, x, y, x <= y);
    }
    /* directory: 16x16 8bpp, a filled square (the icon theme's "folder") */
    s = pool_sprite("directory", 16, 16, 8, mode8, 0, 2);
    for (y = 0; y < 16; y++) for (x = 0; x < 16; x++) {
        put(s, x, y, 8, 0x5C); put_mask(s, 2, 8, 16, x, y, 1);
    }
    /* left, right: 8x4 1bpp at 90 x 45 dpi (each row doubled on screen),
       no palette: Wimp colours 0 and 7 */
    s = pool_sprite("left", 8, 4, 1, mode1h, 0, 0);
    for (y = 0; y < 4; y++) for (x = 0; x < 8; x++) put(s, x, y, 1, x == y);
    s = pool_sprite("right", 8, 4, 1, mode1h, 0, 0);
    for (y = 0; y < 4; y++) for (x = 0; x < 8; x++) put(s, x, y, 1, x == 7 - y);
    {
        int off = 4, n = 0;
        while (off < pool_used) { n++; off += pool[off] / 4; }
        pool[0] = (int) sizeof pool; pool[1] = n; pool[2] = 16; pool[3] = pool_used * 4;
    }
}

/* The Wimp's tool (window border) sprites: a real sprite area, as
   Wimp_ReadSysInfo 9 gives it. 32bpp sprites, no masks, one colour each:
   fake_tool_colour (&BBGGRR) + index (see fake_riscos.h). */
static int tool_area[8192];
int fake_tool_sprites = 1;
const char *fake_tool_names[] = {
    "uicon", "puicon", "dicon", "pdicon", "vwelltcap", "vwellbcap", "vwellt", "vwellb",
    "vbart", "vbarb", "vbarmid", "pvbart", "pvbarb", "pvbarmid",
    "licon", "plicon", "ricon", "pricon", "hwelllcap", "hwellrcap", "hwelll", "hwellr",
    "hbarl", "hbarr", "hbarmid", "phbarl", "phbarr", "phbarmid", "blicon", "sicon", "psicon",
    NULL };

static const int *build_tool_area(void)
{
    /* As a RISC OS 5 Pi's Wimp_ReadSysInfo 9 block (read on the Pi with 0.15):
       +0 a size, +4 the count, +8/+12 words that aren't offsets, +16 and
       +20 a pointer to a table of pointers to the sprites, which are
       elsewhere (here: from word 1024 on, outside the block's size). */
    int i, off = 4096;
    static int table[64];
    unsigned mode32 = (6u << 27) | (90 << 14) | (90 << 1) | 1;
    if (tool_area[1]) return tool_area;
    for (i = 0; fake_tool_names[i]; i++) {
        int *spr = &tool_area[off / 4], x, y, wpx = 14, h = 14;
        memset(spr, 0, 44 + wpx * h * 4);
        spr[0] = 44 + wpx * h * 4;
        strncpy((char *) &spr[1], fake_tool_names[i], 12);
        spr[4] = wpx - 1; spr[5] = h - 1; spr[6] = 0; spr[7] = 31;
        spr[8] = spr[9] = 44; spr[10] = (int) mode32;
        for (y = 0; y < h; y++) for (x = 0; x < wpx; x++)
            spr[11 + y * wpx + x] = 0x002010A0 + i;   /* &BBGGRR: #a01020, + i in red */
        table[i] = (int) (long) spr;
        off += spr[0];
    }
    tool_area[0] = 64; tool_area[1] = i; tool_area[2] = 0x0042F3A4; tool_area[3] = 0x00438124;
    tool_area[4] = tool_area[5] = (int) (long) table;
    tool_area[6] = 0x234; tool_area[7] = 0x20;
    return tool_area;
}


/* ---- the filing system: OS_GBPB 12, OS_FSControl 33 / 18 ----
   A RISC OS directory name is the host's path with '/' and '.' swapped
   (as gdk_riscos_path_to_unix); "FS:$..." / "FS::disc.$..." names are
   under $FAKE_FS_ROOT (default "/"). Types: directories &1000 ("!" names
   &2000), a ",xxx" suffix, else by extension. */
static const struct { int no; const char *name; } fake_fs[] = {
    { 23, "RAM" }, { 46, "Resources" }, { 50, "null" }, { 89, "SDFS" }, { 99, "HostFS" },
    { 102, "LanMan98" },
};
const char *fake_fs_name(int no)
{
    unsigned i;
    for (i = 0; i < sizeof fake_fs / sizeof fake_fs[0]; i++)
        if (fake_fs[i].no == no) return fake_fs[i].name;
    return NULL;
}
static const struct { int type; const char *name; } fake_types[] = {
    { 0xFFF, "Text" }, { 0xFFD, "Data" }, { 0xFAF, "HTML" }, { 0xB60, "PNG" },
    { 0xC85, "JPEG" }, { 0xDFE, "CSV" }, { 0xADF, "PDF" }, { 0xFF9, "Sprite" },
    { 0xFEB, "Obey" }, { 0xFFB, "BASIC" }, { 0xAFF, "DrawFile" }, { 0xA91, "Archive" },
};
void fake_type_name(int type, int *r2, int *r3)
{
    char text[16];
    unsigned i;
    snprintf(text, sizeof text, "&%03X        ", type);
    for (i = 0; i < sizeof fake_types / sizeof fake_types[0]; i++)
        if (fake_types[i].type == type) snprintf(text, sizeof text, "%-8s", fake_types[i].name);
    memcpy(r2, text, 4); memcpy(r3, text + 4, 4);
}
static int fake_ext_type(const char *leaf)
{
    static const struct { const char *ext; int type; } t[] = {
        { "txt", 0xFFF }, { "c", 0xFFF }, { "h", 0xFFF }, { "md", 0xFFF }, { "html", 0xFAF },
        { "png", 0xB60 }, { "jpg", 0xC85 }, { "csv", 0xDFE }, { "pdf", 0xADF }, { "zip", 0xA91 },
    };
    const char *dot = strrchr(leaf, '.');
    unsigned i;
    if (!dot) return 0xFFD;
    for (i = 0; i < sizeof t / sizeof t[0]; i++)
        if (strcasecmp(dot + 1, t[i].ext) == 0) return t[i].type;
    return 0xFFD;
}
static void fake_host_path(const char *ro, char *out, size_t size)
{
    const char *dollar = strstr(ro, ":$") ? strchr(ro, '$') : NULL;
    const char *root = getenv("FAKE_FS_ROOT");
    size_t n;
    char *q;
    if (strstr(ro, "::") && strchr(ro, '$')) dollar = strchr(ro, '$');
    if (dollar) {
        snprintf(out, size, "%s", root ? root : "/");
        ro = dollar + 1;
        if (*ro == '.') ro++;
    } else
        snprintf(out, size, "/");
    n = strlen(out);
    if (*ro && n > 1 && out[n - 1] != '/' && n + 1 < size) { out[n++] = '/'; out[n] = '\0'; }
    for (q = out + n; *ro && (size_t) (q - out) + 1 < size; ro++)
        *q++ = *ro == '.' ? '/' : *ro == '/' ? '.' : *ro;
    *q = '\0';
}
static _kernel_oserror *fake_gbpb12(_kernel_swi_regs *r)
{
    static _kernel_oserror notfound = { 0xD6, "Not found" };
    char host[1024], full[1300];
    struct dirent *de;
    DIR *d;
    int index = 0, start = r->r[4], want = r->r[3], got = 0, more = 0;
    char *buf = (char *) (long) r->r[2], *end = buf + r->r[5], *p = buf;

    fake_host_path((const char *) (long) r->r[1], host, sizeof host);
    if (!(d = opendir(host))) return &notfound;
    while ((de = readdir(d))) {
        struct stat st;
        char leaf[256], *comma;
        int type, i, *w;
        size_t len;
        unsigned long long cs;
        if (de->d_name[0] == '.') continue;
        if (index++ < start) continue;
        if (got >= want) { more = 1; break; }
        snprintf(full, sizeof full, "%s/%s", host, de->d_name);
        if (stat(full, &st) != 0) continue;
        snprintf(leaf, sizeof leaf, "%s", de->d_name);
        comma = strrchr(leaf, ',');
        if (S_ISDIR(st.st_mode)) type = leaf[0] == '!' ? 0x2000 : 0x1000;
        else if (comma && strlen(comma) == 4) { type = (int) strtol(comma + 1, NULL, 16); *comma = '\0'; }
        else type = fake_ext_type(leaf);
        for (i = 0; leaf[i]; i++) if (leaf[i] == '.') leaf[i] = '/';
        len = strlen(leaf);
        if (p + ((24 + len + 1 + 3) & ~3) > end) { more = 1; index--; break; }
        w = (int *) p;
        cs = ((unsigned long long) st.st_mtime + 2208988800ULL) * 100ULL;
        w[0] = (int) (0xFFF00000u | ((unsigned) (type & 0xFFF) << 8) | (unsigned) ((cs >> 32) & 0xFF));
        w[1] = (int) (unsigned) (cs & 0xFFFFFFFFu);
        w[2] = S_ISDIR(st.st_mode) ? 0 : (int) st.st_size;
        w[3] = 3;
        w[4] = S_ISDIR(st.st_mode) ? 2 : 1;
        w[5] = type;
        memcpy(p + 24, leaf, len + 1);
        p += (24 + len + 1 + 3) & ~3;
        got++;
    }
    closedir(d);
    r->r[3] = got;
    r->r[4] = more ? start + got : -1;
    return NULL;
}

int *fake_tool_area_override;
int *fake_pool_area_override;
int fake_window_flags[256];          /* Wimp_CreateWindow +28, by handle & 255 */
int fake_menu_window, fake_menu_windows_opened;
const int *(*fake_pool_lookup)(const char *name);

static const int *pool_find(const char *name)
{
    int off = 0;
    if (fake_pool_lookup) {
        const int *spr = fake_pool_lookup(name);
        if (spr) return spr;
    }
    if (!fake_wimp_sprites) return NULL;
    build_pool();
    off = 4;
    while (off < pool_used) {
        const int *spr = &pool[off];
        if (strncmp((const char *) &spr[1], name, 12) == 0) return spr;
        off += spr[0] / 4;
    }
    return NULL;
}

/* Printing (the PDriver module): a fake printer that records what it's given */
int fake_pdriver = 1, fake_print_dpi = 300;
const char *fake_print_name = "Fake LaserJet";
int fake_print_paper[2] = { 595276, 841890 };       /* A4, millipoints */
int fake_print_jobs, fake_print_pages, fake_print_copies, fake_print_ended, fake_print_aborted;
int fake_print_rect[4], fake_print_scale[4], fake_print_sprite_w, fake_print_sprite_h;
int fake_print_ink, fake_print_ink_top = -1, fake_print_ink_left = -1;
char fake_print_title[64];
static int print_drawing, print_rect_id, print_selected, print_started, print_band;
#define PRINT_BANDS 3                      /* the page asked for in 3 rectangles, as a dot-matrix driver */
int fake_print_plots;                      /* sprites plotted on the last page */
int fake_print_ink_runs;                   /* runs of rows with ink (lines of text) */
static unsigned int *print_page;           /* the last page, put together from the plots */
static int print_page_w, print_page_h, print_page_dpi;

static void print_page_for(int dpi)
{
    if (!print_page || print_page_dpi != dpi) {
        free(print_page);
        print_page_dpi = dpi;
        print_page_w = (fake_print_rect[2] * dpi + 90) / 180;
        print_page_h = (fake_print_rect[3] * dpi + 90) / 180;
        print_page = malloc((size_t) print_page_w * print_page_h * 4);
        memset(print_page, 0xFF, (size_t) print_page_w * print_page_h * 4);
    }
}

/* Draw_Fill / Draw_Stroke to the printer: drawn into the page (at the
   printer's resolution) with cairo; the page's words are &00BBGGRR, so
   cairo is given blue for red */
int fake_print_draws;                      /* Draw_Fill/Draw_Stroke calls on the last page */
static unsigned int print_gcol = 0;        /* ColourTrans_SetGCOL's &BBGGRR00 */

static void print_draw(const int *path, int stroke, int width, const unsigned char *capjoin)
{
    cairo_surface_t *s;
    cairo_t *cr;
    double k;
    print_page_for(fake_print_dpi);
    k = fake_print_dpi / 180.0 / 256.0;           /* Draw units to pixels */
    s = cairo_image_surface_create_for_data((unsigned char *) print_page, CAIRO_FORMAT_RGB24,
                                            print_page_w, print_page_h, print_page_w * 4);
    cr = cairo_create(s);
    cairo_translate(cr, 0, print_page_h);
    cairo_scale(cr, k, -k);
    for (;;) {
        int t = path[0] & 0xFF;
        if (t == 0) break;
        if (t == 2) { cairo_move_to(cr, path[1], path[2]); path += 3; }
        else if (t == 8) { cairo_line_to(cr, path[1], path[2]); path += 3; }
        else if (t == 6) { cairo_curve_to(cr, path[1], path[2], path[3], path[4], path[5], path[6]); path += 7; }
        else if (t == 5 || t == 4) { cairo_close_path(cr); path += 1; }
        else { fprintf(stderr, "fake: Draw path element %d\n", t); break; }
    }
    cairo_set_source_rgb(cr, ((print_gcol >> 24) & 255) / 255.0, ((print_gcol >> 16) & 255) / 255.0,
                         ((print_gcol >> 8) & 255) / 255.0);
    if (stroke) {
        cairo_set_line_width(cr, width ? width : 1 / k);
        if (capjoin) {
            cairo_set_line_join(cr, capjoin[0] == 1 ? CAIRO_LINE_JOIN_ROUND :
                                    capjoin[0] == 2 ? CAIRO_LINE_JOIN_BEVEL : CAIRO_LINE_JOIN_MITER);
            cairo_set_line_cap(cr, capjoin[1] == 1 ? CAIRO_LINE_CAP_ROUND :
                                   capjoin[1] == 2 ? CAIRO_LINE_CAP_SQUARE : CAIRO_LINE_CAP_BUTT);
        }
        cairo_stroke(cr);
    } else
        cairo_fill(cr);
    cairo_destroy(cr);
    cairo_surface_finish(s);
    cairo_surface_destroy(s);
    fake_print_draws++;
}

static void print_band_rect(int *out)
{
    int h = fake_print_rect[3] - fake_print_rect[1];
    out[0] = fake_print_rect[0]; out[2] = fake_print_rect[2];
    out[3] = fake_print_rect[3] - h * print_band / PRINT_BANDS;
    out[1] = fake_print_rect[3] - h * (print_band + 1) / PRINT_BANDS;
}

static void print_page_done(void)
{
    int x, y, w = print_page_w, h = print_page_h;
    fake_print_sprite_w = w; fake_print_sprite_h = h;
    fake_print_ink = 0; fake_print_ink_top = fake_print_ink_left = -1;
    if (!print_page) return;
    for (y = 0; y < h; y++) for (x = 0; x < w; x++)
        if ((print_page[(size_t) y * w + x] & 0xFFFFFF) != 0xFFFFFF) {
            fake_print_ink++;
            if (fake_print_ink_top < 0) fake_print_ink_top = y;
            if (fake_print_ink_left < 0 || x < fake_print_ink_left) fake_print_ink_left = x;
        }
    fake_print_ink_runs = 0;
    for (y = 0, x = 0; y < h; y++) {
        int i, ink = 0;
        for (i = 0; i < w && !ink; i++) ink = (print_page[(size_t) y * w + i] & 0xFFFFFF) != 0xFFFFFF;
        if (ink && !x) fake_print_ink_runs++;
        x = ink;
    }
    if (getenv ("FAKE_PRINT_DUMP")) {          /* the page as a PPM, to look at */
        FILE *pf = fopen (getenv ("FAKE_PRINT_DUMP"), "wb");
        if (pf) {
            fprintf (pf, "P6\n%d %d\n255\n", w, h);
            for (y = 0; y < h; y++) for (x = 0; x < w; x++) {
                unsigned int c = print_page[(size_t) y * w + x];
                fputc (c & 255, pf); fputc ((c >> 8) & 255, pf); fputc ((c >> 16) & 255, pf);
            }
            fclose (pf);
        }
    }
}
int fake_print_polls_selected;   /* Wimp_Poll with a print job selected (redraws to the printer!) */

static _kernel_oserror *sprite_op(_kernel_swi_regs *r)
{
    int reason = r->r[0] & 255;

    if (reason == 15) {                             /* create sprite */
        int *area = (int *) (long) r->r[1];
        int w = r->r[4], h = r->r[5], mode = r->r[6], log2bpp, flags, size;
        int *spr;
        if ((r->r[0] & 0xF00) != 0x100) return error("create: user area only");
        if (!mode_info(mode, &log2bpp, &flags) || log2bpp != 5)
            return error("create: 32bpp only in the fake");
        size = 44 + w * h * 4;
        if (area[3] + size > area[0]) return error("Not enough room in sprite area");
        spr = (int *) ((char *) area + area[3]);
        memset(spr, 0, size);
        spr[0] = size;
        strncpy((char *) &spr[1], (const char *) (long) r->r[2], 12);
        spr[4] = w - 1;
        spr[5] = h - 1;
        spr[6] = 0;
        spr[7] = 31;
        spr[8] = 44;
        spr[9] = 44;
        spr[10] = mode;
        area[1]++;
        area[3] += size;
        fake_sprite_creates++;
        return NULL;
    }
    if (reason == 52 && print_drawing) {           /* to the printer: put into the page */
        const int *spr = (const int *) (long) r->r[2];
        const int *f = (const int *) (long) r->r[6];
        const unsigned int *pix = (const unsigned int *) ((const char *) spr + spr[8]);
        int w = spr[4] + 1, n = spr[5] + 1, dpi, top, y;
        if (!f) return error("print: unscaled sprite");
        memcpy(fake_print_scale, f, sizeof fake_print_scale);
        dpi = 90 * f[2] / f[0];                    /* 2 OS units a pixel x f0/f2 */
        int left = (int) (((long long) r->r[3] * dpi + 90) / 180), cols;
        print_page_for(dpi);
        top = (int) (((long long) (fake_print_rect[3] - r->r[4]) * dpi + 90) / 180) - n;
        cols = left + w > print_page_w ? print_page_w - left : w;
        for (y = 0; y < n && cols > 0; y++)
            if (top + y >= 0 && top + y < print_page_h)
                memcpy(print_page + (size_t) (top + y) * print_page_w + left, pix + (size_t) y * w,
                       (size_t) cols * 4);
        fake_print_plots++;
        return NULL;
    }
    if (reason == 34 || reason == 52) {             /* put sprite (scaled) */
        const int *spr = (const int *) (long) r->r[2];
        const int *f = reason == 52 ? (const int *) (long) r->r[6] : NULL;
        int w = spr[4] + 1, h = spr[5] + 1, log2bpp, flags, dx, dy, ow, oh, swap, sxe, sye;
        int px0 = r->r[3] >> fake_screen.xeig, py0 = r->r[4] >> fake_screen.yeig;
        const unsigned int *pix = (const unsigned int *) ((const char *) spr + spr[8]);
        if ((r->r[0] & 0xF00) != 0x200) return error("plot: pointer form only");
        mode_info(spr[10], &log2bpp, &flags);
        swap = fake_sprite_plot_converts && ((flags ^ fake_screen.flags) & 0x4000) != 0;
        /* the sprite's own pixel size: a mode word's dpi, else the screen's */
        sxe = fake_screen.xeig; sye = fake_screen.yeig;
        if ((spr[10] & 1) && ((unsigned) spr[10] >> 27) != 0) {
            int xdpi = (spr[10] >> 1) & 0x1FFF, ydpi = (spr[10] >> 14) & 0x1FFF;
            sxe = xdpi >= 180 ? 0 : xdpi >= 90 ? 1 : 2;
            sye = ydpi >= 180 ? 0 : ydpi >= 90 ? 1 : 2;
        }
        ow = (int) (((long long) (w << sxe) * (f ? f[0] : 1) / (f ? f[2] : 1)) >> fake_screen.xeig);
        oh = (int) (((long long) (h << sye) * (f ? f[1] : 1) / (f ? f[3] : 1)) >> fake_screen.yeig);
        fake_plots++;
        if (reason == 52) fake_scaled_plots++;
        for (dy = 0; dy < oh; dy++) {               /* dy: rows from the top of the plot */
            int y_up = py0 + (oh - 1 - dy), oy = y_up << fake_screen.yeig;
            int sy = (int) ((long long) dy * h / oh);
            if (y_up < 0 || y_up >= fake_screen.h || oy < clip[1] || oy >= clip[3]) continue;
            for (dx = 0; dx < ow; dx++) {
                int x = px0 + dx, ox = x << fake_screen.xeig;
                unsigned int p = pix[(size_t) sy * w + (int) ((long long) dx * w / ow)];
                if (x < 0 || x >= fake_screen.w || ox < clip[0] || ox >= clip[2]) continue;
                if (swap) p = (p & 0xFF00FF00u) | ((p >> 16) & 0xFF) | ((p & 0xFF) << 16);
                if (x >= fake_watch[0] && x < fake_watch[2] &&
                    (fake_screen.h - 1 - y_up) >= fake_watch[1] && (fake_screen.h - 1 - y_up) < fake_watch[3] &&
                    (p & 0xFFFFFF) == (unsigned) fake_watch_value)
                    fake_watch_hits++;
                fake_screen.mem[(size_t) (fake_screen.h - 1 - y_up) * (fake_screen.line_length / 4) + x] = p;
            }
        }
        return NULL;
    }
    if (reason == 36) {                             /* set pointer shape */
        const int *spr = NULL;
        if ((r->r[0] & 0xF00) == 0x100) {           /* user area, by name */
            const int *area = (const int *) (long) r->r[1];
            const char *p = (const char *) area + area[2];
            int i;
            for (i = 0; i < area[1]; i++, p += ((const int *) p)[0])
                if (strncmp(p + 4, (const char *) (long) r->r[2], 12) == 0) spr = (const int *) p;
            if (!spr) return error("Sprite doesn't exist");
        } else if ((r->r[0] & 0xF00) == 0x200) spr = (const int *) (long) r->r[2];
        else return error("pointer: user area only in the fake");
        /* 256 colours or fewer; a 2bpp sprite (type 2) needs no table */
        if (((unsigned) spr[10] >> 27) != 2 && r->r[7] == 0) return error("pointer: 2bpp sprites only");
        if (spr[8] - 44 != 32) return error("pointer: 2bpp sprite without a palette");
        fake_ptr_sprite = spr;
        strncpy(fake_ptr_name, (const char *) &spr[1], 12);
        fake_ptr_name[12] = 0;
        fake_ptr_shape = r->r[3] & 15;
        fake_ptr_flags = r->r[3];
        fake_ptr_hx = r->r[4]; fake_ptr_hy = r->r[5];
        fake_ptr_sets++;
        return NULL;
    }
    return error("SpriteOp reason not faked");
}

static void full_clip(void)
{
    clip[0] = 0;
    clip[1] = 0;
    clip[2] = fake_screen.w << fake_screen.xeig;
    clip[3] = fake_screen.h << fake_screen.yeig;
}

/* Fill in a redraw block for window w with one rectangle: the visible area
   clipped to the work area rectangle wa (NULL = everything). */
static int start_redraw(int *block, fake_window_t *w, const int *wa)
{
    int ox = w->x0 - w->sx, oy = w->y1 - w->sy;     /* work area origin on screen */
    int r0 = w->x0, r1 = w->y0, r2 = w->x1, r3 = w->y1;
    if (wa) {
        if (wa[0] + ox > r0) r0 = wa[0] + ox;
        if (wa[1] + oy > r1) r1 = wa[1] + oy;
        if (wa[2] + ox < r2) r2 = wa[2] + ox;
        if (wa[3] + oy < r3) r3 = wa[3] + oy;
    }
    block[1] = w->x0; block[2] = w->y0; block[3] = w->x1; block[4] = w->y1;
    block[5] = w->sx; block[6] = w->sy;
    block[7] = r0; block[8] = r1; block[9] = r2; block[10] = r3;
    if (r0 >= r2 || r1 >= r3)
        return 0;
    clip[0] = r0; clip[1] = r1; clip[2] = r2; clip[3] = r3;
    return 1;
}

/* Put the pointer at GDK pixel x, y of the newest window's visible area:
   two OS units a pixel (the screen's pixels in the usual EX1 EY1 mode;
   in an EX0 EY0 mode GDK's pixels are two screen pixels, at scale 2) */
static void pointer_to(int x, int y)
{
    fake_window_t *w = find_window(TARGET_WINDOW);
    if (!w) return;
    fake_pointer[0] = w->x0 + x * 2;
    fake_pointer[1] = w->y1 - y * 2 - 1;
}

/* The window under the pointer: the frontmost one there (the last
   opened), as the Wimp reports it */
static int window_at_pointer(void)
{
    fake_window_t *w, *best = NULL;
    int i;
    for (i = 0; i < FAKE_MAX_WINDOWS; i++) {
        w = &fake_windows[i];
        if (w->handle && fake_pointer[0] >= w->x0 && fake_pointer[0] < w->x1 &&
            fake_pointer[1] >= w->y0 && fake_pointer[1] < w->y1 &&
            (!best || w->stack > best->stack))
            best = w;
    }
    return best ? best->handle : -1;
}

static const char *icon_text(const int *icon_flags_and_data)
{
    static char buf[16];
    if (icon_flags_and_data[0] & 0x100)
        return (const char *) (long) icon_flags_and_data[1];
    memcpy(buf, &icon_flags_and_data[1], 12);
    buf[12] = 0;
    return buf;
}

static void log_menu(const int *m, int depth)
{
    const int *item = m + 7;
    const char *title;
    if (item[0] & 0x100) title = (const char *) (long) m[0];
    else { static char t[13]; memcpy(t, m, 12); t[12] = 0; title = t; }
    fprintf(stderr, "%*smenu \"%s\" (width %d)\n", depth * 2, "", title, m[4]);
    for (;; item += 6) {
        fprintf(stderr, "%*s  %s%s%s\n", depth * 2, "", icon_text(item + 2),
                (item[2] & (1 << 22)) ? " (shaded)" : "", item[1] != -1 ? " >" : "");
        if (item[1] != -1)
            log_menu((const int *) (long) item[1], depth + 2);
        if (item[0] & 0x80) break;
    }
}

static _kernel_oserror *swi(int no, _kernel_swi_regs *in, _kernel_swi_regs *out);

static pthread_mutex_t swi_lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

/* For callers that change the fake windows from threads (dEQP's platform) */
void fake_lock(void)   { pthread_mutex_lock(&swi_lock); }
void fake_unlock(void) { pthread_mutex_unlock(&swi_lock); }

/* ---- AcornSSL (RISC OS's TLS module), for Claws Mail ----------------
   A pass-through stand-in, as riscos-ffmpeg's tests/host/fake_acornssl.c
   (whose behaviour this copies): no encryption, so a "TLS" connection
   talks plain text to a plain server. What it models is what a program
   must get right, from the module's documentation and source: the
   handshake inside Recv/Send, a step a call, failing with ENOTCONN until it's
   over (FAKE_ACORNSSL_STEPS, default 3); FIONBIO (EWOULDBLOCK instead of
   waiting); MSG_PEEK; SO_ACORNSSL_HOSTNAME (recorded: fake_acornssl_host);
   Close leaving a handed-over socket open; errors as RISC OS error blocks,
   0x20E00 + a BSD errno. FAKE_ACORNSSL_ABSENT=1: the module isn't loaded;
   FAKE_ACORNSSL_FAIL=1: the handshake fails.
   Adapted from riscos-ffmpeg's file, which is GPL version 2 or later:
   this part is under that licence (LICENCES.md). */
#include <poll.h>
#define ACORNSSL_MAX 16
static struct { int used, fd, steps, nbio; } assl[ACORNSSL_MAX];
int fake_acornssl_sessions, fake_acornssl_closes, fake_acornssl_closed_open, fake_acornssl_handshake_steps;
char fake_acornssl_host[256];

int __get_ro_socket(int fd) { return fd; }

static _kernel_oserror *dci(int e, const char *msg)
{
    err.errnum = 0x20E00 + e;
    snprintf(err.errmess, sizeof err.errmess, "%s", msg);
    return &err;
}

static _kernel_oserror *acornssl(int swi, _kernel_swi_regs *in, _kernel_swi_regs *out)
{
    int i;
    if (getenv("FAKE_ACORNSSL_ABSENT")) {
        err.errnum = 0x1E6;
        snprintf(err.errmess, sizeof err.errmess, "SWI &%X not known", swi);
        return &err;
    }
    switch (swi) {
    case 0x50F8D:                                           /* Version */
        out->r[0] = 150;
        return NULL;
    case 0x50F88:                                           /* CreateSession */
        for (i = 0; i < ACORNSSL_MAX && assl[i].used; i++)
            ;
        if (i == ACORNSSL_MAX || in->r[1] != 0)
            return dci(22, "Invalid argument");
        memset(&assl[i], 0, sizeof assl[i]);
        assl[i].used = 1;
        assl[i].fd = in->r[0];
        assl[i].steps = getenv("FAKE_ACORNSSL_STEPS") ? atoi(getenv("FAKE_ACORNSSL_STEPS")) : 3;
        out->r[0] = 0x5000 + i;
        fake_acornssl_sessions++;
        return NULL;
    }
    i = in->r[0] - 0x5000;
    if (i < 0 || i >= ACORNSSL_MAX || !assl[i].used) {
        err.errnum = 0x813F21;
        snprintf(err.errmess, sizeof err.errmess, "Bad SSL handle");
        return &err;
    }
    switch (swi) {
    case 0x50F8B:                                           /* Setsockopt */
        if (in->r[1] == 0xFFFF && in->r[2] == 0x11E0 && in->r[4] == 4) {
            snprintf(fake_acornssl_host, sizeof fake_acornssl_host, "%s", (const char *) (long) in->r[3]);
            out->r[0] = 0;
            return NULL;
        }
        return dci(42, "Protocol not available");
    case 0x50F81:                                           /* Ioctl: FIONBIO */
        if ((unsigned) in->r[1] == 0x8004667EU) {
            assl[i].nbio = *(int *) (long) in->r[2] != 0;
            out->r[0] = 0;
            return NULL;
        }
        return dci(22, "Invalid argument");
    case 0x50F84:                                           /* Close: the socket stays open */
        fake_acornssl_closes++;
        if (fcntl(assl[i].fd, F_GETFD) != -1)
            fake_acornssl_closed_open++;
        assl[i].used = 0;
        out->r[0] = 0;
        return NULL;
    case 0x50F87: case 0x50F8F: {                           /* Recv, Send */
        ssize_t n;
        if (assl[i].steps > 0) {                            /* the handshake, a step a call */
            assl[i].steps--;
            fake_acornssl_handshake_steps++;
            if (getenv("FAKE_ACORNSSL_FAIL") && assl[i].steps == 0) {
                err.errnum = 0x813F27;
                snprintf(err.errmess, sizeof err.errmess, "Handshake error (state -9984)");
                return &err;
            }
            return dci(57, "Socket is not connected");    /* BSD ENOTCONN */
        }
        if (swi == 0x50F87) {
            struct pollfd p = { assl[i].fd, POLLIN, 0 };
            if (!assl[i].nbio && in->r[2] > 0)
                poll(&p, 1, -1);
            n = recv(assl[i].fd, (void *) (long) in->r[1], in->r[2],
                     (in->r[3] & 2) ? MSG_PEEK | MSG_DONTWAIT : MSG_DONTWAIT);
        } else {
            struct pollfd p = { assl[i].fd, POLLOUT, 0 };
            if (!assl[i].nbio)
                poll(&p, 1, -1);
            n = send(assl[i].fd, (const void *) (long) in->r[1], in->r[2], MSG_DONTWAIT | MSG_NOSIGNAL);
        }
        if (n < 0) {
            if (errno == EAGAIN)
                return dci(35, "Operation would block");  /* BSD numbers, as RISC OS gives */
            if (errno == ECONNRESET)
                return dci(54, "Connection reset by peer");
            return dci(errno == EPIPE ? 32 : 5, strerror(errno));
        }
        out->r[0] = (int) n;
        return NULL;
    }
    }
    return dci(22, "Invalid argument");
}

_kernel_oserror *_kernel_swi(int no, _kernel_swi_regs *in, _kernel_swi_regs *out)
{
    _kernel_oserror *e;
    if ((no & ~0x20000) >= 0x50F80 && (no & ~0x20000) <= 0x50F90) {
        _kernel_swi_regs r = *in;
        e = acornssl(no & ~0x20000, &r, &r);
        if (out) *out = r;
        return e;
    }
    pthread_mutex_lock(&swi_lock);
    e = swi(no, in, out);
    pthread_mutex_unlock(&swi_lock);
    return e;
}

/* OS_Memory 24: bit 0 if r1..r2 is all readable in user mode, from the
   process's own map (a page can be mapped, so OS_ValidateAddress passes,
   and still not readable: the Pi's privileged memory, seen with 0.11) */
static int user_readable(unsigned long lo, unsigned long hi)
{
    FILE *f = fopen("/proc/self/maps", "r");
    char line[512];
    unsigned long a, b, at = lo;
    char perm[8];
    if (!f) return 0;
    while (at < hi && fgets(line, sizeof line, f))
        if (sscanf(line, "%lx-%lx %7s", &a, &b, perm) == 3 && a <= at && at < b) {
            if (perm[0] != 'r') break;
            at = b;
            rewind(f);
        }
    fclose(f);
    return at >= hi;
}

static _kernel_oserror *swi(int no, _kernel_swi_regs *in, _kernel_swi_regs *out)
{
    _kernel_swi_regs r = *in;
    _kernel_oserror *e = NULL;
    fake_window_t *w;
    int *block;

    if ((no & ~0x20000) == 0x58EC8 && in->r[0] == 0) {
        /* VFPSupport_Features 0, asked by riscos-mesa's shader code
           (gltest-fake): a Cortex-A8 with NEON, as riscos-mesa's own
           ARM checks answer */
        if (out) { out->r[0] = 0x410330C3; out->r[1] = 0x11110222; out->r[2] = 0x00011111; }
        return NULL;
    }
    switch (no) {
    case 0x68:      /* OS_Memory: 24 (check memory access) only */
        if (r.r[0] != 24) { e = error("OS_Memory: not faked"); break; }
        r.r[1] = user_readable((unsigned) in->r[1], (unsigned) in->r[2]) ? (1 | 4) : 0;    /* (completely, partially) readable */
        break;
    case OS_SpriteOp:
        e = sprite_op(&r);
        break;
    case OS_ReadVduVariables: {
        const int *vars = (const int *) (long) r.r[0];
        int *vals = (int *) (long) r.r[1];
        for (; *vars != -1; vars++, vals++) {
            switch (*vars) {
            case 0:   *vals = fake_screen.flags; break;
            case 4:   *vals = fake_screen.xeig; break;
            case 5:   *vals = fake_screen.yeig; break;
            case 6:   *vals = fake_screen.line_length; break;
            case 9:   *vals = fake_screen.log2bpp; break;
            case 11:  *vals = fake_screen.w - 1; break;
            case 12:  *vals = fake_screen.h - 1; break;
            case 7:   *vals = fake_screen.w * fake_screen.h * 4; break;
            case 148: *vals = (int) (long) (fake_screen.mem + (size_t) (fake_screen.vdu_bank - 1) *
                                            fake_screen.w * fake_screen.h); break;
            default:  *vals = 0;
            }
        }
        break;
    }
    case OS_ReadModeVariable: {
        int log2bpp, flags;
        if (!mode_info(r.r[0], &log2bpp, &flags)) { e = error("bad mode"); break; }
        switch (r.r[1]) {
        case 0:  r.r[2] = flags; break;
        case 9:  r.r[2] = log2bpp; break;
        /* the current mode's (-1), or a numbered mode's: modes below 256
           are the old fixed modes, 2 OS units a pixel across (EX1), and
           EY2 for the rectangular-pixel ones (0-2, 8-17, 24) */
        case 4:  r.r[2] = r.r[0] == -1 || (unsigned) r.r[0] >= 256 ? fake_screen.xeig : 1; break;
        case 5:  r.r[2] = r.r[0] == -1 || (unsigned) r.r[0] >= 256 ? fake_screen.yeig :
                          (r.r[0] <= 2 || (r.r[0] >= 8 && r.r[0] <= 17) || r.r[0] == 24) ? 2 : 1; break;
        case 11: r.r[2] = fake_screen.w - 1; break;
        case 12: r.r[2] = fake_screen.h - 1; break;
        default: r.r[2] = 0;
        }
        break;
    }
    case 0x5C:      /* OS_ReadDynamicArea */
        if (r.r[0] != 2) { e = error("only the screen area is faked"); break; }
        r.r[1] = fake_screen.da_size;
        r.r[2] = fake_screen.da_max;
        break;
    case 0x2A:      /* OS_ChangeDynamicArea */
        if (r.r[0] != 2) { e = error("only the screen area is faked"); break; }
        if (fake_screen.da_size + r.r[1] > fake_screen.da_max) { e = error("Area can't grow"); break; }
        fake_screen.da_size += r.r[1];
        break;
    case OS_ScreenMode:
        if (r.r[0] != 1) { e = error("ScreenMode reason not faked"); break; }
        screen_selector[0] = 1;
        screen_selector[1] = fake_screen.w;
        screen_selector[2] = fake_screen.h;
        screen_selector[3] = fake_screen.log2bpp;
        screen_selector[4] = 60;
        screen_selector[5] = 0;
        screen_selector[6] = fake_screen.flags;
        screen_selector[7] = -1;
        r.r[1] = (int) (long) screen_selector;
        break;
    case 0x400C0:   /* Wimp_Initialise */
        fake_wimp_tasks++;
        wimp_stage = wimp_step = 0;
        r.r[0] = 380; r.r[1] = 0x4A00;
        break;
    case 0x400DD:   /* Wimp_CloseDown */
        fake_wimp_tasks--;
        break;
    case 0x400C1:   /* Wimp_CreateWindow: opened by Wimp_OpenWindow */
        block = (int *) (long) r.r[1];
        if (block[14] & 0x100) {         /* indirected */
            fake_wimp_title = (const char *) (long) block[18];
            fake_wimp_title_window = wimp_next_handle;
        }
        r.r[0] = wimp_last_window = wimp_next_handle++;
        if (wimp_num_created < 64) wimp_created[wimp_num_created++] = wimp_last_window;
        fake_window_flags[r.r[0] & 255] = block[7];
        break;
    case 0x400C5:   /* Wimp_OpenWindow */
        block = (int *) (long) r.r[1];
        if (!fake_open_window(block[0], block[1], block[2], block[3], block[4], block[5], block[6]))
            e = error("too many windows");
        break;
    case 0x400C6:   /* Wimp_CloseWindow */
    case 0x400C3: { /* Wimp_DeleteWindow */
        int h = *(int *) (long) r.r[1], i, j;
        fake_window_t *cw = find_window(h);
        if (no == 0x400C3 && h == fake_wimp_title_window) fake_wimp_title = NULL;
        if (cw) cw->handle = 0;
        if (no == 0x400C3) {         /* the newest window left is the "newest" */
            for (i = j = 0; i < wimp_num_created; i++)
                if (wimp_created[i] != h) wimp_created[j++] = wimp_created[i];
            wimp_num_created = j;
            if (h == wimp_last_window)
                wimp_last_window = j ? wimp_created[j - 1] : 0;
        }
        break;
    }
    case 0x400D2:   /* Wimp_SetCaretPosition */
        break;
    case 0x400DC:   /* Wimp_ProcessKey: a key the task didn't use */
        fake_wimp_keys_passed++;
        fake_processed_key = r.r[0];
        break;
    case 0x400C7:   /* Wimp_Poll */
    case 0x400E1: { /* Wimp_PollIdle */
        if (print_selected) fake_print_polls_selected++;
        int reason;
        block = (int *) (long) r.r[1];
        memset(block, 0, 64);
        fake_wimp_polls++;
        if (wimp_stage == 0) { reason = 1; block[0] = wimp_last_window; wimp_stage = 1; }
        else if (wimp_stage == 1 && wimp_step < fake_wimp_nulls && !(r.r[0] & 1)) {
            reason = 0; wimp_step++;
        } else {
            if (wimp_stage == 1) { wimp_stage = 2; wimp_step = 0; wimp_null_due = 0; }
            if (msg_queue_len) {             /* messages from other tasks first */
                reason = msg_queue[0][0];
                memcpy(block, &msg_queue[0][1], 64 * 4);
                memmove(msg_queue[0], msg_queue[1], (size_t) (msg_queue_len - 1) * sizeof msg_queue[0]);
                msg_queue_len--;
            } else if (wimp_null_due && !(r.r[0] & 1)) {
                reason = 0;                  /* a null event after each entry */
                wimp_null_due = 0;
            } else if (wimp_step < fake_wimp_script_len) {
                const int *e = fake_wimp_script[wimp_step++];
                reason = e[0];
                wimp_null_due = 1;
                block[0] = TARGET_WINDOW;
                switch (reason) {
                case 8:                      /* Key_Pressed */
                    block[6] = e[1];
                    break;
                case 2: {                    /* Open_Window_Request: new width and height, OS units */
                    fake_window_t *ow = find_window(TARGET_WINDOW);
                    int v = e[1];
                    if (ow) {
                        /* a = width | height << 16 (0: keep the size); b, c = move by (OS units) */
                        int wd = v ? (v & 0xFFFF) : ow->x1 - ow->x0, ht = v ? (v >> 16) : ow->y1 - ow->y0;
                        block[1] = ow->x0 + e[2]; block[4] = ow->y1 + e[3];
                        block[3] = block[1] + wd; block[2] = block[4] - ht;
                        block[5] = ow->sx; block[6] = ow->sy; block[7] = -1;
                    }
                    break;
                }
                case 6:                      /* Mouse_Click */
                    pointer_to(e[1], e[2]);
                    fake_pointer[2] = e[3] ? e[3] : 4;
                    block[0] = fake_pointer[0]; block[1] = fake_pointer[1];
                    block[2] = fake_pointer[2]; block[3] = TARGET_WINDOW; block[4] = fake_target_icon;
                    break;
                case 10: {                   /* Scroll_Request (extended): a, b = x, y OS units */
                    fake_window_t *sw = find_window(TARGET_WINDOW);
                    if (sw) {
                        block[1] = sw->x0; block[2] = sw->y0; block[3] = sw->x1; block[4] = sw->y1;
                        block[5] = sw->sx; block[6] = sw->sy; block[7] = -1;
                    }
                    block[8] = e[1]; block[9] = e[2]; block[10] = -1;
                    break;
                }
                case 9:                      /* Menu_Selection */
                    block[0] = e[1]; block[1] = e[2]; block[2] = e[3]; block[3] = -1;
                    fake_menu = NULL;        /* the Wimp closes the tree (Adjust: the task reopens it) */
                    break;
                case 17:                     /* User_Message: a = message action */
                    block[0] = 24; block[4] = e[1];
                    if (e[1] == 0x400C9) {   /* Message_MenusDeleted: the open menu */
                        block[5] = (int) (long) fake_menu;
                        fake_menu = NULL;
                    }
                    break;
                case FAKE_MOVE:    pointer_to(e[1], e[2]); reason = 0; wimp_null_due = 0; break;
                case FAKE_RELEASE: fake_pointer[2] = 0;    reason = 0; wimp_null_due = 0; break;
                case FAKE_WHEEL:   fake_wheel += e[1];     reason = 0; wimp_null_due = 0; break;
                case FAKE_NULL:                           reason = 0; wimp_null_due = 0; break;
                case FAKE_BUTTONS: fake_pointer[2] = e[1]; reason = 0; wimp_null_due = 0; break;
                case FAKE_SLEEP:   usleep(e[1] * 1000);    reason = 0; wimp_null_due = 0; break;
                }
            } else { reason = 3; block[0] = TARGET_WINDOW; }
        }
        if (fake_wimp_hook) fake_wimp_hook(reason);
        r.r[0] = reason;
        break;
    }
    case 0x400F2:   /* Wimp_ReadSysInfo 0: active tasks (0 = no desktop); 5: current task;
                       9: the tool sprite area */
        if (r.r[0] == 5) { r.r[0] = 0x4A00; break; }
        if (r.r[0] == 9) {
            r.r[0] = fake_tool_area_override ? (int) (long) fake_tool_area_override :
                     fake_tool_sprites ? (int) (long) build_tool_area() : 0;
            r.r[1] = 0; break;
        }
        if (r.r[0] != 0) { e = error("ReadSysInfo reason not faked"); break; }
        r.r[0] = fake_wimp_desktop ? 1 + fake_wimp_tasks : 0;
        break;
    case 0x29:      /* OS_FSControl 37: canonicalise path (returned as given);
                       33: filing system names; 18: file type names */
        if (r.r[0] == 33) {
            const char *n = fake_fs_name(r.r[1]);
            snprintf((char *) (long) r.r[2], r.r[3], "%s", n ? n : "");
            break;
        }
        if (r.r[0] == 18) { fake_type_name(r.r[2] & 0xFFF, &r.r[2], &r.r[3]); break; }
        if (r.r[0] != 37) { e = error("FSControl reason not faked"); break; }
        if (r.r[2] == 0 || r.r[5] == 0) { r.r[5] = -(int) strlen((const char *) (long) r.r[1]); break; }
        snprintf((char *) (long) r.r[2], r.r[5], "%s", (const char *) (long) r.r[1]);
        r.r[5] -= (int) strlen((const char *) (long) r.r[1]) + 1;
        break;
    case 0x0C:      /* OS_GBPB 12: a host directory's entries */
        if (r.r[0] != 12) { e = error("GBPB reason not faked"); break; }
        e = fake_gbpb12(&r);
        break;
    case 0x23: {    /* OS_ReadVarVal (r4 = 3, string): from the host's environment */
        const char *v = getenv((const char *) (long) r.r[0]);
        static _kernel_oserror notfound = { 0x124, "System variable not found" };
        if (!v) { r.r[2] = 0; e = &notfound; break; }
        if ((int) strlen(v) > r.r[2]) { r.r[2] = ~(int) strlen(v); e = error("Buffer overflow"); break; }
        memcpy((char *) (long) r.r[1], v, strlen(v));
        r.r[2] = (int) strlen(v);
        break;
    }
    case 0x42:      /* OS_ReadMonotonicTime: centiseconds */
        r.r[0] = fake_wimp_polls * 2;
        break;
    case 0x400CB:   /* Wimp_GetWindowState */
        block = (int *) (long) r.r[1];
        if (!(w = find_window(block[0]))) { e = error("Illegal window handle"); break; }
        block[1] = w->x0; block[2] = w->y0; block[3] = w->x1; block[4] = w->y1;
        block[5] = w->sx; block[6] = w->sy; block[7] = w->handle == fake_window_behind ? -1 : fake_window_behind;
        block[8] = 1 << 16;
        break;
    case 0x400C8:   /* Wimp_RedrawWindow */
        block = (int *) (long) r.r[1];
        if (!(w = find_window(block[0]))) { e = error("Illegal window handle"); break; }
        fake_redraw_calls++;
        r.r[0] = start_redraw(block, w, NULL);
        break;
    case 0x400C9: { /* Wimp_UpdateWindow */
        int wa[4];
        block = (int *) (long) r.r[1];
        if (!(w = find_window(block[0]))) { e = error("Illegal window handle"); break; }
        fake_update_calls++;
        memcpy(wa, &block[1], sizeof wa);
        r.r[0] = start_redraw(block, w, wa);
        memcpy(fake_update_rect, &block[7], sizeof fake_update_rect);
        break;
    }
    case 0x400D1:   /* Wimp_ForceRedraw */
        fake_force_redraws++;
        fake_force_rect[0] = r.r[0]; fake_force_rect[1] = r.r[1]; fake_force_rect[2] = r.r[2];
        fake_force_rect[3] = r.r[3]; fake_force_rect[4] = r.r[4];
        break;
    case 0x400CF:   /* Wimp_GetPointerInfo */
        block = (int *) (long) r.r[1];
        block[0] = fake_pointer[0]; block[1] = fake_pointer[1]; block[2] = fake_pointer[2];
        block[3] = window_at_pointer(); block[4] = -1;
        break;
    case 0x400D4:   /* Wimp_CreateMenu */
        if (r.r[1] == -1 && fake_menu_window) {    /* a dialogue box opened as a menu */
            fake_window_t *mw = find_window(fake_menu_window);
            if (mw) mw->handle = 0;
            fake_menu_window = 0;
        }
        {
            int i, is_window = 0;
            for (i = 0; i < wimp_num_created; i++)
                if (wimp_created[i] == r.r[1]) is_window = 1;
            if (is_window) {               /* a window as a menu: opened where asked */
                fake_menu_window = r.r[1];
                fake_menu_windows_opened++;
                break;
            }
        }
        if (r.r[1] != -1) {
            fake_menu = (const int *) (long) r.r[1];
            fake_menu_x = r.r[2]; fake_menu_y = r.r[3];
            fake_menus_opened++;
            if (fake_log_menus) log_menu(fake_menu, 0);
        } else {
            if (fake_menu) fake_menus_closed++;
            fake_menu = NULL;
        }
        break;
    case 0x400F9:   /* Wimp_TextOp 1: string width, 16 OS units a character */
        if (r.r[0] != 1) { e = error("TextOp reason not faked"); break; }
        { const char *t = (const char *) (long) r.r[1]; int n = 0;
          while ((unsigned char) t[n] >= 32 && (r.r[2] == 0 || n < r.r[2])) n++;
          r.r[0] = 16 * n; }
        break;
    case 0x400E0:   /* Wimp_GetWindowOutline: 2 unit border, 40 unit title bar */
        block = (int *) (long) r.r[1];
        if (!(w = find_window(block[0]))) { e = error("Illegal window handle"); break; }
        block[1] = w->x0 - 2; block[2] = w->y0 - 2; block[3] = w->x1 + 2; block[4] = w->y1 + 40;
        break;
    case 0x64:      /* OS_Pointer */
        if (r.r[0] != 2) { e = error("OS_Pointer reason not faked"); break; }
        r.r[0] = 0; r.r[1] = fake_wheel;
        break;
    case 0x07:      /* OS_Word: 21,3 / 21,5 (set mouse / pointer position) */
        if (r.r[0] == 21) {
            const unsigned char *b = (const unsigned char *) (long) r.r[1];
            if (b[0] == 3 || b[0] == 5) {
                fake_pointer[0] = (short) (b[1] | (b[2] << 8));
                fake_pointer[1] = (short) (b[3] | (b[4] << 8));
            }
        }
        break;
    case 0x400E6:   /* Wimp_SetColour */
        wimp_colour = r.r[0] & 15;
        break;
    case 0x45: {    /* OS_Plot: 4 move, 101 rectangle fill (absolute, inclusive) */
        if (r.r[0] == 4) { plot_x = r.r[1]; plot_y = r.r[2]; break; }
        if (r.r[0] == 101) {
            int x0 = plot_x < r.r[1] ? plot_x : r.r[1], x1 = plot_x < r.r[1] ? r.r[1] : plot_x;
            int y0 = plot_y < r.r[2] ? plot_y : r.r[2], y1 = plot_y < r.r[2] ? r.r[2] : plot_y;
            unsigned int c = wimp_palette_tbgr[wimp_colour];
            int px, py;
            if (fake_screen.flags & 0x4000)
                c = (c & 0xFF00) | ((c >> 16) & 0xFF) | ((c & 0xFF) << 16);
            fake_fills++;
            for (py = y0 >> fake_screen.yeig; py <= y1 >> fake_screen.yeig; py++)
                for (px = x0 >> fake_screen.xeig; px <= x1 >> fake_screen.xeig; px++) {
                    int ox = px << fake_screen.xeig, oy = py << fake_screen.yeig;
                    if (px < 0 || px >= fake_screen.w || py < 0 || py >= fake_screen.h) continue;
                    if (ox < clip[0] || ox >= clip[2] || oy < clip[1] || oy >= clip[3]) continue;
                    fake_screen.mem[(size_t) (fake_screen.h - 1 - py) * (fake_screen.line_length / 4) + px] = c;
                }
            plot_x = r.r[1]; plot_y = r.r[2];
            break;
        }
        e = error("OS_Plot code not faked");
        break;
    }
    case 0x80140:   /* PDriver_Info */
        if (!fake_pdriver) { e = error("No printer driver"); break; }
        r.r[0] = 0; r.r[1] = r.r[2] = fake_print_dpi; r.r[3] = 1;
        r.r[4] = (int) (long) fake_print_name; r.r[5] = r.r[6] = fake_print_dpi; r.r[7] = 0;
        break;
    case 0x80143:   /* PDriver_PageSize: the paper, the printable area (5 mm in) */
        if (!fake_pdriver) { e = error("No printer driver"); break; }
        r.r[1] = fake_print_paper[0]; r.r[2] = fake_print_paper[1];
        r.r[3] = 14173; r.r[4] = 14173;
        r.r[5] = fake_print_paper[0] - 14173; r.r[6] = fake_print_paper[1] - 14173;
        break;
    case 0x80145: { /* PDriver_SelectJob: 0 suspends; a new handle starts a job */
        int old = print_selected;
        if (r.r[0] && r.r[0] != print_started) {
            fake_print_jobs++; print_started = r.r[0];
            strncpy(fake_print_title, r.r[1] ? (const char *) (long) r.r[1] : "", 63);
        }
        print_selected = r.r[0];
        r.r[0] = old;
        break;
    }
    case 0x80148: fake_print_ended++; print_started = print_selected = 0; break;    /* PDriver_EndJob */
    case 0x80149: fake_print_aborted++; print_started = print_selected = 0; break;  /* PDriver_AbortJob */
    case 0x40743:   /* ColourTrans_SetGCOL: the colour, for the printer */
        print_gcol = (unsigned) r.r[0];
        break;
    case 0x40702:   /* Draw_Fill */
    case 0x40704:   /* Draw_Stroke */
        if (!print_drawing) { e = error("Draw: only faked for the printer"); break; }
        print_draw((const int *) (long) r.r[0], no == 0x40704, no == 0x40704 ? r.r[4] : 0,
                   no == 0x40704 ? (const unsigned char *) (long) r.r[5] : NULL);
        break;
    case 0x8014B:   /* PDriver_GiveRectangle */
        print_rect_id = r.r[0];
        memcpy(fake_print_rect, (const void *) (long) r.r[1], sizeof fake_print_rect);
        break;
    case 0x8014C:   /* PDriver_DrawPage: one rectangle, the one given */
        fake_print_pages++; fake_print_copies = r.r[0];
        print_band = 0; fake_print_plots = 0; fake_print_draws = 0;
        if (print_page) memset(print_page, 0xFF, (size_t) print_page_w * print_page_h * 4);
        if (r.r[1]) print_band_rect((int *) (long) r.r[1]);
        r.r[0] = 1; r.r[2] = print_rect_id; print_drawing = 1;
        break;
    case 0x8014D:   /* PDriver_GetRectangle: the next band, then no more */
        if (++print_band < PRINT_BANDS) {
            if (r.r[1]) print_band_rect((int *) (long) r.r[1]);
            r.r[0] = 1; r.r[2] = print_rect_id;
        } else {
            r.r[0] = 0; print_drawing = 0;
            print_page_done();
        }
        break;
    case 0x406C0: case 0x406C1: break;                /* Hourglass_On / Off */
    case 0x0D:      /* OS_Find: "printer:" only */
        if (r.r[0] == 0 && r.r[1] == 0x77) break;
        if ((r.r[0] & 0xC0) && r.r[1] && !strcmp((const char *) (long) r.r[1], "printer:")) { r.r[0] = 0x77; break; }
        if (fake_swi_hook && fake_swi_hook(no, &r, &e)) break;
        e = error("OS_Find: not faked");
        break;
    case 0x400DF:   /* Wimp_ReportError: recorded; answered with fake_report_answer */
        fake_reports++;
        strncpy(fake_report_text, (const char *) (long) r.r[0] + 4, sizeof fake_report_text - 1);
        fake_report_flags = r.r[1];
        fake_report_buttons[0] = 0;
        if ((r.r[1] & (1 << 8)) && r.r[5])
            strncpy(fake_report_buttons, (const char *) (long) r.r[5], sizeof fake_report_buttons - 1);
        r.r[1] = fake_report_answer;
        break;
    case 0x400C2:   /* Wimp_CreateIcon: the icon bar (window -1 / -2) only */
        block = (int *) (long) r.r[1];
        if (block[0] != -1 && block[0] != -2) { e = error("CreateIcon: icon bar only"); break; }
        fake_iconbar_flags = block[5];
        if (block[5] & 0x100)
            strncpy(fake_iconbar_sprite, (const char *) (long) block[6], 15);
        else
            strncpy(fake_iconbar_sprite, (const char *) &block[6], 12);
        r.r[0] = ++fake_iconbar_icons;
        break;
    case 0x400DE:   /* Wimp_StartTask: a test's hook (fake_starttask_hook, or fake_swi_hook) */
        if (!fake_starttask_hook) {
            if (fake_swi_hook && fake_swi_hook(no, &r, &e)) break;
            e = error("Wimp_StartTask not faked"); break;
        }
        r.r[0] = fake_starttask_hook((const char *) (long) r.r[0]);
        break;
    case 0x400E7:   /* Wimp_SendMessage: sender and my_ref filled in; to an
                       icon bar icon's creator (R2 = -2): a test's fake_swi_hook */
        if (r.r[2] == -2 && fake_swi_hook && fake_swi_hook(no, &r, &e)) break;
        block = (int *) (long) r.r[1];
        if (r.r[0] >= 17 && r.r[0] <= 19) {
            block[1] = 0x4A00;
            if (r.r[0] != 19) block[2] = next_my_ref++;
        }
        if (fake_message_hook) fake_message_hook(r.r[0], block, r.r[2], r.r[3]);
        break;
    case 0x400F1:   /* Wimp_TransferBlock: one address space here (a test's
                       fake_swi_hook can refuse it) */
        if (fake_swi_hook && fake_swi_hook(no, &r, &e)) break;
        memcpy((void *) (long) (unsigned) r.r[3], (const void *) (long) (unsigned) r.r[1], (size_t) r.r[4]);
        break;
    case 0x400E9:   /* Wimp_SpriteOp 36 and 40 on the Wimp's pointer sprites */
        if (r.r[0] == 24) {          /* select: R2 -> the sprite (the pool) */
            const int *spr = pool_find((const char *) (long) r.r[2]);
            if (!spr) { e = error("Sprite doesn't exist"); break; }
            r.r[2] = (int) (long) spr;
            break;
        }
        if (strcmp((const char *) (long) r.r[2], "ptr_default") != 0 &&
            !(fake_wimp_ptr_write && strcmp((const char *) (long) r.r[2], "ptr_write") == 0)) {
            e = error("Sprite doesn't exist"); break;
        }
        if (r.r[0] == 40) { r.r[3] = 9; r.r[4] = 20; r.r[5] = 0; r.r[6] = 27; break; }  /* 9 x 20 */
        if (r.r[0] != 36) { e = error("Wimp_SpriteOp reason not faked"); break; }
        fake_ptr_flags = r.r[3];
        snprintf(fake_ptr_name, sizeof fake_ptr_name, "%s", (const char *) (long) r.r[2]);
        fake_ptr_sprite = NULL;
        fake_ptr_shape = r.r[3] & 15;
        fake_ptr_hx = r.r[4]; fake_ptr_hy = r.r[5];
        fake_ptr_sets++;
        break;
    case 0x400EA:   /* Wimp_BaseOfSprites: R0 ROM area (none), R1 RAM area */
        r.r[0] = 0;
        if (fake_pool_area_override) r.r[1] = (int) (long) fake_pool_area_override;
        else if (fake_wimp_sprites) { build_pool(); r.r[1] = (int) (long) pool; }
        else r.r[1] = 0;
        break;
    case 0x400E5:   /* Wimp_ReadPalette: R1 -> 20 words */
        memcpy((void *) (long) r.r[1], fake_wimp_palette, sizeof fake_wimp_palette);
        break;
    case 0x2F:      /* OS_ReadPalette: pointer colours (R1 = 25) only */
        if (r.r[1] != 25 || r.r[0] < 0 || r.r[0] > 3) { e = error("ReadPalette: pointer colours only"); break; }
        r.r[2] = r.r[3] = fake_ptr_palette[r.r[0]];
        break;
    case 0x400D0:   /* Wimp_DragBox */
        if (r.r[1] == -1) fake_dragbox_type = -1;
        else { fake_dragbox_type = ((int *) (long) r.r[1])[1]; fake_dragboxes++; }
        break;
    case 0x400CA:   /* Wimp_GetRectangle: only ever one rectangle */
        r.r[0] = 0;
        full_clip();
        break;
    default:
        if (fake_swi_hook && fake_swi_hook(no, &r, &e)) break;
        e = error("SWI not faked");
    }
    if (out) *out = r;
    return e;
}

/* OS_ValidateAddress: carry clear if every page of r0..r1 is mapped. */
_kernel_oserror *_kernel_swi_c(int no, _kernel_swi_regs *in, _kernel_swi_regs *out, int *carry)
{
    if (no == 0x3A) {
        long pg = sysconf(_SC_PAGESIZE);
        unsigned long a = (unsigned long) (unsigned) in->r[0] & ~(pg - 1);
        unsigned long end = (unsigned long) (unsigned) in->r[1];
        unsigned char v;
        /* a range that wraps past the top of memory: the Pi's kernel
           faults in the SWI (seen on the Pi: EMT trap, 0.10) */
        if (end < (unsigned long) (unsigned) in->r[0]) {
            fprintf(stderr, "fake: OS_ValidateAddress &%X-&%X wraps: the kernel would fault\n",
                    (unsigned) in->r[0], (unsigned) in->r[1]);
            abort();
        }
        *carry = 0;
        for (; a < end; a += pg)
            if (mincore((void *) a, pg, &v) != 0) { *carry = 1; break; }
        if (out) *out = *in;
        return NULL;
    }
    *carry = 0;
    return _kernel_swi(no, in, out);
}

int _kernel_osbyte(int op, int x, int y)
{
    (void) y;
    int banks = fake_screen.da_size / (fake_screen.w * fake_screen.h * 4);
    if (op == 19) fake_vsyncs++;
    if (op == 176) return (-fake_vsyncs) & 0xFF;        /* the vsync counter counts down */
    if (op == 106) { fake_pointer_shape = x; return 0; }
    if (op == 121) {                     /* keyboard scan / test one key */
        int k;
        if (x & 0x80) return fake_keys_down[x & 0x7F] ? 0xFF : 0;
        for (k = x; k < 128; k++) if (fake_keys_down[k]) return k;
        return 0xFF;
    }
    if ((op == 112 || op == 113) && x >= 0 && x <= banks) {
        if (x == 0) x = fake_screen.display_bank;
        if (op == 112) fake_screen.vdu_bank = x;
        else fake_screen.display_bank = x;
    }
    return 0;
}

/* VDU 24 (graphics window) only; four signed 16-bit inclusive coordinates. */
int fake_watch[4], fake_watch_value, fake_watch_hits;   /* pixels written in a rect with a value */

int _kernel_oswrch(int c)
{
    static int state = -1, bytes[8];
    if (state < 0) {
        if (c == 24) state = 0;
        return 0;
    }
    bytes[state++] = c & 0xFF;
    if (state == 8) {
        int i, v[4];
        for (i = 0; i < 4; i++)
            v[i] = (short) (bytes[2 * i] | (bytes[2 * i + 1] << 8));
        clip[0] = v[0]; clip[1] = v[1]; clip[2] = v[2] + 1; clip[3] = v[3] + 1;
        state = -1;
    }
    return 0;
}

void fake_reset_clip(void)
{
    full_clip();
}
