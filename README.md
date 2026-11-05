# Argxx

A C++ coding assistant that uses a locally served model to inspect repositories,
draft fixes, run tests, and export reviewable patches. Inspired by local developer-agent
research.

The everyday workflow runs in C++17. Python is only needed for optional LoRA training.
Supported platforms: macOS and Linux.

## What it does

- Repairs a committed snapshot in a separate local Git clone, keeping your working tree untouched.
- Gives the model bounded read, search, edit, create-file, and test actions.
- Feeds test failures back into the next repair turn.
- Exports a Git patch, applies it to a fresh checkout, and runs final validation.
- Records actions, logs, pass/fail results, and elapsed time under `.Argxx/.artifacts`.
- Supports batch issue evaluation and optional Docker test execution.
- Offers GitHub repository analysis and interactive chat as a separate mode.

A passing validation result means the tests you supplied passed. It is not a guarantee
that a patch is correct. Review changes before applying them to your own branch.

## Build and setup

Install a C++17 compiler, CMake 3.20+, Git, CPR, nlohmann-json, yaml-cpp, and Ollama.
On macOS with Homebrew:

```sh
brew install cmake cpr nlohmann-json yaml-cpp ollama
cmake -S . -B build -DARGXX_BUILD_TESTS=ON
cmake --build build -j 2
ctest --test-dir build --output-on-failure
```

On Linux, install the same development libraries through your distribution or your
CMake package manager. Point `CMAKE_PREFIX_PATH` at their installation prefix when
needed. The build does not download dependencies or copy credentials.

Start Ollama and install a model once:

```sh
ollama serve
# In another terminal:
ollama pull gemma4:e4b
./build/agent init
./build/agent doctor
```

Model download requires internet access. After dependencies and model weights are
installed, local repair can run offline. GitHub chat requires a network connection.
`doctor` checks Git, CMake, Ollama, and the configured model. It checks the container
image too when Docker execution is selected.

## Repair an issue

Commit the source and regression tests you want the agent to see, then run:

```sh
./build/agent repair \
  --repo /absolute/path/to/repository \
  --issue 'Describe the observed failure and the expected behavior.' \
  --test-command '["sh", "run-tests.sh"]'
```

Commands are JSON argument arrays, not shell strings. To chain build and test commands,
put them in a repository script and protect that script:

```sh
./build/agent repair --repo /absolute/path/to/repository \
  --issue-file issue.md \
  --test-command '["sh", "run-tests.sh"]' \
  --protect '["run-tests.sh"]'
```

Argxx uses `HEAD` by default; choose another committed snapshot with `--base COMMIT`.
Uncommitted and untracked files are excluded. Preinstall dependencies or choose a
container image containing them. Argxx does not automatically install project dependencies.

The final JSON prints the run directory and result. Inspect `patch.diff`, the action
trace, and validation logs there. To apply a reviewed patch yourself:

```sh
git -C /path/to/repository apply --check /path/to/run/repair/patch.diff
git -C /path/to/repository apply /path/to/run/repair/patch.diff
```

**Host mode executes tests and patched code with your permissions.** Separate clones
are not a security sandbox. For untrusted code, use an appropriately prepared Docker
image with `--execution docker --image YOUR_IMAGE`. Tests then run in a container
with networking disabled, a read-only root filesystem, a writable checkout and `/tmp`,
dropped capabilities, and resource limits. Git and model orchestration stay on the host.
The image must already be installed; Argxx never pulls it automatically.

## Try the complete workflow

```sh
./build/agent fixtures
# Copy the printed manifest path:
./build/agent evaluate --manifest /printed/path/tasks.json
```

These two generated C++ issues are small smoke cases, not a real-world benchmark.
They use the local C++ compiler. See [REPAIR_PIPELINE.md](REPAIR_PIPELINE.md) for custom
issue manifests, separate validation commands, batch limits, and artifacts.

## Configuration and other commands

`agent init` creates defaults without overwriting existing configurations:

- `.Argxx/.artifacts/configs/repair.json`: model, endpoint, execution backend, context and time limits.
- `.Argxx/.artifacts/prompts/system.md`: repair action instructions.
- `.Argxx/.artifacts/configs/training.json`: optional supervised LoRA training settings.

Run from any directory inside your repository; the default artifact root is the nearest
Git repository's `.Argxx/.artifacts`. Outside Git, it is relative to the current directory.
Override it with `--artifacts DIR`. Use `--config FILE` for an alternative repair configuration
or `--model NAME` for a one-run model override.

```sh
./build/agent --help
./build/agent chat owner/repository
./build/agent export-training --run /path/to/successful/run
./build/agent package
```

Chat reads `agent.yaml` in the current directory. Public repositories can be inspected
without a token, subject to GitHub rate limits. Set `GITHUB_PAT` for private repositories
or higher limits. Tokens may also be stored in an untracked `.env` file. Chat produces
analysis reports; use `repair` for actual edits. See [AGENT_USAGE.md](AGENT_USAGE.md).

Packaging creates a ZIP of configuration, prompts, optional adapters, and the native
binary under `.Argxx/.artifacts`. The executable needs compatible shared libraries on
the destination machine; this is a local bundle, not a portable standalone installer.

## Optional post-training

`scripts/train_lora.py` performs supervised LoRA fine-tuning with Transformers and PEFT.
It requires a compatible local Hugging Face checkpoint, training dependencies, suitable
hardware, and reviewed training examples. Training is not required to use Argxx.
Ollama model files are not directly usable as a training checkpoint.
See [REPAIR_PIPELINE.md](REPAIR_PIPELINE.md#optional-post-training).
This feature is not tested and the code is genereated by ai.
## Scope 

The repair engine edits text files up to 100 KB and searches tracked visible files.
Hidden directories, symlinks, dependency/build folders, and validation tests are protected.
It does not support renames, deletions, binary edits, automatic dependency installation,
or automatic commits/PRs. GitHub analysis uses a containment graph and keyword retrieval,
not AST/call graphs or vector search. Larger real-world repair performance is not yet measured.

##Note
Gen AI is used in most of the project.So use with catution.
The project requires Github PAT Token stored it in .env of root folder.

Apache-2.0 license. See [LICENSE](LICENSE).
