# Interactive repository agent

Start Ollama and install the model if necessary:

```sh
ollama serve
# In another terminal:
ollama pull gemma4:e4b
cmake --build build
./build/agent
```

Enter the repository name (or `owner/repository`) at the existing prompt. The application
uses your GitHub credentials and passes the resulting repository URL to `createAgent()`.
Run from the project root or the build directory containing `agent.yaml` and `.env`.

`Agent.cpp` sends CPR POST requests to `http://localhost:11434/api/chat`, using
`gemma4:e4b`, the existing hash map's description and system prompt, and its configured
`max_tokens`. Your original YAML model spelling and configuration parser are preserved.
The configured token environment variable is used for repository reads, with the existing
`PAT_TOKEN`, `GITHUB_TOKEN`, and `PAT` names as fallbacks. Authentication settings are not
sent to Ollama.

Before chat begins, the agent reads the GitHub default branch and open issues and writes:

- `.Argxx/.graph/<owner>/<repository>/graph.json`: file/directory containment graph and commit SHA.
- `.Argxx/.graph/<owner>/<repository>/analysis.md`: Gemma's architecture analysis.
- `.Argxx/.issue/<owner>/<repository>/open-issues.json`: current open issues, excluding pull requests.
- `.Argxx/.issue/<owner>/<repository>/<number>.md`: suggested fixes and verification steps.
- `.Argxx/.issue/<owner>/<repository>/chat.json`: successful chat turns from this session.

Paths are relative to the directory from which you run the program. Repository-specific
subdirectories keep reports for different repositories separate. Reruns replace current
reports; old issue reports remain, so `open-issues.json` is the current open-issue list.

After initial analysis, type messages and press Enter. Type `exit` to finish, or send EOF.
Recent successful chat turns are retained. A failed chat request prints an error and lets
you retry. Each request has a five-minute timeout; `exit` is read between requests.

The JSON graph records file containment, not AST/call dependencies. Source analysis samples
up to 40 supported files / 120 KB, excluding hidden paths, symlinks, dependency/build folders,
and files larger than 24 KB. Each model request uses bounded excerpts ranked by keyword
overlap. This is not vector semantic retrieval. Model reports are suggestions, not verified
fixes. The application does not execute generated commands, edit GitHub, or perform web search.

Offline regression checks:

```sh
cmake -S . -B build -DARGXX_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Ollama API reference: https://docs.ollama.com/api/chat
