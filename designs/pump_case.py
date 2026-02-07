"""
Pump Case Design - For 2x Grothen G528 Peristaltic Pumps
Style: Vertical mounting (Top-Mount).
- Motor hangs INSIDE the case (Z-).
- Pump Head sits ON TOP of the Lid (Z+).
- Lid is the mounting plate.
"""

from build123d import *
from ocp_vscode import show, set_port, set_defaults

# === 1. Import & Orient Pump ===
imported_pump = import_step("designs/Grothen-3x5-Peristaltic-Pump.STEP")

# Orientation Analysis:
# Raw BBox: 54.3 (X) x 46.5 (Y) x 71.7 (Z)
# Deduce: X=Width (48.5 holes), Y=Axis (Length), Z=Plate Height.
# Goal: Z-Axis Vertical (Length). Head DOWN (Z-), Motor UP (Z+).
# Action: Rotate +90 around X (Y -> -Z).

pump_rot = imported_pump.moved(Rotation(90, 0, 0))

bbox = pump_rot.bounding_box()
# print(f"Rotated Pump Size: {bbox.size}")
# Expect: X=54, Y=71, Z=46.

# === Vertical Alignment ===
# We want the "Mounting Plane" at Z=0.
# The Head is at Z- (Bottom). Motor at Z+ (Top).
# Head stickout ~ 12.5mm (Length of head below plate).
# We shift Z so that (MinZ + 12.5) == 0.

head_stickout = 12.5
z_shift = -(bbox.min.Z + head_stickout)

# Center X and Y centered.
pump_loc = Location((-bbox.center().X, -bbox.center().Y, z_shift))
pump_oriented = pump_rot.moved(pump_loc)

# Measure oriented
bbox_or = pump_oriented.bounding_box()
print(f"Oriented Z Range: {bbox_or.min.Z:.1f} to {bbox_or.max.Z:.1f}")
# Top should be Approx +(Total-12.5). Bottom should be -12.5.

# === 2. Auto-Detect Mounting Holes & Alignment ===
def extract_holes_from_part(part, min_r=1.5, max_r=2.5):
    """Extract hole centers from a part."""
    edges = part.edges().filter_by(GeomType.CIRCLE)
    candidates = []
    for e in edges:
        if min_r <= e.radius <= max_r:
            candidates.append(e.arc_center)
    unique = []
    for c in candidates:
        if not any((u - c).length < 1.0 for u in unique):
            unique.append(c)
    return unique

# Find mounting holes on the oriented pump
# Mounting holes on G528 are likely ~3.5-4mm dia (R=1.75-2.0). 
# Let's scan for them.
mounting_holes = extract_holes_from_part(pump_oriented, min_r=1.5, max_r=2.5)
print(f"Detected {len(mounting_holes)} mounting holes: {mounting_holes}")

# We expect 2 holes.
# Finding the Z-level of these holes helps us align the "Flange".
if len(mounting_holes) >= 2:
    # Assume these are the mounting holes.
    # Align their Z to Z=0 (Top of Lid).
    # Current Z of holes:
    hole_z = mounting_holes[0].Z
    print(f"Current Hole Z: {hole_z}")
    
    # Validation: user said "Black rim flush with lid". 
    # Usually holes are IN the rim. So Rim Z == Hole Z.
    # We want Rim at Z=0.
    # So we shift pump by -hole_z.
    z_correction = -hole_z
    pump_oriented = pump_oriented.moved(Location((0,0,z_correction)))
    
    # Update holes to new positions
    mounting_holes = [h + Vector(0,0,z_correction) for h in mounting_holes]
    print(f"Corrected Hole Z: {mounting_holes[0].Z}")

# Recalculate BBox after alignment
bbox_or = pump_oriented.bounding_box()
print(f"Aligned Z Range: {bbox_or.min.Z:.1f} to {bbox_or.max.Z:.1f}")

# === 3. Dimensions based on Aligned Pump ===
pump_count = 2
spacing = 20 # Spacing between pump centers (X)

# Pump Center X offset based on holes?
# The holes are likely symmetric around X=0.
# Let's check hole X positions.
if mounting_holes:
    hx = [h.X for h in mounting_holes]
    mount_dist = max(hx) - min(hx)
    print(f"Mounting Distance: {mount_dist:.1f}mm")

# Case Dimensions
wall_thick = 2.5
lid_thick = 3.0
# Depth of bucket determined by part of pump BELOW Z=0 (Head).
bucket_depth = abs(bbox_or.min.Z) + 5 # +Clearance
if bucket_depth < 20: bucket_depth = 20 # Minimum depth for stability

# Width/Length
# Pump Body Width (X)
p_width = bbox_or.size.X
p_length = bbox_or.size.Y 

# Case Layout
dist = p_width + 10 # Spacing
case_len = (pump_count * dist) + 5 # X - Tighter spacing
case_wid = p_length + 3 # Y - Slightly overlapping detected size? 
# Wait, p_length is BBox Y. If Bbox is loose, we can go smaller.
# But if Bbox is tight, we clip.
# Let's trust Bbox Y (71mm) is the real size.
# Case needs to enclose it.
# User wanted "Narrower".
# Maybe the 71mm includes protruding parts that can stick out?
# Or maybe the "Body" is smaller than 71?
# Let's set it to p_length + 2 to be safe for now.

if case_wid < mount_dist + 10: case_wid = mount_dist + 10

case_h = bucket_depth + wall_thick

# print(f"Case: {case_len:.1f}x{case_wid:.1f}x{case_h:.1f}")

# === 4. Build Lid & Case ===
# Lid at Z=0 (Top surface). Thickness goes down (Z-).
with BuildPart() as lid:
    with BuildSketch():
        Rectangle(case_len, case_wid)
        fillet(vertices(), radius=5)
    extrude(amount=-lid_thick)
    
    # Cutouts for each Pump
    # 1. Motor Hole (Center)
    # 2. Mounting Screw Holes (from detected points)
    
    # Motor Hole: Large hole in center of mounting pattern.
    # Center is avg of mounting holes?
    # Motor Hole: Large hole in center of mounting pattern.
    # Center is avg of mounting holes?
    # Filter for Mounting Pair (approx 48.5mm spacing)
    valid_pair = None
    if len(mounting_holes) >= 2:
        # Search for a pair with dist ~ 48.5 +/- 1.0
        found = False
        from itertools import combinations
        for h1, h2 in combinations(mounting_holes, 2):
            dist_h = (h1 - h2).length
            if 48.0 <= dist_h <= 49.0:
                valid_pair = (h1, h2)
                found = True
                print(f"Found Matching Mounting Pair! Dist={dist_h:.2f}mm")
                break
        
        if not found:
             print("Warning: Could not find hole pair with ~48.5mm spacing. Using First 2.")
             valid_pair = (mounting_holes[0], mounting_holes[1])

    if valid_pair:
        print("Generating Lid Cutouts for Motor and Screws...")
        h1, h2 = valid_pair
        
        center_x = (h1.X + h2.X)/2
        center_y = (h1.Y + h2.Y)/2
        center = Vector(center_x, center_y, 0)
        
        # Determine locations for Pump 1 and Pump 2
        for loc in [Location((-dist/2, 0, 0)), Location((dist/2, 0, 0))]:
            with Locations(loc):
                 # Screw Holes (Exactly 2)
                 with Locations([(h1.X, h1.Y), (h2.X, h2.Y)]):
                      Cylinder(radius=2, height=lid_thick*2, align=(Align.CENTER, Align.CENTER, Align.CENTER), mode=Mode.SUBTRACT)
                 
                 # Motor Hole (Center)
                 # Sits between holes.
                 # Diameter ~ 33mm (Motor is 32mm).
                 # User requested "margin" (paar mm groter).
                 # Let's align to 35mm (1.5mm gap all around).
                 with Locations((center.X, center.Y, 0)):
                     Cylinder(radius=35/2, height=lid_thick*2, align=(Align.CENTER, Align.CENTER, Align.CENTER), mode=Mode.SUBTRACT)

# Case Bucket
with BuildPart() as bucket:
    # Outer Shell with Fillets
    # Bottom of Head (MinZ) is at -bucket_depth.
    # Lid Bottom is at -lid_thick.
    
    with BuildSketch(Plane.XY.offset(-lid_thick - bucket_depth)):
        Rectangle(case_len, case_wid)
        fillet(vertices(), radius=5)
    extrude(amount=bucket_depth)
    
    # Hollow Inner (Top Open)
    with BuildSketch(Plane.XY.offset(-lid_thick - bucket_depth + wall_thick)):
        Rectangle(case_len - 2*wall_thick, case_wid - 2*wall_thick)
        fillet(vertices(), radius=5-wall_thick)
    extrude(amount=bucket_depth, mode=Mode.SUBTRACT) # Cut all way up
    
    # Screw posts...
    with Locations([(case_len/2 - 4, case_wid/2 - 4, -lid_thick - 5),
                   (-case_len/2 + 4, case_wid/2 - 4, -lid_thick - 5),
                   (case_len/2 - 4, -case_wid/2 + 4, -lid_thick - 5),
                   (-case_len/2 + 4, -case_wid/2 + 4, -lid_thick - 5)]):
         Cylinder(radius=3, height=10, align=(Align.CENTER, Align.CENTER, Align.CENTER))
         Cylinder(radius=1.5, height=12, align=(Align.CENTER, Align.CENTER, Align.CENTER), mode=Mode.SUBTRACT)

    # Keyholes on BOTTOM FLOOR (XY Plane at Z = -lid_thick - bucket_depth)
    # Unit mounts flat. Y+ is Top (against wall).
    # Keyholes move towards Top (Y+).
    # Slot points UP (Screw enters bottom, slides UP relative to case? No.)
    # Gravity pulls Case DOWN. Screw stays put.
    # Relative to Case, Screw moves UP. So Slot points UP (Y+).
    
    with BuildSketch(Plane.XY.offset(-lid_thick - bucket_depth)) as floor_sk:
         # Shift Y to near top (case_wid/2 - 15)
         with Locations([(-dist/2, case_wid/2 - 15), (dist/2, case_wid/2 - 15)]): 
             # Keyhole Shape (Longer Slot)
             # Entry Hole (Bottom)
             Circle(5/2)
             # Slot End (Top) - Increased length to 8mm
             with Locations((0, 8)): 
                 Circle(2.5/2)
             # Connector (Rectangle)
             # Center of Rect is at (0, 4). Height 8. Width 2.5.
             with Locations((0, 4)):
                 Rectangle(2.5, 8)
    extrude(amount=wall_thick*2, mode=Mode.SUBTRACT)

    # GX12 Connector (Right Wall -> X+)
    # Right Wall is YZ Plane at X = case_len/2.
    gx12_z = -lid_thick - bucket_depth/2
    right_face_plane = Plane.YZ.offset(case_len/2)
    with BuildSketch(right_face_plane):
        with Locations((0, gx12_z)): 
             Circle(12/2)
    extrude(amount=-wall_thick*2, mode=Mode.SUBTRACT)

    # Tube Cutouts on SIDE WALL (Front/Bottom Edge -> Y-min)
    # If mounted on wall (Floor vertical), tubes exit downwards (Y-min).
    # "Slangetjes vrij hangen".
    
    front_face_plane = Plane.XZ.offset(-case_wid/2) # Y-min face
    with BuildSketch(front_face_plane) as tube_sk:
        with Locations([(-dist/2, -lid_thick - bucket_depth/2), (dist/2, -lid_thick - bucket_depth/2)]):
             # Slot for tubes. 
             Rectangle(20, 15)
             fillet(vertices(), radius=5)
    extrude(amount=-wall_thick*2, mode=Mode.SUBTRACT)
         
    # Screw posts...
    with Locations([(case_len/2 - 4, case_wid/2 - 4, -lid_thick - 5),
                   (-case_len/2 + 4, case_wid/2 - 4, -lid_thick - 5),
                   (case_len/2 - 4, -case_wid/2 + 4, -lid_thick - 5),
                   (-case_len/2 + 4, -case_wid/2 + 4, -lid_thick - 5)]):
         Cylinder(radius=3, height=10, align=(Align.CENTER, Align.CENTER, Align.CENTER))
         Cylinder(radius=1.5, height=12, align=(Align.CENTER, Align.CENTER, Align.CENTER), mode=Mode.SUBTRACT)

# Ghosts
# GX12 Connector
# Import
imported_gx12 = import_step("designs/gx12-4p-m.stp")
gx_bbox = imported_gx12.bounding_box()
gx_center = gx_bbox.center()
# Center at origin
gx12_def = imported_gx12.moved(Location((-gx_center.X, -gx_center.Y, -gx_center.Z)))

# Position at Cutout Location in Case
gx12_z = -lid_thick - bucket_depth/2
# Right Wall X = case_len/2
# Flip Orientation: User says it's inside out.
# try Rotation(0, -90, 0) instead of (0, 90, 0) to flip X alignment.
gx12_loc = Location((case_len/2, 0, gx12_z)) * Rotation(0, -90, 0) * Rotation(90, 0, 0)
gx12_ghost = gx12_def.moved(gx12_loc)

p1_ghost = pump_oriented.moved(Location((-dist/2, 0, 0)))
p2_ghost = pump_oriented.moved(Location((dist/2, 0, 0)))

# Export
export_stl(bucket.part, "designs/pump_case_bucket.stl")
export_stl(lid.part, "designs/pump_case_lid.stl")
export_step(bucket.part, "designs/pump_case_bucket.step")
export_step(lid.part, "designs/pump_case_lid.step")

print("Pump Case Design generated: designs/pump_case_bucket.stl, designs/pump_case_lid.stl, designs/pump_case_bucket.step, designs/pump_case_lid.step")

# Show
set_port(3940)
show(bucket.part, lid.part, p1_ghost, p2_ghost, gx12_ghost, alphas=[0.8, 1.0, 0.5, 0.5, 0.8])

