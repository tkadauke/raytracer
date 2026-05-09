# 8. Materials and BRDFs

> **Status:** Stub. Narrative not written yet. Source anchors and
> embed plan are committed below; expect prose to fill in.

## Arc

What a material is (a `shade()` function over a hit), what a BRDF
is (the directional reflectance distribution), how Matte / Phong /
Reflective / Transparent / Portal compose primitive BRDFs / BTDFs.
Pins the new `BSDF` interface as the container abstraction. The
material parameter space (ambient/diffuse/specular coefficients)
gets concrete with sweep tables. Behavior contracts pinned by
[`test/functional/render/materials/`](../../../test/functional/render/materials/).

## Source anchors

<!-- source-anchors -->
- `include/render/materials/Material.h`
- `include/render/materials/MatteMaterial.h`
- `include/render/materials/PhongMaterial.h`
- `include/render/materials/ReflectiveMaterial.h`
- `include/render/materials/TransparentMaterial.h`
- `include/render/materials/PortalMaterial.h`
- `include/render/brdf/BRDF.h`
- `include/render/brdf/BTDF.h`
- `include/render/brdf/Lambertian.h`
- `include/render/brdf/GlossySpecular.h`
- `include/render/brdf/PerfectSpecular.h`
- `include/render/brdf/PerfectTransmitter.h`
- `include/render/bsdf/BSDF.h`
- `test/functional/render/materials/MatteMaterialTest.cpp`
- `test/functional/render/materials/PhongMaterialTest.cpp`
<!-- /source-anchors -->

## Planned embeds

<!-- widget: phong_lambertian_lobes -->
<!-- widget: reflective_material_recursion -->
<!-- widget: transparent_material_refraction -->
<!-- widget: portal_material_ray_redirection -->

Plus material doc renders (Matte / Phong / Reflective / Transparent
sweeps).

## See also

- Volume index: [Volume II — Ray rendering](README.md)
- Previous: [7. Primitives and intersection](07-primitives-and-intersection.md)
- Next: [9. Lights and shading](09-lights-and-shading.md)
- Texture inputs: [11. Textures](11-textures.md)
