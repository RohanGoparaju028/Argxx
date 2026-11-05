# Repository analysis and chat

For local repair, see [README.md](README.md). GitHub analysis/chat is a separate mode:

```sh
ollama serve
# In another terminal, after building and installing the model:
./build/agent chat owner/repository
```

Run where `agent.yaml` is available (the project root or build directory), or copy the
installed configuration from `share/argxx/agent.yaml` to your working directory. The
configured `agent.model.name`, `endpoint`, and `max_tokens` are honored. Chat needs
network access to read GitHub; model inference uses your configured Ollama endpoint.

Public repositories need no token, subject to GitHub rate limits. For authenticated
access, set `GITHUB_PAT` in the environment or a local untracked `.env` file. Existing
environment values take precedence; `PAT_TOKEN`, `GITHUB_TOKEN`, and `PAT` are accepted
as fallbacks. Credentials are never included in model prompts.

Before chat begins, Argxx reads the default branch and open issues, then writes:

- `.Argxx/.graph/<owner>/<repository>/graph.json`: file containment graph and commit SHA.
- `.Argxx/.graph/<owner>/<repository>/analysis.md`: model-generated architecture analysis.
- `.Argxx/.issue/<owner>/<repository>/open-issues.json`: current open issues.
- `.Argxx/.issue/<owner>/<repository>/<number>.md`: suggested fixes and verification steps.
- `.Argxx/.issue/<owner>/<repository>/chat.json`: successful chat turns.

Reports are relative to the current directory and refreshed on reruns. Old issue reports
remain; `open-issues.json` identifies the current open issues. Type `exit` or send EOF to
leave chat. A failed model request reports an error and allows retry. Recent successful
turns are retained, and requests have a five-minute timeout.

Analysis samples up to 40 visible supported source files / 120 KB. Each request uses
bounded excerpts ranked by keyword overlap. The graph records containment, not AST or
call dependencies. Chat does not edit source, execute tests, perform web search, or
modify GitHub. Its suggestions are not verified fixes; use the repair command for that
workflow.
