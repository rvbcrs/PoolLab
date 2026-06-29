from build123d import *
from ocp_vscode import show, show_object, set_port, set_defaults
import os

# Run from the repo root regardless of how the script is launched, so the
# relative "designs/..." paths below resolve (VS Code's Run button sets the
# cwd to the file's folder, which breaks them otherwise).
os.chdir(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# Define parameters
wall_thickness = 2

# === Helper: Dynamic Hole Finder ===
def find_hole_locations(part_step, min_r=1.0, max_r=2.5):
    """
    Analyzes a STEP part, finds holes of specific radius on a common plane.
    Returns: list of Vector points (centers).
    """
    # 1. Find circular edges
    edges = part_step.edges().filter_by(GeomType.CIRCLE)
    
    # 2. Filter by Radius
    hole_candidates = []
    print(f"Scanning {len(edges)} circles...")
    for e in edges:
        if min_r <= e.radius <= max_r:
            hole_candidates.append(e.arc_center)
            
    # 3. Deduplicate (Spatial clustering)
    unique_holes = []
    for h in hole_candidates:
        if not any((u - h).length < 0.5 for u in unique_holes):
            unique_holes.append(h)
    
    if not unique_holes:
        print("Warning: No holes found.")
        return []
        
    # 4. Group by Z (Find the PCB plane)
    from collections import Counter
    # Round to 1 decimal place to group
    # Note: If the part is rotated in the file, "Z" might not be the plane normal.
    # But usually PCBs are drawn on XY.
    z_coords = [round(h.Z, 1) for h in unique_holes]
    common_z = Counter(z_coords).most_common(1)[0][0]
    final_holes = [h for h in unique_holes if abs(h.Z - common_z) < 1.0]
    
    print(f"Found {len(final_holes)} mounting holes at Z={common_z}")
    return final_holes

def filter_corner_holes(points, count=4):
    """
    Selects the 'count' points closest to the bounding box corners of the point set.
    """
    if not points: return []
    if len(points) <= count: return points
    
    # 1. Bounds
    xs = [p.X for p in points]
    ys = [p.Y for p in points]
    min_x, max_x = min(xs), max(xs)
    min_y, max_y = min(ys), max(ys)
    
    # 2. Define ideal corners
    # TL: (MinX, MaxY), TR: (MaxX, MaxY), BL: (MinX, MinY), BR: (MaxX, MinY)
    corners = [
        Vector(min_x, max_y, 0), # TL
        Vector(max_x, max_y, 0), # TR
        Vector(min_x, min_y, 0), # BL
        Vector(max_x, min_y, 0)  # BR
    ]
    
    selected = []
    # For each ideal corner, find closest point
    
    if count == 2:
        # Diagonal: TL and BR?
        target_corners = [corners[0], corners[3]]
    else:
        target_corners = corners
        
    for c in target_corners:
        # Find point closest to c ignoring Z
        best_p = min(points, key=lambda p: (Vector(p.X, p.Y, 0) - c).length)
        # Avoid duplicates if multiple corners resolve to same point (unlikely with >count pts)
        is_dup = False
        for s in selected:
            if (s - best_p).length < 0.1:
                is_dup = True
                break
        if not is_dup:
            selected.append(best_p)
            
    return selected

# === 2. Define PH-4502C Sensor Part ===
# Load from STEP file
imported_sensor = import_step("designs/PH-4502C.STEP")
sensor_hole_pts = find_hole_locations(imported_sensor, min_r=1.3, max_r=1.8)
sensor_hole_pts = filter_corner_holes(sensor_hole_pts, count=4) # Ensure 4 corners

# === 3. Define LM2596 (Buck Converter) ===
imported_lm2596 = import_step("designs/LM2596.stp")
# LM2596: Try tightening radius filter to avoid vias. 3mm hole is R=1.5. 
lm2596_hole_pts = find_hole_locations(imported_lm2596, min_r=1.4, max_r=1.6) 
# Filter to 2 diagonal corners (Top-Left, Bottom-Right)
lm2596_hole_pts = filter_corner_holes(lm2596_hole_pts, count=2) 

# === 1. Panel-mount USB-C socket — REMOVED ===
# User request: the panel-mount USB-C on the +X wall is cancelled. The PD trigger
# module (CTP09) now provides USB-C access directly through the right wall instead.


# Re-define constants used for placement
pcb_l = 42
pcb_w = 32
pcb_h = 1.6
bnc_d = 12 
bnc_stickout = 15

# Calibration Constants
bnc_center_z = 9   # Modified to 9mm (15mm Top - 6mm Radius)
standoff_h = 6      # For M3x8 screws (8mm - 1.6mm PCB = ~6mm)





# Fix Rotation: User said 90 was upside down. Try -90.
lm_rotated_geom = imported_lm2596.moved(Rotation(-90, 0, 0))
lm_bbox = lm_rotated_geom.bounding_box()
lm_width_y = lm_bbox.size.Y
print(f"LM2596 Width (Y): {lm_width_y}")

# === Case Dimensions ===
# User request: Width of LM + Length of Sensor + 1cm margin
margin = 10
width = pcb_l + lm_width_y + margin + (wall_thickness * 2) 
# Let's add a bit more to be safe, say 20mm margin internal (User request: wider for LM2596)
internal_y_space = pcb_l + lm_width_y + 20
width = internal_y_space + (wall_thickness * 2) 

length = 140 # Increased to 140 per user request (was 130)
height = 35  

print(f"Calculated Case Width (Y): {width}")

# === PH Sensor: Direct STEP move (preserve colors) ===
# Rotate and center like TB6612
ph_sensor_rotated = imported_sensor.moved(Rotation(90, 0, 0))
ph_bbox = ph_sensor_rotated.bounding_box()

# Align: MinX at -pcb_l/2, Center Y, MinZ at 0
target_min_x = -pcb_l / 2
shift_x = target_min_x - ph_bbox.min.X
shift_y = -ph_bbox.center().Y
shift_z = -ph_bbox.min.Z

# Manual offset (from original code)
offset_x = -6 
offset_y = 2 
offset_z = -3 # Fix "Floating": Compensate for pins (~3mm) below PCB so PCB rests on standoff 

ph_sensor_centered = ph_sensor_rotated.moved(Location((shift_x + offset_x, shift_y + offset_y, shift_z + offset_z)))

# === LM2596: Direct STEP move (preserve colors) ===
lm_rotated = imported_lm2596.moved(Rotation(-90, 0, 0))
lm_bbox = lm_rotated.bounding_box()
# Center XY, MinZ at 0
lm2596_centered = lm_rotated.moved(Location((-lm_bbox.center().X, -lm_bbox.center().Y, -lm_bbox.min.Z)))

# === 3b. Define TB6612 Part & Mount ===
imported_tb6612 = import_step("designs/TB6612.step")
# 1. Flip upside down (Rotation 180 on X) so pins point UP
tb6612_flipped = imported_tb6612.moved(Rotation(180, 0, 0))

# 2. Center the part so we can build the mount around (0,0,0)
tb_bbox_raw = tb6612_flipped.bounding_box()
tb_center = tb_bbox_raw.center()
# Move part to origin and align bottom (lowest point) to Z=0
tb6612_centered = tb6612_flipped.moved(Location((-tb_center.X, -tb_center.Y, -tb_bbox_raw.min.Z)))

# Re-measure centered part
tb_bbox = tb6612_centered.bounding_box()
print(f"TB6612 Centered Size: {tb_bbox.size}")
tb_w = tb_bbox.size.X
tb_l = tb_bbox.size.Y
tb_h = 1.6 # PCB Thickness estimate (usually 1.6)

# Mount Logic: Snap-fit clip mount.
# ±X fixed guide walls + 2 cantilever snap clips on BOTH ±Y sides (4 clips total).
# PCB pressed in from above; both pairs snap simultaneously.
# To release: flex one clip with a screwdriver, tilt PCB out.
with BuildPart() as tb6612_mount:
    tb_standoff_z = 4       # clearance under PCB for bottom components
    wall_t  = 2.5           # fixed guide wall thickness (±X sides only)
    guide_h = 3.0           # ±X wall height above PCB top
    arm_t   = 1.1           # clip arm thickness (flex direction, keep thin)
    hook_d  = 0.65          # hook barb overhang into PCB area
    hook_h  = 1.5           # height of hook barb zone
    clr     = 0.30          # lateral fit clearance per side

    pcb_h   = tb_h
    pkt_w   = tb_w + 2 * clr
    pkt_l   = tb_l + 2 * clr
    pcb_top = tb_standoff_z + pcb_h
    arm_h   = pcb_top + hook_h + arm_t + hook_d

    # 1. Floor platform – extends arm_t beyond PCB on ±Y to anchor clip bases
    Box(pkt_w + 2 * wall_t, pkt_l + 2 * arm_t, tb_standoff_z,
        align=(Align.CENTER, Align.CENTER, Align.MIN))

    # 2. Side walls (±X only) – lateral guide, no front/back wall needed
    for sx in [1, -1]:
        with Locations((sx * (pkt_w / 2 + wall_t / 2), 0, tb_standoff_z)):
            Box(wall_t, pkt_l, pcb_h + guide_h,
                align=(Align.CENTER, Align.CENTER, Align.MIN))

    # 3. Snap clips on BOTH ±Y sides – 2 arms per side at ±pkt_w/4 in X
    #    For each side: iy = inner PCB edge, outward direction = sign
    #      arm body:  from iy outward by arm_t
    #      hook barb: from iy inward by hook_d (into PCB area) at pcb_top level
    #      45 deg lead-in: from outer-top down to hook tip top
    arm_w = min(4.0, pkt_w / 3)   # arm extrusion width, max 4 mm

    for sign, iy in [(-1, -pkt_l / 2), (+1, +pkt_l / 2)]:
        # sign = outward direction (+1 for +Y side, -1 for -Y side)
        for cx in [-pkt_w / 4, pkt_w / 4]:
            with BuildSketch(Plane.YZ.offset(cx)):
                with BuildLine():
                    Polyline(
                        (iy,                    0),
                        (iy + sign * arm_t,     0),              # outer base
                        (iy + sign * arm_t,     arm_h),          # outer top
                        (iy - sign * hook_d,    pcb_top + hook_h), # hook tip top
                        (iy - sign * hook_d,    pcb_top),        # hook tip bottom
                        (iy,                    pcb_top),        # inner at PCB surface
                        close=True,
                    )
                make_face()
            extrude(amount=arm_w, both=True)
        
# === 3c. Define CH224K Part & Mount ===
# CH224K USB-C PD trigger module: 31 x 20 x 4 mm. (Variable names kept as ctp_*
# from the previous CTP09 module — same mount recipe, just resized.)
ctp_w = 31
ctp_l = 20
ctp_h = 4
# Thin outer right-wall lip carrying the USB-C slot; the board's +X edge sinks into
# the wall and butts the back of this faceplate so the socket sits near-flush.
# (Validated with designs/ch224k_mount_test.py.)
ctp_faceplate_t = 0.6

with BuildPart() as ctp09_def:
    Box(ctp_w, ctp_l, ctp_h, align=(Align.CENTER, Align.CENTER, Align.MIN))

ctp09_centered = ctp09_def.part

# Mount Logic: Snap-fit clip mount for CH224K (USB-C PD trigger), USB-C facing +X.
# Geometry validated with designs/ch224k_mount_test.py (ABS print):
#   - clips clamp the bare PCB (~1mm) with a small gap so the board tilts in
#   - long+thick clip arms (arm_extra_h cantilever) so ABS still flexes
#   - clip pairs symmetric at ±ctp_w/4
#   - -X back wall has a central slot for the power wires
#   - +X (USB-C) side is open; the board edge butts the right-wall faceplate
with BuildPart() as ctp09_mount:
    ctp_standoff_z = 1.5
    clamp_t = 1.0
    wall_t  = 2.0
    guide_h = 1.0
    arm_t   = 1.8            # thick for ABS strength
    hook_d  = 0.5            # smaller barb = easier insert
    hook_h  = 1.0
    arm_extra_h = 5.0        # extra outer-arm height = longer cantilever → flexes
    clr     = 0.30
    clamp_gap = 0.5          # tilt-in clearance above PCB top
    clip_cx = [-ctp_w / 4, ctp_w / 4]   # symmetric clip pairs
    clip_w  = 6.0            # wide for strength
    wire_slot_w = 10.0       # power-wire opening in -X back wall
    back_wall_shift = 0.2    # -X wall nudged toward the PCB (snug fit)

    pkt_w   = ctp_w + 2 * clr
    pkt_l   = ctp_l + 2 * clr
    pcb_top = ctp_standoff_z + clamp_t
    hook_z  = pcb_top + clamp_gap
    arm_h   = hook_z + hook_h + arm_t + hook_d + arm_extra_h

    # 1. Floor platform (extends -X under the back wall, ±Y to anchor clip bases)
    with Locations((-wall_t / 2, 0, 0)):
        Box(pkt_w + wall_t, pkt_l + 2 * arm_t, ctp_standoff_z,
            align=(Align.CENTER, Align.CENTER, Align.MIN))

    # 2. -X back wall (inner stop) WITH a central wire slot (leaves two corner ribs)
    bw_x = -(pkt_w / 2 + wall_t / 2) + back_wall_shift
    with Locations((bw_x, 0, ctp_standoff_z)):
        Box(wall_t, pkt_l, clamp_t + guide_h,
            align=(Align.CENTER, Align.CENTER, Align.MIN))
    with Locations((bw_x, 0, ctp_standoff_z)):
        Box(wall_t + 2, wire_slot_w, clamp_t + guide_h + 2,
            align=(Align.CENTER, Align.CENTER, Align.MIN), mode=Mode.SUBTRACT)

    # 3. Snap clips on ±Y sides – symmetric clip pairs
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

# === 3d. Define GX12 Connector ===
imported_gx12 = import_step("designs/gx12-4p-m.stp")
# Check Size
gx_bbox = imported_gx12.bounding_box()
print(f"GX12 Size: {gx_bbox.size}")

# Center it
gx_center = gx_bbox.center()
gx12_def = imported_gx12.moved(Location((-gx_center.X, -gx_center.Y, -gx_center.Z)))
# Re-measure
gx_bbox = gx12_def.bounding_box()
print(f"GX12 Centered Size: {gx_bbox.size}")

# Define Cutout Dims
gx12_thread_d = 12
gx12_cutout_d = 12.2 # Tolerance

# === 4. Define 3.5" Screen Part ===
# Logic: Screen Rim sits ON TOP of Lid.
# Z=0 is the interface surface (Back of Rim/Front Plate).
screen_front_w = 94.5
screen_front_h = 62
screen_front_t = 4 # Bezel thickness
screen_back_w = 84.5 
screen_back_h = 52
screen_back_t = 8  # Rear housing thickness
screen_corner_radius = 4

# Mounting holes
# Assumption: Screws come from INSIDE case (Lid Bottom) -> UP into the Rim.
# Located in the Rim area.
# User constraints: Hole Spacing 84mm x 52mm (Centered).
mount_dx = 84 / 2 # 42
mount_dy = 52 / 2 # 26

# === 4. Define 3.5" Screen Part ===
# Logic: Screen Rim sits ON TOP of Lid.
# Z=0 is the interface surface (Back of Rim / Top of Lid).
screen_front_w = 94.5
screen_front_h = 62
screen_front_t = 4 # Bezel thickness
screen_back_w = 82.2 # User spec: Ronding naar 82.2
screen_back_h = 57.8 # User spec: Ronding naar 57.8
screen_back_t = 8  # Rear housing thickness
screen_corner_radius = 4

# Mounting holes (Centered on PCB)
# Spacing: 84mm (X), 52mm (Y)
# dx = 84/2 = 42
# dy = 52/2 = 26
mount_dx = 42 
mount_dy = 26

with BuildPart() as screen_def:
    # Front Plate (Bezel/Glass) - Sits ON TOP (Z=0 to +4)
    with Locations((0,0,0)):
        with BuildSketch():
            rect = Rectangle(screen_front_w, screen_front_h, align=(Align.CENTER, Align.CENTER))
            fillet(rect.vertices(), radius=screen_corner_radius)
        # Extrude logic: Create solid, set color, then add
        bezel = extrude(amount=screen_front_t, mode=Mode.PRIVATE)
        bezel.color = Color(0.2, 0.2, 0.2) # Dark Gray Bezel
        add(bezel)
        screen_def.bezel = bezel # Expose for export
        
    # Rear Body ("Bottom") - Falls IN (Z=0 to -8)
    with Locations((0,0, 0)):
        with BuildSketch():
            Rectangle(screen_back_w, screen_back_h, align=(Align.CENTER, Align.CENTER))
        rear = extrude(amount=-screen_back_t, mode=Mode.PRIVATE)
        rear.color = Color(0.2, 0.2, 0.2) # Match Bezel Color
        add(rear)
        screen_def.rear = rear # Expose for export
        
    # Active Area (Visual on Top Face)
    with Locations((0,0, screen_front_t)):
         # 'Box' adds implicitly. Since we can't use 'color' in Box() in this version?,
         # let's use the explicit object creation pattern too.
         lcd = Box(73.4, 49, 0.1, align=(Align.CENTER, Align.CENTER, Align.MIN), mode=Mode.PRIVATE)
         lcd.color = Color(0, 0, 0) # Black
         add(lcd)
         screen_def.lcd = lcd # Expose for export
    
    # Mounting Inserts (In the Rim, accessible from Back/Bottom)
    # Visual cylinders at Z=0 extending UP into the Rim
    # Use explicit loop to collect all solids for export
    inserts_solids = []
    with Locations((0,0, 0)):
        for loc in [(mount_dx, mount_dy), (mount_dx, -mount_dy), (-mount_dx, mount_dy), (-mount_dx, -mount_dy)]:
             with Locations(loc):
                 inserts_solids.append(Cylinder(radius=3/2, height=3.5, align=(Align.CENTER, Align.CENTER, Align.MIN), mode=Mode.PRIVATE))
    
    # Combine inserts into one Part for export
    if inserts_solids:
        inserts_compound = Compound(children=inserts_solids)
        inserts_compound.color = Color("Silver")
        add(inserts_compound)
        screen_def.inserts = inserts_compound
    else:
        screen_def.inserts = None # Safety

# === 5. Placements & Layout ===

# USB panel-mount: removed (see top of file). PD module now uses the right wall.

# GX12: "Next to" USB.
# User feedback: "Niet dwars". Needs rotation around insertion axis.
# GX12: Move to FRONT WALL (-Y), LEFT SIDE (X < 0)
# User: "Linkerzijwand" (Left of BNCs), "Space from TB6612".
# BNCs are at Front. TB6612 at X=-10.
# GX12 at X=-45 (1cm to the right of -55).
# Y aligned with Wall (-width/2). 
gx12_loc_y = -width/2 + wall_thickness # On the wall
# GX12 at X=-45 (1cm to the right of -55).
# User request: Shift 5mm Left. -45 -> -50.
gx12_loc_x = -50
gx12_pos = Location((gx12_loc_x, gx12_loc_y, 0))

# Rotation: 
# Part (GX12 STP): User said "Achterstevoren" with (0,0,180).
# This means (0,0,180) pointed it WRONG.
# So we try (0,0,0).
gx12_loc = gx12_pos * Rotation(0, 0, 0) # Visual (Flip 180)
# Cutout (Cylinder): Default Z. Becomes -Y with Rotation(90, 0, 0).
gx12_cutout_loc = gx12_pos * Rotation(90, 0, 0) 

# === Sensors ===
sensor_x_shift = 23 # +1 right to align BNC cutouts with the front-wall holes
sensor_space = 35 
sensor1_x = sensor_x_shift - sensor_space/2
sensor2_x = sensor_x_shift + sensor_space/2

sensor_y = (-width/2 + wall_thickness + 2) + pcb_l/2
sensor_z = -height/2 + wall_thickness + standoff_h + 0.5  # +0.5 up to align BNC cutouts
sensor_rot = Rotation(0, 0, -90) 

sensor1_loc = Location((sensor1_x, sensor_y, sensor_z)) * sensor_rot
sensor2_loc = Location((sensor2_x, sensor_y, sensor_z)) * sensor_rot

# === LM2596 ===
# Shift with sensors to keep right side organized.
# Was -15. Move to +5?
# Push as far left as possible: left inner wall + component half-width + 1 mm margin
lm2596_loc_x = -(length/2 - wall_thickness - lm_bbox.size.X/2 - 1)
lm2596_loc_y = (sensor_y + pcb_l/2) + 5 + lm_width_y/2
lm2596_loc = Location((lm2596_loc_x, lm2596_loc_y, -height/2 + wall_thickness + standoff_h))

# === Left Side Modules (TB6612 & CTP09) ===
# Move to -20 (Right Shift) based on feedback "Further towards sensors".
# Y-gap needed: Separate them (Tight).
# LM2596 ends at Y=-29.
# CTP09 at Y=-40. (Center). Max Y = -34.25. Gap to LM = 5mm. Safe.
# TB6612 at Y=-65. (Center). Max Y = -54. Gap to CTP = 14mm. Safe.
# Left Side Modules (TB6612 & CTP09)
# User request: Shift further LEFT to avoid sensor mount conflict.
# Previously -20. Move to -30.
left_module_x = -30

# TB6612 (Front-Left)
# User: Increase gap to CTP09, mounts were overlapping.
# Moving from -50 to -55 (toward front wall).
tb6612_loc_y = -55
tb6612_loc = Location((left_module_x, tb6612_loc_y, -height/2 + wall_thickness)) * Rotation(0, 0, 90) 

# PD Trigger — RIGHT WALL (+X), USB-C facing outward through the wall.
# User request: the PD module's USB-C must be reachable from outside so a cable
# plugs directly into the socket. It lies flat on the floor against the right wall.
# Board +X edge sinks into the right wall and butts the back of the faceplate, so
# the edge sits at (outer wall - faceplate_t). center_x = that - ctp_w/2.
ctp09_loc_y = 0  # same Y as the old panel-mount USB
ctp09_loc_x = (length/2 - ctp_faceplate_t) - ctp_w/2
ctp09_loc = Location((ctp09_loc_x, ctp09_loc_y, -height/2 + wall_thickness))

# Screen: On top of the Lid
# Screen Z=0 (Rim Bottom) aligns with Lid Top Surface.
# User requested shift UP (Y+) by 5mm.
screen_offset_y = 5
# Lid is placed at height/2. Thickness is 2.5 (Refined). Top is height/2 + 2.5.
lid_thickness = 2.5
lid_top_z = height/2 + lid_thickness
screen_loc = Location((0, 0, lid_top_z))  # Y=0 to match lid cutout and holes

# === 6. Create Case with Cutouts ===
# === 6. Create Case with Cutouts ===
with BuildPart() as case:
    # Outer Shell with Fillets (Radius 5)
    # Bottom at -height/2
    with BuildSketch(Plane.XY.offset(-height/2)):
        Rectangle(length, width)
        fillet(vertices(), radius=5)
    extrude(amount=height)
    
    # Hollow Inner
    with BuildSketch(Plane.XY.offset(-height/2 + wall_thickness)):
        Rectangle(length - 2*wall_thickness, width - 2*wall_thickness)
        fillet(vertices(), radius=5-wall_thickness)
    extrude(amount=height, mode=Mode.SUBTRACT)
    
    # Screw Posts
    post_r = 3.5 # 7mm boss
    post_hole_r = 1.4 # ~2.8mm hole for self-tapping M3
    post_dx = length/2 - post_r
    post_dy = width/2 - post_r
    post_z_bottom = -height/2 + wall_thickness
    
    # Add Posts (Fuse to corner)
    # We place them at the corners of the INTERNAL space.
    # Adjust for fillet? R=3.5 fits nicely in/near R=3 inner fillet.
    post_dx = length/2 - wall_thickness - post_r
    post_dy = width/2 - wall_thickness - post_r
    
    with Locations([(post_dx, post_dy), (post_dx, -post_dy), (-post_dx, post_dy), (-post_dx, -post_dy)]):
         with Locations((0,0, post_z_bottom)):
             Cylinder(radius=post_r, height=height-wall_thickness, align=(Align.CENTER, Align.CENTER, Align.MIN), mode=Mode.ADD)
             
    # Subtract Holes (From Top)
    with Locations([(post_dx, post_dy), (post_dx, -post_dy), (-post_dx, post_dy), (-post_dx, -post_dy)]):
         with Locations((0,0, height/2)):
             Cylinder(radius=post_hole_r, height=15, align=(Align.CENTER, Align.CENTER, Align.MAX), mode=Mode.SUBTRACT)
    
    # USB-C access (Right Wall, +X) — matches the validated mount test fixture.
    # A thin outer faceplate carries a connector-sized slot; behind it an inner
    # pocket (PCB-edge sized) lets the wide board front edge sink into the wall so
    # the socket sits near-flush, while the module (11.5mm) can't pass the slot.
    usbc_cut_w = 9.5     # Y width of the connector slot
    usbc_cut_h = 3.6     # Z height
    usbc_cut_fil = 0.8   # corner radius
    floor_inner_z = -height/2 + wall_thickness
    # socket centre = floor + standoff + 2.6 (socket sits 2.6mm above board underside)
    usbc_cut_z = floor_inner_z + ctp_standoff_z + 2.6
    wall_inner_x = length/2 - wall_thickness
    board_edge_x = ctp09_loc_x + ctp_w/2          # = length/2 - ctp_faceplate_t

    # Inner pocket: from a bit inside the case up to the faceplate back (board edge)
    pocket_w  = ctp_l + 1.0
    pocket_x0 = wall_inner_x - 1.0
    pocket_x1 = board_edge_x
    pocket_z0 = floor_inner_z + ctp_standoff_z
    pocket_z1 = usbc_cut_z + usbc_cut_h / 2 + 0.5
    with Locations(((pocket_x0 + pocket_x1) / 2, ctp09_loc_y, (pocket_z0 + pocket_z1) / 2)):
        Box(pocket_x1 - pocket_x0, pocket_w, pocket_z1 - pocket_z0,
            align=(Align.CENTER, Align.CENTER, Align.CENTER), mode=Mode.SUBTRACT)

    # Connector slot through the faceplate (rounded rect, centred on the socket)
    with BuildSketch(Plane.YZ.offset(length/2)):
        with Locations((ctp09_loc_y, usbc_cut_z)):
            usbc_rect = Rectangle(usbc_cut_w, usbc_cut_h)
            fillet(usbc_rect.vertices(), radius=usbc_cut_fil)
    extrude(amount=-(wall_thickness + 1), mode=Mode.SUBTRACT)

    # GX12 Cutout
    with Locations(gx12_cutout_loc):
        Cylinder(radius=gx12_cutout_d/2, height=20, align=(Align.CENTER, Align.CENTER, Align.CENTER), mode=Mode.SUBTRACT)

    # Sensor BNC Cutouts
    cutter_local_loc = Location((pcb_l/2, 0, bnc_center_z)) * Rotation(0, 90, 0)
    with Locations([sensor1_loc * cutter_local_loc, sensor2_loc * cutter_local_loc]):
        Cylinder(radius=bnc_d/2 + 0.5, height=30, align=(Align.CENTER, Align.CENTER, Align.CENTER), mode=Mode.SUBTRACT)

    # CTP09 USB-C Cutout (Left Wall) - REMOVED per user request
    # User: "er moet dus geen cutout in de wand"
    pass


# === 7. Create Lid ===
with BuildPart() as lid:
    # Lid Plate with Fillets
    with Locations((0,0,0)):
        with BuildSketch():
             Rectangle(length, width)
             fillet(vertices(), radius=5)
        extrude(amount=lid_thickness, dir=(0,0,1)) # Upwards from 0?
        # Original was: Box(..., align=(..., MIN)). Yes, 0 to +thick.
        # Wait, Line 458: Box(..., align=...MIN).
        # My extrude default is centered? No, extrude is from sketch plane.
        # Screen Cutout (Rectangle with Inverted Fillets for Screws)
    # Bezel: 94.5 x 62.
    # PCB/Body: ~82 x 57.
    # Mounting Holes: 84mm x 52mm apart.
    # Original Cutout: 82.4 x 57.6.
    # User Request: "Scheelt rondom 1mm of 2mm" -> Needs to be larger to fit screen body between posts.
    # Holes MUST stay at 84x52.
    # New Cutout: +2mm (1mm all around).
    screen_cutout_w = 84.4 
    screen_cutout_h = 59.6
    
    # Inverted Fillet Radius
    # Holes are at X=42. Cutout Edge at X=42.2.
    # Hole is INSIDE the cutout rectangle.
    # We need "Ears" to protrude back into the cutout to hold the screws.
    # Inverted Fillet with R=5 (Diameter 10).
    # Center of Fillet Arc will be at Corner + (-5, -5).
    # Corner of Rect is at (42.2, 29.8).
    # This might not align perfectly with hole at (42, 26).
    # BETTER STRATEGY: 
    # Define Cutout as Rectangle MINUS Circles at corners?
    # No, "Inverted Fillet" logic in build123d is `fillet(vertices)`? 
    # Wait, `fillet` makes convex corners (removes material).
    # If I want CONCAVE corners (adding material), I should use `chamfer`? No.
    # The previous code likely used a custom "make rectangle, then subtract circles" logic?
    # Let's check the code below.
    
    # Actually, simpler:
    # 1. Create Main Rect (84.4 x 59.6).
    # 2. Create 4 Circles at Mounting Holes (Radius = R_ear).
    # 3. Cutout = Rect MINUS Circles. (This leaves "Ears" in the Lid).
    
    mount_dx = 84
    mount_dy = 52
    
    # User Request: Flush Bezel was 94.5, but verifying confirmed 83.4 for body.
    # The large cutout block here was causing the "94.4" print result.
    # We REMOVED the duplicate cutout operation here.
    # The correct cutout is performed at the end of the script (line 618+).

    # === Recessed Mounting Brackets ===
    # These hang BELOW the lid to hold the screen.
    recess_depth = 5 
    bracket_thickness = 3
    # Z-level relative to Lid Base (0 to lid_thickness).
    # Lid Base is at 0 in this Local Part context?
    # No, this context builds `lid`. Z=0 is bottom of lid?
    # Extrude defaulted to Z=0 up?
    # Usually `lid` is built flat on XY then moved.
    # If `extrude(lid_thickness)`, Z ranges 0 to 2.5.
    # We want Top of Bracket at Z = 2.5 (Top) - 5 = -2.5.
    # So Bracket is from -5.5 to -2.5.
    
    bracket_z_top = lid_thickness - recess_depth # 2.5 - 5 = -2.5
    
    with BuildPart() as brackets:
        with Locations((0,0, bracket_z_top)):
            with Locations([(mount_dx/2, mount_dy/2), (mount_dx/2, -mount_dy/2),
                          (-mount_dx/2, mount_dy/2), (-mount_dx/2, -mount_dy/2)]):
                 # Mounting Tab Cylinder
                 Cylinder(radius=5, height=bracket_thickness, align=(Align.CENTER, Align.CENTER, Align.MAX))
                 # Align MAX means Top of cylinder is at bracket_z_top. Correct.
                 
                 # Hole for Screw
                 Cylinder(radius=1.8, height=bracket_thickness, align=(Align.CENTER, Align.CENTER, Align.MAX), mode=Mode.SUBTRACT)

    mount_dx = 84
    mount_dy = 52 # 26*2
    
    # User Request: Cutout fit for Body (~82.4 x 57.6).
    # BUT: Mounting holes are at 84mm spacing (outside the 82.4mm body).
    # To access screws, cutout must be wider than 84mm. 
    # Bezel covers up to 94.5mm.
    # User feedback: LCD doesn't fit. Widening to 91mm.
    flush_cutout_w = 91.0 
    flush_cutout_h = 57.6 + 1.0 # 58.6
    
    # NOTE: Cutout is done AFTER brackets so it cuts through pillar too

    # === Simple Mounting Brackets with Wall Connection ===
    # 4 circles at mount positions, connected to wall via small arm
    # - Circle Diameter: 4mm (R=2)
    # - Top at: 5mm below outer surface (Z = -2.5)
    # - Height: 3mm
    bracket_h = 3
    bracket_radius = 1.75  # User: slightly smaller islands for screen fit (diameter 3.5mm)
    bracket_z_top = lid_thickness - 5  # 5mm below outer surface = -2.5
    
    # Mount positions (mx, my) and corresponding cutout corners (cx, cy)
    mount_data = [
        (mount_dx/2, mount_dy/2, flush_cutout_w/2, flush_cutout_h/2),       # Top-right
        (mount_dx/2, -mount_dy/2, flush_cutout_w/2, -flush_cutout_h/2),     # Bottom-right
        (-mount_dx/2, mount_dy/2, -flush_cutout_w/2, flush_cutout_h/2),     # Top-left
        (-mount_dx/2, -mount_dy/2, -flush_cutout_w/2, -flush_cutout_h/2)    # Bottom-left
    ]
    
    for mx, my, cx, cy in mount_data:
        with BuildPart(mode=Mode.ADD):
            # Anchor position: 8mm outside the cutout edge
            anchor_r = 3.0  # User: smaller screw circles (R=3 -> 6mm diameter)
            anchor_offset = 8.0  # 8mm from cutout edge
            
            ax = cx + (anchor_offset if cx > 0 else -anchor_offset)
            ay = cy + (anchor_offset if cy > 0 else -anchor_offset)
            
            # NEW DESIGN: Extrude the FULL ARM SHAPE from lid (Z=0) down to arm bottom
            # This ensures the entire arm is solidly attached to the lid, not just a thin pillar.
            # User feedback: old design broke during support removal.
            
            # Total extrusion: from Z=0 down to bracket bottom
            # bracket_z_top = -2.5 (5mm below lid top, but lid is 2.5mm thick, so 2.5mm below lid bottom)
            # bracket_h = 3mm (arm thickness)
            # Total depth from Z=0: |bracket_z_top| + bracket_h = 2.5 + 3 = 5.5mm
            full_arm_depth = abs(bracket_z_top) + bracket_h
            
            # Create the full arm shape (hull from ear to anchor)
            with BuildSketch(Plane.XY.offset(0)):
                with Locations((mx, my)):
                    Circle(bracket_radius)  # Ear R=2 (4mm diameter)
                with Locations((ax, ay)):
                    Circle(anchor_r)  # Anchor R=3 (6mm diameter)
                make_hull()
            extrude(amount=-full_arm_depth)  # Extrude entire arm from lid bottom
            
            # Screw hole through the mounting circle
            with Locations((mx, my, 0)):
                Cylinder(radius=1.3, height=full_arm_depth + 1, 
                        align=(Align.CENTER, Align.CENTER, Align.MAX), mode=Mode.SUBTRACT)

    # === CUTOUT (Final Depth) ===
    # Cut through Lid (2.5mm) AND Pillar space (2.5mm)
    # Stops exactly at bracket top (Z=-2.5), leaving ears intact
    cutout_depth = 5.0 
    with BuildSketch(Plane.XY.offset(lid_thickness)):
        Rectangle(flush_cutout_w, flush_cutout_h, align=(Align.CENTER, Align.CENTER))
    extrude(amount=-cutout_depth, mode=Mode.SUBTRACT)
    # Lid Corner Screw Holes (M3 Countersunk) (Rest of file...)
    post_r = 3.5
    post_dx = length/2 - wall_thickness - post_r
    post_dy = width/2 - wall_thickness - post_r
    
    with Locations([(post_dx, post_dy), (post_dx, -post_dy), (-post_dx, post_dy), (-post_dx, -post_dy)]):
         Cylinder(radius=1.6, height=lid_thickness, align=(Align.CENTER, Align.CENTER, Align.MIN), mode=Mode.SUBTRACT)
         # Countersink (Top)
         with Locations((0,0, lid_thickness)):
             Cone(bottom_radius=1.6, top_radius=3, height=1.6, align=(Align.CENTER, Align.CENTER, Align.MAX), mode=Mode.SUBTRACT)

# === 7b. Define Standoffs & Mounts (Sensors + LM2596) ===
# Sensor Standoffs (PH-4502C)
# Analysis showed Holes are shifted relative to BBox Center due to BNC stickout.
# Hole Pattern Center is shifted approx -5.5mm in X relative to BBox Center.
# Pattern is approx 36mm x 26mm.
with BuildPart() as sensor_mount:
    # 36x26 pattern
    so_dx = 36/2
    so_dy = 26/2
    with Locations([(so_dx, so_dy), (so_dx, -so_dy), (-so_dx, so_dy), (-so_dx, -so_dy)]):
         Cylinder(radius=2, height=standoff_h, align=(Align.CENTER, Align.CENTER, Align.MIN))
         with Locations((0,0, standoff_h)):
             Cylinder(radius=1.2, height=2, align=(Align.CENTER, Align.CENTER, Align.MIN))

# LM2596 Standoffs
# User reported mismatch (likely asymmetric holes or interference).
# Standard generic module often has 2 diagonal mounting holes.
# Assuming Diagonal (Top-Left / Bottom-Right) or similar.
# Let's try 2 diagonal holes with significant margin to avoid components.
# Module size ~43x21.
# Holes often at In- and Out- corners.
with BuildPart() as lm_mount:
    lm_dx = (43/2) - 4 # 4mm from edge
    lm_dy = (21/2) - 4 # 4mm from edge
    # Diagonal pair: (+X, -Y) and (-X, +Y)?
    # Or (-X, -Y) and (+X, +Y).
    # Let's add 2 diagonal.
    with Locations([(lm_dx, -lm_dy), (-lm_dx, lm_dy)]):
         Cylinder(radius=2, height=standoff_h, align=(Align.CENTER, Align.CENTER, Align.MIN))
         with Locations((0,0, standoff_h)):
             Cylinder(radius=1.2, height=2, align=(Align.CENTER, Align.CENTER, Align.MIN))

# === 8. Ghosts & Show ===
sensor1_ghost = ph_sensor_centered.moved(sensor1_loc)
sensor2_ghost = ph_sensor_centered.moved(sensor2_loc)
lm2596_ghost = lm2596_centered.moved(lm2596_loc)
screen_ghost = screen_def.part.moved(screen_loc) # Screen is built from primitives, no STEP colors
lid_part = lid.part.moved(Location((0,0, height/2))) # Lid Base at top of case

# TB6612 Ghost & Mount
# Use the actual standoff height from each mount so the ghost sits on the floor.
tb6612_ghost = tb6612_centered.moved(tb6612_loc * Location((0,0, tb_standoff_z)))
ts6612_mount_part = tb6612_mount.part.moved(tb6612_loc)

# CH224K Ghost & Mount
ctp09_ghost = ctp09_centered.moved(ctp09_loc * Location((0,0, ctp_standoff_z)))
ctp09_mount_part = ctp09_mount.part.moved(ctp09_loc)

print(f"DEBUG: CH224K Loc: {ctp09_loc}")
print(f"DEBUG: CH224K Ghost Volume: {ctp09_ghost.volume}")
print(f"DEBUG: CH224K Mount Volume: {ctp09_mount_part.volume}")

# GX12 Ghost
gx12_ghost = gx12_def.moved(gx12_loc)

# === Dynamic Standoff Generation ===
# Extract holes from ALREADY POSITIONED ghosts (world coordinates)
# Then build standoffs on case floor at those X,Y locations

floor_z = -height/2 + wall_thickness

def extract_holes_from_part(part, min_r=1.3, max_r=1.8):
    """Extract hole centers from an already-positioned part."""
    edges = part.edges().filter_by(GeomType.CIRCLE)
    candidates = []
    for e in edges:
        if min_r <= e.radius <= max_r:
            candidates.append(e.arc_center)
    # Deduplicate
    unique = []
    for c in candidates:
        if not any((u - c).length < 1.0 for u in unique):
            unique.append(c)
    return unique

def select_corner_holes(holes, count=4):
    """Select holes closest to bbox corners (mounting holes are typically in corners)."""
    if len(holes) <= count:
        return holes
    if not holes:
        return []
    
    # Get bounding box of hole positions (ignore Z)
    xs = [h.X for h in holes]
    ys = [h.Y for h in holes]
    min_x, max_x = min(xs), max(xs)
    min_y, max_y = min(ys), max(ys)
    
    # Define corners (using average Z from holes)
    avg_z = sum(h.Z for h in holes) / len(holes)
    corners = [
        Vector(min_x, min_y, avg_z),  # BL
        Vector(max_x, min_y, avg_z),  # BR
        Vector(min_x, max_y, avg_z),  # TL
        Vector(max_x, max_y, avg_z),  # TR
    ]
    
    selected = []
    used = set()
    for c in corners[:count]:
        # Find closest hole not already used
        best_dist = float('inf')
        best_h = None
        for i, h in enumerate(holes):
            if i in used:
                continue
            dist = ((h.X - c.X)**2 + (h.Y - c.Y)**2)**0.5
            if dist < best_dist:
                best_dist = dist
                best_h = (i, h)
        if best_h:
            used.add(best_h[0])
            selected.append(best_h[1])
    
    return selected

def build_standoffs_on_floor(hole_points, floor_z, standoff_height=2.0):
    """Build standoffs at (X, Y, floor_z) for each hole point."""
    if not hole_points:
        return None
    with BuildPart() as so:
        for p in hole_points:
            with Locations((p.X, p.Y, floor_z)):
                Cylinder(radius=3.5, height=standoff_height, align=(Align.CENTER, Align.CENTER, Align.MIN))
                # M3 Screw Hole (2.5mm tap drill, say 1.25 radius)
                Cylinder(radius=1.25, height=standoff_height + 1, align=(Align.CENTER, Align.CENTER, Align.MIN), mode=Mode.SUBTRACT)
    return so.part

# Extract from Sensor Ghosts
# M3 mounting holes are typically 3.2mm diameter (R=1.6). Use tight filter.
sensor1_holes_raw = extract_holes_from_part(sensor1_ghost, min_r=1.5, max_r=1.7)
sensor2_holes_raw = extract_holes_from_part(sensor2_ghost, min_r=1.5, max_r=1.7)
# LM2596: Same filter
lm2596_holes_raw = extract_holes_from_part(lm2596_ghost, min_r=1.5, max_r=1.7)

# Select only corner holes (4 for sensors, 2 diagonal for LM2596)
sensor1_holes = select_corner_holes(sensor1_holes_raw, count=4)
sensor2_holes = select_corner_holes(sensor2_holes_raw, count=4)
lm2596_holes = select_corner_holes(lm2596_holes_raw, count=4)

# print(f"Sensor1 Holes: {len(sensor1_holes_raw)} -> {len(sensor1_holes)} corners")
# print(f"Sensor2 Holes: {len(sensor2_holes_raw)} -> {len(sensor2_holes)} corners")
# print(f"LM2596 Holes: {len(lm2596_holes_raw)} -> {len(lm2596_holes)} corners")

# Build Standoffs
sensor1_mount_part = build_standoffs_on_floor(sensor1_holes, floor_z, standoff_h)
sensor2_mount_part = build_standoffs_on_floor(sensor2_holes, floor_z, standoff_h)
lm2596_mount_part = build_standoffs_on_floor(lm2596_holes, floor_z, standoff_h)

# Add Standoffs to Case
parts_to_add = [case.part, ts6612_mount_part, ctp09_mount_part]
if sensor1_mount_part: parts_to_add.append(sensor1_mount_part)
if sensor2_mount_part: parts_to_add.append(sensor2_mount_part)
if lm2596_mount_part: parts_to_add.append(lm2596_mount_part)

start_case_fuse = Compound(parts_to_add)



# === Wall Mounting Keyholes ===
# Subtract from Case Bottom
with BuildPart() as result_case:
    add(start_case_fuse)
    
    # Keyholes
    # Back of case (Bottom). Z = -height/2 + wall_thickness (Inner floor).
    # We want to cut through the wall_thickness (2mm).
    # Locations: Along X axis (since Length is 130).
    # Spaced 60mm apart? (+/- 30).
    keyhole_x = 30
    
    with Locations([(keyhole_x, 0, -height/2), (-keyhole_x, 0, -height/2)]):
         # Create Keyhole Shape
         # Large hole (Head) D=8mm.
         # Slot (Shaft) D=4mm.
         # Orientation: Slot points UP (Y+)? Or X+?
         # "Ophangen" implies Vertical (Y axis is Width, X is Length).
         # If hung horizontally (Length horizontal), slots should point UP (Y+).
         # Let's assume standard hanging.
         
         # Keyholes on Bottom Floor (Manual Geometry)
         # Floor is at Z = -height/2. Thickness 2mm.
         # We want to cut through it.
         # Sketch on Plane.XY at Z = -height/2 - 1 (Below floor)
         
         with BuildSketch(Plane.XY.offset(-height/2 - 1)) as keyhole_sk:
             with Locations([(keyhole_x, 0), (-keyhole_x, 0)]):
                 # Keyhole Shape
                 Circle(5/2)
                 with Locations((0, 8)):
                     Circle(2.5/2)
                 with Locations((0, 4)):
                     Rectangle(2.5, 8)
         
         # Extrude UP through floor
         extrude(amount=height + 5, mode=Mode.SUBTRACT) # Cut through entire bottom

start_case_fuse = result_case.part

# Export lines moved to end of script to capture all modifications (like Text)

print("Design generated: designs/case.stl, designs/lid.stl, designs/case.step, designs/lid.step")

# Export full assembly STEP for Fusion color verification
# Assign labels and colors for export
# Wrap in Part() to ensure exporter handles them correctly (avoid Unknown Compound type warning)
# Note: start_case_fuse and lid_part are likely already Parts, but wrapping doesn't hurt or we can check via type
# For imported STEPs (Compound), we MUST wrap in Part given the exporter logic

# Assign labels and colors for export
# Note: Manually coloring imported parts because internal STEP colors are lost in re-export

case_export = start_case_fuse if isinstance(start_case_fuse, Part) else Part(start_case_fuse)
case_export.label = "Case"
case_export.color = Color(0.2, 0.2, 0.2) # Dark Grey

lid_export = lid_part if isinstance(lid_part, Part) else Part(lid_part)
lid_export.label = "Lid" 
lid_export.color = Color(0.0, 1.0, 0.0) # Neon Green

# For ghosts, assign representative colors
# Screen Export: Exploded for Colors
# screen_export = Part(screen_ghost)
# screen_export.label = "Screen"
# screen_export.color = Color("Black") # Removed to allow multi-color screen (Bezel/LCD)

# Bezel
# RE-CREATE Geometry Fresh to ensure color works (Abandoning screen_def.bezel)
with BuildPart() as bezel_fresh:
    with BuildSketch():
        rect = Rectangle(screen_front_w, screen_front_h, align=(Align.CENTER, Align.CENTER))
        fillet(rect.vertices(), radius=screen_corner_radius)
    extrude(amount=screen_front_t)
    
screen_bezel_export = Part(bezel_fresh.part.moved(screen_loc))
screen_bezel_export.label = "Screen Bezel"
screen_bezel_export.color = Color(0.2, 0.2, 0.2) # Dark Gray

# Rear Body
# Re-create fresh
with BuildPart() as rear_fresh:
    with BuildSketch():
        Rectangle(screen_back_w, screen_back_h, align=(Align.CENTER, Align.CENTER))
    extrude(amount=-screen_back_t)

screen_rear_export = Part(rear_fresh.part.moved(screen_loc))
screen_rear_export.label = "Screen Rear"
screen_rear_export.color = Color(0.2, 0.2, 0.2) # Dark Gray

screen_rear_export.color = Color(0.2, 0.2, 0.2) # Dark Gray

# LCD
# Re-create fresh
with BuildPart() as lcd_fresh:
    with Locations((0,0, screen_front_t)):
         Box(73.4, 49, 0.1, align=(Align.CENTER, Align.CENTER, Align.MIN))

screen_lcd_export = Part(lcd_fresh.part.moved(screen_loc))
screen_lcd_export.label = "Screen LCD"
screen_lcd_export.color = Color(0, 0, 0) # Black

# Text "Pura" (On Lid Surface)
# Save Clean Lid for dimension verification (User Request)
lid_no_text = lid_export

try:
    # 1. Font Selection: System Font "Arial Black"
    t_sketch = Text("Pura", font_size=25, font="Arial Black")
    
    # 2. Thicken (Extra Bold)
    t_thick = offset(t_sketch, amount=0.2)
    
    # 3. Extrude
    # User Request: "minimaal een paar layers dik" (at least a few layers thick)
    # 0.6mm (3 layers) might be thin. Let's go to 1.0mm (5 layers) for full opacity.
    t_part = extrude(t_thick, amount=1.0) # 1.0mm thickness
    
    # 4. Position FLUSH with Lid Surface (Inlay)
    # Z = lid_top_z - 1.0 (Top face is at lid_top_z)
    text_flush_loc = Location((0, -50, lid_top_z - 1.0))
    text_geom = t_part.moved(text_flush_loc)
    
    screen_text_export = Part(text_geom)
    screen_text_export.label = "Lid Text"
    screen_text_export.color = Color(0.8, 0.8, 0.8) # Gray (Case Color)
    
    # 5. SUBTRACT from Lid to create Inlay capability
    # This allows multi-color printing face-down
    print("DEBUG: Subtracting Text from Lid for Flush Inlay...")
    # lid_export has the original lid. We subtract the text geom.
    # Error Fix: lid_export is already a Part object, it has no .part attribute.
    # text_geom is also a Part object.
    # Direct subtraction works in build123d.
    lid_cut = lid_export - text_geom
    lid_export = Part(lid_cut) # Create fresh Part from result? 
    # Actually result of (-) is a Part.
    # So lid_export = lid_cut is enough?
    # But let's wrap in Part() to be safe or just assign.
    # lid_cut is a Part.
    lid_export = lid_cut
    lid_export.label = "Lid"
    lid_export.color = Color(0, 1, 0) # Maintain Key Green
    
    # Update main assembly to use new lid_export if it was already added?
    # assembly = Compound(...) is defined later at 836?
    # Line 836 (in original file, inferred) defines assembly.
    # We are at line 813. assembly is defined AFTER this block?
    # Checking file viewing... Yes, assembly is at 842.
    # So updating lid_export here works perfectly.
    
except Exception as e:
    print(f"Warning: Text Generation Failed: {e}")
    screen_text_export = Part(Cylinder(radius=0.1, height=0.1)) # Empty placeholder
    
except Exception as e:
    print(f"Warning: Text Generation Failed: {e}")
    screen_text_export = Part(Cylinder(radius=0.1, height=0.1)) # Empty placeholder

# Inserts
if screen_def.inserts:
    # Inserts is already a Compound
    screen_inserts_export = Part(screen_def.inserts.moved(screen_loc))
    screen_inserts_export.label = "Screen Inserts"
    screen_inserts_export.color = Color(0.75, 0.75, 0.75) # Silver
else:
    # Fallback if inserts failed
    screen_inserts_export = Part()

ctp09_export = Part(ctp09_ghost)
ctp09_export.label = "CH224K"
ctp09_export.color = Color("Red") # PCB Color

sensor1_export = Part(sensor1_ghost)
sensor1_export.label = "Sensor1"
sensor1_export.color = Color("Green") # PCB Color

sensor2_export = Part(sensor2_ghost)
sensor2_export.label = "Sensor2"
sensor2_export.color = Color("Green") # PCB Color

lm2596_export = Part(lm2596_ghost)
lm2596_export.label = "LM2596"
lm2596_export.color = Color("Blue") # PCB Color

tb6612_export = Part(tb6612_ghost)
tb6612_export.label = "TB6612"
# Original STEP colors preserved (user request)

gx12_export = Part(gx12_ghost)
gx12_export.label = "GX12"
gx12_export.color = Color("Silver")

# === Final Exports ===
print("Exporting STLs and STEPs...")

# 1. Main Case
# 1. Main Case
# case_export is a Part object (build123d.topology.Part) which is valid for export.
export_stl(case_export, "designs/case.stl")
export_step(case_export, "designs/case.step")

# 2. Lid (With Flush Text Cuts)
export_stl(lid_export, "designs/lid.stl")
export_step(lid_export, "designs/lid.step")

# 2b. Lid (Clean / No Text) - For Verification
export_stl(lid_no_text, "designs/lid_no_text.stl")
export_step(lid_no_text, "designs/lid_no_text.step")
    
# 3. Lid Text (The Flush Insert)
export_stl(screen_text_export, "designs/lid_text.stl")
export_step(screen_text_export, "designs/lid_text.step")

print("Exports Complete: designs/case.stl, designs/lid.stl, designs/lid_text.stl")


assembly = Compound(children=[case_export, lid_export, ctp09_export, sensor1_export, sensor2_export, lm2596_export,
                            screen_bezel_export, screen_rear_export, screen_lcd_export, screen_text_export, screen_inserts_export, 
                            tb6612_export, gx12_export])
assembly.label = "Pura_Assembly_V7"
export_step(assembly, "designs/full_assembly_v7.step")
print("Full assembly exported: designs/full_assembly_v7.step")

# All Objects - Names and alphas for visualization
# Using GHOSTS for imported parts (to keep native STEP colors)
show(case_export, lid_export, ctp09_ghost, sensor1_ghost, sensor2_ghost, lm2596_ghost,
     screen_bezel_export, screen_rear_export, screen_lcd_export, screen_text_export, screen_inserts_export,
     tb6612_ghost, gx12_ghost,
     names=["Case", "Lid", "CH224K", "Sensor1", "Sensor2", "LM2596",
            "Screen Bezel", "Screen Rear", "Screen LCD", "Screen Text", "Screen Inserts",
            "TB6612", "GX12"],
     alphas=[0.5, 0.5, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1])

# DEBUG EXPORT CTP09
ctp_debug_assembly = Compound(children=[ctp09_ghost, ctp09_mount_part])
ctp_debug_assembly.label = "CH224K_Debug"
export_step(ctp_debug_assembly, "designs/ctp09_debug.step")
print("Debug export: designs/ctp09_debug.step")
