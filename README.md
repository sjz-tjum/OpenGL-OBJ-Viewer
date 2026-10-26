# OpenGL OBJ Viewer

计算机图形技术 Assignment 1：OpenGL + C++ 的 OBJ 模型查看器。

项目使用原生 Win32 窗口与 WGL 创建 OpenGL 3.3 Core 上下文，不依赖 GLFW、GLM、Assimp 等第三方库，直接用 MinGW 构建。VS Code 的构建、运行和调试配置已放在 `.vscode/` 中。

## 功能

- 读取 Wavefront OBJ：`v`、`vt`、`vn`、`f`、`o`、`g`、`usemtl`、`mtllib`，支持负索引和 `v//vn` 写法。
- 读取 MTL 材质：`Kd`、`Ks`、`Ke`、`Ns`、`d`，以及 `map_Kd` 漫反射贴图（PNG/JPEG，经 Windows GDI+ 解码）。
- 支持 UV 与 diffuse 贴图：顶点按 `(position, normal, texcoord)` 去重，保留 UV 接缝；着色器按 UV 采样贴图并做 sRGB 到线性的转换。
- 路径处理：文件对话框、拖放和命令行路径按 ANSI/UTF-8 双编码回退，模型与贴图放在中文目录中也能正常加载。
- 自动计算包围盒，将模型归一化到视野中心；没有法线的模型自动做 40° 折痕保护的面积加权平滑。
- 键盘控制模型的平移、旋转、缩放，模型矩阵独立保存。
- 鼠标控制相机：左键环绕（改变 lookAt），右键平移（改变 viewpoint position），滚轮缩放。
- 窗口右下角键盘/鼠标实时监视：按下的键、鼠标左/右键和滚轮会高亮，按 `K` 开关；显示尺寸跟随 HUD 字号一起缩放，方便录制操作演示视频。
- 默认加载 `assets/models/rm2019.obj`（自建 RM2019 步兵机器人，68,618 三角形、10 材质、含 UV 贴图）；找不到时回退到无人机示例。
- 实时显示模型变换、相机参数、FPS、三角形数量和材质数量；HUD 字号可用 `[` / `]` 或 `Ctrl+滚轮` 自由调节。
- 右上角 XYZ 坐标轴 gizmo：点击 X/Y/Z 轴端点转到正方向；点击下方 -X/-Y/-Z 按钮转到负方向。
- 线框叠加、透明地面网格、HUD 开关、一键复位。
- 运行中打开任意 OBJ：按 `O` / `Ctrl+O` 选择文件，或直接把 `.obj` 拖进窗口。
- 兼容只有顶点/面、没有 `vn` 法线的 OBJ：自动做 40° 折角保护的面积加权平滑法线，避免整模型变成硬边。
- 也支持命令行传入任意 OBJ 路径：

```bat
build\OpenGL_OBJ_Viewer.exe D:\models\some_model.obj
```

## 环境

- Windows 10/11
- MinGW-w64 g++ 8.1 或更高版本（当前配置：`C:\mincw`）
- CMake 3.16+
- VS Code + C/C++ 扩展
- 显卡驱动支持 OpenGL 3.3

项目不依赖 Visual Studio。`build.bat` 使用 `MinGW Makefiles` 生成器。

## 构建与运行

### 方式一：VS Code

1. 用 VS Code 打开 `D:\OpenGL_OBJ_Viewer`。
2. `Ctrl + Shift + B` 执行默认构建任务 `build (MinGW)`。
3. 按 `F5` 运行（`launch.json` 已配置为运行 `build/OpenGL_OBJ_Viewer.exe`）。

### 方式二：命令行

```bat
cd /d D:\OpenGL_OBJ_Viewer
build.bat
build\OpenGL_OBJ_Viewer.exe
```

也可以直接双击 `run.bat`。

构建产物：`build\OpenGL_OBJ_Viewer.exe`，采用静态链接，不依赖 MinGW 运行时 DLL。

## 操作说明

| 输入 | 功能 |
| --- | --- |
| `←` / `→` | 模型沿屏幕水平方向左/右平移 |
| `↑` / `↓` | 模型沿屏幕竖直方向上/下平移 |
| `PageUp` / `PageDown` | 模型沿视线方向前/后平移 |
| `W` / `S` | 绕模型 X 轴旋转 |
| `A` / `D` | 绕模型 Y 轴旋转 |
| `Q` / `E` | 绕模型 Z 轴旋转 |
| `+` / `-` | 均匀放大 / 缩小 |
| `0` | 只复位模型变换 |
| `R` | 复位模型与相机 |
| 按住 `Shift` | 平移和旋转加速 |
| 鼠标左键拖动 | 环绕模型旋转相机，改变 lookAt，模型不动 |
| 鼠标右键拖动 | 平移相机视点，模型不动 |
| 滚轮 | 拉近 / 拉远相机 |
| 点击右上角 X / Y / Z 轴端点 | 转到该轴正方向视图 |
| 点击坐标轴下方 -X / -Y / -Z | 转到该轴负方向视图 |
| `O` / `Ctrl+O` | 打开任意 OBJ 文件 |
| 拖放 `.obj` 到窗口 | 直接载入该模型 |
| `F` | 线框叠加开关 |
| `G` | 透明地面网格开关 |
| `H` | HUD 开关（坐标轴 gizmo 也随之显示/隐藏） |
| `K` | 键盘/鼠标实时监视开关 |
| `[` / `]` | 缩小 / 放大 HUD 字号（75%–180%）；键鼠监视器同步缩放 |
| `Ctrl+滚轮` | 连续调节 HUD 字号 |
| `Ctrl+0` | HUD 字号复位到 100% |
| `Esc` | 退出 |

鼠标操作只修改相机，不修改 `ModelTransform`；所以拖动过程中模型本身保持固定在场景中。
键盘操作只修改模型矩阵，不修改相机。

## 目录结构

```text
OpenGL_OBJ_Viewer/
├─ src/
│  ├─ main.cpp           Win32 窗口、WGL 上下文、输入、主循环、HUD
│  ├─ GLFunctions.*      OpenGL 3.3 函数指针加载
│  ├─ ObjModel.*         OBJ / MTL 解析、三角化、归一化
│  ├─ Renderer.*         网格、背景、地面、线框渲染
│  ├─ TextRenderer.*     GDI 字体图集 + OpenGL 文字 HUD
│  ├─ Camera.h           环绕/平移/缩放相机
│  ├─ Math.h             向量、矩阵、透视、lookAt
│  ├─ ModelTransform.h   模型 TRS 变换
│  └─ Shaders.h          全部 GLSL 着色器
├─ assets/models/
│  ├─ rm2019.obj        默认模型（RM2019 步兵机器人，68,618 三角形）
│  ├─ rm2019.mtl        默认模型材质
│  ├─ armor4_black_bg.png 默认模型贴图
│  ├─ drone.obj         备用示例模型（28,256 三角形）
│  ├─ drone.mtl         备用示例材质
│  ├─ drone_preview.png Blender 预览
│  ├─ build_drone.py    示例模型生成脚本
│  └─ samples/          额外测试模型（cow、teapot、spider、armadillo 等）
├─ .vscode/             VS Code 构建、运行、IntelliSense 配置
├─ CMakeLists.txt
├─ build.bat
├─ run.bat
└─ README.md
```

## 实现要点

### OBJ 与 MTL

解析器逐行读取 OBJ。每个面按扇形三角化；顶点按 `(positionIndex, normalIndex)` 去重。没有法线的面使用叉积计算面法线，并按平面着色。材质名前向映射到子网格，每个子网格记录索引区间，绘制时按材质切换 uniform。

模型载入后计算包围盒，平移到原点并按最大边缩放，保证任意尺寸的模型都能直接进入视野。

### 渲染

- OpenGL 3.3 Core，GLSL 330。
- 深度测试、MSAA 4x、面剔除关闭（兼容绕序不规范的 OBJ）。
- 网格使用 Blinn-Phong 主光、补光、轮廓光、环境反射和 ACES 色调映射。
- 地面网格由 fragment shader 用 `fwidth` 做屏幕空间抗锯齿，不生成额外网格；网格在模型之后绘制，开启深度测试但关闭深度写入：模型在网格前方时会挡住网格线，网格在模型前方时网格线会正常压在模型上；网格单元本身是透明 alpha，所以模型沉到地面以下时仍能从格子中看到，效果类似 Blender 的透明地面。
- 线框通过 `glPolygonMode(GL_LINE)` 叠加，并用 polygon offset 避免 z-fighting。
- HUD 先把字体烘焙到一张 192×192 的 GDI 位图，再上传为 OpenGL 纹理，用像素坐标正交投影绘制。

### 坐标与矩阵

模型矩阵：

```text
M = T * Rz * Ry * Rx * S
```

相机由 `yaw`、`pitch`、`distance` 和目标点描述，`lookAt(eye, target, up)` 生成视图矩阵。左键改变 `yaw/pitch`，目标点不动，因此是环绕观察；右键同时平移 `eye` 和 `target`，视线方向不变。两者都不触碰模型矩阵。

方向键的平移根据相机右向量和上向量计算，所以“向右”始终是屏幕上的向右，而不是固定的世界 +X。

右上角坐标轴 gizmo 把世界 X/Y/Z 投影到屏幕：红色 X、绿色 Y、蓝色 Z。点击某个轴的端点会把相机放到该轴正方向并看向模型中心；坐标轴下方的 -X/-Y/-Z 按钮直接转到负方向。左键拖动环绕后 gizmo 继续按当前视角实时更新。

## 常见问题

**构建时报找不到 `cmake` 或 `g++`**

把 CMake 和 `C:\mincw\bin` 加入系统 `PATH`，或在 `build.bat` 中改成自己的路径。`c_cpp_properties.json` 和 `launch.json` 中的 MinGW 路径也需要同步修改。

**启动时报 “OpenGL 3.3 is required”**

更新显卡驱动，或在远程桌面/虚拟机中启用 GPU 加速。项目使用 3.3 Core Profile，不支持只提供 OpenGL 1.1 的软件环境。

**窗口里没有模型**

确认 `assets/models/rm2019.obj` 存在（默认模型），或通过命令行传入绝对路径。程序会按当前目录、可执行文件目录的 `../assets/models` 依次查找；默认模型缺失时回退到 `drone.obj`。

**想换一个 OBJ 模型**

按 `O` 或 `Ctrl+O` 选择文件，也可以把 `.obj` 直接拖进窗口；换模型后相机会按新模型自动重新取景。命令行传入路径同样有效。程序每次启动会写 `build/viewer.log`，记录 OpenGL 版本、加载路径和三角形数量。
