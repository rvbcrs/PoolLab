from build123d import *
from ocp_vscode import show, show_object, set_port, set_defaults

# Define parameters
wall_thickness = 2
# Other params defined later based on component measurements

# === 1. Define USB Socket Part ===
usb_flange_d = 27
usb_flange_t = 3
usb_thread_d = 22
usb_thread_l = 17.4
usb_back_d = 17.5 
usb_back_l = 12.1

with BuildPart() as usb_socket_def:
    # Flange (Outside)
    with Locations((0, 0, -usb_flange_t)):
        Cylinder(radius=usb_flange_d/2, height=usb_flange_t, align=(Align.CENTER, Align.CENTER, Align.MIN))
    # Threaded body (Inside)
    with Locations((0, 0, 0)):
        Cylinder(radius=usb_thread_d/2, height=usb_thread_l, align=(Align.CENTER, Align.CENTER, Align.MIN))
    # Back body (Inside)
    with Locations((0, 0, usb_thread_l)):
        Cylinder(radius=usb_back_d/2, height=usb_back_l, align=(Align.CENTER, Align.CENTER, Align.MIN))

# === 2. Define PH-4502C Sensor Part ===
# Load from STEP file
imported_sensor = import_step("designs/PH-4502C.STEP")

# Re-define constants used for placement
pcb_l = 42
pcb_w = 32
pcb_h = 1.6
bnc_d = 12 
bnc_stickout = 15

# Calibration Constants
bnc_center_z = 14   
standoff_h = 2      

# === 3. Define LM2596 (Buck Converter) ===
imported_lm2596 = import_step("designs/LM2596.stp")

# Fix Rotation: User said 90 was upside down. Try -90.
lm_rotated_geom = imported_lm2596.moved(Rotation(-90, 0, 0))
lm_bbox = lm_rotated_geom.bounding_box()
lm_width_y = lm_bbox.size.Y
print(f"LM2596 Width (Y): {lm_width_y}")

# === Case Dimensions ===
# User request: Width of LM + Length of Sensor + 1cm margin
margin = 10
width = pcb_l + lm_width_y + margin + (wall_thickness * 2) 
# Let's add a bit more to be safe, say 15mm margin internal
internal_y_space = pcb_l + lm_width_y + 15
width = internal_y_space + (wall_thickness * 2) 

length = 130 # Keep X fixed for now
height = 35  

print(f"Calculated Case Width (Y): {width}")

with BuildPart() as ph_sensor_def:
    # Add imported part
    part_rotated = imported_sensor.moved(Rotation(90, 0, 0))
    bbox = part_rotated.bounding_box()
    
    # Align
    target_min_x = -pcb_l / 2
    target_center_y = 0
    target_min_z = 0 
    
    shift_x = target_min_x - bbox.min.X
    shift_y = target_center_y - bbox.center().Y
    shift_z = target_min_z - bbox.min.Z
    
    align_loc = Location((shift_x, shift_y, shift_z))
    
    offset_x = -6 
    offset_y = 2 
    offset_z = 0 
    manual_offset = Location((offset_x, offset_y, offset_z)) 
    
    add(part_rotated.moved(align_loc * manual_offset))

with BuildPart() as lm2596_def:
    # Center XY, Min Z at 0
    bbox_lm = lm_rotated_geom.bounding_box()
    # Center it
    align_loc_lm = Location((-bbox_lm.center().X, -bbox_lm.center().Y, -bbox_lm.min.Z))
    add(lm_rotated_geom.moved(align_loc_lm))

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

# Mount Logic: 4 Corner "Pillars"
# Simple Cylinders at the corners, with the PCB shape cut out of them.
with BuildPart() as tb6612_mount:
    # Standoff Height (clearance for inverted chips/components)
    standoff_z = 4 
    tb_standoff_z = standoff_z # Expose for ghosting 
    
    # Pillar settings
    pillar_r = 3.5
    pillar_h = standoff_z + tb_h + 1.5 # 1.5mm stickout on top
    
    # Locations for the 4 corners (Centered on PCB corners)
    dx = tb_w/2
    dy = tb_l/2
    
    # 1. Create 4 Solid Pillars
    with Locations([(dx, dy), (dx, -dy), (-dx, dy), (-dx, -dy)]):
         Cylinder(radius=pillar_r, height=pillar_h, align=(Align.CENTER, Align.CENTER, Align.MIN))
         
    # 2. Cut the "Seat" (The PCB volume)
    # We cut a box corresponding to the PCB size, starting from standoff_z
    with Locations((0,0, standoff_z)):
        Box(tb_w, tb_l, pillar_h, align=(Align.CENTER, Align.CENTER, Align.MIN), mode=Mode.SUBTRACT)
        
# === 3c. Define CTP09 Part & Mount ===
imported_ctp09 = import_step("designs/CTP09.step")
# Assume orientation needs checking. BBox first.
ctp_bbox_raw = imported_ctp09.bounding_box()
ctp_center = ctp_bbox_raw.center()
# Center it
ctp09_centered = imported_ctp09.moved(Location((-ctp_center.X, -ctp_center.Y, -ctp_bbox_raw.min.Z)))
ctp_bbox = ctp09_centered.bounding_box()
print(f"CTP09 Centered Size: {ctp_bbox.size}")

ctp_w = ctp_bbox.size.X
ctp_l = ctp_bbox.size.Y
ctp_h = 1.6 # Estimate

# Mount Logic: Same Pillar Seat as TB6612
with BuildPart() as ctp09_mount:
    standoff_z = 3 
    ctp_standoff_z = standoff_z # Expose
    pillar_r = 3.5
    pillar_h = standoff_z + ctp_h + 1.5 
    
    dx = ctp_w/2
    dy = ctp_l/2
    
    # Pillars
    with Locations([(dx, dy), (dx, -dy), (-dx, dy), (-dx, -dy)]):
         Cylinder(radius=pillar_r, height=pillar_h, align=(Align.CENTER, Align.CENTER, Align.MIN))
    # Seat Cut
    with Locations((0,0, standoff_z)):
        Box(ctp_w, ctp_l, pillar_h, align=(Align.CENTER, Align.CENTER, Align.MIN), mode=Mode.SUBTRACT)

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
# Located in the Rim area (between 84.5x52 and 94.5x62).
# Let's center them in the 5mm rim space.
# 5mm rim. Center is 2.5mm from edge of Rear Body.
mount_dx = (screen_back_w / 2) + 2.5 
mount_dy = (screen_back_h / 2) + 2.5

# === 4. Define 3.5" Screen Part ===
# Logic: Screen Rim sits ON TOP of Lid.
# Z=0 is the interface surface (Back of Rim / Top of Lid).
screen_front_w = 94.5
screen_front_h = 62
screen_front_t = 4 # Bezel thickness
screen_back_w = 84.5 
screen_back_h = 52
screen_back_t = 8  # Rear housing thickness
screen_corner_radius = 4

# Mounting holes
mount_dx = (screen_back_w / 2) + 2.5 
mount_dy = (screen_back_h / 2) + 2.5

with BuildPart() as screen_def:
    # Front Plate (Bezel/Glass) - Sits ON TOP (Z=0 to +4)
    with Locations((0,0,0)):
        with BuildSketch():
            rect = Rectangle(screen_front_w, screen_front_h, align=(Align.CENTER, Align.CENTER))
            fillet(rect.vertices(), radius=screen_corner_radius)
        extrude(amount=screen_front_t) # EXTRUDE UP
        
    # Rear Body ("Bottom") - Falls IN (Z=0 to -8)
    with Locations((0,0, 0)):
        with BuildSketch():
            Rectangle(screen_back_w, screen_back_h, align=(Align.CENTER, Align.CENTER))
        extrude(amount=-screen_back_t) # EXTRUDE DOWN
        
    # Active Area (Visual on Top Face)
    with Locations((0,0, screen_front_t)):
         Box(73.4, 49, 0.1, align=(Align.CENTER, Align.CENTER, Align.MIN), mode=Mode.ADD)
    
    # Mounting Inserts (In the Rim, accessible from Back/Bottom)
    # Visual cylinders at Z=0 extending UP into the Rim
    with Locations((0,0, 0)):
        with Locations([(mount_dx, mount_dy), (mount_dx, -mount_dy), (-mount_dx, mount_dy), (-mount_dx, -mount_dy)]):
             Cylinder(radius=3/2, height=4, align=(Align.CENTER, Align.CENTER, Align.MIN), mode=Mode.ADD)

# === 5. Placements & Layout ===

# USB: Right Side (X+ Face).
usb_loc = Location((length/2, 0, 0)) * Rotation(0, -90, 0) 

# GX12: "Next to" USB.
# User feedback: "Niet dwars". Needs rotation around insertion axis.
gx12_loc_y = -25 # Negative Y = Left relative to X+ viewer (if Right was +22)
# Rotation trace:
# Rotation(0, -90, 0) -> Points connection axis OUT of X+.
# Adding Rotation(90, 0, 0) -> Rotates 90 deg around X axis (Roll).
gx12_pos = Location((length/2, gx12_loc_y, 0))
gx12_loc = gx12_pos * Rotation(0, -90, 0) * Rotation(90, 0, 0) # For Ghost (Visual)

# For Cutout, we want the Cylinder aligned with the Hole Axis (X-axis).
# The standard "Cylinder" is Z-aligned. We need to rotate it -90 around Y to point X.
# We DO NOT want the extra "Roll" rotation (90 around X) because that doesn't change a Cylinder's shape 
# ... UNLESS it was applied to the axis, but here simple is better.
gx12_cutout_loc = gx12_pos * Rotation(0, -90, 0) # Points Z -> X. No Roll.

# Layout Strategy:
# Right Side (X > 0): Sensors + LM2596
# Left Side (X < 0): TB6612 + CTP09

# Sensors: Shifted Right (less aggressive)
# Previous: 15, 50. Too far.
# LM2596: Back to 0.
# Let's shift sensors enough to clear left side for TB6612.
# If TB6612 is at -40.
# Sensor 1 at +5? Sensor 2 at +40?
sensor_x_shift = 20 # Shift center of pair by 20mm right
sensor_space = 35 
sensor1_x = sensor_x_shift - sensor_space/2 # 20 - 17.5 = 2.5
sensor2_x = sensor_x_shift + sensor_space/2 # 20 + 17.5 = 37.5

sensor_y = (-width/2 + wall_thickness + 2) + pcb_l/2
sensor_z = -height/2 + wall_thickness + standoff_h 
sensor_rot = Rotation(0, 0, -90) 

sensor1_loc = Location((sensor1_x, sensor_y, sensor_z)) * sensor_rot
sensor2_loc = Location((sensor2_x, sensor_y, sensor_z)) * sensor_rot

# LM2596: Behind Sensors (Right side)
# User request: "Back to old place" (Center X=0)
lm2596_loc_x = 0
lm2596_loc_y = (sensor_y + pcb_l/2) + 5 + lm_width_y/2
lm2596_loc = Location((lm2596_loc_x, lm2596_loc_y, -height/2 + wall_thickness + standoff_h)) 

# Left Side Modules
left_module_x = -40 

# TB6612 (Front-Left)
# Align Y roughly with Sensors?
tb6612_loc_y = sensor_y 
tb6612_loc = Location((left_module_x, tb6612_loc_y, -height/2 + wall_thickness)) 

# CTP09 (Rear-Left, behind TB6612)
# Gap of 5mm
ctp09_loc_y = tb6612_loc_y + tb_l/2 + 5 + ctp_l/2
ctp09_loc = Location((left_module_x, ctp09_loc_y, -height/2 + wall_thickness)) 

# Screen: On top of the Lid
# Screen Z=0 (Rim Bottom) aligns with Lid Top Surface.
# User requested shift UP (Y+) by 5mm.
screen_offset_y = 5
# Lid is placed at height/2. Thickness is 4. Top is height/2 + 4.
lid_thickness = 4
lid_top_z = height/2 + lid_thickness
screen_loc = Location((0, screen_offset_y, lid_top_z))

# === 6. Create Case with Cutouts ===
with BuildPart() as case:
    # Outer box
    Box(length, width, height)
    # Hollow
    offset(amount=-wall_thickness, openings=[case.faces().sort_by(Axis.Z)[-1]])
    
    # USB Cutout
    with Locations(usb_loc):
        Cylinder(radius=usb_thread_d/2, height=20, align=(Align.CENTER, Align.CENTER, Align.CENTER), mode=Mode.SUBTRACT)
        
    # GX12 Cutout
    with Locations(gx12_cutout_loc):
        Cylinder(radius=gx12_cutout_d/2, height=20, align=(Align.CENTER, Align.CENTER, Align.CENTER), mode=Mode.SUBTRACT)

    # Sensor BNC Cutouts
    cutter_local_loc = Location((pcb_l/2, 0, bnc_center_z)) * Rotation(0, 90, 0)
    with Locations([sensor1_loc * cutter_local_loc, sensor2_loc * cutter_local_loc]):
        Cylinder(radius=bnc_d/2 + 0.5, height=30, align=(Align.CENTER, Align.CENTER, Align.CENTER), mode=Mode.SUBTRACT)

# === 7. Create Lid ===
with BuildPart() as lid:
    # Lid Plate
    with Locations((0,0,0)):
        Box(length, width, lid_thickness, align=(Align.CENTER, Align.CENTER, Align.MIN))
    
    # Cut Through Hole for Rear Body (84.5x52)
    # Apply Screen Offset Y
    with Locations((0, screen_offset_y, 0)):
         Box(screen_back_w + 0.5, screen_back_h + 0.5, lid_thickness, align=(Align.CENTER, Align.CENTER, Align.MIN), mode=Mode.SUBTRACT)
         
    # Mounting Holes (For screws from Inside-Up)
    # Apply Screen Offset Y
    with Locations((0, screen_offset_y, 0)):
        with Locations([(mount_dx, mount_dy), (mount_dx, -mount_dy), (-mount_dx, mount_dy), (-mount_dx, -mount_dy)]):
             Cylinder(radius=3.2/2, height=lid_thickness, align=(Align.CENTER, Align.CENTER, Align.MIN), mode=Mode.SUBTRACT)

# === 8. Ghosts & Show ===
usb_ghost = usb_socket_def.part.moved(usb_loc)
sensor1_ghost = ph_sensor_def.part.moved(sensor1_loc)
sensor2_ghost = ph_sensor_def.part.moved(sensor2_loc)
lm2596_ghost = lm2596_def.part.moved(lm2596_loc)
screen_ghost = screen_def.part.moved(screen_loc)
lid_part = lid.part.moved(Location((0,0, height/2))) # Lid Base at top of case

# Export STLs
# Export STLs
# TB6612 Ghost: Use Centered part + Lift by standoff
standoff_lift = 3 # Hardcode or match above if scope issue
tb6612_ghost = tb6612_centered.moved(tb6612_loc * Location((0,0, standoff_lift)))
ts6612_mount_part = tb6612_mount.part.moved(tb6612_loc)

# CTP09 Ghost
ctp09_ghost = ctp09_centered.moved(ctp09_loc * Location((0,0, standoff_lift))) # Same lift
ctp09_mount_part = ctp09_mount.part.moved(ctp09_loc)

gx12_ghost = gx12_def.moved(gx12_loc)

# Add mounting to Case
start_case_fuse = case.part + ts6612_mount_part + ctp09_mount_part

# Export STLs
export_stl(start_case_fuse, "case.stl")
export_stl(lid.part, "lid.stl")

print("Design generated.")
show(start_case_fuse, lid_part, usb_ghost, sensor1_ghost, sensor2_ghost, lm2596_ghost, screen_ghost, tb6612_ghost, ctp09_ghost, gx12_ghost,
     names=["Case", "Lid", "USB", "Sensor1", "Sensor2", "LM2596", "Screen", "TB6612", "CTP09", "GX12"], 
     colors=[None, None, (0.2,0.2,0.2), (0,0.8,0), (0,0.8,0), (0,0,0.8), (0.1, 0.1, 0.1), (0.8, 0, 0), (0, 0.8, 0.8), (0.6, 0.6, 0.6)], 
     alphas=[1.0, 0.8, 0.5, 0.5, 0.5, 0.5, 0.8, 0.8, 0.8, 0.8])
