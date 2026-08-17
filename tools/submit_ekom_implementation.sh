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

AUTHORIZED_BY="$(git -C "$REPOSITORY_ROOT" config --get user.name || true)"
if [ -z "${AUTHORIZED_BY//[[:space:]]/}" ]; then
    fail "configure o usuário Git com: git config user.name \"Seu nome\"."
fi

if [[ "$AUTHORIZED_BY" == *$'\n'* || "$AUTHORIZED_BY" == *$'\r'* ]]; then
    fail "o usuário Git deve ocupar uma única linha."
fi

git -C "$REPOSITORY_ROOT" fetch --quiet origin "$CURRENT_BRANCH" ||
    fail "não foi possível consultar a branch remota $CURRENT_BRANCH."

LOCAL_HEAD="$(git -C "$REPOSITORY_ROOT" rev-parse HEAD)"
REMOTE_HEAD="$(git -C "$REPOSITORY_ROOT" rev-parse FETCH_HEAD)"
if [ "$LOCAL_HEAD" != "$REMOTE_HEAD" ]; then
    fail "a branch local não está sincronizada com origin/$CURRENT_BRANCH."
fi

export CURRENT_BRANCH AUTHORIZED_BY

PAYLOAD="$(python3 <<'PY'
import json
import os

print(json.dumps({
    "event": "submit_for_implementation",
    "source": "repository_command",
    "working_branch": os.environ["CURRENT_BRANCH"],
    "authorized_by": os.environ["AUTHORIZED_BY"],
    "allow_tests": False,
    "allow_hardware": False,
}))
PY
)"

printf 'Submetendo implementação da branch %s.\n' "$CURRENT_BRANCH"
printf 'Autorizada por: %s\n' "$AUTHORIZED_BY"

curl --fail-with-body --silent --show-error \
    --request POST \
    "$SUBMMITION_IMPLEMENTATION_URL" \
    --header 'Content-Type: application/json' \
    --header "X-EKOM-Token: $TOKEN_EKOM" \
    --data-binary "$PAYLOAD"

printf '\nSubmissão enviada.\n'
