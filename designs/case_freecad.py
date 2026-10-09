"""Pura case v8 — built directly in FreeCAD (run via the freecad MCP or as a macro).

Layout: front wall (-Y) = GX12 + two PH-4502C BNCs, back wall (+Y) = Pura mainboard
USB-C, 3.5" screen in the lid above everything. Parameters live in P below; the case
width W is derived from the parts so the chain sensors -> PCB -> back wall stays tight.
"""
import FreeCAD, FreeCADGui, Part, ImportGui
from FreeCAD import Vector as V, Placement, Rotation

REPO = "/Users/rvbcrs/GitHub/PoolLab"
P = dict(
    L=112.0, H=40.0, wall=2.0, floor_t=2.0, lid_t=5.5, corner_r=5.0,
    post_r=3.5, post_hole_r=1.4, post_hole_depth=15.0,
    # Pura mainboard
    pcb=50.0, pcb_t=1.6, pcb_standoff=4.0, usb_recess=1.2,
    clr=0.3, arm_t=1.8, hook_d=0.5, hook_h=1.0, arm_extra_h=5.0, clip_w=6.0, clamp_gap=0.5,
    ledge_side=1.5, ledge_front=2.0, guide_t=2.0, guide_h=1.0,
    clip_ys=(10.0, 22.0), guide_y=(32.0, 48.0),
    usb_slot_w=9.5, usb_slot_h=3.6, usb_slot_r=0.8,
    # sensors / GX12
    sens_standoff=5.0, sens_gap_front=1.0, bnc_x=(-9.0, 26.0), bnc_hole_r=6.5,
    gx_x=-37.0, gx_z=0.0, gx_hole_r=6.1, gx_outside=6.75,
    # screen in lid
    scr_bezel=(94.5, 62.0, 4.0), scr_rear=(82.2, 57.8, 8.0), scr_lcd=(73.4, 49.0),
    scr_window=(84.0, 56.0, 3.0), scr_pocket_clr=0.25, scr_pocket_extra=0.2,   # window r, pocket clearance, pocket depth above bezel
    ret_outer=(107.0, 76.0), ret_inner=(84.0, 58.8), ret_t=3.0, ret_holes=(50.5, 35.0), ret_hole_r=1.7,
    boss_r=3.0, boss_hole_r=1.4,
    # clearance rules
    dupont_h=16.5, wire_room=5.0,
)

def vertical_edges(shape):
    return [e for e in shape.Edges if abs(e.Vertexes[0].X-e.Vertexes[-1].X)<1e-6 and abs(e.Vertexes[0].Y-e.Vertexes[-1].Y)<1e-6 and e.Length>1]
def edges_along(shape, axis):
    out=[]
    for e in shape.Edges:
        d=e.Vertexes[-1].Point-e.Vertexes[0].Point
        if d.Length>0.1 and abs(abs(d.normalize().dot(axis))-1)<1e-6: out.append(e)
    return out
def box(l,w,h, cx,cy, z0):            # centred in XY, bottom at z0
    return Part.makeBox(l,w,h, V(cx-l/2, cy-w/2, z0))
def cyl(r,h, x,y,z0, d=V(0,0,1)):
    return Part.makeCylinder(r,h, V(x,y,z0), d)
def cyls(shape, rmin, rmax):
    return [f for f in shape.Faces if f.Surface.TypeId=='Part::GeomCylinder' and rmin<=f.Surface.Radius<=rmax]

# ---------- reference parts (measured in their placed frame) ----------
sensor_raw = Part.Shape(); sensor_raw.read(REPO+"/designs/PH-4502C.STEP")
sens_rot = Rotation(V(0,0,1),-90).multiply(Rotation(V(1,0,0),90))   # Rx(90) first, then Rz(-90)
s0 = sensor_raw.copy(); s0.Placement = Placement(V(0,0,0), sens_rot)
bnc = cyls(s0, 5.5, 6.5)[0].Surface
holes = sorted(set((round(f.Surface.Center.x,2), round(f.Surface.Center.y,2)) for f in cyls(s0,1.3,1.7)))
hb = s0.BoundBox
board_front_y = max(h[1] for h in holes) + 3.0 if False else None
# board end on the BNC side = holes nearest the BNC + 3 mm
hy_front = min(h[1] for h in holes); hy_back = max(h[1] for h in holes)
board_front_edge = hy_front - 3.0                     # -Y side (BNC side)
board_bottom = min(f.Surface.Center.z for f in cyls(s0,1.3,1.7)) - 0.8   # hole axis at mid-thickness
Ls = hb.YMax - board_front_edge                       # front edge -> rearmost point (header)
print("sensor: BNC axis x=%.2f z=%.2f (above board bottom %.2f), holes %s, Ls=%.1f, x-extent %.1f..%.1f rel BNC" % (
    bnc.Center.x, bnc.Center.z, bnc.Center.z-board_bottom, holes, Ls, hb.XMin-bnc.Center.x, hb.XMax-bnc.Center.x))

gx_raw = Part.Shape(); gx_raw.read(REPO+"/designs/gx12-4p-m.stp")
pcb_raw = Part.Shape(); pcb_raw.read(REPO+"/designs/../pcb/pura-mainboard-0805-0.3.step")
pb = pcb_raw.BoundBox
pcb_top_parts = max(s.BoundBox.ZMax for s in pcb_raw.Solids) - 1.51
print("pcb raw bbox X %.1f..%.1f Y %.1f..%.1f, tallest part %.1f above board top" % (pb.XMin,pb.XMax,pb.YMin,pb.YMax,pcb_top_parts))

# ---------- derived layout ----------
L,H,wall,ft = P["L"],P["H"],P["wall"],P["floor_t"]
interior_y = P["sens_gap_front"] + Ls + 1.0 + P["pcb"] - P["usb_recess"]
W = interior_y + 2*wall
y0 = -W/2 + wall                     # front wall inner face
z0 = -H/2 + ft                       # floor inner face
yb = W/2 - wall                      # back wall inner face
pcb_front = yb + P["usb_recess"] - P["pcb"]
pcb_bot = z0 + P["pcb_standoff"]; pcb_top = pcb_bot + P["pcb_t"]
sens_bot = z0 + P["sens_standoff"]
sens_dy = (y0 + P["sens_gap_front"]) - board_front_edge
sens_dz = sens_bot - board_bottom
print("W=%.1f  interior %.1f x %.1f  pcb y %.1f..%.1f  z0=%.1f pcb_bot=%.1f" % (W, L-2*wall, interior_y, pcb_front, pcb_front+P["pcb"], z0, pcb_bot))

# ---------- case body ----------
outer = Part.makeBox(L, W, H, V(-L/2,-W/2,-H/2)); outer = outer.makeFillet(P["corner_r"], vertical_edges(outer))
inner = Part.makeBox(L-2*wall, W-2*wall, H, V(-L/2+wall,-W/2+wall,z0))
inner = inner.makeFillet(P["corner_r"]-wall, vertical_edges(inner))
case = outer.cut(inner)
post_dx, post_dy = L/2-wall-P["post_r"], W/2-wall-P["post_r"]
posts = [(sx*post_dx, sy*post_dy) for sx in (1,-1) for sy in (1,-1)]
for (px,py) in posts:
    case = case.fuse(cyl(P["post_r"], H-ft, px,py, z0))
    case = case.cut(cyl(P["post_hole_r"], P["post_hole_depth"], px,py, H/2-P["post_hole_depth"]))

# PCB pocket in back wall + USB-C slot (CH224K faceplate recipe)
usb_x = -P["pcb"]/2 + 7.5                    # socket centre 7.5 mm from the board's left edge
usb_z = pcb_bot + 2.76                       # shell centre measured on the PCB STEP
pkt_w = P["pcb"] + 2*P["clr"]
pkt_z0, pkt_z1 = pcb_bot-0.2, usb_z + P["usb_slot_h"]/2 + 0.5
case = case.cut(Part.makeBox(pkt_w, P["usb_recess"]+0.01, pkt_z1-pkt_z0, V(-pkt_w/2, yb-0.01, pkt_z0)))
slot = Part.makeBox(P["usb_slot_w"], wall+2, P["usb_slot_h"], V(usb_x-P["usb_slot_w"]/2, yb-1, usb_z-P["usb_slot_h"]/2))
slot = slot.makeFillet(P["usb_slot_r"], edges_along(slot, V(0,1,0)))
case = case.cut(slot)

# GX12 hole + BNC holes through the front wall
case = case.cut(cyl(P["gx_hole_r"], wall+2, P["gx_x"], -W/2-1, P["gx_z"], V(0,1,0)))
for bx in P["bnc_x"]:
    case = case.cut(cyl(P["bnc_hole_r"], wall+2, bx + (bnc.Center.x - bnc.Center.x), -W/2-1, bnc.Center.z + sens_dz, V(0,1,0)))

# PCB mount: ledges, guide walls, snap clips
bh = P["pcb"]/2
mount = box(P["pcb"], P["ledge_front"], P["pcb_standoff"], 0, pcb_front+P["ledge_front"]/2, z0)
for sx in (1,-1):
    xi = sx*(bh+P["clr"])                                     # inner face of arms/guides
    mount = mount.fuse(box(P["ledge_side"]+P["clr"]+P["arm_t"], P["pcb"], P["pcb_standoff"],
                           sx*(bh-P["ledge_side"]+(P["ledge_side"]+P["clr"]+P["arm_t"])/2), pcb_front+bh, z0))
    gy0,gy1 = P["guide_y"]
    mount = mount.fuse(box(P["guide_t"], gy1-gy0, P["pcb_standoff"]+P["pcb_t"]+P["guide_h"],
                           xi + sx*P["guide_t"]/2, pcb_front+(gy0+gy1)/2, z0))
    hook_z = pcb_top + P["clamp_gap"]
    arm_h = (hook_z-z0) + P["hook_h"] + P["arm_t"] + P["hook_d"] + P["arm_extra_h"]
    for cy in P["clip_ys"]:
        yc = pcb_front + cy
        pts = [(xi,0),(xi+sx*P["arm_t"],0),(xi+sx*P["arm_t"],arm_h),(xi-sx*P["hook_d"],hook_z-z0+P["hook_h"]),
               (xi-sx*P["hook_d"],hook_z-z0),(xi,hook_z-z0),(xi,0)]
        wire = Part.makePolygon([V(x, yc-P["clip_w"]/2, z0+z) for x,z in pts])
        mount = mount.fuse(Part.Face(wire).extrude(V(0,P["clip_w"],0)))
case = case.fuse(mount)

# sensor standoffs
for bx in P["bnc_x"]:
    dx = bx - bnc.Center.x
    for (hx,hy) in holes:
        case = case.fuse(cyl(2.0, P["sens_standoff"], hx+dx, hy+sens_dy, z0))
        case = case.fuse(cyl(1.2, P["sens_standoff"]+2.0, hx+dx, hy+sens_dy, z0))
case = case.removeSplitter()

# ---------- lid: window on top, bezel pocket from below, clamp frame + 4 bosses ----------
lz = H/2; lt = P["lid_t"]
lid = Part.makeBox(L, W, lt, V(-L/2,-W/2,lz)); lid = lid.makeFillet(P["corner_r"], vertical_edges(lid))
bw,bl,bt = P["scr_bezel"]; rw,rl,rt = P["scr_rear"]
pc = P["scr_pocket_clr"]; pd = bt + P["scr_pocket_extra"]
pocket = Part.makeBox(bw+2*pc, bl+2*pc, pd, V(-bw/2-pc, -bl/2-pc, lz-0.01)); pocket = pocket.makeFillet(4.0+pc, vertical_edges(pocket))
lid = lid.cut(pocket)
ww, wl, wr = P["scr_window"]
win = Part.makeBox(ww, wl, lt+2, V(-ww/2,-wl/2,lz-1)); win = win.makeFillet(wr, vertical_edges(win))
lid = lid.cut(win)
bx_, by_ = P["ret_holes"]; boss_h = P["ret_t"] - P["scr_pocket_extra"]
for sx in (1,-1):
    for sy in (1,-1):
        lid = lid.fuse(cyl(P["boss_r"], boss_h, sx*bx_, sy*by_, lz-boss_h))
        lid = lid.cut(cyl(P["boss_hole_r"], 6.0, sx*bx_, sy*by_, lz-boss_h-0.5))
for (px,py) in posts:
    lid = lid.cut(cyl(1.6, lt+1, px,py, lz-0.5))
    lid = lid.cut(Part.makeCone(1.6, 3.0, 1.6, V(px,py,lz+lt-1.6), V(0,0,1)))
lid = lid.removeSplitter()
ro_w, ro_l = P["ret_outer"]; ri_w, ri_l = P["ret_inner"]; rtk = P["ret_t"]
ret_top = lz + P["scr_pocket_extra"]                      # = bezel underside
ret = Part.makeBox(ro_w, ro_l, rtk, V(-ro_w/2,-ro_l/2, ret_top-rtk)); ret = ret.makeFillet(4.0, vertical_edges(ret))
ret = ret.cut(Part.makeBox(ri_w, ri_l, rtk+2, V(-ri_w/2,-ri_l/2, ret_top-rtk-1)))
for sx in (1,-1):
    for sy in (1,-1):
        ret = ret.cut(cyl(P["ret_hole_r"], rtk+2, sx*bx_, sy*by_, ret_top-rtk-1))

# screen reference: bezel in the pocket, rear body hanging through the frame
bez_top = lz + pd
bezel = Part.makeBox(bw,bl,bt, V(-bw/2,-bl/2,bez_top-bt)); bezel = bezel.makeFillet(4.0, vertical_edges(bezel))
rear = Part.makeBox(rw,rl,rt, V(-rw/2,-rl/2,bez_top-bt-rt))
lcd = Part.makeBox(P["scr_lcd"][0],P["scr_lcd"][1],0.2, V(-P["scr_lcd"][0]/2,-P["scr_lcd"][1]/2,bez_top-0.2))
scr_bottom = bez_top - bt - rt

# ---------- document ----------
for d in list(FreeCAD.listDocuments()):
    if d in ("RefMeasure","PuraCaseV8"): FreeCAD.closeDocument(d)
doc = FreeCAD.newDocument("PuraCaseV8")
def add(shape, label, color):
    o = doc.addObject("Part::Feature", label); o.Shape = shape; o.ViewObject.ShapeColor = color; return o
add(case, "Case", (0.75,0.75,0.75)); add(lid, "Lid", (0.2,0.8,0.2)); add(ret, "ScreenRetainer", (0.9,0.6,0.1))
add(bezel, "ScreenBezel", (0.2,0.2,0.2)); add(rear, "ScreenRear", (0.25,0.25,0.25)); add(lcd, "ScreenLCD", (0,0,0))

def insert_ghost(path, label, placement):
    before = set(o.Name for o in doc.Objects)
    ImportGui.insert(path, doc.Name)
    new = [o for o in doc.Objects if o.Name not in before]
    roots = [o for o in new if not any(p in new for p in o.InList)]
    for r in roots: r.Placement = placement; r.Label = label
    return roots
sens_pl = [Placement(V(bx-bnc.Center.x, sens_dy, sens_dz), sens_rot) for bx in P["bnc_x"]]
insert_ghost(REPO+"/designs/PH-4502C.STEP", "Sensor1", sens_pl[0])
insert_ghost(REPO+"/designs/PH-4502C.STEP", "Sensor2", sens_pl[1])
insert_ghost(REPO+"/designs/gx12-4p-m.stp", "GX12", Placement(V(P["gx_x"], -W/2-P["gx_outside"], P["gx_z"]), Rotation()))
pcb_pl = Placement(V(-100.0, pcb_front+125.0, pcb_bot), Rotation())
insert_ghost(REPO+"/pcb/pura-mainboard-0805-0.3.step", "PuraPCB", pcb_pl)
doc.recompute()

# ---------- checks ----------
def bb(shape, pl): s=shape.copy(); s.Placement=pl; return s.BoundBox
def overlap(a,b): return a.XMax>b.XMin and b.XMax>a.XMin and a.YMax>b.YMin and b.YMax>a.YMin and a.ZMax>b.ZMin and b.ZMax>a.ZMin
sb = [bb(sensor_raw, pl) for pl in sens_pl]; gb = bb(gx_raw, Placement(V(P["gx_x"], -W/2-P["gx_outside"], P["gx_z"]), Rotation())); pbb = bb(pcb_raw, pcb_pl)
post_bbs = [FreeCAD.BoundBox(px-P["post_r"],py-P["post_r"],z0, px+P["post_r"],py+P["post_r"],H/2) for px,py in posts]
checks = {
 "pcb dupont under screen": scr_bottom - (pcb_top + P["dupont_h"] + P["wire_room"]),
 "sensor dupont under screen": scr_bottom - (sens_bot + 1.6 + P["dupont_h"] + P["wire_room"]),
 "pcb tallest part under screen": scr_bottom - (pcb_top + pcb_top_parts),
 "pcb inside walls X": (L/2-wall) - pbb.XMax, "pcb front clear of sensors": pbb.YMin - max(s.YMax for s in sb),
 "gx12 clear of sensor1": sb[0].XMin - gb.XMax, "sensor gap": sb[1].XMin - sb[0].XMax,
 "sensor2 clear of post": min(p.XMin for p in post_bbs if p.XMin>0) - sb[1].XMax,
 "gx12 clear of post": gb.XMin - max(p.XMax for p in post_bbs if p.XMax<0),
 "sensor pins above floor": min(bb(so, sens_pl[0]).ZMin for so in sensor_raw.Solids if so.BoundBox.ZLength < 20) - z0,   # ignores the 23 mm "ptc" lead artefact
}
for k,v in checks.items(): print("%-32s %+.1f mm %s" % (k, v, "OK" if v>=0 else "<<< COLLISION"))
print("case outer: %.1f x %.1f x %.1f mm (+lid %.1f); screen glass %.1f mm below lid top" % (L, W, H, P["lid_t"], lz+lt-bez_top))
doc.saveAs(REPO+"/designs/case_v8.FCStd")
FreeCADGui.SendMsgToActiveView("ViewFit")
