# Decker

Decker 是一个基于 **Vulkan + C++23 + CMake** 的实时渲染与物理实验项目，包含：

- Vulkan 渲染基础设施（设备/交换链/管线/资源管理）
- Render Graph 与多种渲染 Pass（Opaque、Outline、Blur、Distortion、UI Gizmo 等）
- 物理模块（流体、质量弹簧、PBD、Euler/Verlet 等求解器）
- 编辑器向 UI 能力（Hierarchy、ControlPanel、Gizmo、Tool）
- 资源导入与缓存（Assimp / fastgltf / 贴图与网格加载）

---

## 环境要求

- CMake **4.2+**
- C++23 编译器
- Python 3（用于 Shader 编译脚本）
- Vulkan SDK（或可用 Vulkan 开发环境）
- vcpkg（通过 `VCPKG_ROOT` 环境变量提供 toolchain）

> 说明：根目录 `CMakeLists.txt` 会在未检测到 `VCPKG_ROOT` 时直接报错退出。

---

## 依赖安装（vcpkg）

仓库提供了依赖安装脚本：

```bash
bash third_lib_install.sh
```

该脚本会安装项目主要依赖（如 SDL3、glm、imgui、implot、assimp、fastgltf、slang、tracy、vk-bootstrap、Vulkan Memory Allocator、pybind11、sqlite3 等）。

---

## 构建方式

### Linux / macOS（示例）

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

### Windows（示例）

```powershell
cmake -S . -B build
cmake --build build --config Release
```

构建完成后主程序为：

- `build/bin/Decker`（Linux/macOS）
- `build/bin/Release/Decker.exe`（Windows 多配置生成器下常见路径）

---

## 运行

程序入口位于 `src/main/main.cpp`，当前直接初始化引擎并进入主循环：

```cpp
engine.init();
engine.run();
engine.cleanup();
```

可直接运行生成的可执行文件。

---

## Shader 编译与资源拷贝

- `assets/CMakeLists.txt` 定义了 `compile_shaders` 目标。
- 构建时会调用 `assets/shaders/build_glsl_to_spv.py`，将 `assets/shaders/glsl` 下着色器编译到 `assets/shaders/spv`。
- 主程序目标依赖 `compile_shaders`，并在构建后自动将 `assets/` 复制到可执行文件旁边，方便直接运行。

---

## 测试

项目启用了 CTest：

- 测试目标：`DeckerPhysicsTests`
- 测试文件：`src/tests/FluidSystemTests.cpp`

运行示例：

```bash
ctest --test-dir build --output-on-failure
```

---

## 目录概览

```text
Decker/
├─ assets/                # 资源与 shader
├─ models/                # 示例模型
├─ src/
│  ├─ core/               # Vulkan/窗口/输入等底层能力
│  ├─ gpu/                # 渲染图与渲染 pass
│  ├─ physics/            # 物理系统与求解器
│  ├─ runtime/            # 运行时渲染与资源管理
│  ├─ scene/              # 场景结构
│  ├─ ui/                 # 编辑器界面、gizmo、tool
│  └─ tests/              # 单元测试
├─ CMakeLists.txt
└─ third_lib_install.sh
```

---

## 许可证

本仓库包含 `LICENSE` 文件，请按其中条款使用。
