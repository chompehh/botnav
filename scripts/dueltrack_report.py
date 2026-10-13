#!/usr/bin/env python3
"""Reproducible duel-track report for botnav bot tuning.

Every figure printed comes from a query in QUERIES below, so humans, Copilot and
AI Studio can quote the same numbers. Requires: pip install duckdb

Usage: python3 scripts/dueltrack_report.py [csv_dir] [--revision REV]

Loading rules (see docs/bot-cvar-outline.md, "Duel tracking"):
- Some export parts contain latin-1 bytes; files are re-encoded to UTF-8 first.
- Event and geometry parts have different column sets; they are unioned by name
  and read as text, so numbers are cast explicitly.
- Exact duplicate rows are removed before counting.
- participant_kind: 0 bot, 1 account human, 2 guest human.
- range_bucket 1 = under 128 units. self_footing: 0 still, 1 advance, 2 retreat, 3 lateral.
- dueltrack_learned rows are only current-format when (ctx_key & 0x60000000) = 0x60000000;
  mode = (ctx_key >> 16) & 3 (0 saber-only, 1 full force).
"""

import argparse
import glob
import os
import sys
import tempfile

try:
	import duckdb
except ImportError:
	sys.exit("duckdb is required: pip install duckdb")


QUERIES = [
	("Swing starts and roll-stab use (saber_move 14 = LS_ROLL_STAB) by participant kind", """
SELECT participant_kind,
	count(*) AS swing_starts,
	count(*) FILTER (WHERE saber_move = '14') AS roll_stabs
FROM events WHERE event_type = 'swing_start' {rev}
GROUP BY 1 ORDER BY 1"""),
	("Every roll-stab row (own or enemy)", """
SELECT participant_key, opponent_key, event_type, quality, note, self_force, amount
FROM events WHERE (saber_move = '14' OR enemy_saber_move = '14')
	AND event_type NOT IN ('regen', 'range', 'air') {rev}
ORDER BY participant_key, TRY_CAST(rel_time AS INTEGER)"""),
	("Close-range (<128u) swing-start footing input", """
SELECT participant_kind,
	count(*) AS n,
	count(*) FILTER (WHERE TRY_CAST(forwardmove AS INTEGER) < 0) AS back_input,
	count(*) FILTER (WHERE TRY_CAST(forwardmove AS INTEGER) > 0) AS forward_input,
	round(avg(TRY_CAST(radial_speed AS DOUBLE)), 1) AS avg_radial_speed
FROM events
WHERE event_type = 'swing_start' AND range_bucket = '1' AND forwardmove IS NOT NULL {rev}
GROUP BY 1 ORDER BY 1"""),
	("Close-range self_footing share (events that record footing)", """
SELECT participant_kind, self_footing, count(*) AS n,
	round(100.0 * count(*) / sum(count(*)) OVER (PARTITION BY participant_kind), 1) AS pct
FROM events
WHERE range_bucket = '1' AND self_footing IN ('0', '1', '2', '3') {rev}
GROUP BY 1, 2 ORDER BY 1, 2"""),
	("Attack chains per close-range swing end, and accepted links", """
SELECT participant_kind,
	count(*) FILTER (WHERE event_type = 'swing_end') AS swing_ends,
	count(*) FILTER (WHERE event_type = 'attack_chain') AS chains,
	round(count(*) FILTER (WHERE event_type = 'attack_chain') * 1.0 /
		nullif(count(*) FILTER (WHERE event_type = 'swing_end'), 0), 2) AS chains_per_swing,
	count(*) FILTER (WHERE event_type = 'attack_chain' AND note = 'accepted_link') AS accepted_links
FROM events WHERE range_bucket = '1' {rev}
GROUP BY 1 ORDER BY 1"""),
	("Bot back-input swing starts by controller family (5 = BURST)", """
SELECT controller_owns_inputs, controller_family, count(*) AS back_input_swings
FROM events
WHERE event_type = 'swing_start' AND participant_kind = '0' AND range_bucket = '1'
	AND TRY_CAST(forwardmove AS INTEGER) < 0 AND controller_family IS NOT NULL {rev}
GROUP BY 1, 2 ORDER BY 3 DESC"""),
	("Capture revisions present (bots)", """
SELECT capture_revision, count(*) AS events
FROM events WHERE participant_kind = '0' AND capture_revision IS NOT NULL
GROUP BY 1 ORDER BY 2 DESC"""),
	("Top current-format learned sequences by source and mode (samples >= 20)", """
SELECT source, (CAST(ctx_key AS BIGINT) >> 16) & 3 AS mode,
	stimulus || '>' || response || '>' || follow1 || '>' || follow2 AS sequence,
	sum(CAST(samples AS INTEGER)) AS samples,
	round(100.0 * sum(CAST(wins AS INTEGER)) / sum(CAST(samples AS INTEGER)), 0) AS win_pct,
	round(sum(CAST(net_damage AS DOUBLE)) / sum(CAST(samples AS INTEGER)), 1) AS net_dmg_per_sample
FROM learned
WHERE (CAST(ctx_key AS BIGINT) & 1610612736) = 1610612736
GROUP BY 1, 2, 3 HAVING sum(CAST(samples AS INTEGER)) >= 20
ORDER BY 1, 2, 4 DESC LIMIT 40"""),
	("Learned samples per token (current format)", """
SELECT source, tok, sum(CAST(samples AS INTEGER)) AS samples
FROM (SELECT source, samples, ctx_key, unnest([stimulus, response, follow1, follow2]) AS tok FROM learned)
WHERE (CAST(ctx_key AS BIGINT) & 1610612736) = 1610612736
GROUP BY 1, 2 ORDER BY 1, 3 DESC"""),
]


def to_utf8(paths, outdir):
	converted = []
	for path in paths:
		target = os.path.join(outdir, os.path.basename(path))
		with open(path, "rb") as src:
			data = src.read()
		try:
			data.decode("utf-8")
		except UnicodeDecodeError:
			data = data.decode("latin-1").encode("utf-8")
		with open(target, "wb") as dst:
			dst.write(data)
		converted.append(target)
	return converted


def load(con, name, paths):
	if not paths:
		return False
	files = ", ".join("'" + p.replace("'", "''") + "'" for p in paths)
	con.execute(f"""CREATE TABLE {name} AS SELECT DISTINCT * FROM read_csv([{files}],
		union_by_name = true, all_varchar = true, header = true)""")
	return True


def main():
	parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
	parser.add_argument("csv_dir", nargs="?", default=".")
	parser.add_argument("--revision", help="only events with this capture_revision")
	args = parser.parse_args()

	con = duckdb.connect()
	with tempfile.TemporaryDirectory() as tmp:
		events = to_utf8(sorted(glob.glob(os.path.join(args.csv_dir, "dueltrack_events*.csv"))), tmp)
		learned = to_utf8(sorted(glob.glob(os.path.join(args.csv_dir, "dueltrack_learned.csv"))), tmp)
		if not load(con, "events", events):
			sys.exit(f"no dueltrack_events*.csv in {args.csv_dir}")
		have_learned = load(con, "learned", learned)

	rev = ""
	if args.revision:
		rev = "AND capture_revision = '" + args.revision.replace("'", "''") + "'"

	for title, sql in QUERIES:
		if "FROM learned" in sql and not have_learned:
			continue
		query = sql.format(rev=rev)
		print("## " + title)
		print("```sql" + query + "\n```")
		print(con.sql(query))
		print()


if __name__ == "__main__":
	main()
