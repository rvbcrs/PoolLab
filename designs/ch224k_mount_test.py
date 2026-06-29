from build123d import *
import os

# Run from repo root so the "designs/..." export paths resolve regardless of cwd.
os.chdir(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# =============================================================================
# CH224K (USB-C PD trigger) MOUNT TEST FIXTURE — board 31 x 20 mm
# Same recipe as designs/ctp09_mount_test.py, just resized. Tune the dims at
# the top after the first print. Recentred to origin, print floor-down.
# =============================================================================

# --- Board geometry (CH224K module is 31 x 20 mm) ---
board_w      = 31.0   # X (length); USB-C on the +X edge
board_l      = 20.0   # Y (width)
pcb_only_t   = 1.0    # bare PCB thickness (typical for these modules)
clamp_t      = 1.0    # board height where the clips grab (assumes bare-PCB edge)

# --- Vertical placement ---
standoff_z   = 1.5    # clearance under the board
socket_above_underside = 2.6  # USB-C opening centre above the board underside

# --- USB-C cutout (connector + small margin; smaller than board, can't pass) ---
usbc_cut_w   = 9.5
usbc_cut_h   = 3.6
usbc_cut_fil = 0.8
faceplate_t  = 0.6    # thin outer lip; socket recess from outer wall = this + 0.5

# --- Snap clips ---
wall_t       = 2.0
guide_h      = 1.0
arm_t        = 1.8    # 1.4 still snapped in ABS; 1.8 = ~1.65x stronger again,
                      #   still flexy thanks to arm_extra_h (longer cantilever)
hook_d       = 0.5    # smaller barb = less travel during insertion
hook_h       = 1.0
arm_extra_h  = 5.0    # extra outer-arm height above the hook = longer cantilever
clr          = 0.30
clamp_gap    = 0.5    # tilt-in clearance above PCB top
clip_cx      = [-board_w/4, board_w/4]   # symmetric clip pairs (move if a component lands here)
clip_w       = 6.0    # wider too, ~1.3x stronger along the print direction

# --- Back-wall wire slot ---
wire_slot_w  = 10.0   # wider for the bigger board

# =============================================================================

pkt_w   = board_w + 2 * clr
pkt_l   = board_l + 2 * clr
pcb_top = standoff_z + clamp_t
hook_z  = pcb_top + clamp_gap

with BuildPart() as mount:
    with Locations((-wall_t / 2, 0, 0)):
        Box(pkt_w + wall_t, pkt_l + 2 * arm_t, standoff_z,
            align=(Align.CENTER, Align.CENTER, Align.MIN))

    with Locations((-(pkt_w / 2 + wall_t / 2) + 0.2, 0, standoff_z)):
        Box(wall_t, pkt_l, clamp_t + guide_h,
            align=(Align.CENTER, Align.CENTER, Align.MIN))
    with Locations((-(pkt_w / 2 + wall_t / 2) + 0.2, 0, standoff_z)):
        Box(wall_t + 2, wire_slot_w, clamp_t + guide_h + 2,
            align=(Align.CENTER, Align.CENTER, Align.MIN), mode=Mode.SUBTRACT)

    arm_h = hook_z + hook_h + arm_t + hook_d + arm_extra_h
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

board_edge_x = board_w / 2
wall_outer_x = board_edge_x + faceplate_t
wall_inner_x = wall_outer_x - wall_t

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
    extrude(amount=wall_t)

    add(mount.part)

    with Locations(((wall_inner_x + wall_outer_x) / 2, 0, base_z)):
        Box(wall_t, fixture_y, wall_top_z - base_z,
            align=(Align.CENTER, Align.CENTER, Align.MIN))

    pocket_w  = board_l + 1.0
    pocket_z0 = standoff_z
    pocket_z1 = usbc_cut_z + usbc_cut_h / 2 + 0.5
    with Locations(((wall_inner_x - 0.5 + board_edge_x) / 2, 0, (pocket_z0 + pocket_z1) / 2)):
        Box(board_edge_x - (wall_inner_x - 0.5), pocket_w, pocket_z1 - pocket_z0,
            align=(Align.CENTER, Align.CENTER, Align.CENTER), mode=Mode.SUBTRACT)

    with BuildSketch(Plane.YZ.offset(wall_outer_x)):
        with Locations((0, usbc_cut_z)):
            usbc_rect = Rectangle(usbc_cut_w, usbc_cut_h)
            fillet(usbc_rect.vertices(), radius=usbc_cut_fil)
    extrude(amount=-(wall_t + 1), mode=Mode.SUBTRACT)

test_part = fixture.part
test_part.label = "CH224K Mount Test"

export_stl(test_part, "designs/ch224k_mount_test.stl")
export_step(test_part, "designs/ch224k_mount_test.step")
print(f"Exported CH224K test fixture (board {board_w}x{board_l} mm)")
print(f"  PCB top Z={pcb_top:.1f} | hook underside Z={hook_z:.1f} | USB-C slot {usbc_cut_w}x{usbc_cut_h} @Z={usbc_cut_z:.1f}")

try:
    from ocp_vscode import show
    show(test_part, names=["CH224K Mount Test"])
except Exception as e:
    print(f"(viewer skipped: {e})")
