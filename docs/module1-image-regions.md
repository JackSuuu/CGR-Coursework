# Module 1 标注区域与事实索引

这是图像区域、场景参数和证据边界的核对清单，不是报告正文或图注。
课程规范 §1.3 禁止 AI 撰写报告；学生自行解释这些标注及照片对应关系。

## 文件与复现

- 原始渲染与 `.log`：`output/{WSRT,DRT}_{simple,creative}.png/.log`。
- 标注图：同目录的 `*_annotated.png`；报告四处图片引用已接入这些文件。
- 生成器：`python3 scripts/annotateModule1Images.py`。
- 工具依赖：Python、Pillow 和可缩放 TrueType 字体；仅用于可选图片标注，不参与 C++ renderer。
- 不更改原 PNG、场景或计时日志；显示放大使用 nearest-neighbour，不添加新的渲染细节。
- 四张图宽度统一为 1440；DRT 额外添加全图参数栏。计算统计值时使用原始渲染，不使用带标注的图片。
- 原图分辨率改变时生成器会报错；构图改变时需要重新核对区域坐标。
- 可指定已有输出目录或字体：`python3 scripts/annotateModule1Images.py --output-dir output --font /path/to/font.ttf`。

## WSRT_simple（A–G）

| ID | 圈选位置 | 对应对象/事实 |
|---|---|---|
| A | 左后镜面球 | `mirrorSphere` / `chrome`，reflectance=(0.95,0.96,0.97)；棋盘与三角形倒影 |
| B | 中央玻璃球 | `glassSphere` / `glass`，eta=1.52；透过球体的棋盘图案发生畸变，同时有 Fresnel 反射 |
| C | 右侧绿色球上的两个高光 | `greenSphere` / `greenBall`，Blinn-Phong shininess=60；场景含两个点光源 |
| D | 左前蓝/米色棋盘球 | `texturedSphere`，解析 sphere；`broad_checker.ppm`，32×32，球面 UV |
| E | 绿色球右侧的地面投影 | 点光源可见性与硬阴影；不同灯的阴影可以重叠 |
| F | 左侧橙色三角形 | `triA`、`triB`，individual triangle primitives |
| G | 前景棋盘地面 | `ground`，旋转后的 plane；PPM 平面 UV，uscale=vscale=0.55 |

点光源本身不作为可见面板绘制；位置/强度的依据是场景文件，不是高光数量的单独证明。
球体/平面/三角形几何分别对应 A–D、G、F。

## WSRT_creative（A–D）

| ID | 圈选位置 | 对应对象/事实 |
|---|---|---|
| A | 瓶架下方托盘中的倒影 | `polishedTrayA/B`，mirror reflectance=(0.91,0.92,0.93)；瓶身、标签与架子倒影 |
| B | 左后透明瓶的瓶身 | `amberBottle` / `amberGlass`，eta=1.5；封闭实心旋转体，有反射/折射路径；不是玻璃 slab |
| C | 笔记本屏幕图案 | `screenA/B`，双三角形 UV；程序 PPM `laptop_screen.ppm`，128×80 |
| D | 笔记本右侧桌面投影 | 两个点光源下的硬阴影边界 |

托盘是为了展示镜面反射而新增的艺术化物件；不是参考照片中的原样部件。
玻璃瓶标签是不透明的独立表面，不把标签图案本身当成透射现象。

## DRT_simple（A–E）

| ID | 圈选位置 | 对应对象/事实 |
|---|---|---|
| A | 左侧棋盘网格球 | `ico` / `uv_geodesic.obj`，320 faces、960 vt、162 vn；逐顶点 normal/UV；不是解析 sphere |
| B | 后方棋盘盒的可见部分 | `checkerBlock` / `display_box.obj`，12 faces、显式 normal/UV；离焦边缘 |
| C | 蓝色球左上方高光 | `ball` / `ballMat`，Phong exponent=60，specularreflectance=(0.35,0.35,0.35) |
| D | 蓝球下方、右侧投影 | 两个矩形面光源采样产生的软阴影边缘；叠加 Monte Carlo 噪声 |
| E | 左前小红球 | `foreground`；薄透镜离焦候选区域，A=0.18、D=9 |

全图参数栏：per-channel extended Reinhard，W=8；spp=128。
右侧可见白色区域为面光源；曝光/纯白不能单独验证色调映射正确。

## DRT_creative（A–D）

| ID | 圈选位置 | 对应对象/事实 |
|---|---|---|
| A | 托盘中的瓶子倒影 | mirror 递归路径；不是 Phong 高光 |
| B | 笔记本右侧桌面投影边缘 | 矩形面光源产生的半影区域，仍可见采样噪声 |
| C | 键盘图案 | `keysA/B`，三角形重心 UV；程序 PPM `laptop_keys.ppm`，128×64 |
| D | 屏幕后左侧绿色椅背 | `leftChairBack`，背景离焦候选区域；A=0.14、D=10 |

全图参数栏：per-channel extended Reinhard，W=8；spp=96。
当前 creative 没有旧版 foreground green sphere，也没有镜面球/玻璃 slab。

## 不能只靠圈选证明的性质

- TIR：没有为上述像素记录路径，不能把某个暗带确定标为 TIR；已有 glass→air 60° 数值测试。
- Fresnel 的角度变化：图中不同区域不构成受控角度实验；已有正入射反射率/TIR 数值测试。
- Phong 能量守恒：高光不是证明；参考现有半球积分检查。
- 正确的 Reinhard 公式、无裁剪：作用于整张图；已有白点映射测试，creative max=1 不能证明无裁剪。
- DoF：圈选是观察位置；尚无 A=0 的同场景对照，不把颗粒噪声等同于离焦。

## 报告中等待学生修改的条目

| 项目 | 已核对事实/待操作 |
|---|---|
| WSRT 最大深度 | simple=8、creative=10；已同步数字 |
| creative 照片 | 两个 figure 都保留 `cgr_ref_img.jpg` 与渲染并排；自行说明照片来源和物体对应 |
| 两个 creative 图注 | 旧 slab/green sphere 图注保存在 `.tex` 注释；可见图注已设为 student placeholder |
| creative 折射观察 | 旧 slab/eta=1.60 描述保存在注释；实际 amberBottle eta=1.5 |
| Phong 示例 | 蓝球位于 `DRT_simple` 标注 C；旧 creative balls 句子移入注释 |
| Reinhard 观察 | 旧无裁剪结论移入注释；学生根据代码/测试/统计自行解释 |
| AI 声明 | 仍需按真实参与及代码/场景/标注生成历史自行修订 |

没有替学生生成新的正文、图注、设计理由或观察结论。
