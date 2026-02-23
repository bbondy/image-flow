# image-flow

`image-flow` is a C++ image toolkit with raster codecs, a layered compositor, and a deterministic CLI ops engine for procedural image editing.

## Features
- BMP/PNG/JPG/GIF encode and decode
- WebP encode and decode via `cwebp` and `dwebp` tooling (optional)
- Layer and group compositing with transforms, masks, and blend modes
- IFLOW project format for full layer-stack serialization
- CLI ops pipeline for reproducible edits from shell scripts
- Procedural drawing, filtering, color grading, texturing, and geometry ops

## Build
```bash
make
```

Binaries:
- `build/bin/image_flow`
- `build/bin/generate_samples`
- `build/bin/tests`

## CLI Commands
```bash
./build/bin/image_flow help
./build/bin/image_flow help ops
```

- `image_flow new --width <w> --height <h> --out <project.iflow>`
- `image_flow new --from-image <file> [--fit <w>x<h>] --out <project.iflow>`
- `image_flow info --in <project.iflow>`
- `image_flow render --in <project.iflow> --out <image.{png|bmp|jpg|gif|webp|svg}>`
- `image_flow ops --in <project.iflow> --out <project.iflow> --op "<action key=value ...>" [--op ...]`
- `image_flow ops --in <project.iflow> --out <project.iflow> --ops-file <ops.txt>`
- `cat ops.txt | image_flow ops --in <project.iflow> --out <project.iflow> --stdin`

CLI behavior notes:
- `new`: choose exactly one source mode:
  - `--width/--height` for a blank project, or
  - `--from-image` (optionally with `--fit`) to seed from an image.
- `ops`: choose exactly one input mode:
  - `--in <project.iflow>`, or
  - `--width/--height` to start from an empty in-memory document.
- `--op` tokenization supports quoted values:
  - `name="Layer One"` or `name='Layer One'`
  - Escape quote or backslash inside values with `\`.

## Blend Modes
- `normal`
- `multiply`
- `screen`
- `overlay`
- `darken`
- `lighten`
- `add`
- `subtract`
- `difference`
- `color-dodge`

## IFLOW Ops
### Structure
- `add-layer`
- `add-group`
- `add-grid-layers`
- `set-layer`
- `set-group`
- `import-image`
- `resize-layer`

### Transform
- `set-transform`
- `concat-transform`
- `clear-transform`

`set-transform` and `concat-transform` support:
- `matrix=a,b,c,d,tx,ty`
- or composed params: `translate=x,y` `scale=s|sx,sy` `skew=degx,degy` `rotate=deg` `pivot=x,y`

### Drawing
- `draw-fill`
- `draw-line`
- `draw-rect`
- `draw-fill-rect`
- `draw-round-rect`
- `draw-fill-round-rect`
- `draw-ellipse`
- `draw-fill-ellipse`
- `draw-polyline`
- `draw-polygon`
- `draw-fill-polygon`
- `draw-flood-fill`
- `draw-circle`
- `draw-fill-circle`
- `draw-arc`
- `draw-quadratic-bezier`
- `draw-bezier`

All draw ops support:
- required: `path=<layer_path>`
- optional: `target=image|mask` (default `image`)
- required for drawing color: `rgba=r,g,b,a`

Draw op parameter reference:
- `draw-fill`: `path rgba [target]`
- `draw-line`: `path x0 y0 x1 y1 rgba [target]`
- `draw-rect`: `path x y width height rgba [target]`
- `draw-fill-rect`: `path x y width height rgba [target]`
- `draw-round-rect`: `path x y width height radius rgba [target]`
- `draw-fill-round-rect`: `path x y width height radius rgba [target]`
- `draw-ellipse`: `path cx cy rx ry rgba [target]`
- `draw-fill-ellipse`: `path cx cy rx ry rgba [target]`
- `draw-polyline`: `path points=x0,y0;x1,y1;... rgba [target]`
- `draw-polygon`: `path points=x0,y0;x1,y1;... rgba [target]`
- `draw-fill-polygon`: `path points=x0,y0;x1,y1;... rgba [target]`
- `draw-flood-fill`: `path x y rgba [tolerance] [target]`
- `draw-circle`: `path cx cy radius rgba [target]`
- `draw-fill-circle`: `path cx cy radius rgba [target]`
- `draw-arc`: `path cx cy radius rgba (start_rad/end_rad | start_deg/end_deg) [counterclockwise] [target]`
- `draw-quadratic-bezier`: `path x0 y0 cx cy x1 y1 rgba [target]`
- `draw-bezier`: `path x0 y0 cx1 cy1 cx2 cy2 x1 y1 rgba [target]`

Drawing examples:
```bash
./build/bin/image_flow ops --in in.iflow --out out.iflow \
  --op "draw-round-rect path=/0 x=40 y=30 width=220 height=140 radius=18 rgba=255,200,40,255" \
  --op "draw-fill-round-rect path=/0 x=48 y=38 width=204 height=124 radius=14 rgba=20,20,30,220"

./build/bin/image_flow ops --in in.iflow --out out.iflow \
  --op "draw-polyline path=/0 points=20,20;120,40;200,25;260,90 rgba=0,255,255,255" \
  --op "draw-flood-fill path=/0 x=80 y=80 rgba=30,80,200,255 tolerance=12"

./build/bin/image_flow ops --in in.iflow --out out.iflow \
  --op "draw-arc path=/0 cx=160 cy=120 radius=70 start_deg=350 end_deg=20 counterclockwise=false rgba=255,0,0,255" \
  --op "draw-quadratic-bezier path=/0 x0=20 y0=180 cx=140 cy=40 x1=280 y1=180 rgba=255,255,0,255" \
  --op "draw-bezier path=/0 x0=20 y0=200 cx1=90 cy=80 cx2=210 cy=80 x1=280 y1=200 rgba=0,255,255,255"
```

## Drawable API
`Drawable` supports both immediate primitives and retained path drawing.

Path model:
- `beginPath()`
- `moveTo(x, y)`
- `lineTo(x, y)`
- `quadraticCurveTo(cx, cy, x, y)`
- `bezierCurveTo(cx1, cy1, cx2, cy2, x, y)`
- `closePath()`
- `stroke(color)`
- `fillPath(color)`

Stroke style controls:
- `setLineWidth(width)`
- `setLineCap(Butt|Round|Square)`
- `setLineJoin(Miter|Round|Bevel)`
- `setMiterLimit(limit)`

Shape and fill primitives:
- `fill`, `line`
- `rect`, `fillRect`
- `roundRect`, `fillRoundRect`
- `ellipse`, `fillEllipse`
- `circle`, `fillCircle`
- `polyline`, `polygon`, `fillPolygon`
- `arc(..., counterclockwise)`
- `floodFill(x, y, color, tolerance)`

### Masks and Pixels
- `mask-enable`
- `mask-clear`
- `mask-set-pixel`
- `fill-layer`
- `set-pixel`

### Procedural and Noise
- `gradient-layer` (`linear` or `radial`)
- `checker-layer`
- `noise-layer`
- `fractal-noise`
- `hatch`
- `pencil-strokes`

### Color and Tone
- `apply-effect effect=grayscale|sepia|invert|threshold`
- `replace-color`
- `channel-mix` (3x3 RGB matrix)
- `levels`
- `gamma`
- `curves`

### Filtering and Morphology
- `gaussian-blur`
- `edge-detect method=sobel|canny`
- `morphology op=erode|dilate`

## Example Ops Workflow
```bash
./build/bin/image_flow new --from-image samples/tahoe200-finish.webp --fit 1400x900 --out build/output/images/demo.iflow

./build/bin/image_flow ops \
  --in build/output/images/demo.iflow \
  --out build/output/images/demo.iflow \
  --op "add-layer name=Sketch width=1400 height=900 fill=0,0,0,0" \
  --op "import-image path=/1 file=samples/tahoe200-finish.webp" \
  --op "apply-effect path=/1 effect=grayscale" \
  --op "apply-effect path=/1 effect=invert" \
  --op "gaussian-blur path=/1 radius=8 sigma=3.6" \
  --op "set-layer path=/1 blend=color-dodge opacity=0.95" \
  --render build/output/images/demo.png
```

## Sample Scripts
Run one script:
```bash
./sample_scripts/iflow_pencil_sketch_example.sh
```

Run all scripts:
```bash
./sample_scripts/run_all.sh
# or
make run-scripts
```

Current sample scripts:
- `sample_scripts/iflow_ops_example.sh`
- `sample_scripts/iflow_orbital_wireframe_example.sh`
- `sample_scripts/iflow_mask_etch_example.sh`
- `sample_scripts/iflow_chroma_tunnel_example.sh`
- `sample_scripts/iflow_pencil_sketch_example.sh`

## Sample Generator
```bash
./build/bin/generate_samples
```

## Tests
```bash
make test
```

## Output Artifacts
- App output images: `build/output/images`
- Test output artifacts: `build/output/test-images`
