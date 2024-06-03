import datetime
import subprocess

current_datetime = datetime.datetime.now().strftime("%Y-%m-%d %H:%M")
git_branch = subprocess.check_output(['git', 'rev-parse', '--abbrev-ref', 'HEAD']).decode().strip()
git_commit = subprocess.check_output(['git', 'rev-parse', 'HEAD']).decode().strip()[:7]

output_string = f"{current_datetime} | {git_branch}: {git_commit}"
print(output_string)

string = f"#define BUILD_INFO \"{output_string}\""
with open("include/build_info.h", "w") as f:
    f.write(string)