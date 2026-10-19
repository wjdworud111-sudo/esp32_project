import argparse
import base64
import json
import os
import pathlib
import subprocess


PROJECT = pathlib.Path(__file__).resolve().parents[1]


def gh(executable: pathlib.Path, endpoint: str, method: str = "GET", data=None):
    command = [str(executable), "api", endpoint]
    if method != "GET":
        command.extend(["--method", method, "--input", "-"])
    result = subprocess.run(
        command,
        input=None if data is None else json.dumps(data),
        text=True,
        capture_output=True,
        env=os.environ,
    )
    if result.returncode:
        raise RuntimeError(result.stderr.strip() or result.stdout.strip())
    return json.loads(result.stdout) if result.stdout.strip() else None


def source_files():
    files = [
        PROJECT / ".gitignore",
        PROJECT / "README.md",
        PROJECT / "build.cmd",
        PROJECT / "platformio.ini",
        PROJECT / "prepare-release.cmd",
    ]
    for pattern in (
        ".github/workflows/*.yml",
        "include/*.h",
        "src/*.cpp",
        "tools/*.py",
    ):
        files.extend(sorted(PROJECT.glob(pattern)))
    return sorted(set(files))


def main():
    parser = argparse.ArgumentParser(description="Publish the project through the GitHub Git Data API")
    parser.add_argument("repository", help="OWNER/REPOSITORY")
    parser.add_argument("--branch", default="main")
    parser.add_argument("--message", default="Add ESP32 clock firmware and automatic releases")
    parser.add_argument("--gh", required=True, type=pathlib.Path)
    args = parser.parse_args()

    base_commit = None
    base_tree = None
    try:
        reference = gh(args.gh, f"repos/{args.repository}/git/ref/heads/{args.branch}")
        base_commit = reference["object"]["sha"]
        commit = gh(args.gh, f"repos/{args.repository}/git/commits/{base_commit}")
        base_tree = commit["tree"]["sha"]
    except RuntimeError as error:
        if "Git Repository is empty" not in str(error) and "Reference does not exist" not in str(error):
            raise

    entries = []
    files = source_files()
    for file in files:
        content = base64.b64encode(file.read_bytes()).decode("ascii")
        blob = gh(
            args.gh,
            f"repos/{args.repository}/git/blobs",
            "POST",
            {"content": content, "encoding": "base64"},
        )
        entries.append(
            {
                "path": file.relative_to(PROJECT).as_posix(),
                "mode": "100644",
                "type": "blob",
                "sha": blob["sha"],
            }
        )

    tree_request = {"tree": entries}
    if base_tree:
        tree_request["base_tree"] = base_tree
    tree = gh(args.gh, f"repos/{args.repository}/git/trees", "POST", tree_request)
    commit_request = {"message": args.message, "tree": tree["sha"], "parents": []}
    if base_commit:
        commit_request["parents"] = [base_commit]
    new_commit = gh(args.gh, f"repos/{args.repository}/git/commits", "POST", commit_request)

    if base_commit:
        gh(
            args.gh,
            f"repos/{args.repository}/git/refs/heads/{args.branch}",
            "PATCH",
            {"sha": new_commit["sha"], "force": False},
        )
    else:
        gh(
            args.gh,
            f"repos/{args.repository}/git/refs",
            "POST",
            {"ref": f"refs/heads/{args.branch}", "sha": new_commit["sha"]},
        )

    print(f"Published {len(files)} files in commit {new_commit['sha']}")


if __name__ == "__main__":
    main()
