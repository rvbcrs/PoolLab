Import("env")

import os

def remove_ds_store_files(root_dir: str) -> None:
	for current_root, dirnames, filenames in os.walk(root_dir):
		for filename in filenames:
			if filename == ".DS_Store":
				file_path = os.path.join(current_root, filename)
				try:
					os.remove(file_path)
					print(f"Removed {file_path}")
				except OSError as error:
					print(f"Warning: failed to remove {file_path}: {error}")

project_root = env["PROJECT_DIR"]
remove_ds_store_files(project_root)

