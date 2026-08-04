#!/usr/bin/env python3
"""audit_graph.py -- the drift check for the mandalark agent/contract graph.

Read-only. Touches no tree it audits. Run it from any agent's session start:

    python D:/mandalark-kernel/tools/audit_graph.py

Reads docs/agents.tsv and docs/CONTRACTS.tsv and reports:

  TREE hazards   -- missing vcs, missing remote, missing upstream, unpushed commits,
                    dirty files, and dirty files edited OUTSIDE their owner's authority.
  CONTRACT drift -- every derived copy compared against its declared authority.

Exit code: 0 if no RED, 1 if any RED. AMBER never fails the run.

Design rule, learned the hard way (G21, check_tape 2026-08-03): a probe whose anchor
stops matching is reported RED as PROBE-FAILED. A check that silently tests nothing is
the failure mode this tool exists to catch, so it must never exhibit it itself.
"""

import fnmatch
import json
import re
import subprocess
import sys
from pathlib import Path

DOCS = Path(__file__).resolve().parent.parent / "docs"
AGENTS_TSV = DOCS / "agents.tsv"
CONTRACTS_TSV = DOCS / "CONTRACTS.tsv"

RED, AMBER, GREEN = "RED", "AMBER", "GREEN"
findings = []


def report(level, scope, msg):
    findings.append((level, scope, msg))


def read_tsv(path):
    """Rows as dicts. '#' comment lines skipped; first non-comment line is the header."""
    rows, header = [], None
    for line in path.read_text(encoding="utf-8").splitlines():
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        cells = line.split("\t")
        if header is None:
            header = cells
            continue
        rows.append(dict(zip(header, cells)))
    return rows


def git(tree, *args):
    """Read-only git. Returns (ok, stdout)."""
    try:
        p = subprocess.run(["git", "-C", str(tree), *args],
                           capture_output=True, text=True, timeout=60)
        # rstrip only: `status --porcelain` encodes status in the FIRST TWO COLUMNS,
        # so stripping leading whitespace corrupts every path on the first line.
        return p.returncode == 0, p.stdout.rstrip()
    except Exception as e:  # noqa: BLE001 -- a broken tree must not kill the audit
        return False, str(e)


# ---------------------------------------------------------------- probes

def probe(spec, path):
    """Run a probe from CONTRACTS.tsv against a file. Returns (ok, value_or_reason)."""
    p = Path(path)
    if not p.exists():
        return False, f"file does not exist: {path}"
    name, _, arg = spec.partition(":")

    try:
        if name == "sha256":
            import hashlib
            return True, hashlib.sha256(p.read_bytes()).hexdigest()[:16]

        text = p.read_text(encoding="utf-8", errors="replace")

        if name == "json_required_count":
            obj = json.loads(text)
            for key in ("required", "fields", "columns"):
                if key in obj and isinstance(obj[key], (list, dict)):
                    return True, str(len(obj[key]))
            return False, "no required/fields/columns list in JSON"

        if name == "json_len":
            obj = json.loads(text)
            for part in arg.split("."):
                if not isinstance(obj, dict) or part not in obj:
                    return False, f"no such JSON path: {arg}"
                obj = obj[part]
            if not isinstance(obj, (list, dict)):
                return False, f"JSON path {arg} is not a list/dict"
            return True, str(len(obj))

        if name == "json_key":
            obj = json.loads(text)
            for part in arg.split("."):
                if not isinstance(obj, dict) or part not in obj:
                    return False, f"no such JSON path: {arg}"
                obj = obj[part]
            return True, str(obj)

        if name == "tsv_max_ord":
            ords = []
            for line in text.splitlines():
                first = line.split("\t")[0].strip()
                if first.isdigit():
                    ords.append(int(first))
            if not ords:
                return False, "no numeric ord rows found"
            return True, str(max(ords))

        if name == "tsv_last":
            # Last DATA row (first cell numeric), column `arg`. A ledger row is a
            # frozen record of one moment: pinning a contract to a fixed row number
            # compares a live authority against history and goes RED the moment a new
            # row lands. C7 did exactly that when the eagle's denominator moved.
            idx = int(arg)
            last = None
            for line in text.splitlines():
                cells = line.split("\t")
                if cells and cells[0].strip().isdigit():
                    last = cells
            if last is None:
                return False, "no numeric-keyed data rows found"
            if idx >= len(last):
                return False, f"last row has {len(last)} cols, wanted {idx}"
            return True, last[idx].strip()

        if name == "tsv_field":
            row_key, _, col = arg.partition(":")
            idx = int(col)
            for line in text.splitlines():
                cells = line.split("\t")
                if cells and cells[0].strip() == row_key:
                    if idx >= len(cells):
                        return False, f"row '{row_key}' has {len(cells)} cols, wanted {idx}"
                    return True, cells[idx].strip()
            return False, f"no row keyed '{row_key}'"

        if name == "regex_capture":
            m = re.search(arg, text, re.MULTILINE)
            if not m:
                return False, f"pattern never matched: {arg}"
            return True, (m.group(1) if m.groups() else m.group(0))

        if name == "regex_count":
            n = len(re.findall(arg, text, re.MULTILINE))
            if n == 0:
                return False, f"pattern never matched: {arg}"
            return True, str(n)

        if name == "literal":
            return True, arg

    except Exception as e:  # noqa: BLE001
        return False, f"{type(e).__name__}: {e}"

    return False, f"unknown probe: {spec}"


# ---------------------------------------------------------------- tree audit

def audit_trees(agents):
    """Mechanical hazards, plus the cross-authority dirty-file check."""
    # (tree, glob) -> agent, so a dirty path can be attributed to its true owner.
    # An entry may be prefixed '!' to CARVE A PATH OUT of a broader lane the same
    # agent holds -- e.g. harness owns harness/** except TAPE-SCHEMA.tsv, which Chad
    # transferred to cascade-recorder on 2026-08-03 because it tracks the emitter.
    # Without this, the narrower grant would be shadowed by the wider one and two
    # agents would read as co-owners of a path with exactly one owner.
    authority, excluded = [], []
    for a in agents:
        for entry in filter(None, a["write_authority"].split(";")):
            neg = entry.startswith("!")
            tree, _, glob = entry.lstrip("!").partition("::")
            (excluded if neg else authority).append(
                (tree.rstrip("/"), glob, a["agent_id"]))

    def owns(tree, rel, aid):
        if any(t == tree and aid_ == aid and fnmatch.fnmatch(rel, g)
               for t, g, aid_ in excluded):
            return False
        return any(t == tree and aid_ == aid and fnmatch.fnmatch(rel, g)
                   for t, g, aid_ in authority)

    # Audit every tree an agent may WRITE to, not only trees an agent is rooted in.
    # A tree someone can write but nobody is rooted in is the blind spot this tool
    # exists to remove: mandalark-cascade-research holds the gate.py that actually
    # executes, and no agent's `tree` column names it.
    # A tree may be rooted by MORE THAN ONE agent (kernel-docs and harness both root
    # D:/mandalark-kernel with disjoint authority). Keying a dict by tree silently kept
    # the last one and inverted the dirty-file check -- caught 2026-08-03 by running it.
    rooted = {}
    for a in agents:
        rooted.setdefault(a["tree"].rstrip("/"), []).append(a)
    all_trees = list(rooted)
    for t, _g, _aid in authority:
        if t not in all_trees:
            all_trees.append(t)

    for tree in all_trees:
        here = rooted.get(tree, [])
        a = here[0] if here else None
        tree_agents = {x["agent_id"] for x in here}
        owners = sorted({aid for t, _g, aid in authority if t == tree})
        aid = "/".join(sorted(tree_agents)) if tree_agents else (owners[0] if owners else "?")
        scope = f"tree:{tree}"

        if not Path(tree).is_dir():
            report(RED, scope, f"tree does not exist (declared by agent '{aid}')")
            continue

        if a is None:
            report(AMBER, scope,
                   f"writable by {'/'.join(owners)} but no agent is ROOTED here; "
                   f"agents.tsv declares no vcs/remote/branch expectation for it")

        if a is not None and a["vcs"] == "NONE":
            report(RED, scope,
                   f"NO VERSION CONTROL. Agent '{aid}' works here with no history and "
                   f"no remote; one bad edit is unrecoverable.")
            continue

        ok, _ = git(tree, "rev-parse", "--git-dir")
        if not ok:
            report(RED, scope, "declared as git but `git rev-parse` fails here")
            continue

        ok, remotes = git(tree, "remote", "-v")
        if not remotes.strip():
            report(RED, scope,
                   f"NO REMOTE configured. Work by '{aid}' exists on this disk only.")
        elif a is not None and a["remote"] != "NONE" and a["remote"] not in remotes:
            report(AMBER, scope,
                   f"remote in agents.tsv ({a['remote']}) not found in `git remote -v`")

        ok, branch = git(tree, "rev-parse", "--abbrev-ref", "HEAD")
        if ok and a is not None and branch != a["branch"]:
            report(AMBER, scope,
                   f"on branch '{branch}', agents.tsv says '{a['branch']}'")

        ok, counts = git(tree, "rev-list", "--left-right", "--count", "@{u}...HEAD")
        if not ok:
            report(RED, scope,
                   f"branch '{branch}' has NO UPSTREAM. Commits here are unpushed and "
                   f"invisible to every other agent.")
        else:
            behind, _, ahead = counts.partition("\t")
            if ahead.strip().isdigit() and int(ahead) > 0:
                report(AMBER, scope, f"{ahead.strip()} commit(s) unpushed")
            if behind.strip().isdigit() and int(behind) > 0:
                report(AMBER, scope, f"{behind.strip()} commit(s) behind upstream")

        ok, porcelain = git(tree, "status", "--porcelain")
        for line in filter(None, porcelain.splitlines()):
            rel = line[3:].strip().strip('"')
            file_owners = sorted({aid_ for _t, _g, aid_ in authority
                                  if owns(tree, rel, aid_)})
            # git records WHAT changed, never WHICH AGENT changed it. So this check
            # does not claim to detect authorship. It reports two things it can
            # actually know, and refuses to imply a third.
            if not file_owners:
                report(RED, scope,
                       f"UNOWNED DIRTY FILE: {rel} matches no agent's write_authority. "
                       f"Either an agent wrote outside its lane or agents.tsv is "
                       f"incomplete -- both are defects, neither is a nit.")
                continue

            # A file is CONTESTED when its own owner declares it blocked. That is
            # declared state, not inferred authorship -- the only sound way to surface
            # a cross-boundary edit like the TAPE-SCHEMA.tsv v3 dispute.
            contested = [x for x in agents
                         if x["agent_id"] in file_owners
                         and Path(rel).name in x.get("blocked_on", "")]
            if contested:
                report(RED, scope,
                       f"CONTESTED: {rel} is dirty and its owner "
                       f"'{contested[0]['agent_id']}' declares it blocked: "
                       f"{contested[0]['blocked_on']}")
            else:
                report(GREEN, scope,
                       f"dirty, in-authority ({'/'.join(file_owners)}): {rel}")

        for x in here:
            if x["blocked_on"] and x["blocked_on"] != "none":
                report(AMBER, f"agent:{x['agent_id']}",
                       f"BLOCKED ON -> {x['blocked_on']}")


# ---------------------------------------------------------------- contract audit

def audit_contracts(contracts):
    for c in contracts:
        cid, scope = c["contract_id"], f"contract:{c['contract_id']}"
        ok, auth_val = probe(c["authority_probe"], c["authority"])
        if not ok:
            report(RED, scope,
                   f"{cid} PROBE-FAILED on its own AUTHORITY ({c['authority']}): "
                   f"{auth_val}. A check that cannot read its authority is testing "
                   f"nothing -- this is RED by design, never a skip.")
            continue

        copies = [x for x in c["derived_copies"].split(";") if x and x != "NONE"]
        if not copies:
            report(GREEN, scope, f"{cid} authority={auth_val} (no automated copies; "
                                 f"verifier: {c['verifier']})")
            continue

        for entry in copies:
            parts = entry.split("::")
            path, spec = parts[0], parts[1]
            strip = parts[2] if len(parts) > 2 else ""
            ok, val = probe(spec, path)
            if not ok:
                report(RED, scope, f"{cid} PROBE-FAILED on copy {path}: {val}")
                continue
            a_cmp = auth_val[len(strip):] if strip and auth_val.startswith(strip) else auth_val
            v_cmp = val[len(strip):] if strip and val.startswith(strip) else val
            if a_cmp == v_cmp:
                report(GREEN, scope, f"{cid} {path} = {val} (agrees)")
            else:
                report(RED, scope,
                       f"{cid} DRIFT: authority ({c['authority']}) = {auth_val}, "
                       f"copy ({path}) = {val}")


# ---------------------------------------------------------------- main

def audit_rulings():
    """Count Chad's open queue. A ruling nobody surfaces is the eagle-parked-two-days
    failure; reporting it every run is what stops that recurring."""
    path = DOCS / "RULINGS-PENDING.md"
    if not path.exists():
        report(AMBER, "rulings", "docs/RULINGS-PENDING.md is missing")
        return
    text = path.read_text(encoding="utf-8", errors="replace")
    open_ids = re.findall(r"^## (R-\d+)\s*—\s*(.*)$", text, re.MULTILINE)
    if not open_ids:
        report(GREEN, "rulings", "no open rulings queued for Chad")
        return
    blocking = [(i, h) for i, h in open_ids if "BLOCKING" in h]
    for rid, head in blocking:
        report(RED, "rulings",
               f"{rid} is BLOCKING and awaits Chad: {head.split('·')[1].strip() if '·' in head else head}")
    report(AMBER, "rulings",
           f"{len(open_ids)} ruling(s) queued for Chad ({len(blocking)} blocking) "
           f"-- docs/RULINGS-PENDING.md")


def main():
    if not AGENTS_TSV.exists() or not CONTRACTS_TSV.exists():
        print(f"missing {AGENTS_TSV} or {CONTRACTS_TSV}", file=sys.stderr)
        return 2

    agents = read_tsv(AGENTS_TSV)
    contracts = read_tsv(CONTRACTS_TSV)

    audit_trees(agents)
    audit_contracts(contracts)
    audit_rulings()

    order = {RED: 0, AMBER: 1, GREEN: 2}
    findings.sort(key=lambda f: (order[f[0]], f[1]))

    width = 78
    print("=" * width)
    print(f"MANDALARK GRAPH AUDIT -- {len(agents)} agents, {len(contracts)} contracts")
    print("=" * width)
    last = None
    for level, scope, msg in findings:
        if level != last:
            print(f"\n--- {level} ---")
            last = level
        print(f"  [{scope}] {msg}")

    reds = sum(1 for f in findings if f[0] == RED)
    ambers = sum(1 for f in findings if f[0] == AMBER)
    print("\n" + "=" * width)
    print(f"RED {reds}   AMBER {ambers}   GREEN {len(findings) - reds - ambers}")
    print("=" * width)
    return 1 if reds else 0


if __name__ == "__main__":
    sys.exit(main())
