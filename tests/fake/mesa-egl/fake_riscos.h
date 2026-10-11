#ifndef FAKE_RISCOS_H
#define FAKE_RISCOS_H

#include <kernel.h>

#define FAKE_MAX_WINDOWS 16

typedef struct {
    int w, h, xeig, yeig, log2bpp, flags, line_length;
    unsigned int *mem;          /* room for 3 banks; bank n at mem + (n-1) * w * h */
    int da_size, da_max;        /* screen dynamic area size and limit, bytes */
    int vdu_bank, display_bank;
} fake_screen_t;

typedef struct {
    int handle;
    int x0, y0, x1, y1;     /* visible area, OS units */
    int sx, sy;             /* scroll offsets */
    int stack;              /* higher = nearer the front (last opened) */
} fake_window_t;

extern fake_screen_t fake_screen;
extern fake_window_t fake_windows[FAKE_MAX_WINDOWS];
extern int fake_vsyncs, fake_update_calls, fake_redraw_calls, fake_plots;
extern int fake_sprite_creates;           /* OS_SpriteOp 15 calls */
/* Wimp_ForceRedraw calls and the last rectangle (window, x0, y0, x1, y1) */
extern int fake_force_redraws, fake_force_rect[5];
extern int fake_scaled_plots;          /* OS_SpriteOp 52 */

void fake_set_screen(int w, int h, int trgb, int log2bpp);
/* The lock every fake SWI takes (recursive), for threaded callers. */
void fake_lock(void);
void fake_unlock(void);
fake_window_t *fake_open_window(int handle, int x0, int y0, int x1, int y1, int sx, int sy);
unsigned int fake_screen_pixel(int x, int y_from_top);
unsigned int fake_bank_pixel(int bank, int x, int y_from_top);
void fake_reset_clip(void);
/* Count pixels of value fake_watch_value plotted inside pixel rect
   fake_watch = {x0, y0 (from top), x1, y1} (exclusive). */
extern int fake_watch[4], fake_watch_value, fake_watch_hits;

/* A scripted Wimp task (Wimp_Initialise/CreateWindow/OpenWindow/Poll...).
   Wimp_Poll first returns one Redraw_Window_Request for the newest window,
   then fake_wimp_nulls Null_Reason_Codes (when nulls are unmasked), then
   each fake_wimp_script entry {reason, a, b, c}, each followed by a null
   event when nulls are unmasked:
     8 = Key_Pressed (a = RISC OS key code)
     6 = Mouse_Click at pixel a, b of the newest window's visible area
         (from its top left) with buttons c (4 Select, 2 Menu, 1 Adjust);
         the buttons stay held (Wimp_GetPointerInfo) until an FAKE_RELEASE
     2 = Open_Window_Request (a = width | height << 16, OS units, 0 = same size;
         b, c = move the top left by b, c OS units)
     9 = Menu_Selection (a, b, c = item indices, -1 ends)
    10 = Scroll_Request, extended (the wheel): a, b = x, y OS units (y + = up)
    17 = User_Message (a = message action, e.g. 0x400C1 ModeChange)
     FAKE_MOVE = pointer to pixel a, b (null event only)
     FAKE_RELEASE = buttons released (null event only)
     FAKE_WHEEL = scroll wheel moved by a (null event only)
     FAKE_NULL = a null event
   then Close_Window_Request.
   fake_wimp_hook, if set, is called before each Poll returns (a frame). */
extern int fake_sprite_plot_converts, fake_fills;
extern int fake_target_window;           /* scripted events go here (0 = newest window) */
/* Mouse_Click's icon handle (-1); the icon bar icon made (Wimp_CreateIcon):
   how many, its flags and sprite name; Wimp_ProcessKey's last key (-1) */
extern int fake_target_icon, fake_iconbar_icons, fake_iconbar_flags, fake_processed_key;
extern char fake_iconbar_sprite[16];
/* Wimp_ReportError: how many, the last text, flags and extra buttons;
   the answer it gives (1 OK, 2 Cancel, 3 the first extra button) */
extern int fake_reports, fake_report_flags, fake_report_answer;
extern char fake_report_text[256], fake_report_buttons[64];
/* The printer (PDriver SWIs): installed?, its resolution, name, paper
   (millipoints); what was printed: jobs, pages, copies (last DrawPage),
   EndJob / AbortJob calls, the rectangle given (OS units), the page
   sprite's size and plot scale, its non-white pixels (and the first such
   row and column), the job's title */
extern int fake_pdriver, fake_print_dpi, fake_print_paper[2];
extern const char *fake_print_name;
extern int fake_print_jobs, fake_print_pages, fake_print_copies, fake_print_ended, fake_print_aborted;
extern int fake_print_rect[4], fake_print_scale[4], fake_print_sprite_w, fake_print_sprite_h;
extern int fake_print_ink, fake_print_ink_top, fake_print_ink_left;
extern char fake_print_title[64];
extern int fake_print_plots, fake_print_ink_runs;
extern int fake_print_draws;             /* Draw_Fill/Draw_Stroke on the last page (vectors) */
extern int fake_print_polls_selected;   /* Wimp_Polls while a print job was selected */
int fake_wimp_script_pos(void);          /* script entry last returned, -1 before */
#define FAKE_SCRIPT_MAX 8192
extern int fake_wimp_nulls, fake_wimp_script[FAKE_SCRIPT_MAX][4], fake_wimp_script_len;
extern int fake_wimp_polls, fake_wimp_keys_passed, fake_wimp_tasks;
extern void (*fake_wimp_hook)(int reason);
/* Called for SWIs the fake doesn't know; returns 1 if it handled the SWI
   (setting *e for an error), 0 for "SWI not faked". */
extern int (*fake_swi_hook)(int no, _kernel_swi_regs *r, _kernel_oserror **e);
/* What Wimp_GetWindowState reports as the window in front (-1 = none). */
extern int fake_window_behind;
extern const char *fake_wimp_title;      /* the newest window's indirected title */
extern int fake_wimp_desktop;           /* Wimp_ReadSysInfo 0 reports a desktop */
#define FAKE_MOVE    100
#define FAKE_RELEASE 101
#define FAKE_WHEEL   102
#define FAKE_NULL    103
#define FAKE_SLEEP   105      /* a null event after sleeping a ms (timers, tooltips) */
#define FAKE_BUTTONS 104      /* set the buttons Wimp_GetPointerInfo reports (null event) */
/* The pointer (OS units, buttons), the wheel count, menus opened,
   OS_Byte 106 calls (pointer on/off) and the internal keys held down
   (OS_Byte 121) */
extern int fake_pointer[3], fake_wheel, fake_menus_opened, fake_pointer_shape;
extern const int *fake_menu;            /* the open Wimp_CreateMenu block (NULL = none) */
extern int fake_menu_x, fake_menu_y, fake_menus_closed;
extern unsigned char fake_keys_down[128];
extern int fake_log_menus;              /* print each menu opened to stderr */
/* Wimp_SendMessage calls (after the fake fills in the sender, 0x4A00,
   and my_ref), and messages from "other tasks" for Wimp_Poll to return
   (reason 17/18/19, block) before the next script entry. */
extern void (*fake_message_hook)(int reason, int *block, int dest, int icon);
void fake_queue_message(int reason, const int *block);
/* Wimp_StartTask: the command, and R0 to return (the new task's handle; 0:
   it has ended). No hook: an error. */
extern int (*fake_starttask_hook)(const char *command);
int fake_new_ref(void);                 /* a my_ref for a queued message */
extern int fake_dragbox_type, fake_dragboxes;
/* OS_SpriteOp / Wimp_SpriteOp 36: the pointer shape set last (sprite name,
   its sprite if from a user area, shape number, active point), and calls */
extern const int *fake_ptr_sprite;
extern char fake_ptr_name[16];
extern int fake_ptr_shape, fake_ptr_flags, fake_ptr_hx, fake_ptr_hy, fake_ptr_sets;
extern int fake_ptr_palette[4];          /* pointer colours (OS_ReadPalette 25), &BBGGRRxx */
extern int fake_wimp_ptr_write;          /* the Wimp has ptr_write (9 x 20) */
/* Wimp_ReadPalette (the 16 Wimp colours, &BBGGRRxx; the standard palette
   unless a test changes it), and the Wimp sprite pool (Wimp_SpriteOp 24),
   if fake_wimp_sprites:
     optoff, opton  12x12 8bpp, standard 256-colour palette, 1bpp mask;
                    border &33 = #bb7733, inside white (opton: a 4x4 &17 dot)
     radiooff       12x12 4bpp with a palette: ring #12ab34, inside white,
                    1bpp mask (round)
     radioon        12x12 32bpp #3456de, RISC OS 5 alpha mask (left half 255,
                    right half 128)
     gright         10x10 numbered mode 28 (8bpp), old-style mask (x <= y)
     left, right    8x4 1bpp at 90 x 45 dpi (8x8 on an EIG 1 screen) */
extern unsigned int fake_wimp_palette[20];
/* Wimp_ReadSysInfo 9: the tool (window border) sprites, a sprite area of
   14x14 32bpp sprites named fake_tool_names[i] (with first/free words
   that aren't offsets, as a Pi's), each one colour:
   #a01020 with i added to its red (uicon #a01020, vbarmid i = 10: #aa1020,
   sicon i = 29: #bd1020) */
extern int fake_tool_sprites;
extern const char *fake_tool_names[];
extern int fake_window_flags[256];       /* each window's flags (Wimp_CreateWindow +28), by handle & 255 */
extern int fake_menu_window, fake_menu_windows_opened;   /* a window opened as a menu (Wimp_CreateMenu) */
extern int *fake_tool_area_override;     /* a test's own tool sprite area instead */
extern int *fake_pool_area_override;     /* a test's own sprite pool (Wimp_BaseOfSprites R1) */
extern const int *(*fake_pool_lookup)(const char *name);   /* a test's Wimp pool sprites first */
extern int fake_wimp_sprites;   /* Wimp_DragBox: type now (-1 none), calls */

/* AcornSSL (fake_riscos.c): sessions created, closed (and of those, with
   the socket still open), handshake steps, the last host name (SNI) */
extern int fake_acornssl_sessions, fake_acornssl_closes, fake_acornssl_closed_open, fake_acornssl_handshake_steps;
extern char fake_acornssl_host[256];

/* The fake filing systems (OS_FSControl 33) and type names (18) */
const char *fake_fs_name(int no);
void fake_type_name(int type, int *r2, int *r3);

#endif
