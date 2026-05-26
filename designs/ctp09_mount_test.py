from build123d import *
import os

# Run from repo root so the "designs/..." export paths resolve regardless of cwd.
os.chdir(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# =============================================================================
# CTP09 (USB-C PD trigger) MOUNT TEST FIXTURE
# -----------------------------------------------------------------------------
# Small standalone printable piece to dial in the fit before printing the case:
#   - snap-fit clips that clamp the board down (no vertical play)
#   - a -X back wall with a SLOT so the power wires can exit
#   - a +X wall with a USB-C cutout sized to the CONNECTOR (not the whole board)
# Recentred to origin: board centre at X=0,Y=0, floor (inner surface) at Z=0.
# Print floor-down. Snap the real board in from the top; plug a USB-C cable
# through the slot to confirm it reaches the socket.
#
# >>> All tunable fit dimensions are here at the top. <<<
# =============================================================================

# --- Board geometry (measured) ---
board_w      = 23.0   # X (length); USB-C is on the +X edge
board_l      = 11.5   # Y (width)
pcb_only_t   = 1.0    # bare PCB thickness
clamp_t      = 1.0    # board height where the clips actually grab. At the clip
                      #   positions (+2/+7.5, near the USB-C/IC end) the edge is
                      #   bare PCB (~1mm); the 2mm edge components are elsewhere.

# --- Vertical placement ---
standoff_z   = 1.5    # clearance under the board (board underside rests here)
socket_above_underside = 2.5  # USB-C opening centre above the board's underside

# --- USB-C cutout (connector size + a little margin; smaller than the board so
#     the module can't pass through) ---
usbc_cut_w   = 9.5    # Y width
usbc_cut_h   = 3.6    # Z height
usbc_cut_fil = 0.8    # corner radius
# Thin outer faceplate carrying the connector slot. The board's +X edge sinks
# into the wall and butts the BACK of this faceplate. The socket sits ~0.5mm
# BEHIND the PCB front edge, so socket recess from the outer wall = faceplate_t
# + 0.5. Truly flush isn't possible (the PCB edge would have to poke past the
# wall, removing the stop). 0.6 -> ~1.1mm recess, still a printable retention
# lip. Lower for a shallower socket, raise to retain the module better.
faceplate_t  = 0.6

# --- Snap clips ---
wall_t       = 2.0    # back/guide wall thickness
guide_h      = 1.0    # how far the side guidance rises above the clamp level
arm_t        = 1.8    # clip arm thickness (flex direction) - thicker = stronger
hook_d       = 0.7    # hook barb overhang onto the board top
hook_h       = 1.0    # hook barb zone height
clr          = 0.30   # lateral fit clearance per side
clamp_gap    = 0.0    # vertical play under the hook (0 = snug)
# A diode stands ~2mm proud on one long edge, centred ~19mm from the USB-C end
# (X ~ -7.5, body spanning roughly X[-9.5..-5.5]). The clips straddle it: they
# sit on the clear band between the diode and the USB-C wall. Same X on both
# ±Y edges so the insert orientation doesn't matter.
clip_cx      = [2.0, 7.5]   # clip-pair X positions (both ±Y edges); both clear of
                            #   the diode (which sits around the board centre, X<-2)
clip_w       = 4.5          # clip width along X - wider = stronger

# --- Back-wall wire slot ---
wire_slot_w  = 8.0    # central opening in the -X wall for the power wires

# =============================================================================

pkt_w   = board_w + 2 * clr
pkt_l   = board_l + 2 * clr
pcb_top = standoff_z + clamp_t          # height the hooks clamp at
hook_z  = pcb_top + clamp_gap

# --- Snap-fit mount ---
with BuildPart() as ctp09_mount:
    # 1. Floor platform (extends -X under the back wall, ±Y to anchor clips)
    with Locations((-wall_t / 2, 0, 0)):
        Box(pkt_w + wall_t, pkt_l + 2 * arm_t, standoff_z,
            align=(Align.CENTER, Align.CENTER, Align.MIN))

    # 2. -X back wall (inner stop) WITH a central wire slot
    with Locations((-(pkt_w / 2 + wall_t / 2), 0, standoff_z)):
        Box(wall_t, pkt_l, clamp_t + guide_h,
            align=(Align.CENTER, Align.CENTER, Align.MIN))
    # carve the wire slot through the back wall (leaves two corner ribs)
    with Locations((-(pkt_w / 2 + wall_t / 2), 0, standoff_z)):
        Box(wall_t + 2, wire_slot_w, clamp_t + guide_h + 2,
            align=(Align.CENTER, Align.CENTER, Align.MIN), mode=Mode.SUBTRACT)

    # 3. Snap clips on ±Y sides – clip pairs at clip_cx, straddling the diode
    arm_h = hook_z + hook_h + arm_t + hook_d
    for sign, iy in [(-1, -pkt_l / 2), (+1, +pkt_l / 2)]:
        for cx in clip_cx:
            with BuildSketch(Plane.YZ.offset(cx)):
                with BuildLine():
                    Polyline(
                        (iy,                 0),
                        (iy + sign * arm_t,  0),
                        (iy + sign * arm_t,  arm_h),
                        (iy - sign * hook_d, hook_z + hook_h),
                        (iy - sign * hook_d, hook_z),
                        (iy,                 hook_z),
                        close=True,
                    )
                make_face()
            extrude(amount=clip_w / 2, both=True)

# --- Mount <-> wall spacing (matches case.py logic) ---
# Board +X edge is at +board_w/2 (mount at origin). The wall sits so its OUTER
# face is faceplate_t beyond the board edge: the edge sinks (wall_t - faceplate_t)
# into the wall and stops against the faceplate back.
board_edge_x = board_w / 2
wall_outer_x = board_edge_x + faceplate_t
wall_inner_x = wall_outer_x - wall_t
pocket_depth = wall_t - faceplate_t           # how far the PCB edge sinks in

# --- Fixture (base plate joins mount + USB-C wall into one print) ---
fixture_y  = max(board_l, usbc_cut_w) + 8
wall_top_z = pcb_top + hook_h + arm_t + hook_d + 1
base_z     = -wall_t
usbc_cut_z = standoff_z + socket_above_underside

with BuildPart() as fixture:
    floor_x_min = -(pkt_w / 2 + wall_t + 2)
    floor_x_max = wall_outer_x
    with BuildSketch(Plane.XY.offset(base_z)):
        with Locations(((floor_x_min + floor_x_max) / 2, 0)):
            Rectangle(floor_x_max - floor_x_min, fixture_y)
    extrude(amount=wall_t)                    # base plate up to Z = 0

    add(ctp09_mount.part)

    # +X USB-C wall
    with Locations(((wall_inner_x + wall_outer_x) / 2, 0, base_z)):
        Box(wall_t, fixture_y, wall_top_z - base_z,
            align=(Align.CENTER, Align.CENTER, Align.MIN))

    # Inner pocket: PCB-edge-sized recess so the board front edge sinks into the
    # wall (board width, NOT connector width — keeps the narrow outer slot).
    pocket_w  = board_l + 1.0
    pocket_z0 = standoff_z
    pocket_z1 = usbc_cut_z + usbc_cut_h / 2 + 0.5
    with Locations(((wall_inner_x - 0.5 + board_edge_x) / 2, 0, (pocket_z0 + pocket_z1) / 2)):
        Box(board_edge_x - (wall_inner_x - 0.5), pocket_w, pocket_z1 - pocket_z0,
            align=(Align.CENTER, Align.CENTER, Align.CENTER), mode=Mode.SUBTRACT)

    # USB-C connector slot through the faceplate (rounded rect, centred on socket)
    with BuildSketch(Plane.YZ.offset(wall_outer_x)):
        with Locations((0, usbc_cut_z)):
            usbc_rect = Rectangle(usbc_cut_w, usbc_cut_h)
            fillet(usbc_rect.vertices(), radius=usbc_cut_fil)
    extrude(amount=-(wall_t + 1), mode=Mode.SUBTRACT)

test_part = fixture.part
test_part.label = "CTP09 Mount Test"

export_stl(test_part, "designs/ctp09_mount_test.stl")
export_step(test_part, "designs/ctp09_mount_test.step")
print("Exported: designs/ctp09_mount_test.stl + .step")
print(f"  clips clamp at Z={pcb_top:.1f} | USB-C slot {usbc_cut_w}x{usbc_cut_h} centred Z={usbc_cut_z:.1f}")
print(f"  fixture ~ X[{floor_x_min:.1f}..{wall_outer_x:.1f}] Y[±{fixture_y/2:.1f}] Z[{base_z:.1f}..{wall_top_z:.1f}]")

# Optional live preview (ignored if no viewer running)
try:
    from ocp_vscode import show
    show(test_part, names=["CTP09 Mount Test"])
except Exception as e:
    print(f"(viewer skipped: {e})")
