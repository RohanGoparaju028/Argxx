# Repair, evaluation, and artifacts

All routine commands use the native `agent` executable. Python is reserved for optional
model training. Existing artifacts from the earlier Python prototype remain readable;
new native runs use the same report and trace structure with label `local-native`.

## Issue manifests

`agent repair --task task.json` accepts one object. `agent evaluate --manifest tasks.json`
accepts an array of objects, for example:

```json
[
  {
    "id": "issue-123",
    "repo": "/absolute/path/to/repository",
    "base_commit": "HEAD",
    "issue": "Describe observed and expected behavior.",
    "test_command": ["sh", "run-tests.sh"],
    "validation_command": ["sh", "validate.sh"],
    "protected_paths": ["run-tests.sh", "validate.sh"]
  }
]
```

Use absolute repository paths. Task IDs must be unique and contain only letters,
numbers, hyphens, and underscores. `base_commit` defaults to `HEAD`;
`validation_command` defaults to `test_command`. Commands execute inside the checkout.
An external host validator may use an absolute path; Docker mode can only access the
checkout, container filesystem and temporary directory. Include validators in the
chosen commit/image when using Docker. Test-like paths are automatically protected;
use `protected_paths` for custom runners and fixtures.

## Repair lifecycle

1. Clone the committed snapshot and record baseline test output.
2. Supply the issue, tracked file list, and failure to the local model.
3. Execute one read/search/edit/write/test/finish action per turn.
4. Feed action results and failures back into the conversation.
5. Export a Git diff, including newly created files.
6. Apply the patch to a fresh checkout and run the final validator.

A nonempty patch plus successful fresh-checkout validation counts as a pass. A baseline
that already passes is recorded and excluded from automatic training exports. A failing
baseline can indicate an environment problem rather than a code bug, so review logs.
Failed or interrupted inference preserves partial patches when possible. Those patches
are not validated fixes.

Model actions are bounded: up to 24 turns, 100 KB text reads/edits, 80 search matches,
and recent conversation history. The model cannot request arbitrary shell commands;
it can only invoke the test command you supplied. This does not make patched code safe
on the host. Docker execution is available for isolation; it requires a functioning
Docker daemon and a preinstalled image with your project's test dependencies.

## Configuration

`agent init` writes defaults under `.Argxx/.artifacts`; it preserves existing files.
`configs/repair.json` controls:

| Setting | Default |
|---|---|
| `model` | `gemma4:e4b` |
| `endpoint` | `http://127.0.0.1:11434/api/chat` |
| `max_steps` | 24 |
| `task_seconds` | 900 |
| `test_seconds` | 120 |
| `total_seconds` | 43200 |
| `num_ctx` | 16384 |
| `num_predict` | 2048 |
| `execution` | `host` |
| `docker_image` | empty; required for Docker |

Inference requests are bounded to three minutes or the remaining task budget. Test
process groups are terminated on timeout. Docker timeout cleanup and partial-patch
preservation may add a few seconds. Batch timing includes setup and validation.
Subprocess output is drained continuously and only a bounded tail is kept in memory.

Progress goes to stderr; the final summary goes to stdout. Exit status is 0 for success,
1 for a failed repair/evaluation/diagnostic, and 2 for invalid input or setup errors.
No source patch is automatically applied to the original repository.

## Output layout

```text
.Argxx/.artifacts/
  configs/repair.json
  configs/training.json
  prompts/system.md
  fixtures/<id>/tasks.json
  runs/<id>/config.json
  runs/<id>/tasks.json
  runs/<id>/summary.json
  runs/<id>/<issue>/patch.diff
  runs/<id>/<issue>/report.json
  runs/<id>/<issue>/baseline.json
  runs/<id>/<issue>/trace.json
  runs/<id>/<issue>/test-*.json
  runs/<id>/<issue>/validation.json
  runs/<id>/<issue>/workspace/
  runs/<id>/<issue>/validation/
  training/train-<run-id>.jsonl
  adapters/main_lora/                 Only after training
  package/<id>/
  argxx-bundle-<id>.zip
```

Artifacts are ignored by Git. Remove old run directories yourself when no longer needed;
each run includes two repository clones. Traces and logs can include repository content.
Packaging excludes runs, repositories, training examples and credentials. The manifest
contains file SHA-256 hashes and reports whether trained adapter files are present.
Packages are local configuration/runtime bundles, not competition submissions.

## Optional post-training

Export successful model trajectories:

```sh
./build/agent export-training --run /absolute/path/to/run
python3 -m venv .Argxx/.artifacts/training-env
.Argxx/.artifacts/training-env/bin/pip install -r scripts/requirements-training.txt
```

Edit `.Argxx/.artifacts/configs/training.json`: set `base_model` to a local Hugging Face
checkpoint and `dataset` to the printed JSONL path. Then:

```sh
.Argxx/.artifacts/training-env/bin/python scripts/train_lora.py \
  --config .Argxx/.artifacts/configs/training.json
```

The checkpoint must support `AutoModelForCausalLM` and a compatible chat template in
your installed Transformers version. Install versions and select LoRA target modules
appropriate to your exact model. Training does not download weights or use remote code.
Full base weights must fit in memory; quantized training is not implemented. This is
supervised fine-tuning, not reinforcement learning.

Only the final assistant response contributes to loss. Overlength examples are skipped
and counted. Successful training saves adapter weights/config, tokenizer, metrics, data
hash and library versions under the configured adapter directory. Existing outputs are
never silently overwritten. Smoke trajectories validate plumbing only: use diverse,
reviewed demonstrations and reserve separate repositories/issues for evaluation.

To evaluate a trained adapter, serve it through a compatible Ollama-style chat endpoint
and configure its model name and endpoint. Argxx does not automatically import PEFT
weights into Ollama. Adapter/runtime compatibility depends on the selected model.

## Verification

```sh
cmake -S . -B build -DARGXX_BUILD_TESTS=ON
cmake --build build -j 2
ctest --test-dir build --output-on-failure
./build/agent fixtures
./build/agent evaluate --manifest /printed/path/tasks.json
```

Native tests exercise failed-test revision, fresh patch replay, new files, path/test
protection, symlinks, subprocess argument handling, output bounds, deadlines, partial
patch preservation, and independent validation failures. The generated model smoke
issues test empty-input handling and clamping in C++; they are not a real-world benchmark.
