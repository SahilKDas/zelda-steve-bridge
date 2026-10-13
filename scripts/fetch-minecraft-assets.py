"""Fetch only pinned Minecraft textures and build native, unfiltered face atlases."""
import hashlib
import json
from pathlib import Path
import urllib.request
from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
REVISION = "0c2358f50e81e34def33b75af34fc65409893dc0"
BASE = f"https://raw.githubusercontent.com/InventivetalentDev/minecraft-assets/{REVISION}/assets/minecraft/textures/"
DEST = ROOT / "cache/minecraft-assets"
DEST.mkdir(parents=True, exist_ok=True)
paths = ["entity/player/wide/steve.png", "entity/equipment/wings/elytra.png", "entity/shield_base.png"]
paths += [f"item/{name}.png" for name in ("iron_pickaxe", "iron_sword", "cooked_beef", "bow", "arrow", "netherite_axe", "flint_and_steel", "elytra", "diamond_chestplate")]
paths += [f"block/{name}.png" for name in ("stone", "cobblestone", "tnt_side", "tnt_top", "tnt_bottom")]
paths += ["gui/container/inventory.png", "gui/sprites/hud/crosshair.png", "gui/sprites/hud/hotbar.png", "gui/sprites/hud/hotbar_selection.png"]
paths += ["entity/equipment/humanoid/diamond.png", "entity/equipment/humanoid_leggings/diamond.png", "misc/enchanted_glint_armor.png"]
paths += ["item/diamond_pickaxe.png","item/diamond_axe.png","item/golden_apple.png","item/firework_rocket.png","item/totem_of_undying.png","block/obsidian.png","entity/horse/horse_skeleton.png"]
manifest = {"revision": REVISION, "source": BASE, "files": {}}
for name in paths:
    target = DEST / name
    target.parent.mkdir(parents=True, exist_ok=True)
    if not target.exists():
        with urllib.request.urlopen(BASE + name, timeout=60) as response:
            target.write_bytes(response.read())
    manifest["files"][name] = hashlib.sha256(target.read_bytes()).hexdigest()

def rgba16(img):
    rgba = img.convert("RGBA")
    storage_width=max(8,(rgba.width+3)//4*4)
    if rgba.width != storage_width:
        padded=Image.new("RGBA",(storage_width,rgba.height));padded.paste(rgba,(0,0))
        for y in range(rgba.height):
            for x in range(rgba.width,padded.width):padded.putpixel((x,y),rgba.getpixel((rgba.width-1,y)))
        rgba=padded
    values = [((r >> 3) << 11) | ((g >> 3) << 6) | ((b >> 3) << 1) | (a >= 128)
              for r, g, b, a in (rgba.get_flattened_data() if hasattr(rgba,"get_flattened_data") else rgba.getdata())]
    # Fast3D consumes big-endian N64 texels, even on a little-endian host.
    return [byte for value in values for byte in value.to_bytes(2,"big")]

skin = Image.open(DEST / paths[0]).convert("RGBA")
assert skin.size == (64, 64)
parts = [(0,0,32,0,8,8,8), (16,16,16,32,8,12,4),
         (40,16,40,32,4,12,4), (32,48,48,48,4,12,4),
         (0,16,0,32,4,12,4), (16,48,0,48,4,12,4)]
lines = ["/* Generated from pinned Minecraft assets; do not edit. */",
         "typedef struct SteveSkinFace { const u8* pixels; int width,height; } SteveSkinFace;"]
faces = []
for part, (u,v,ou,ov,w,h,d) in enumerate(parts):
    layers = []
    for layer, (su,sv) in enumerate(((u,v),(ou,ov))):
        rectangles = [(su+d,sv,w,d),(su+d+w,sv,w,d),(su,sv+d,d,h),
                      (su+d,sv+d,w,h),(su+d+w,sv+d,d,h),(su+d+w+d,sv+d,w,h)]
        descriptors = []
        for face, (x,y,fw,fh) in enumerate(rectangles):
            identifier = f"sSkin_{part}_{layer}_{face}"
            pixels = rgba16(skin.crop((x,y,x+fw,y+fh)))
            lines.append(f"alignas(8) static const u8 {identifier}[] = {{" + ",".join(hex(p) for p in pixels) + "};")
            descriptors.append(f"{{{identifier},{fw},{fh}}}")
        layers.append("{" + ",".join(descriptors) + "}")
    faces.append("{" + ",".join(layers) + "}")
lines.append("static const SteveSkinFace sSkinFaces[6][2][6] = {" + ",".join(faces) + "};")
icons = [None,"item/diamond_pickaxe.png","item/iron_sword.png","item/golden_apple.png","item/bow.png",
         "block/cobblestone.png","item/arrow.png","block/tnt_side.png","item/flint_and_steel.png","item/elytra.png","item/diamond_axe.png",
         "item/diamond_chestplate.png","item/diamond_chestplate.png","block/obsidian.png","item/firework_rocket.png","item/totem_of_undying.png"]
descriptors = ["{NULL,0,0}"]
for kind,name in enumerate(icons[1:],1):
    img=Image.open(DEST/name).convert("RGBA")
    identifier=f"sItem_{kind}"
    lines.append(f"alignas(8) static const u8 {identifier}[] = {{"+",".join(hex(p) for p in rgba16(img))+"};")
    descriptors.append(f"{{{identifier},{img.width},{img.height}}}")
lines.append("static const SteveSkinFace sItemFaces[16] = {"+",".join(descriptors)+"};")
for name,path,rect in [("sTntTop","block/tnt_top.png",None),("sTntBottom","block/tnt_bottom.png",None),("sStoneFace","block/stone.png",None),
                       ("sShieldFace","entity/shield_base.png",(0,0,16,24)),
                       ("sElytraWing","entity/equipment/wings/elytra.png",(22,0,34,22))]:
    img=Image.open(DEST/path).convert("RGBA")
    if rect: img=img.crop(rect)
    lines.append(f"alignas(8) static const u8 {name}Pixels[] = {{"+",".join(hex(p) for p in rgba16(img))+"};")
    lines.append(f"static const SteveSkinFace {name} = {{{name}Pixels,{img.width},{img.height}}};")

armor = Image.open(DEST/"entity/equipment/humanoid/diamond.png").convert("RGBA")
leggings = Image.open(DEST/"entity/equipment/humanoid_leggings/diamond.png").convert("RGBA")
armor_faces=[]
glint=Image.open(DEST/"misc/enchanted_glint_armor.png").convert("RGBA").resize((16,16),Image.Resampling.NEAREST)
glint_faces=[]
for part,(su,sv,w,h,d) in enumerate([(0,0,8,8,8),(16,16,8,12,4),(40,16,4,12,4),
                                    (40,16,4,12,4),(0,16,4,12,4),(0,16,4,12,4),
                                    (0,16,4,12,4),(0,16,4,12,4)]):
    image=leggings if part>=6 else armor
    descriptors=[]
    phase_descriptors=[[] for _ in range(8)]
    for face,(x,y,fw,fh) in enumerate([(su+d,sv,w,d),(su+d+w,sv,w,d),(su,sv+d,d,h),
                                      (su+d,sv+d,w,h),(su+d+w,sv+d,d,h),(su+d+w+d,sv+d,w,h)]):
        identifier=f"sArmor_{part}_{face}"
        cropped=image.crop((x,y,x+fw,y+fh))
        lines.append(f"alignas(8) static const u8 {identifier}[] = {{"+",".join(hex(p) for p in rgba16(cropped))+"};")
        descriptors.append(f"{{{identifier},{fw},{fh}}}")
        for phase in range(8):
            overlay=Image.new("RGBA",cropped.size)
            for gy in range(fh):
                for gx in range(fw):
                    r,g,b,_=glint.getpixel(((gx+phase*2)%16,(gy+phase)%16))
                    # Preserve the helmet opening and other armor cutouts in every phase.
                    overlay.putpixel((gx,gy),(r,g,b,cropped.getpixel((gx,gy))[3] if max(r,g,b)>20 else 0))
            name=f"sArmorGlint_{part}_{phase}_{face}"
            lines.append(f"alignas(8) static const u8 {name}[] = {{"+",".join(hex(p) for p in rgba16(overlay))+"};")
            phase_descriptors[phase].append(f"{{{name},{fw},{fh}}}")
    armor_faces.append("{"+",".join(descriptors)+"}")
    glint_faces.append("{"+",".join("{"+",".join(faces)+"}" for faces in phase_descriptors)+"}")
lines.append("static const SteveSkinFace sArmorFaces[8][6] = {"+",".join(armor_faces)+"};")
lines.append("static const SteveSkinFace sArmorGlintFaces[8][8][6] = {"+",".join(glint_faces)+"};")

def cuboid_faces(name,path,parts):
    image=Image.open(DEST/path).convert("RGBA")
    tables=[]
    for part,(su,sv,w,h,d) in enumerate(parts):
        descriptors=[]
        for face,(x,y,fw,fh) in enumerate([(su+d,sv,w,d),(su+d+w,sv,w,d),(su,sv+d,d,h),
                                          (su+d,sv+d,w,h),(su+d+w,sv+d,d,h),(su+d+w+d,sv+d,w,h)]):
            identifier=f"{name}_{part}_{face}"
            lines.append(f"alignas(8) static const u8 {identifier}[] = {{"+",".join(hex(p) for p in rgba16(image.crop((x,y,x+fw,y+fh))))+"};")
            descriptors.append(f"{{{identifier},{fw},{fh}}}")
        tables.append("{"+",".join(descriptors)+"}")
    lines.append(f"static const SteveSkinFace {name}[{len(parts)}][6] = {{"+",".join(tables)+"};")
cuboid_faces("sShieldModelFaces","entity/shield_base.png",[(0,0,12,22,1),(26,0,2,6,6)])
cuboid_faces("sWingFaces","entity/equipment/wings/elytra.png",[(22,0,10,20,2)])
cuboid_faces("sHorseFaces","entity/horse/horse_skeleton.png",[(0,32,10,10,22),(0,35,4,12,7),
    (0,13,6,5,7),(0,25,4,5,5),(48,21,3,11,3),(19,16,2,3,1),(42,36,3,14,2)])
generated = ROOT / "src/soh_mod/generated"
generated.mkdir(exist_ok=True)
(generated / "minecraft_textures.inc").write_text("\n".join(lines) + "\n")
(DEST / "manifest.json").write_text(json.dumps(manifest, indent=2))
print(f"Fetched {len(paths)} pinned textures; generated official Steve face atlas")
