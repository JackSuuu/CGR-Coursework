# 参考照片与 creative 场景：事实对应记录

这是输入、几何、材质和参数索引，不是报告正文或图注。

## 参考输入

- 用户提供的原图：`scenes/photograph/cgr_ref_img.jpg`。
- 拍摄者与地点：学生本人在学校 Teviot 楼拍摄（用户本次明确提供）；未提供拍摄日期，不推测日期。
- 显示方向：LaTeX 图中顺时针旋转 90°，不修改 JPEG 原文件。
- 两个 creative 使用同一照片的布局；不是逐像素复刻或相机标定实验。
- 屏幕、键盘、标签及大理石纹理均为程序资产，不是从照片裁切并贴到渲染图。

## 对象映射

| 照片中可见内容 | 场景对象 | 具体实现 |
|---|---|---|
| 打开的笔记本 | laptopBase、screenFrame、screenA/B、keysA/B、trackpadA/B | 薄盒 OBJ、旋转后的双三角形面板；屏幕和键盘 PPM |
| 浅色大理石桌面、深色木边 | marbleInset、tableFrame | 两个不同尺寸的盒形 OBJ；marble/wood PPM |
| 左侧金属调料架 | rackLower、rackUpper、rackPost0–3、handlePost0–1、rackHandle | 椭圆环、杆的三角形网格；mirror 材质 |
| 展示反射的艺术化新增物件（不是照片中的原样部件） | trayBase、polishedTrayA/B | 深色薄盒底座、3.20×2.84 的水平双三角形镜面；chrome reflectance=(0.91,0.92,0.93)，表面 y=0.115 |
| 瓶子、浅色瓶盖与标签 | amberBottle、sauceBottle、mustardBottle 及其 Cap/Label | 旋转体 OBJ、圆柱盖、带 UV 的环形标签；其中 amberBottle 使用 eta=1.5 的玻璃 |
| 背景绿色椅子 | leftChairBack/Seat、rightChairBack/Seat | 缩放球体、绿色漫反射/高光材质 |
| 暖色室内环境 | wall、floor、warmCeiling、coolFill | 木色表面、暖主光与冷补光；光源位置/形状是建模近似 |

## 光学与几何近似

- 瓶子是封闭旋转体，不是空心壳加液体；玻璃 tint 为表面权重，不是体吸收模型。
- 架子以低面数网格近似钢丝；小镜面面片会有可见折面/走样。
- 大面积托盘用于展示瓶身、标签和架子的倒影；不是在纹理中绘制倒影，使用 integrator 的递归镜面路径。
- 标签图案为抽象色带，不复刻品牌或文字。
- 屏幕表面有自发光项，但当前 DRT 没有 diffuse indirect lighting；不能声称模拟了屏幕照亮桌面。
- 两张图使用相同布局；WSRT/DRT 的材质模型、光源和相机采样不同，不是只改变 integrator 的受控对比。
- 照片来源已由学生确认是本人在 Teviot 楼拍摄；报告中的来源文字仍由学生自行填写。

## 当前参数与输出

| 项目 | WSRT_creative | DRT_creative |
|---|---|---|
| 场景 | `scenes/WSRT_creative.pbrt` | `scenes/DRT_creative.pbrt` |
| 输出 | `output/WSRT_creative.png` | `output/DRT_creative.png` |
| 分辨率 | 960×640 | 480×320 |
| 相机位置 | (4.4,4.2,8.2) | (4.4,4.2,8.2) |
| LookAt | (-0.1,0.9,-0.05) | (-0.1,0.9,-0.05) |
| FOV（较大轴） | 46° | 46° |
| 最大深度 | 10 | 5 |
| 实际主样本/像素 | 1 | 96 |
| 光圈/焦距 | pinhole | 0.14 / 10.0 |
| 基元数 | 1230 | 1230 |
| 灯 | 2 个点光源 | 2 个矩形面光源 |
| 面积 | — | 29.4、9.0 |
| Reinhard | — | per-channel，W=8 |
| 本次 render time | 22.864423 s | 437.498226 s |
| parse warnings/errors | 0 / 0 | 0 / 0 |

本次两张 creative 串行渲染；计时不含全部解析和写图时间，光源/材质模型不同，不是仅切换 integrator 的性能实验。

## 程序资产

- `scripts/generateCafeCreative.py`：标准库 Python 场景/资产生成器。
- `cafe_bottle.obj`：144 triangles，显式 normal/UV，闭合 caps。
- `cafe_rod.obj`：32 triangles；`cafe_rack_ring.obj`：160 triangles。
- `cafe_label_band.obj`：24 triangles，非封闭标签表面。
- `cafe_marble.ppm`：128×128；`laptop_screen.ppm`：128×80。
- `laptop_keys.ppm`：128×64；`cafe_label.ppm`：48×32；均为 P3。
- `display_box.obj`、`wood.ppm` 继续使用现有核心资产。

## 学生需核对的报告材料

- 参考照片已并排链接到两张 creative 的 LaTeX 图片区。
- 旧 draft 的玻璃 slab、两侧镜面球、绿色球及书本描述不再对应新 creative。
- 四张图片已生成独立 `*_annotated.png` 并接入报告，区域索引见 `module1-image-regions.md`。
- 上一轮 agent 已生成报告正文和图注，当前 PDF 不是仅经语法润色的版本；该参与历史应真实记录并向老师确认处理方式。
- 学生自行完成正文、图注和焦距/深度解释；旧 creative 图注/观察仍在源码注释中保留。
- `simple` 场景没有改变，主要功能证据仍可使用其清晰区域及现有数学测试。
