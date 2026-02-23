#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BIN="$ROOT_DIR/build/bin/image_flow"
OUT_DIR="$ROOT_DIR/build/output/images"
PROJECT_PATH="$OUT_DIR/prismatic_mech_garden.iflow"
RENDER_PATH="$OUT_DIR/prismatic_mech_garden.png"
OPS_PATH="$OUT_DIR/prismatic_mech_garden.ops"

WIDTH=1400
HEIGHT=900

if [[ ! -x "$BIN" ]]; then
  make -C "$ROOT_DIR"
fi

mkdir -p "$OUT_DIR"

"$BIN" new --width "$WIDTH" --height "$HEIGHT" --out "$PROJECT_PATH"

cat > "$OPS_PATH" <<OPS
# Layer 0: deep backdrop with flood-filled interior and subtle frame geometry
add-layer name=Backdrop width=${WIDTH} height=${HEIGHT} fill=0,0,0,0

draw-fill path=/0 rgba=4,8,20,255
draw-rect path=/0 x=22 y=22 width=1356 height=856 rgba=18,34,82,255
draw-flood-fill path=/0 x=44 y=44 rgba=7,13,34,255 tolerance=8
draw-rect path=/0 x=40 y=40 width=1320 height=820 rgba=50,90,180,255
draw-fill-rect path=/0 x=160 y=620 width=1080 height=170 rgba=10,18,48,220
draw-line path=/0 x0=160 y0=704 x1=1240 y1=704 rgba=70,140,255,200
draw-line path=/0 x0=160 y0=748 x1=1240 y1=748 rgba=40,90,180,170

# Layer 2: nebula plumes (ellipse + fillEllipse)
add-layer name=Nebula width=${WIDTH} height=${HEIGHT} fill=0,0,0,0
set-layer path=/1 blend=screen opacity=0.82
draw-fill-ellipse path=/1 cx=280 cy=230 rx=260 ry=170 rgba=80,30,170,145
draw-fill-ellipse path=/1 cx=620 cy=190 rx=320 ry=150 rgba=20,150,240,120
draw-fill-ellipse path=/1 cx=980 cy=290 rx=300 ry=190 rgba=255,90,170,110
draw-ellipse path=/1 cx=760 cy=260 rx=520 ry=220 rgba=180,220,255,90
draw-ellipse path=/1 cx=760 cy=260 rx=620 ry=300 rgba=90,140,255,70

# Layer 3: glass shards (polygon + fillPolygon)
add-layer name=GlassShards width=${WIDTH} height=${HEIGHT} fill=0,0,0,0
set-layer path=/2 blend=overlay opacity=0.88
draw-fill-polygon path=/2 points=210,160;380,110;430,260;280,300 rgba=50,200,255,130
draw-fill-polygon path=/2 points=460,360;700,290;760,500;520,560 rgba=255,120,200,120
draw-fill-polygon path=/2 points=820,140;1080,90;1180,280;930,350 rgba=120,255,200,105
draw-polygon path=/2 points=210,160;380,110;430,260;280,300 rgba=180,250,255,180
draw-polygon path=/2 points=460,360;700,290;760,500;520,560 rgba=255,220,240,170
draw-polygon path=/2 points=820,140;1080,90;1180,280;930,350 rgba=210,255,240,165

# Layer 4: orbital rails and signal paths (polyline + arc + circle)
add-layer name=OrbitalRails width=${WIDTH} height=${HEIGHT} fill=0,0,0,0
set-layer path=/3 blend=screen opacity=0.9
draw-circle path=/3 cx=700 cy=450 radius=260 rgba=90,180,255,170
draw-circle path=/3 cx=700 cy=450 radius=340 rgba=70,140,230,120
draw-arc path=/3 cx=700 cy=450 radius=260 start_deg=195 end_deg=343 counterclockwise=false rgba=255,220,120,210
draw-arc path=/3 cx=700 cy=450 radius=340 start_deg=16 end_deg=160 counterclockwise=false rgba=180,220,255,180
draw-polyline path=/3 points=180,760;320,640;460,680;600,590;780,640;960,560;1160,640 rgba=255,170,70,175
draw-polyline path=/3 points=220,220;360,300;540,250;720,340;900,300;1120,360 rgba=110,220,255,160
draw-fill-circle path=/3 cx=320 cy=640 radius=8 rgba=255,220,120,230
draw-fill-circle path=/3 cx=960 cy=560 radius=7 rgba=150,230,255,225
draw-fill-circle path=/3 cx=720 cy=340 radius=6 rgba=255,150,210,225

# Layer 5: mechanical shrine core (roundRect + rect/fillRect)
add-layer name=ShrineCore width=${WIDTH} height=${HEIGHT} fill=0,0,0,0
set-layer path=/4 blend=normal opacity=0.95
draw-fill-round-rect path=/4 x=560 y=260 width=280 height=380 radius=44 rgba=16,28,64,245
draw-round-rect path=/4 x=560 y=260 width=280 height=380 radius=44 rgba=110,190,255,235
draw-fill-rect path=/4 x=614 y=324 width=172 height=36 rgba=35,180,255,210
draw-fill-rect path=/4 x=614 y=382 width=172 height=36 rgba=255,130,215,180
draw-fill-rect path=/4 x=614 y=440 width=172 height=36 rgba=180,230,255,170
draw-rect path=/4 x=614 y=324 width=172 height=152 rgba=180,230,255,210
draw-fill-circle path=/4 cx=700 cy=548 radius=32 rgba=255,220,120,235
draw-circle path=/4 cx=700 cy=548 radius=50 rgba=120,200,255,220

# Layer 6: flowing vines and conduits (quadratic/cubic Bezier)
add-layer name=Vines width=${WIDTH} height=${HEIGHT} fill=0,0,0,0
set-layer path=/5 blend=screen opacity=0.86
draw-quadratic-bezier path=/5 x0=150 y0=860 cx=380 cy=520 x1=660 y1=760 rgba=120,255,180,160
draw-quadratic-bezier path=/5 x0=1240 y0=860 cx=1010 cy=520 x1=760 y1=760 rgba=120,255,180,160
draw-bezier path=/5 x0=120 y0=700 cx1=330 cy1=420 cx2=520 cy2=640 x1=720 y1=500 rgba=255,190,120,150
draw-bezier path=/5 x0=1280 y0=700 cx1=1080 cy1=430 cx2=880 cy2=640 x1=680 y1=500 rgba=255,190,120,150
draw-bezier path=/5 x0=260 y0=180 cx1=480 cy1=80 cx2=930 cy2=80 x1=1140 y1=180 rgba=160,220,255,135

# Layer 7: volumetric mist masked by draw primitives on mask target
add-layer name=Mist width=${WIDTH} height=${HEIGHT} fill=180,210,255,255
set-layer path=/6 blend=screen opacity=0.28
mask-enable path=/6 fill=0,0,0,255
draw-fill-ellipse path=/6 target=mask cx=700 cy=450 rx=540 ry=300 rgba=255,255,255,255
draw-fill-round-rect path=/6 target=mask x=500 y=220 width=400 height=460 radius=80 rgba=255,255,255,255
draw-fill-polygon path=/6 target=mask points=270,720;700,520;1130,720;700,860 rgba=255,255,255,255
draw-arc path=/6 target=mask cx=700 cy=450 radius=360 start_deg=205 end_deg=335 counterclockwise=false rgba=255,255,255,255

# Layer 8: spark accents
add-layer name=Sparks width=${WIDTH} height=${HEIGHT} fill=0,0,0,0
set-layer path=/7 blend=add opacity=0.74
draw-fill-circle path=/7 cx=420 cy=236 radius=3 rgba=255,240,170,255
draw-fill-circle path=/7 cx=610 cy=192 radius=2 rgba=255,255,255,255
draw-fill-circle path=/7 cx=860 cy=186 radius=3 rgba=255,210,240,255
draw-fill-circle path=/7 cx=1020 cy=282 radius=2 rgba=170,240,255,255
draw-line path=/7 x0=420 y0=236 x1=448 y1=224 rgba=255,240,170,190
draw-line path=/7 x0=860 y0=186 x1=888 y1=172 rgba=255,210,240,190
OPS

"$BIN" ops --in "$PROJECT_PATH" --out "$PROJECT_PATH" --ops-file "$OPS_PATH" --render "$RENDER_PATH"

echo "Wrote project: $PROJECT_PATH"
echo "Wrote render:  $RENDER_PATH"
