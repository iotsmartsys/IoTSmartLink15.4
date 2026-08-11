#!/usr/bin/env python3
"""Valida regras estruturais objetivas do roteamento e mapa EKOM 3.2."""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


REPORT_FIELDS = (
    "**Classe da fonte:** Relatório",
    "**Papel:**",
    "**Especificação:**",
    "**Revisão confrontada:**",
    "**Estado:**",
)

FORBIDDEN_SPEC_HEADINGS = (
    "análise de implementabilidade",
    "resultado da implementação",
    "evidências da implementação",
    "revisão independente",
    "relatório de implementação",
    "relatório de revisão",
    "validação e decisão do arquiteto",
)


def markdown_files(root: Path, selected: list[str]) -> list[Path]:
    if selected:
        paths = [(root / value).resolve() for value in selected]
        return [path for path in paths if path.suffix.lower() == ".md"]
    return sorted((root / "docs").rglob("*.md"))


def headings(text: str) -> list[str]:
    return [
        match.group(1).strip().lower()
        for match in re.finditer(r"^#{1,6}\s+(.+)$", text, re.MULTILINE)
    ]


def validate_report(path: Path, text: str) -> list[str]:
    if path.name.lower() == "readme.md":
        return []
    return [f"campo obrigatório ausente: {field}" for field in REPORT_FIELDS if field not in text]


def validate_adr(path: Path, text: str) -> list[str]:
    if path.name.lower() == "readme.md":
        return []
    errors: list[str] = []
    if "**Estado:**" not in text:
        errors.append("campo obrigatório ausente: **Estado:**")
    found = headings(text)
    for required in ("contexto", "decis", "consequên"):
        if not any(item.startswith(required) for item in found):
            errors.append(f"seção obrigatória ausente: {required}…")
    return errors


def validate_spec(text: str) -> list[str]:
    found = headings(text)
    return [
        f"seção de relatório não permitida na especificação: {heading}"
        for heading in found
        if any(heading.startswith(prefix) for prefix in FORBIDDEN_SPEC_HEADINGS)
    ]


def section_body(text: str, heading_prefix: str) -> str | None:
    pattern = re.compile(
        rf"^##\s+{re.escape(heading_prefix)}[^\n]*\n(.*?)(?=^##\s+|\Z)",
        re.MULTILINE | re.DOTALL | re.IGNORECASE,
    )
    match = pattern.search(text)
    return match.group(1) if match else None


def validate_knowledge_map(text: str) -> list[str]:
    errors: list[str] = []
    required_sections = (
        "1. Governança",
        "2. Índice de domínios e autoridade",
        "3. Árvore de conhecimento",
        "4. Diagrama de relações",
        "5. Lacunas",
        "6. Manutenção",
    )
    found = headings(text)
    for required in required_sections:
        if required.lower() not in found:
            errors.append(f"seção obrigatória do mapa ausente: {required}")

    for tabular in ("1. Governança", "2. Índice de domínios e autoridade"):
        body = section_body(text, tabular)
        if body is not None and "|---" not in body:
            errors.append(f"visão tabular sem tabela Markdown: {tabular}")

    tree = section_body(text, "3. Árvore de conhecimento")
    tree_not_applicable = bool(
        tree and re.search(r"^\s*(?:\*\*)?não se aplica", tree, re.MULTILINE | re.IGNORECASE)
    )
    if tree is not None and "```text" not in tree and not tree_not_applicable:
        errors.append("árvore deve conter bloco text ou justificativa 'Não se aplica'")

    diagram = section_body(text, "4. Diagrama de relações")
    diagram_not_applicable = bool(
        diagram and re.search(r"^\s*(?:\*\*)?não se aplica", diagram, re.MULTILINE | re.IGNORECASE)
    )
    if diagram is not None and "```mermaid" not in diagram and not diagram_not_applicable:
        errors.append("diagrama deve conter Mermaid ou justificativa 'Não se aplica'")
    if diagram is not None:
        for block in re.findall(r"```mermaid\s*(.*?)```", diagram, re.DOTALL | re.IGNORECASE):
            if re.search(r"<[A-ZÀ-Ü][^>\n]*>", block):
                errors.append(
                    "Mermaid contém placeholder entre <...>; use texto sem delimitadores HTML"
                )
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Valida o roteamento e o mapa estrutural do EKOM 3.2."
    )
    parser.add_argument("root", nargs="?", default=".", help="raiz do projeto")
    parser.add_argument(
        "files",
        nargs="*",
        help="arquivos relativos; omitir para verificar todos em docs/",
    )
    args = parser.parse_args()
    root = Path(args.root).resolve()
    failures: list[str] = []

    for path in markdown_files(root, args.files):
        if not path.exists():
            failures.append(f"{path}: arquivo inexistente")
            continue
        try:
            relative = path.relative_to(root)
        except ValueError:
            failures.append(f"{path}: arquivo fora da raiz")
            continue
        text = path.read_text(encoding="utf-8")
        parts = relative.parts
        errors: list[str] = []
        if len(parts) >= 2 and parts[:2] == ("docs", "reports"):
            errors = validate_report(path, text)
        elif len(parts) >= 2 and parts[:2] == ("docs", "adr"):
            errors = validate_adr(path, text)
        elif len(parts) >= 2 and parts[:2] == ("docs", "specs"):
            errors = validate_spec(text)
        if path.name.upper() == "KNOWLEDGE-MAP.MD":
            errors.extend(validate_knowledge_map(text))
        for error in errors:
            failures.append(f"{relative}: {error}")

    if failures:
        print("Roteamento documental EKOM inválido:")
        for failure in failures:
            print(f"- {failure}")
        return 1

    print("Roteamento documental EKOM válido.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
