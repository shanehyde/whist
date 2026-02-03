---
name: create-pr
description: Create a PR from the current branch. Ensure the PR has a conventional commit style title and description summarizing the changes made.
allowed-tools: Bash(git:*), Read, Grep, Edit
user-invocable: true
---

# Create Pull Request Skill

## Overview

Create a pull request (PR) from the current branch with a clear, conventional commit style title and description summarizing the changes made.

## Instructions

When creating a PR, follow these steps:

1. **Check the current branch** - Run `git branch --show-current` to determine the current branch name.
2. **Determine the base branch** - Identify the target branch for the PR, typically `main` or `develop`. You may need to ask the user if unsure.
3. **Ensure unmerged** - Ensure this branch is not already used in a merged PR.
4. **Generate PR title and description** - Based on the commits in the current branch, generate a PR title and description:
   - The title should follow conventional commit style, summarizing the main change (e.g., `feat: add user authentication`).
   - The description should provide more detail about what was changed and why, referencing any relevant issues or tickets.
5. **Create the PR** - Use the appropriate git hosting service CLI (e.g., `gh pr create` for GitHub) or web interface to create the pull request with the generated title and description.
6. **Provide the PR link** - After creating the PR, provide the user with the link to the newly created pull request for review.
