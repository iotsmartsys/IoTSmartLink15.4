#!/usr/bin/env bash

set -euo pipefail

fail() {
    printf 'Erro: %s\n' "$*" >&2
    exit 1
}

command -v git >/dev/null 2>&1 || fail "git não está disponível."
command -v curl >/dev/null 2>&1 || fail "curl não está disponível."
command -v python3 >/dev/null 2>&1 || fail "python3 não está disponível."

REPOSITORY_ROOT="$(git rev-parse --show-toplevel 2>/dev/null)" ||
    fail "execute o script dentro do repositório Git."

CURRENT_BRANCH="$(git -C "$REPOSITORY_ROOT" branch --show-current)"

case "$CURRENT_BRANCH" in
    spec/*)
        ;;
    "")
        fail "o repositório está em detached HEAD; use uma branch spec/*."
        ;;
    *)
        fail "a branch atual deve iniciar com spec/. Branch atual: $CURRENT_BRANCH"
        ;;
esac

SPEC_SLUG="${CURRENT_BRANCH#spec/}"

case "$SPEC_SLUG" in
    ""|*/*|*[!a-z0-9-]*)
        fail "o nome após spec/ deve conter somente letras minúsculas, números e hífens."
        ;;
esac

if [ -n "$(git -C "$REPOSITORY_ROOT" status --porcelain --untracked-files=all)" ]; then
    fail "a submissão exige árvore de trabalho limpa."
fi

ENV_FILE="$REPOSITORY_ROOT/.env"

[ -f "$ENV_FILE" ] || fail "arquivo .env não encontrado na raiz do repositório."

if ! git -C "$REPOSITORY_ROOT" check-ignore -q .env; then
    fail "o arquivo .env precisa estar ignorado pelo Git."
fi

set -a
# shellcheck disable=SC1090
. "$ENV_FILE"
set +a

: "${SUBMMITION_IMPLEMENTATION_URL:?Defina SUBMMITION_IMPLEMENTATION_URL no arquivo .env}"
: "${TOKEN_EKOM:?Defina TOKEN_EKOM no arquivo .env}"
: "${SUBMITTED_BY:?Defina SUBMITTED_BY no arquivo .env}"

SPECIFICATION_CANDIDATES=()

while IFS= read -r candidate; do
    candidate_name="$(basename "$candidate" .md)"
    candidate_slug="$(
        printf '%s' "$candidate_name" |
            tr '[:upper:]' '[:lower:]' |
            tr '_' '-' |
            sed 's/[^a-z0-9-]//g'
    )"

    if [ "$candidate_slug" = "$SPEC_SLUG" ]; then
        SPECIFICATION_CANDIDATES+=("$candidate")
    fi
done < <(find "$REPOSITORY_ROOT/docs/specs" -maxdepth 1 -type f -name '*.md' -print)

if [ "${#SPECIFICATION_CANDIDATES[@]}" -eq 0 ]; then
    fail "nenhuma especificação corresponde à branch $CURRENT_BRANCH."
fi

if [ "${#SPECIFICATION_CANDIDATES[@]}" -gt 1 ]; then
    fail "mais de uma especificação corresponde à branch $CURRENT_BRANCH."
fi

SPECIFICATION_FILE="${SPECIFICATION_CANDIDATES[0]}"
SPECIFICATION_PATH="${SPECIFICATION_FILE#"$REPOSITORY_ROOT"/}"

CHANGE_ID="$(
    sed -nE 's/^\*\*ID:\*\*[[:space:]]*`([^`]+)`.*/\1/p' "$SPECIFICATION_FILE" |
        head -n 1
)"

[ -n "$CHANGE_ID" ] ||
    fail "não foi possível obter o campo **ID:** da especificação $SPECIFICATION_PATH."

git -C "$REPOSITORY_ROOT" fetch --quiet origin "$CURRENT_BRANCH" ||
    fail "não foi possível consultar a branch remota $CURRENT_BRANCH."

LOCAL_HEAD="$(git -C "$REPOSITORY_ROOT" rev-parse HEAD)"
REMOTE_HEAD="$(git -C "$REPOSITORY_ROOT" rev-parse FETCH_HEAD)"
if [ "$LOCAL_HEAD" != "$REMOTE_HEAD" ]; then
    fail "a branch local não está sincronizada com origin/$CURRENT_BRANCH."
fi

ANALYSIS_REPORT_PATH="$(python3 - \
    "$REPOSITORY_ROOT" \
    "$SPECIFICATION_PATH" \
    "$SPEC_SLUG" <<'PY'
import re
import subprocess
import sys
from pathlib import Path

root = Path(sys.argv[1])
specification_path = sys.argv[2]
spec_slug = sys.argv[3]
report_prefix = f"docs/reports/{spec_slug}/analysis/"

tracked = subprocess.run(
    ["git", "-C", str(root), "ls-files", f"{report_prefix}*implementability-analysis.md"],
    check=True,
    capture_output=True,
    text=True,
).stdout.splitlines()

for report_path in sorted(tracked, reverse=True):
    report = (root / report_path).read_text(encoding="utf-8")
    if f"**Especificação:** `{specification_path}`" not in report:
        continue
    if "**Papel:** Engenheiro Analista" not in report:
        continue
    if "**Estado:** Concluído" not in report:
        continue

    classification_match = re.search(
        r"^\*\*Classificação principal:\*\*\s*(.+?)\s*$",
        report,
        re.MULTILINE,
    )
    if not classification_match:
        continue

    classification = re.sub(
        r"\*\*([^*\n]+)\*\*",
        r"\1",
        classification_match.group(1),
    ).strip()
    if not re.fullmatch(r"Pronta(?:\s+\[`Ready`\])?", classification):
        continue

    revision_match = re.search(
        r"^\*\*Revisão confrontada:\*\*\s*`([0-9a-f]{40})`\s*$",
        report,
        re.MULTILINE,
    )
    if not revision_match:
        continue

    analyzed_revision = revision_match.group(1)
    ancestry = subprocess.run(
        [
            "git",
            "-C",
            str(root),
            "merge-base",
            "--is-ancestor",
            analyzed_revision,
            "HEAD",
        ]
    )
    if ancestry.returncode != 0:
        continue

    short_revision = subprocess.run(
        ["git", "-C", str(root), "rev-parse", "--short=7", analyzed_revision],
        check=True,
        capture_output=True,
        text=True,
    ).stdout.strip()
    if not re.fullmatch(
        rf".+-{re.escape(short_revision)}-.+-implementability-analysis\.md",
        Path(report_path).name,
    ):
        continue

    result = subprocess.run(
        [
            "git",
            "-C",
            str(root),
            "diff",
            "--quiet",
            analyzed_revision,
            "HEAD",
            "--",
            specification_path,
        ]
    )
    if result.returncode != 0:
        continue

    unanalyzed = subprocess.run(
        [
            "git",
            "-C",
            str(root),
            "diff",
            "--name-only",
            f"{analyzed_revision}..HEAD",
            "--",
            ".",
            f":(exclude){report_prefix}**",
        ],
        check=True,
        capture_output=True,
        text=True,
    ).stdout.strip()
    if unanalyzed:
        continue

    print(report_path)
    break
else:
    raise SystemExit("nenhum relatório Ready aplicável à versão corrente foi encontrado")
PY
)" || fail "não foi possível localizar análise Ready aplicável."

export CHANGE_ID SPECIFICATION_PATH ANALYSIS_REPORT_PATH CURRENT_BRANCH SUBMITTED_BY

PAYLOAD="$(python3 <<'PY'
import json
import os

print(json.dumps({
    "event": "submit_for_implementation",
    "source": "repository_command",
    "change_id": os.environ["CHANGE_ID"],
    "specification_path": os.environ["SPECIFICATION_PATH"],
    "analysis_report_path": os.environ["ANALYSIS_REPORT_PATH"],
    "working_branch": os.environ["CURRENT_BRANCH"],
    "architect_authorization": True,
    "authorized_by": os.environ["SUBMITTED_BY"],
    "allow_tests": False,
    "allow_hardware": False,
}))
PY
)"

printf 'Submetendo implementação de %s da branch %s.\n' \
    "$SPECIFICATION_PATH" "$CURRENT_BRANCH"
printf 'Análise Ready: %s\n' "$ANALYSIS_REPORT_PATH"
printf 'Autorizada por: %s\n' "$SUBMITTED_BY"

curl --fail-with-body --silent --show-error \
    --request POST \
    "$SUBMMITION_IMPLEMENTATION_URL" \
    --header 'Content-Type: application/json' \
    --header "X-EKOM-Token: $TOKEN_EKOM" \
    --data-binary "$PAYLOAD"

printf '\nSubmissão enviada.\n'
