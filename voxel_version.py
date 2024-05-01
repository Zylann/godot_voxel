
# Version information for the voxel project.
# TODO Could not name the file just "version.py" because otherwise `import version` imports the wrong one from Godot...

MAJOR = 1
MINOR = 3
PATCH = 0
STATUS = "dev"

import os


def generate_version_header():
    git_hash = get_git_commit_hash()

    info = {
        "major": MAJOR,
        "minor": MINOR,
        "patch": PATCH,
        "status": STATUS,
        "git_hash": git_hash
    }

    f = open("constants/version.gen.h", "w")

    f.write(
        """/* THIS FILE IS GENERATED DO NOT EDIT */
#ifndef VOXEL_VERSION_GEN_H
#define VOXEL_VERSION_GEN_H

#define VOXEL_VERSION_MAJOR {major}
#define VOXEL_VERSION_MINOR {minor}
#define VOXEL_VERSION_PATCH {patch}
#define VOXEL_VERSION_STATUS "{status}"
#define VOXEL_VERSION_GIT_HASH "{git_hash}"

#endif // VOXEL_VERSION_GENERATED_GEN_H
""".format(**info))

    f.close()


def get_git_commit_hash():
    # Parse Git hash if we're in a Git repo.
    # Copied from Godot methods.py

    githash = ""
    gitfolder = ".git"

    if os.path.isfile(".git"):
        module_folder = open(".git", "r").readline().strip()
        if module_folder.startswith("gitdir: "):
            gitfolder = module_folder[8:]

    head_path = os.path.join(gitfolder, "HEAD")

    if os.path.isfile(head_path):
        head = open(head_path, "r", encoding="utf8").readline().strip()
        if head.startswith("ref: "):
            ref = head[5:]
            # If this directory is a Git worktree instead of a root clone.
            parts = gitfolder.split("/")
            if len(parts) > 2 and parts[-2] == "worktrees":
                gitfolder = "/".join(parts[0:-2])
            head = os.path.join(gitfolder, ref)
            packedrefs = os.path.join(gitfolder, "packed-refs")
            if os.path.isfile(head):
                githash = open(head, "r").readline().strip()
            elif os.path.isfile(packedrefs):
                # Git may pack refs into a single file. This code searches .git/packed-refs file for the current ref's hash.
                # https://mirrors.edge.kernel.org/pub/software/scm/git/docs/git-pack-refs.html
                for line in open(packedrefs, "r").read().splitlines():
                    if line.startswith("#"):
                        continue
                    (line_hash, line_ref) = line.split(" ")
                    if ref == line_ref:
                        githash = line_hash
                        break
        else:
            githash = head
    
    return githash
