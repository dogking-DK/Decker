# compile_shaders.py
import os
import subprocess
from pathlib import Path

# 扩展名 -> 阶段
EXT_TO_STAGE = {
    ".vert": "vert",
    ".frag": "frag",
    ".comp": "comp",
    ".geom": "geom",
    ".tesc": "tesc",
    ".tese": "tese",
    ".mesh": "mesh",   # 需要显式 -S mesh
    ".task": "task",   # 需要显式 -S task
}

def compile_shaders(shader_dir, output_dir,
                    glslang_validator="glslangValidator",
                    target_env="vulkan1.3",
                    target_spv="1.6",
                    with_debug=True):
    """
    遍历 shader_dir，把支持的着色器编译为 SPIR-V，输出到 output_dir。
    - 对 .mesh/.task 会自动加上 `-S mesh/-S task`
    - 默认目标环境 Vulkan 1.3 / SPIR-V 1.6（可按需调整）
    - with_debug=True 时使用 -gVS，便于 RenderDoc 显示源码
    """
    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    for root, _, files in os.walk(shader_dir):
        for fname in files:
            ext = Path(fname).suffix.lower()
            if ext not in EXT_TO_STAGE:
                continue

            stage = EXT_TO_STAGE[ext]
            src_path = Path(root) / fname
            # 输出文件名：保持源文件名，加 .spv
            spv_path = output_dir / f"{fname}.spv"

            cmd = [
                glslang_validator,
                "-V",                          # Vulkan 语义
                "-S", stage,                   # ★ 显式指定阶段（mesh/task 必须）
                "--target-env", target_env,    # 例如 vulkan1.2 / vulkan1.3
                "-o", str(spv_path),
                str(src_path)
            ]
            if with_debug:
                # -gVS: 保留调试信息和源码（RenderDoc 能展示原始 HLSL/GLSL）
                cmd.insert(1, "-gVS")

            try:
                r = subprocess.run(cmd, check=True, capture_output=True, text=True)
                print(f"[OK] {src_path} -> {spv_path}")
                if r.stderr.strip():
                    print(r.stderr)
            except subprocess.CalledProcessError as e:
                print(f"[FAIL] {src_path}")
                print("Command:", " ".join(cmd))
                print(e.stdout)
                print(e.stderr)

if __name__ == "__main__":
    compile_shaders(
        shader_dir = "./",
        output_dir = "./spv",
        glslang_validator = "glslangValidator",
        target_env = "vulkan1.3",
        target_spv = "1.6",
        with_debug = True,
    )
    input("Press Enter to exit...")
