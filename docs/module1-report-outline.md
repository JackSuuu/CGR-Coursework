# Module 1 报告结构与填写模板

这是章节骨架、填写问题和证据索引，不是报告正文。
依据 CWSpec2026.pdf §1.3，学生应自行撰写所有解释、图注和讨论，不直接复制 AI 文本。
完成后由学生导出为 `reports/Module1.pdf`。

## 使用方法

详细的真实实现、场景参数、测试预期值、修复记录与指南纠正项：
[module1-verification.md](module1-verification.md)。
其中 F01–F06 对应 WSRT，F07–F12 对应 DRT；不是需要照抄的正文。

每个功能小节自行填写下面五类内容，保持简短、具体：

- 设计：用了什么算法/数据结构/参数？为什么适合这张图？
- 实现：哪个函数完成这项功能？关键变量的方向或单位是什么？
- 验证：测试输入是什么？预期值是什么？实际是否符合？
- 图像：对应图片的哪个区域？由学生添加箭头或放大框与图注。
- 困难/观察：实际遇到什么错误、如何定位修正、还存在什么限制？

---

# 1. Whitted-Style Ray Tracer（WSRT）

## 1.1 Specular reflection

- [ ] 自己说明 `Reflect(wo,n)` 的方向约定与递归过程。
- [ ] 说明反射率、throughput、最大深度分别控制什么。
- [ ] 选择 `WSRT_simple` 镜面球或 `WSRT_creative` 两侧镜面球区域。
- [ ] 给出镜面方向/实际反射路径的测试证据。

代码索引：`source/core/vec.h::Reflect`；`source/integrator/whitted.cpp::Radiance`。

## 1.2 Specular refraction

- [ ] 自己说明 Snell 定律、Fresnel 分光和 TIR。
- [ ] 解释代码如何区分玻璃入口和出口。
- [ ] 标注 `WSRT_simple` 玻璃球或 `WSRT_creative` 中间玻璃椭球。
- [ ] 核对 30° 折射、正入射 4% Fresnel、60° 玻璃到空气 TIR 等测试。
- [ ] 不把普通透明效果直接当作图中发生 TIR 的证明。

代码索引：`RefractIncident`；`source/scene/material.cpp::FrDielectric`。

## 1.3 Blinn-Phong shading

- [ ] 自己说明 ambient、diffuse、specular 三部分。
- [ ] 说明 halfway vector 和 shininess 对高光的影响。
- [ ] 列出图中所用材质参数，并标注红/绿球的高光。

代码索引：`Material::BlinnPhongSpecular`；WSRT 直接光照循环。

## 1.4 Point light sources

- [ ] 列出场景中的点光源位置与 radiant intensity。
- [ ] 自己说明距离平方衰减，并与实际照明区域对应。

代码索引：`source/scene/light.h::PointLight`；`SceneReader::ParseLight`。

## 1.5 Shadows

- [ ] 自己解释 shadow ray、有限最大距离与 epsilon 偏移。
- [ ] 标注硬阴影边缘。
- [ ] 给出“无遮挡/有遮挡”或自相交测试证据。

代码索引：`source/integrator/integrator.cpp::Occluded`；WSRT shadow ray 分支。

## 1.6 Textures

- [ ] 自己说明球面/平面 UV、PPM 加载和纹理查找。
- [ ] 标注 `WSRT_simple` 棋盘球与地面。
- [ ] 如提及走样，区分纹理采样与尚未实现的像素超采样。

代码索引：`Sphere::UV`、`Plane::UV`、`LoadTexturePPM`、`TextureLookup`。

## 1.7 Geometry

- [ ] 自己概述球、平面、三角形的求交与最近命中选择。
- [ ] 说明缩放时为何保留射线参数 t；三角形重心坐标如何用于 UV。
- [ ] 标注三角形装饰、球体与平面。

代码索引：`source/scene/shape.cpp`；`IntersectScene`；`TransformRay`。

### WSRT 图片材料

- [ ] `output/WSRT_simple.png`：功能展示图，附本人图注和标注。
- [ ] `output/WSRT_creative.png`：创意图，附本人拍摄的参考照片。
- [ ] 自己说明照片中哪些物体/布局启发了创意场景；当前构图尚无真实照片。

# 2. Distributed Ray Tracer（DRT）

## 2.1 Triangle meshes

- [ ] 自己说明 OBJ 的 v/vt/vn/f、逐顶点法线和插值。
- [ ] 指明 `DRT_simple` 棋盘球本身也是 OBJ 网格，不只是背景盒。
- [ ] 列出实际使用的网格和三角形数量。

代码索引：`source/scene/mesh.cpp::LoadOBJ`、`BuildTriangles`；`Triangle::Intersect`。
资产索引：`uv_geodesic.obj`、`display_box.obj`。

## 2.2 Textures on mesh surfaces

- [ ] 自己说明重心 UV 插值、纹理接缝和纹理如何参与 diffuse BRDF。
- [ ] 放大 `DRT_simple` 左侧蓝/米色棋盘网格，必要时补充背景盒区域。
- [ ] 不以“材质绑定了纹理文件”代替 UV 数据和实际纹理输出的证据。

代码索引：`Triangle::Intersect`；`Material::TextureAt`；DRT 的 baseColor 计算。

## 2.3 Area light sources and soft shadows

- [ ] 自己说明矩形光源、均匀随机采样、可见性测试和样本平均。
- [ ] 说明本模块未使用 Grid/Halton，也不计算漫反射间接光。
- [ ] 标注物体投影的半影边缘；区别发光面板、光照区域和软阴影。
- [ ] 从场景文件核对光源面积/辐亮度及样本数。

代码索引：`AreaLight::SamplePoint`、`AreaLight::Area`；DRT 直接光照循环。

## 2.4 Phong BRDF

- [ ] 自己说明 diffuse 和 specular lobe，以及 cosine/PDF 权重。
- [ ] 说明归一化系数和 diffuse/specular 能量预算。
- [ ] 标注蓝色或绿色 Phong 球的高光；不要把镜面网格的反射当成 Phong 高光。
- [ ] 使用半球 `f*cos` 积分测试作为能量核验。

代码索引：`source/scene/material.cpp::Material::F`。

## 2.5 Defocus blur and depth of field

- [ ] 自己说明光圈圆盘采样、平面焦面与射线汇聚。
- [ ] 核对实际 aperture radius、focal distance、spp。
- [ ] 标注 `DRT_simple` 前方红球、左侧纹理网格和后方棋盘盒。
- [ ] 区分离焦模糊、纹理过滤和 Monte Carlo 噪声。
- [ ] 如声称光圈变化效果，用同场景不同光圈输出验证；当前尚无闭合光圈对照图。

代码索引：`source/scene/camera.cpp::Camera::GenerateRay`。

## 2.6 Reinhard tone mapping

- [ ] 核对实际为 per-channel 还是 luminance 模式，以及 white point。
- [ ] 自己说明 tone mapping 与随后 sRGB 编码的区别。
- [ ] 引用白点映射测试；如果讨论 HDR 压缩，说明使用了什么定量或对照证据。
- [ ] 不以“曝光自然”证明公式正确；W=1 是恒等，超过白点仍可能裁剪为纯白。

代码索引：`source/core/image.cpp::ToneMapReinhard`；DRT `Render`。

### DRT 图片材料与参数索引

| 图片 | 分辨率 | spp | 光圈半径 | 焦距 | 白点 | 模式 |
|---|---|---:|---:|---:|---:|---|
| `DRT_simple` | 600×400 | 32 | 0.18 | 9.0 | 8 | per-channel |
| `DRT_creative` | 480×320 | 24 | 0.18 | 13.5 | 8 | per-channel |

- [ ] `output/DRT_simple.png`：附本人特性标注。
- [ ] `output/DRT_creative.png`：附真实参考照片、本人构图说明和标注。
- [ ] 这些参数如发生改动，重新核对 `.pbrt` 和 `.log`。

# 3. PBRT Scene Compatibility

- [ ] 自己列出支持的标准 PBRT 子集及 legacy block 格式。
- [ ] 说明 FOV 约定、变换顺序、材质/光源参数、相对资产路径。
- [ ] 列出不支持或近似处理的功能，说明日志如何报告。
- [ ] 选择标准语法与错误输入 fixture 的日志证据。
- [ ] 不把自身解析测试写成 PBRT-v4 渲染差分验证。

索引：`source/scene/parser.cpp`；`tests/fixtures/standard.pbrt`、`broken.pbrt`。

# 4. Profiling Results

自行串行运行 `./renderReportedImages.sh`，从同名日志填表。

| Scene | Resolution | Actual spp | Primitive count | Render time (s) | Parse warnings/errors |
|---|---|---|---|---|---|
| WSRT_simple | 待核对 | 待核对 | 待核对 | 待实测 | 待核对 |
| WSRT_creative | 待核对 | 待核对 | 待核对 | 待实测 | 待核对 |
| DRT_simple | 待核对 | 待核对 | 待核对 | 待实测 | 待核对 |
| DRT_creative | 待核对 | 待核对 | 待核对 | 待实测 | 待核对 |

- [ ] 注明硬件、OS、编译器版本和优化 flags。
- [ ] 明确 frame time 是 render 阶段，不是完整程序耗时。
- [ ] 不将不同场景间的时间差直接归因于单个参数。

# 5. Use of AI（按 §1.3 自行声明）

- [ ] 本人使用了哪些工具，在哪些代码部分依赖了生成/修改？
- [ ] 本人提供了哪些要求、进行了哪些检查与手动修改？
- [ ] 自行记录一个真实 prompt、错误诊断和修复过程。
- [ ] 如使用 agent 工作流，说明本人参与的程度。
- [ ] 真实说明报告写作过程；报告正文与图注必须由本人完成。

## 导出前检查

- [ ] 四张规定名称的图均与最终场景/日志一致。
- [ ] 两张 creative 图各有真实照片与本人说明。
- [ ] 每个要求有具体图片区域或测试证据。
- [ ] 困难、局限、AI 使用和计时均已真实说明。
- [ ] 输出 `reports/Module1.pdf`，保留场景和引用资产以便重现。

事实记录另见 [`module1-verification.md`](module1-verification.md)。
