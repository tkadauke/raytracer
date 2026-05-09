// Interactive widget for `render::BVH` construction and traversal.
// It shows a 2D slice of primitive AABBs, centroid-ordered split
// candidates, SAH-style child-box costs, the selected split, and a ray
// that can skip an entire missed child subtree.

class BVHSahTraversal {
  constructor() {
    this.width = 560;
    this.height = 340;
    this.scene = { x: 28, y: 28, width: 324, height: 234 };
    this.costPanel = { x: 382, y: 48, width: 150, height: 182 };
    this.mode = 'splits';
    this.boxes = [
      { x: 48, y: 66, width: 66, height: 50 },
      { x: 84, y: 154, width: 78, height: 62 },
      { x: 198, y: 88, width: 52, height: 58 },
      { x: 250, y: 154, width: 76, height: 46 },
    ];
    this.ray = {
      origin: { x: 140, y: 28 },
      target: { x: 310, y: 210 },
    };
  }

  element() {
    if (this.widget) return this.widget.root;

    this.widget = new FigureWidget({ className: 'bvh-sah-traversal-widget' });
    this.canvas = new FigureSvg({
      width: this.width,
      height: this.height,
      viewBox: `0 0 ${this.width} ${this.height}`,
    });

    this.modeControl = new FigureSegmentedControl({
      label: 'BVH topic',
      value: this.mode,
      options: [
        { label: 'Split candidates', value: 'splits' },
        { label: 'SAH cost', value: 'costs' },
        { label: 'Traversal', value: 'traversal' },
      ],
      onChange: (value) => {
        this.mode = value;
        this.render();
      },
    });

    this.widget.addControl(this.modeControl.element());
    this.widget.setContent(this.canvas.element);
    this.render();
    return this.widget.root;
  }

  clamp(value, min, max) {
    return Math.max(min, Math.min(max, value));
  }

  clampBox(box, next) {
    return {
      x: this.clamp(next.x, this.scene.x + 4, this.scene.x + this.scene.width - box.width - 4),
      y: this.clamp(next.y, this.scene.y + 4, this.scene.y + this.scene.height - box.height - 4),
      width: box.width,
      height: box.height,
    };
  }

  clampPoint(point) {
    return {
      x: this.clamp(point.x, this.scene.x, this.scene.x + this.scene.width),
      y: this.clamp(point.y, this.scene.y, this.scene.y + this.scene.height + 48),
    };
  }

  centroid(box) {
    return {
      x: box.x + box.width * 0.5,
      y: box.y + box.height * 0.5,
    };
  }

  sortedBoxes() {
    return this.boxes
      .map((box, index) => ({ box, index, centroid: this.centroid(box) }))
      .sort((a, b) => a.centroid.x - b.centroid.x);
  }

  bounds(items) {
    const left = Math.min(...items.map(item => item.box.x));
    const top = Math.min(...items.map(item => item.box.y));
    const right = Math.max(...items.map(item => item.box.x + item.box.width));
    const bottom = Math.max(...items.map(item => item.box.y + item.box.height));
    return { x: left, y: top, width: right - left, height: bottom - top };
  }

  boxArea(box) {
    return Math.max(0, box.width) * Math.max(0, box.height);
  }

  candidates() {
    const sorted = this.sortedBoxes();
    return sorted.slice(0, -1).map((_, index) => {
      const leftItems = sorted.slice(0, index + 1);
      const rightItems = sorted.slice(index + 1);
      const leftBox = this.bounds(leftItems);
      const rightBox = this.bounds(rightItems);
      const cost = this.boxArea(leftBox) * leftItems.length
                 + this.boxArea(rightBox) * rightItems.length;
      return {
        index,
        x: (sorted[index].centroid.x + sorted[index + 1].centroid.x) * 0.5,
        leftItems,
        rightItems,
        leftBox,
        rightBox,
        cost,
      };
    });
  }

  selectedCandidate() {
    return this.candidates().reduce((best, candidate) => (
      !best || candidate.cost < best.cost ? candidate : best
    ), null);
  }

  rayVector() {
    return {
      x: this.ray.target.x - this.ray.origin.x,
      y: this.ray.target.y - this.ray.origin.y,
    };
  }

  rayIntersectsBox(box) {
    const origin = this.ray.origin;
    const dir = this.rayVector();
    let tMin = 0;
    let tMax = Infinity;
    const slabs = [
      { origin: origin.x, dir: dir.x, min: box.x, max: box.x + box.width },
      { origin: origin.y, dir: dir.y, min: box.y, max: box.y + box.height },
    ];

    for (const slab of slabs) {
      if (Math.abs(slab.dir) < 0.0001) {
        if (slab.origin < slab.min || slab.origin > slab.max) return false;
        continue;
      }
      const t1 = (slab.min - slab.origin) / slab.dir;
      const t2 = (slab.max - slab.origin) / slab.dir;
      tMin = Math.max(tMin, Math.min(t1, t2));
      tMax = Math.min(tMax, Math.max(t1, t2));
      if (tMax < tMin) return false;
    }
    return tMax >= 0;
  }

  render() {
    this.canvas.clear();
    this.renderSceneFrame();
    const selected = this.selectedCandidate();

    if (this.mode !== 'traversal') this.renderCandidates(selected);
    if (this.mode === 'costs') this.renderCosts(selected);
    if (this.mode === 'traversal') this.renderTraversal(selected);
    this.renderPrimitives();
    if (this.mode === 'traversal') {
      this.renderRay();
      this.renderRayHandles();
    }
  }

  renderSceneFrame() {
    this.canvas.add('rect', {
      x: this.scene.x,
      y: this.scene.y,
      width: this.scene.width,
      height: this.scene.height,
      fill: '#fff',
      stroke: '#999',
      'stroke-width': FigurePixelGuideStrokeWidth,
    });
    this.canvas.add('text', {
      x: this.scene.x,
      y: this.scene.y - 9,
      'font-size': 13,
      'font-family': 'monospace',
      fill: '#333',
    }).textContent = 'primitive AABBs sorted by centroid x';
  }

  renderCandidates(selected) {
    this.candidates().forEach((candidate) => {
      const isSelected = selected && candidate.index === selected.index;
      this.canvas.add('line', {
        x1: candidate.x,
        y1: this.scene.y,
        x2: candidate.x,
        y2: this.scene.y + this.scene.height,
        stroke: isSelected ? '#d12' : '#777',
        'stroke-width': isSelected ? FigurePixelStrokeWidth : FigurePixelGuideStrokeWidth,
        'stroke-dasharray': isSelected ? '' : '5 5',
        'data-candidate-split': candidate.index,
        'data-selected-split': isSelected ? 'true' : 'false',
      });
    });
    if (selected) {
      this.canvas.add('text', {
        x: selected.x + 6,
        y: this.scene.y + 16,
        'font-size': 12,
        'font-family': 'monospace',
        fill: '#d12',
      }).textContent = 'selected split';
    }
  }

  renderCosts(selected) {
    const candidates = this.candidates();
    const maxCost = Math.max(...candidates.map(candidate => candidate.cost));

    this.canvas.add('text', {
      x: this.costPanel.x,
      y: this.costPanel.y - 14,
      'font-size': 13,
      'font-family': 'monospace',
      fill: '#333',
    }).textContent = 'SAH cost';

    candidates.forEach((candidate, i) => {
      const barWidth = (candidate.cost / maxCost) * this.costPanel.width;
      const y = this.costPanel.y + i * 48;
      const selectedBar = selected && candidate.index === selected.index;
      this.canvas.add('rect', {
        x: this.costPanel.x,
        y,
        width: barWidth,
        height: 26,
        fill: selectedBar ? '#f6b2a8' : '#d7e3f8',
        stroke: selectedBar ? '#d12' : '#5778aa',
        'stroke-width': FigurePixelGuideStrokeWidth,
        'data-sah-cost': candidate.index,
        'data-selected-cost': selectedBar ? 'true' : 'false',
      });
      this.canvas.add('text', {
        x: this.costPanel.x + 5,
        y: y + 18,
        'font-size': 12,
        'font-family': 'monospace',
        fill: '#222',
      }).textContent = `split ${i + 1}: ${Math.round(candidate.cost)}`;
    });
  }

  renderTraversal(selected) {
    if (!selected) return;
    const leftHit = this.rayIntersectsBox(selected.leftBox);
    const rightHit = this.rayIntersectsBox(selected.rightBox);
    this.renderChildBox(selected.leftBox, 'left subtree', leftHit);
    this.renderChildBox(selected.rightBox, 'right subtree', rightHit);
  }

  renderChildBox(box, label, hit) {
    this.canvas.add('rect', {
      x: box.x,
      y: box.y,
      width: box.width,
      height: box.height,
      fill: hit ? '#dff3dd' : '#eee',
      'fill-opacity': 0.42,
      stroke: hit ? '#228b22' : '#777',
      'stroke-width': FigurePixelStrokeWidth,
      'stroke-dasharray': hit ? '' : '6 5',
      'data-subtree-status': hit ? 'hit' : 'miss',
    });
    this.canvas.add('text', {
      x: box.x + 4,
      y: box.y - 6,
      'font-size': 12,
      'font-family': 'monospace',
      fill: hit ? '#176b17' : '#666',
    }).textContent = hit ? label : `${label} pruned`;
  }

  renderPrimitives() {
    this.boxes.forEach((box, index) => {
      const rect = this.canvas.add('rect', {
        x: box.x,
        y: box.y,
        width: box.width,
        height: box.height,
        fill: '#ffffff',
        'fill-opacity': 0.78,
        stroke: '#111',
        'stroke-width': FigurePixelStrokeWidth,
        cursor: 'move',
        'data-drag-handle': 'primitive-aabb',
        'data-box-index': index,
      });
      this.addBoxDrag(rect, box, index);
      const c = this.centroid(box);
      this.canvas.add('circle', {
        cx: c.x,
        cy: c.y,
        r: 4,
        fill: '#111',
        stroke: '#111',
        'stroke-width': FigurePixelGuideStrokeWidth,
      });
    });
  }

  renderRay() {
    this.renderArrowMarker();
    const dir = this.rayVector();
    const length = Math.sqrt(dir.x * dir.x + dir.y * dir.y) || 1;
    const scale = 460 / length;
    this.canvas.add('line', {
      x1: this.ray.origin.x,
      y1: this.ray.origin.y,
      x2: this.ray.origin.x + dir.x * scale,
      y2: this.ray.origin.y + dir.y * scale,
      stroke: '#2060d0',
      'stroke-width': FigurePixelStrokeWidth,
      'marker-end': 'url(#bvh-traversal-arrow)',
      'data-traversal-ray': 'true',
    });
    this.canvas.add('text', {
      x: this.ray.origin.x + 8,
      y: this.ray.origin.y - 10,
      'font-size': 12,
      'font-family': 'monospace',
      fill: '#2060d0',
    }).textContent = 'ray';
  }

  renderArrowMarker() {
    const defs = createSvgElement('defs');
    const marker = createSvgElement('marker', {
      id: 'bvh-traversal-arrow',
      markerWidth: 10,
      markerHeight: 10,
      refx: 8,
      refy: 3,
      orient: 'auto',
      markerUnits: 'strokeWidth',
    });
    marker.appendChild(createSvgElement('path', {
      d: 'M0,0 L0,6 L9,3 z',
      fill: '#2060d0',
    }));
    defs.appendChild(marker);
    this.canvas.append(defs);
  }

  addBoxDrag(rect, box, index) {
    rect.addEventListener('pointerdown', (event) => {
      const start = this.canvas.pointFromEvent(event);
      const initial = { x: box.x, y: box.y };
      const move = (moveEvent) => {
        const point = this.canvas.pointFromEvent(moveEvent);
        this.boxes[index] = this.clampBox(box, {
          x: initial.x + point.x - start.x,
          y: initial.y + point.y - start.y,
        });
        this.render();
        moveEvent.preventDefault();
      };
      const up = () => {
        document.removeEventListener('pointermove', move);
        document.removeEventListener('pointerup', up);
      };
      document.addEventListener('pointermove', move);
      document.addEventListener('pointerup', up);
      event.preventDefault();
    });
  }

  renderRayHandles() {
    [
      ['ray-origin', 'origin', '#2060d0'],
      ['ray-target', 'target', '#d12'],
    ].forEach(([handleName, key, fill]) => {
      const handle = new FigureDraggablePoint({
        canvas: this.canvas,
        point: this.ray[key],
        radius: 8,
        attrs: {
          fill,
          stroke: '#000',
          'stroke-width': FigurePixelStrokeWidth,
          'data-drag-handle': handleName,
        },
        onDrag: (point) => {
          this.ray[key] = this.clampPoint(point);
          this.render();
        },
      });
      this.canvas.append(handle.element());
    });
  }
}

((scriptElement) => {
  const figure = new BVHSahTraversal();
  scriptElement.parentNode.appendChild(figure.element());
})(document.currentScript);
