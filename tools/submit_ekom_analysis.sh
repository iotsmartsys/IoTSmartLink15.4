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

ENV_FILE="$REPOSITORY_ROOT/.env"

[ -f "$ENV_FILE" ] || fail "arquivo .env não encontrado na raiz do repositório."

if ! git -C "$REPOSITORY_ROOT" check-ignore -q .env; then
    fail "o arquivo .env precisa estar ignorado pelo Git."
fi

set -a
# shellcheck disable=SC1090
. "$ENV_FILE"
set +a

: "${SUBMMITION_URL:?Defina SUBMMITION_URL no arquivo .env}"
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

export CHANGE_ID SPECIFICATION_PATH CURRENT_BRANCH

PAYLOAD="$(python3 <<'PY'
import json
import os

print(json.dumps({
    "event": "submit_for_analysis",
    "source": "repository_command",
    "change_id": os.environ["CHANGE_ID"],
    "specification_path": os.environ["SPECIFICATION_PATH"],
    "working_branch": os.environ["CURRENT_BRANCH"],
    "submitted_by": os.environ["SUBMITTED_BY"],
}))
PY
)"

printf 'Submetendo %s da branch %s para análise.\n' \
    "$SPECIFICATION_PATH" "$CURRENT_BRANCH"

curl --fail-with-body --silent --show-error \
    --request POST \
    "$SUBMMITION_URL" \
    --header 'Content-Type: application/json' \
    --header "X-EKOM-Token: $TOKEN_EKOM" \
    --data-binary "$PAYLOAD"

printf '\nSubmissão enviada.\n'
