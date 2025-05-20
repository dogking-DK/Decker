import os
import subprocess

def compile_shaders(shader_dir, output_dir, glslang_validator_path="glslangValidator"):
    """
    Compiles all .vert and .frag files in the shader_dir to SPIR-V using glslangValidator.
    
    Args:
        shader_dir (str): Directory containing shader files.
        output_dir (str): Directory to save compiled .spv files.
        glslang_validator_path (str): Path to the glslangValidator executable.
    """
    # Ensure output directory exists
    os.makedirs(output_dir, exist_ok=True)

    # Traverse the shader directory
    for root, _, files in os.walk(shader_dir):
        for file in files:
            # Check if the file is a .vert or .frag file
            if file.endswith(".vert") or file.endswith(".frag") or file.endswith(".comp"):
                shader_path = os.path.join(root, file)
                spv_filename = f"{file}.spv"
                spv_path = os.path.join(output_dir, spv_filename)

                try:
                    # Compile the shader using glslangValidator
                    subprocess.run(
                        [glslang_validator_path, "-V", shader_path, "-o", spv_path],
                        check=True,
                        stdout=subprocess.PIPE,
                        stderr=subprocess.PIPE,
                    )
                    print(f"Compiled: {shader_path} -> {spv_path}")
                except subprocess.CalledProcessError as e:
                    print(f"Failed to compile {shader_path}: {e.stderr.decode('utf-8')}")
                except FileNotFoundError:
                    print(f"Error: glslangValidator not found at {glslang_validator_path}")
                    return

if __name__ == "__main__":
    # Directory containing your shader files
    shader_directory = "./"

    # Directory where the compiled .spv files will be saved
    output_directory = "./spv/"

    # Path to glslangValidator executable (adjust if necessary)
    glslang_validator_exe = "glslangValidator"

    # Compile shaders
    compile_shaders(shader_directory, output_directory, glslang_validator_exe)

    # Prevent the command window from closing immediately..
    input("Press Enter to exit...")