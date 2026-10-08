# Module 1：学生参与记录与补充资料

这是事实整理和待回答问题，不是 AI 使用声明或报告正文的代写版本。
实际声明、设计理由、观察和图注由学生自行撰写。

## 1. 你的参与如何记录

以下来自你本次提供的描述，需你补充真实例子；不能据此推断每段代码都由你手写。

| 学生描述的参与 | 可以保存的实际证据 | 需要自己补充的细节 |
|---|---|---|
| 发现算法问题 | 原图、错误数值、提问记录 | 你首先观察到哪个异常？预期是什么？ |
| 理解 AI 建议 | 自己的推导/笔记、追问、手算 | 你如何确认建议的符号、单位或假设正确？ |
| 根据判断修改 | 修改记录、接受/拒绝建议的记录 | 哪些由你亲手编辑，哪些由 agent 执行？ |
| 决定场景和方向 | 你的构图要求、参数选择、真实参考照片 | 哪些布局来自你的原始想法？哪些是 AI 提出的备选？ |
| 检验结果 | 运行命令、实际输出、测试记录 | 哪些测试是你本人运行并解释的？ |

会话中可确认的 AI 工作：integrator、反射/折射、BRDF、相机、网格、parser 的修改；
场景与程序资产生成；测试和技术文档。问题提出与采纳判断属于你的贡献，具体生成/执行
属于 AI 的贡献。两者可以同时真实记录，不必用“全部手写”证明你理解了算法。

工具名称应按实际使用核对；当前会话是 OpenCode / gpt-6.1-sol。若还使用了其它模型，
由你另行记录。不能编造工具、参与比例、手动编辑或未做过的实验。

## 2. 当前图片的准确路径

| 报告标签 | 项目图片 | 对应输入 |
|---|---|---|
| fig:wsrt_simple | `output/WSRT_simple.png` | `scenes/WSRT_simple.pbrt` |
| fig:wsrt_creative | `output/WSRT_creative.png` | `scenes/WSRT_creative.pbrt` |
| fig:drt_simple | `output/DRT_simple.png` | `scenes/DRT_simple.pbrt` |
| fig:drt_creative | `output/DRT_creative.png` | `scenes/DRT_creative.pbrt` |

当前源码是 `reports/module1/module1.tex`。
使用 `\graphicspath{{../../output/}{../output/}{output/}}`，图名不再重复写相对前缀。
四张原图都没有箭头；caption 若提到 arrow，必须由学生添加对应标注并核对。
尚未提供真实参考照片，没有在报告中虚构照片或图注。

## 3. 可以补充的技术证据（由你自行解释）

| Draft 小节 | 已有事实 | 你需要自己回答的问题 |
|---|---|---|
| Reflection | 实际调用 `Reflect(-ray.d,ng)`；MD=8/12；终止返回0 | 输入方向为什么要反号？如何用简单方向验证？ |
| Refraction | 30° air/glass 的透射正弦=1/3；正入射 Fresnel=4%；60° glass/air TIR | 几何法线翻转前后，对入口/出口判断有什么影响？ |
| Shading | 红球 shininess=120，绿球=60，但颜色和光照位置也不同 | 哪些视觉差异能归因于指数，哪些不能？ |
| Shadows | 有限 shadow segment；几何法线偏移；delta ray 沿方向偏移 | 灯后面的物体为何不能遮光？不同偏移用于什么情况？ |
| Geometry | 缩放2倍的单位球，从 z=5 向 -z 命中 t=3 | 若将 object-space 方向归一化，为什么 t 会错误？ |
| Mesh/Texture | 棋盘球 OBJ 有320 faces、960 vt、162 vn；背景盒12 faces | 光滑外观为什么不意味着它是解析 sphere？UV 与 normal 插值有何区别？ |
| Area lights | PCG32 uniform samples；样本贡献含 cosine/pdf；每灯与相机样本均平均 | 部分面光源可见如何形成半影？颗粒噪声为什么不是阴影本身？ |
| Phong energy | 每通道 `kd=clamp(diffuse)*(1-ks)`；半球积分测试 | lobe 归一化与总能量预算各保证什么？ |
| DoF | A=0.18；D=9/13.5；平面焦距沿 forward 测量 | 固定射线长度与焦面距离有什么不同？哪些对象位于焦面之前/之后？ |
| Reinhard | per-channel，W=8；creative 的 film max=1 | 公式、clamp、sRGB 各做什么？有什么证据支持压缩效果，不能仅看面板外观？ |

完整来源和限制见 [module1-verification.md](module1-verification.md)。

## 4. 仍需本人准备的材料

- [ ] 真实创意参考照片及与场景的具体对应关系。
- [ ] 图片中真实存在的箭头、框与局部放大。
- [ ] 一个本人发现问题、理解建议、判断修复并核验结果的完整例子。
- [ ] 对测试预期值的本人解释；不能仅列 PASS 数量。
- [ ] 串行重测基线，或准确说明现有计时的并发负载条件。
- [ ] 当前 draft 的事实核对：glass slab 实际为椭球；焦点公式起点是 camera.position；
      面板纯白不能证明不存在裁剪；没有像素路径证据时不声称特定区域发生 TIR。
- [ ] 本人撰写的 AI 声明、正文与图注；完成后导出 `reports/Module1.pdf`。
