import sys, gi
gi.require_version("Gtk", "3.0")
from gi.repository import Gtk, GLib, Gdk

mode = sys.argv[1] if len(sys.argv) > 1 else "dialog"

win = Gtk.Window(title="Export")
win.set_border_width(10)
win.move(0, 0)
outer = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=8)
win.add(outer)

if mode == "menu":
    mb = Gtk.MenuBar()
    for name in ("File", "Edit", "View"):
        mi = Gtk.MenuItem(label=name); mb.append(mi)
    outer.pack_start(mb, False, False, 0)

nb = Gtk.Notebook()
outer.pack_start(nb, True, True, 0)

# --- page 1
g = Gtk.Grid(column_spacing=8, row_spacing=6, margin=10)
g.attach(Gtk.Label(label="Filename", xalign=1), 0, 0, 1, 1)
e = Gtk.Entry(text="Accounts2026/csv"); e.set_hexpand(True)
g.attach(e, 1, 0, 2, 1)

fmt = Gtk.Frame(label="Format")
fb = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=2, margin=8)
r1 = Gtk.RadioButton.new_with_label(None, "CSV (comma separated)")
r2 = Gtk.RadioButton.new_with_label_from_widget(r1, "Tab separated")
r3 = Gtk.RadioButton.new_with_label_from_widget(r1, "Fireworkz document")
r3.set_sensitive(False)
for w in (r1, r2, r3): fb.pack_start(w, False, False, 0)
fmt.add(fb)
g.attach(fmt, 0, 1, 2, 1)

opt = Gtk.Frame(label="Options")
ob = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=2, margin=8)
c1 = Gtk.CheckButton(label="Include headings"); c1.set_active(True)
c2 = Gtk.CheckButton(label="Quote all fields")
c3 = Gtk.CheckButton(label="Set filetype"); c3.set_active(True); c3.set_sensitive(False)
c4 = Gtk.CheckButton(label="All sheets"); c4.set_inconsistent(True)
for w in (c1, c2, c3, c4): ob.pack_start(w, False, False, 0)
hb = Gtk.Box(spacing=6)
hb.pack_start(Gtk.Label(label="Decimal places"), False, False, 0)
sp = Gtk.SpinButton.new_with_range(0, 10, 1); sp.set_value(2)
hb.pack_start(sp, False, False, 0)
ob.pack_start(hb, False, False, 4)
opt.add(ob)
g.attach(opt, 2, 1, 1, 1)

g.attach(Gtk.Label(label="Encoding", xalign=1), 0, 2, 1, 1)
cb = Gtk.ComboBoxText()
for t in ("Latin-1 (RISC OS)", "UTF-8", "UTF-16"): cb.append_text(t)
cb.set_active(0)
g.attach(cb, 1, 2, 1, 1)

g.attach(Gtk.Label(label="Size", xalign=1), 0, 4, 1, 1)
ro = Gtk.Entry(text="12,480 bytes"); ro.set_editable(False); ro.set_can_focus(False)
ro.get_style_context().add_class("display-field")
g.attach(ro, 1, 4, 1, 1)

g.attach(Gtk.Label(label="Compression", xalign=1), 0, 3, 1, 1)
sc = Gtk.Scale.new_with_range(Gtk.Orientation.HORIZONTAL, 0, 9, 1)
sc.set_value(6); sc.set_draw_value(False); sc.set_hexpand(True)
g.attach(sc, 1, 3, 2, 1)
nb.append_page(g, Gtk.Label(label="General"))

# --- page 2 (list)
store = Gtk.ListStore(str, str, str)
rows = [("Sheet1", "1,204", "Accounts"), ("Sheet2", "88", "Budget"),
        ("Summary", "12", "Totals"), ("Q1", "310", "Jan-Mar"),
        ("Q2", "295", "Apr-Jun"), ("Q3", "301", "Jul-Sep"),
        ("Q4", "0", "Oct-Dec"), ("Notes", "4", "Misc"),
        ("Archive", "2,018", "Old years"), ("Rates", "14", "VAT"),
        ("Staff", "9", "Payroll"), ("Assets", "41", "Register")]
rows += [("Year%d" % y, str(y * 7 % 300), "Archive %d" % y) for y in range(2010, 2026)]
for r in rows: store.append(r)
tv = Gtk.TreeView(model=store)
for i, t in enumerate(("Sheet", "Rows", "Description")):
    tv.append_column(Gtk.TreeViewColumn(t, Gtk.CellRendererText(), text=i))
tv.get_selection().select_path(Gtk.TreePath(1))
sw = Gtk.ScrolledWindow(); sw.set_shadow_type(Gtk.ShadowType.IN)
sw.set_policy(Gtk.PolicyType.AUTOMATIC, Gtk.PolicyType.ALWAYS)
sw.set_size_request(-1, 120); sw.add(tv)
p2 = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, margin=10); p2.pack_start(sw, True, True, 0)
nb.append_page(p2, Gtk.Label(label="Sheets"))

pb = Gtk.ProgressBar(); pb.set_fraction(0.42)
outer.pack_start(pb, False, False, 0)

bb = Gtk.Box(spacing=8)
cancel = Gtk.Button(label="Cancel")
save = Gtk.Button(label="Export"); save.set_can_default(True)
bb.pack_end(save, False, False, 0); bb.pack_end(cancel, False, False, 0)
outer.pack_start(bb, False, False, 0)
win.set_default(save); save.grab_default()
win.connect("destroy", Gtk.main_quit)
win.show_all()

if mode == "popup":
    GLib.timeout_add(300, lambda: cb.popup() or False)

if mode == "list":
    nb.set_current_page(1)

if mode == "menu":
    def pop():
        m = Gtk.Menu()
        def item(label, accel=None, sub=False, sens=True):
            mi = Gtk.MenuItem(label=label)
            if accel:
                k, mods = Gtk.accelerator_parse(accel)
                mi.get_child().set_accel(k, mods)
            if sub:
                s = Gtk.Menu(); s.append(Gtk.MenuItem(label="x")); mi.set_submenu(s)
            mi.set_sensitive(sens); m.append(mi)
        item("Info", sub=True)
        item("Save as...", "F3")
        item("Export", sub=True)
        m.append(Gtk.SeparatorMenuItem())
        ck = Gtk.CheckMenuItem(label="Show grid"); ck.set_active(True); m.append(ck)
        ra = Gtk.RadioMenuItem(label="Portrait"); ra.set_active(True); m.append(ra)
        rb = Gtk.RadioMenuItem(label="Landscape", group=ra); m.append(rb)
        item("Print...", "Ctrl+P")
        item("Undo", "F8", sens=False)
        m.show_all()
        rect = Gdk.Rectangle(); rect.x, rect.y, rect.width, rect.height = 40, 30, 1, 1
        m.popup_at_rect(win.get_window(), rect, Gdk.Gravity.NORTH_WEST, Gdk.Gravity.NORTH_WEST, None)
        # highlight an item
        m.select_item(m.get_children()[1])
        return False
    GLib.timeout_add(300, pop)

GLib.timeout_add(1000, lambda: print("default-class:", save.get_style_context().has_class("default"), save.has_default()) or False)
GLib.timeout_add(1500, Gtk.main_quit)
Gtk.main()
